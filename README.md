# convex-ue

A [Convex](https://convex.dev) client plugin for **Unreal Engine 5.8**, built
on [convex-cpp](https://github.com/Potionify/convex-cpp) (vendored — the
plugin is fully self-contained, no external dependencies).

- **Realtime**: live query subscriptions over the Convex WebSocket sync
  protocol, ordered mutations (read-your-writes), actions, auto-reconnect.
- **Game-thread friendly**: all delegates fire on the game thread (the client
  pumps callbacks once per tick).
- **Full value fidelity**: Int64, Bytes, special floats, unicode — the whole
  Convex type system, byte-exact (validated live against a real backend).
- **HTTP + files**: one-shot HTTP query/mutation/action, file upload/download.
- **Blueprint**: async nodes (Connect / Query / Mutation / Run Action /
  Subscribe), chainable args builder, full value/result function library.
- **UE-native networking**: uses UE's WebSockets and HTTP modules — no
  bundled OpenSSL or third-party sockets, so packaging stays clean across
  platforms.
- **In-editor dashboard**: a dockable **Convex** tab (Tools menu) with a
  function runner, live data browser (dev-gated document editing), schema
  viewer, deployment log tail, and a websocket traffic inspector.
- **Codegen**: typed C++ and Blueprint wrappers for your deployed functions
  via [convex-ue-codegen](https://github.com/Potionify/convex-ue-codegen) —
  from the editor (Generate API), a `.bat`, or CI.

## Installation

1. Copy this repository (or just `Convex.uplugin`, `Source/`) into your
   project's `Plugins/Convex` folder — or use a packaged build (below), which
   ships prebuilt binaries so Blueprint-only projects need no C++ toolchain.
2. Regenerate project files and build.
3. Set your deployment URL in **Project Settings → Plugins → Convex**
   (`DeploymentUrl`, e.g. `https://your-deployment.convex.cloud`).

## Building & packaging

`convex-ue-build.bat` wraps the engine's build tools (override the engine path
with `-engine=<path>` or the `CONVEX_UE_ENGINE` environment variable):

| Command | What it does |
|---|---|
| `convex-ue-build.bat` (or `dev`) | Builds the Example editor target — the fast day-to-day loop |
| `convex-ue-build.bat test` | Builds, then runs the `Convex.*` automation tests |
| `convex-ue-build.bat package` | Produces a redistributable plugin in `Dist\Convex` (RunUAT `BuildPlugin`) |
| `convex-ue-build.bat clean` | Deletes `Binaries/` and `Intermediate/` |

**Close the Unreal Editor before building.** A running editor locks the plugin
DLLs, and reflection changes (UCLASS/UFUNCTION/delegate signatures) are never
picked up by Live Coding — a stale editor is the usual cause of "my node has no
pins" or "my class isn't a Blueprint type".

The packaged output (~25 MB) contains `Convex.uplugin`, `Source/` and prebuilt
`Binaries/Win64`; drop that folder into any project's `Plugins/`. It excludes
the `Example/` host project and build intermediates. Delete
`Binaries/Win64/*.pdb` (~22 MB of debug symbols) to shrink it further.

## Quick start (C++)

```cpp
#include "ConvexSubsystem.h"
#include "ConvexClient.h"
#include "ConvexSubscription.h"

UConvexClient* Client = UConvexSubsystem::Get(this)->GetDefaultClient();

// Live subscription — fires on every change, on the game thread.
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

Auth: `Client->SetUserAuth(Jwt)` (the client-side auth path) / `ClearAuth()`.
`SetAdminAuth(DeployKey)` exists in C++ only and is intentionally **not**
Blueprint-callable: deployment/admin keys are secrets — anything referenced
from a Blueprint graph can end up in cooked assets and packaged builds. Use
admin auth only in trusted server or editor tooling code.
Connection: `GetConnectionState()`, `OnConnectionStateChanged`.
Files: `UploadFile(UploadUrl, Bytes, ContentType, ...)` / `DownloadFile(Url, ...)`
(generate the upload URL with a mutation calling `ctx.storage.generateUploadUrl()`).

## Quick start (Blueprint)

- **Connect To Convex** → **Convex Query / Convex Mutation / Convex Run
  Action** (pins `OnSuccess`/`OnFailure`), **Convex Subscribe** (pins
  `OnSubscribed` once + repeating `OnUpdate`).
- Build args with **Make Empty Args** → chained **Add String Arg / Add Int
  Arg / Add Float Arg / Add Bool Arg / Add Bytes Arg / Add Value Arg**.
- Read results with **Is Result Success / Get Result Value / Get Result Error
  Message / Get Result Error Data**, and values with **Get *X* Value /
  Get Object Field / Get Array Element** (all with `bSuccess` outputs).
- Debug any value with **Value To Json String**.

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
```

The protocol/state-machine logic lives entirely in
[convex-cpp](https://github.com/Potionify/convex-cpp) (sans-IO); this plugin
contributes only transports, UObject wrappers, and Blueprint bindings. Update
the vendored copy with `Tools/sync-convex-cpp.ps1` (records the source commit
in `Source/ConvexCore/convex-cpp.version.txt`).

## The Convex editor tab

Open **Tools → Convex** (the `ConvexEditor` module). The tab connects with a
deploy key resolved the same way the Convex CLI does — process environment
first (`CONVEX_DEPLOY_KEY`, or the self-hosted pair), then `.env.local`,
`convex.env.local`, `.env` discovered upward from the project directory; point
**Editor Preferences → Plugins → Convex Editor** at a specific env file to
override. Keys are never written to `.ini` files.

| Section | What it does |
|---|---|
| **Functions** | Every deployed function (live via `apiSpec`), args editor seeded from the validator, run queries/mutations/actions, pretty results |
| **Data** | Live table browser with pagination and ordering; on dev/preview deployments (and a non-read-only key) documents can be added, replaced, and deleted |
| **Schema** | Declared schema per table, live index state incl. backfill, inferred shapes |
| **Logs** | Tails deployment function logs (structured lines, filter, pause) |
| **Traffic** | Local inspector of every Convex websocket frame in the process — the editor session and PIE clients |

Writes are hard-disabled on prod-typed deployments; the deployment type comes
from the deploy-key prefix, and `check_admin_key`'s `allowedOps`/`isReadOnly`
gate the UI as well.

## Generated typed API

`Tools/generate-convex-api.bat` (or the **Generate API** button in the Convex
tab) runs [convex-ue-codegen](https://github.com/Potionify/convex-ue-codegen)
against your deployment and emits `ConvexApi.h/.cpp` (typed native wrappers,
`TOptional<>` for optional args) plus `ConvexApiBP.h/.cpp` (one Blueprint node
per function with typed pins) into a folder inside one of your modules —
`Example/Source/ConvexExample/ConvexApi` shows the output for the integration
schema.

## Example project & live test

`Example/` hosts a minimal UE 5.8 project for developing the plugin
(`Tools/link-example-plugin.ps1` junctions the plugin into it). With the local
Convex backend from convex-cpp running (`integration/backend: docker compose
up -d`), the end-to-end automation test drives the full stack — connect,
subscribe, mutate with read-your-writes ordering, kitchen-sink value checks
over both WebSocket and HTTP:

```
UnrealEditor-Cmd.exe Example\ConvexExample.uproject ^
  -ExecCmds="Automation RunTests Convex.Live; Quit" -unattended -nullrhi -nosplash
```

The test self-skips (with a warning) when no backend is reachable.

## License

Apache-2.0. Not an official Convex product.
