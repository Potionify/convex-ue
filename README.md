# convex-ue

[![Vendor sync](https://github.com/Potionify/convex-ue/actions/workflows/vendor-sync.yml/badge.svg)](https://github.com/Potionify/convex-ue/actions/workflows/vendor-sync.yml)
[![License](https://img.shields.io/github/license/Potionify/convex-ue)](LICENSE)

<!-- demo video: replace this comment with the URL GitHub gives the uploaded
     convex-ue-demo.mp4, on a line of its own, so it renders inline. -->

A 35-second demo: a document edited in the editor's Convex tab shows up in
the running game, a message sent from the game shows up in the tab, then the
Functions, Logs and Traffic sections and Generate API.

A community [Convex](https://convex.dev) client plugin for Unreal Engine 5.8,
maintained by Potionify. Not an official Convex product. The protocol core is
[convex-cpp](https://github.com/Potionify/convex-cpp), vendored into the
plugin, so a checkout of this repository builds with nothing beyond the
engine.

The runtime client speaks the Convex WebSocket sync protocol: live query
subscriptions, mutations whose callback fires only after subscriptions
already reflect the write, actions, authentication, and automatic
reconnection. Every delegate fires on the game thread. The client pumps
callbacks once per tick, so an update never arrives from a socket thread.
Networking goes through the engine's own WebSockets and HTTP modules. The
plugin bundles no OpenSSL and no third-party sockets, so packaging works
the same on every platform the engine supports. One-shot HTTP queries,
mutations and actions, and file upload and download come with it.

Values round-trip byte-exact against a real backend: Int64, Bytes, the
special floats, unicode strings, and nested arrays and objects. The live
automation test checks all of them over both WebSocket and HTTP.

Blueprint gets async nodes (Connect, Query, Mutation, Run Action, Subscribe),
a chainable args builder, and a function library for reading values and
results. A dockable Convex tab in the editor runs functions, browses and
edits data, shows the schema, tails deployment logs, and inspects websocket
traffic. A Generate API button in the same tab emits typed C++ and Blueprint
wrappers for the deployed functions through a vendored copy of
[convex-ue-codegen](https://github.com/Potionify/convex-ue-codegen). On the
[Hazelight UnrealEngine-Angelscript](https://angelscript.hazelight.se) fork
the plugin builds unmodified, the client is callable from script, and codegen
emits typed `.as` wrappers.

Development and testing happen on Windows with the Win64 editor. Nothing
platform-specific is compiled in, so other platforms build from the same
source, but nobody has exercised them yet. Reports are welcome.

## Installation

1. Copy this repository, or just `Convex.uplugin` and `Source/`, into your
   project's `Plugins/Convex` folder. A packaged build (below) ships prebuilt
   binaries, so a Blueprint-only project needs no C++ toolchain.
2. Regenerate project files and build.
3. Set your deployment URL under Project Settings > Plugins > Convex
   (`DeploymentUrl`, for example `https://your-deployment.convex.cloud`).

## Building and packaging

`convex-ue-build.bat` wraps the engine's build tools. It reads the engine
root from the `CONVEX_UE_ENGINE` environment variable. Pass `-engine=<path>`
to override it.

| Command | What it does |
|---|---|
| `convex-ue-build.bat` (or `dev`) | Builds the Example editor target. The day-to-day loop. |
| `convex-ue-build.bat test` | Builds, then runs the `Convex.*` automation tests |
| `convex-ue-build.bat package` | Produces a redistributable plugin in `Dist\Convex` (RunUAT `BuildPlugin`) |
| `convex-ue-build.bat angelscript` | Builds a copy against the AngelScript fork and runs the script tests there |
| `convex-ue-build.bat clean` | Deletes `Binaries/` and `Intermediate/` |

Close the Unreal Editor before building. A running editor locks the plugin
DLLs, and Live Coding never picks up reflection changes (UCLASS, UFUNCTION,
delegate signatures). A stale editor is the usual cause of "my node has no
pins" and "my class isn't a Blueprint type".

The packaged output (about 25 MB) contains `Convex.uplugin`, `Source/` and
prebuilt `Binaries/Win64`. Drop that folder into any project's `Plugins/`.
It excludes the `Example/` host project and build intermediates. Deleting
`Binaries/Win64/*.pdb` removes about 22 MB of debug symbols.

## Quick start (C++)

```cpp
#include "ConvexSubsystem.h"
#include "ConvexClient.h"
#include "ConvexSubscription.h"

UConvexClient* Client = UConvexSubsystem::Get(this)->GetDefaultClient();

// Live subscription: fires on every change, on the game thread.
TMap<FString, FConvexValue> Args;
Args.Add(TEXT("channel"), FConvexValue::String(TEXT("general")));
UConvexSubscription* Sub = Client->SubscribeNative(TEXT("messages:list"), Args,
    [](const FConvexResult& Result)
    {
        if (Result.bSuccess) { /* Result.Value */ }
    });

// Ordered mutation: the callback fires only after subscriptions already
// reflect the write (read-your-writes).
Client->MutationNative(TEXT("messages:send"),
    {
        {TEXT("channel"), FConvexValue::String(TEXT("general"))},
        {TEXT("body"),    FConvexValue::String(TEXT("hello from UE"))},
    },
    [](const FConvexResult& Result) { /* ... */ });
```

`SetUserAuth(Jwt)` authenticates as a user and `ClearAuth()` drops it.
Tokens expire, so call `SetUserAuth` again whenever your identity provider
issues a new one. `SetUserAuthWithRefreshEvent(Jwt)` adds a reconnect
hook: the client presents the most recent token on every reconnect and
fires `OnAuthRefreshRequested` on the game thread, from where Blueprint or
script can fetch a fresh token and pass it to `SetUserAuth`. C++ can
instead give `SetUserAuthWithRefresh` a fetcher the client calls itself.
`SetAdminAuth(DeployKey)` exists in C++ only and is deliberately not
Blueprint-callable. Deployment keys are secrets, and anything referenced
from a Blueprint graph can end up in cooked assets and packaged builds. Use
admin auth only in trusted server or editor tooling code.

`GetConnectionState()` and `OnConnectionStateChanged` expose the connection.
`UploadFile(UploadUrl, Bytes, ContentType, ...)` and `DownloadFile(Url, ...)`
handle file storage. Generate the upload URL with a mutation that calls
`ctx.storage.generateUploadUrl()`.

## Quick start (Blueprint)

- **Connect To Convex**, then **Convex Query**, **Convex Mutation** or
  **Convex Run Action** (pins `OnSuccess` and `OnFailure`), or **Convex
  Subscribe** (`OnSubscribed` once, then `OnUpdate` on every change).
- Build args with **Make Empty Args** and chain **Add String Arg**, **Add
  Int Arg**, **Add Float Arg**, **Add Bool Arg**, **Add Bytes Arg**, or
  **Add Value Arg**.
- Read results with **Is Result Success**, **Get Result Value**, **Get Result
  Error Message** and **Get Result Error Data**. Read values with the
  **Get *X* Value** family, **Get Object Field** and **Get Array Element**,
  each with a `bSuccess` output.
- **Value To Json String** prints any value for debugging.

## Architecture

```
Source/ConvexCore/     vendored convex-cpp public headers (#include <convex/...>)
Source/ConvexClient/   UE layer:
  Private/convex-cpp/    vendored convex-cpp sources (compiled here)
  UEWebSocketTransport   convex transport over UE WebSockets (game-thread marshaled)
  UEHttpTransport        convex transport over UE HTTP (binary-safe)
  UConvexClient          pumped delivery via FTSTicker -> game-thread delegates
  UConvexSubsystem       default + named clients (GameInstance subsystem)
  Blueprint/             function library + async action nodes
Source/ConvexEditor/   the Convex tab, the ConvexCodegen commandlet, and the
                       vendored convex-ue-codegen emission core
```

The protocol and state-machine logic lives entirely in
[convex-cpp](https://github.com/Potionify/convex-cpp), which is sans-IO.
This plugin contributes transports, UObject wrappers, Blueprint bindings and
editor tooling. `Tools/sync-convex-cpp.ps1` refreshes the vendored copy from
a sibling checkout and records the source commit in
`Source/ConvexCore/convex-cpp.version.txt`. `Tools/check-vendor-sync.ps1`
verifies the copy is byte-identical, and CI runs it against that commit on
every pull request.

## The Convex editor tab

Open Tools > Convex (the `ConvexEditor` module). The tab connects with a
deploy key resolved the same way the Convex CLI does: process environment
first (`CONVEX_DEPLOY_KEY`, or the self-hosted pair), then `.env.local`,
`convex.env.local` and `.env` discovered upward from the project directory.
Point Editor Preferences > Plugins > Convex Editor at a specific env file to
override. Keys are never written to `.ini` files.

| Section | What it does |
|---|---|
| **Functions** | Every deployed function (live via `apiSpec`), an args editor seeded from the validator, run queries, mutations and actions, pretty results |
| **Data** | Live table browser with pagination and ordering. On dev and preview deployments with a key that allows writes, documents can be added, replaced and deleted |
| **Schema** | Declared schema per table, live index state including backfill, inferred shapes |
| **Logs** | Tails deployment function logs (structured lines, filter, pause) |
| **Traffic** | Local inspector of every Convex websocket frame in the process, from the editor session and PIE clients alike |

Writes are hard-disabled on prod-typed deployments. The deployment type comes
from the deploy-key prefix, and the `allowedOps` and `isReadOnly` fields of
`check_admin_key` gate the UI as well.

## Generated typed API

The pure emission core of
[convex-ue-codegen](https://github.com/Potionify/convex-ue-codegen) is
vendored into the `ConvexEditor` module, the same way convex-cpp is vendored
into `ConvexClient` (`Tools/sync-convex-ue-codegen.ps1` resyncs it). Three
ways to generate, all producing byte-identical output:

- The **Generate API** button in the Convex tab, which runs in-process.
- `Tools/generate-convex-api.bat`, which is headless. It uses the
  `ConvexCodegen` commandlet (`UnrealEditor-Cmd -run=ConvexCodegen
  -Out=<dir> ...`), or the standalone CLI when a convex-ue-codegen build is
  available, which skips booting headless UE.
- The standalone repo's CLI, or the web app at
  [ue-codegen.potionify.com](https://ue-codegen.potionify.com), for CI and
  non-UE workflows.

Output is `ConvexApi.h/.cpp` (typed native wrappers, `TOptional<>` for
optional args) plus `ConvexApiBP.h/.cpp` (one Blueprint node per function
with typed pins), emitted into a folder inside one of your modules.
`Example/Source/ConvexExample/ConvexApi` shows the output for the
convex-cpp integration schema. A script output directory (below) adds
`ConvexApi.as`.

## AngelScript

The plugin supports the [Hazelight UnrealEngine-Angelscript](https://angelscript.hazelight.se)
fork of UE 5.8. Nothing fork-specific is compiled in: the fork binds every
`BlueprintCallable` function and `BlueprintType` type into script by
reflection, so the subsystem, the value and args libraries, subscriptions and
the generated Blueprint libraries are all reachable from `.as` files as they
are. The client's dynamic-delegate methods (`Query`, `Mutation`, `Action`,
`Subscribe`, `SubscribePaginated` and the HTTP one-shots) carry the fork's
`ScriptCallable` metadata, which stock UE ignores. That is the whole
difference.

Function libraries become namespaces in script. `UConvexBlueprintLibrary`
is `Convex::`, and a generated `UConvexApiCountersLibrary` is
`ConvexApiCounters::`. The value, result and snapshot structs also carry
methods in script, bound from `UConvexScriptMixins` through the fork's
`ScriptMixin` metadata: `Result.Value.Field("player.stats.hp").AsFloat()`
reads a nested number, `Get` reads one field by exact name, `HasField`,
`At`, `Length`, `Keys` and `ToJson` do what their names say, and every
accessor returns a default instead of failing, with `AsIntOr` and friends
taking the fallback.

```angelscript
class AScoreboard : AActor
{
    UFUNCTION()
    void OnScores(TArray<FConvexApiScoresTopElement> Scores, FConvexResult Result)
    {
        if (Result.bSuccess) Print("top: " + Scores[0].Player);
        else Print(Result.Describe());
    }

    UFUNCTION(BlueprintOverride)
    void BeginPlay()
    {
        UConvexClient Client = UConvexSubsystem::Get().GetDefaultClient();
        FConvexApiScoresTopDelegate Handler;
        Handler.BindUFunction(this, n"OnScores");
        ConvexApi::Scores::WatchTop(Client, 10, Handler);
    }
}
```

`ConvexApi::Scores::WatchTop`, the `FConvexApiScoresTopElement` struct and
the delegate come from the generated `ConvexApi.as`. Every object shape the
deployed functions declare is a script struct, and a function with a
declared return takes a typed delegate whose second parameter is the raw
`FConvexResult`. Functions without a declared return keep
`FConvexResultDelegate`. Set **Script output directory** in Editor
Preferences > Plugins > Convex Editor (or pass `-ScriptOut=<dir>` to the
commandlet, `-script-out <dir>` to `generate-convex-api.bat`) and Generate
API writes the file there. Point it at your project's `Script/` folder. The
wrappers only call the plugin, so they need no generated C++ and no build;
the fork reloads them on save. Functions with optional arguments get two
overloads, required-only and all-arguments, because the fork's `TOptional`
cannot hold containers.

Script can also own a client: `Initialize` and `Shutdown` are
script-callable, and `SetUserAuthWithRefreshEvent` plus the
`OnAuthRefreshRequested` event let script supply a fresh token on every
reconnect.

Build against the fork with `convex-ue-build.bat angelscript` after setting
`CONVEX_UEAS_ENGINE` to the fork's root. The command copies the Example
project and the plugin to a work folder (fork binaries never mix with a
stock build), builds there, and runs `Example/Script/ConvexApi_Test.as`
through the fork's `AngelscriptTest` commandlet. The tests compile against
`ConvexApi.as` (the local backend's API) and `ConvexFixture.as` (the
codegen golden fixture, which declares return shapes), and the live ones
talk to the Docker backend at 127.0.0.1:3210, skipping when it is down.
Prebuilt binaries from `Dist/` do not load on the fork, which patches
CoreUObject and UHT. Build the plugin from source there like everything
else.

## Example project and live test

`Example/` hosts a minimal UE 5.8 project for developing the plugin
(`Tools/link-example-plugin.ps1` junctions the plugin into it). With the
local Convex backend from convex-cpp running (`docker compose up -d` in
`integration/backend`), the end-to-end automation test drives the full
stack: connect, subscribe, mutate with read-your-writes ordering, and
kitchen-sink value checks over both WebSocket and HTTP.

```
UnrealEditor-Cmd.exe Example\ConvexExample.uproject ^
  -ExecCmds="Automation RunTests Convex.Live; Quit" -unattended -nullrhi -nosplash
```

The test self-skips, with a warning, when no backend is reachable.

## License

Apache-2.0. The vendored [nlohmann/json](Source/ConvexClient/Private/convex-cpp/third_party/nlohmann/json.hpp)
is MIT, and its license sits next to each copy.
