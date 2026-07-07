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

## Installation

1. Copy this repository (or just `Convex.uplugin`, `Source/`, `Tools/`) into
   your project's `Plugins/Convex` folder.
2. Regenerate project files and build.
3. Set your deployment URL in **Project Settings → Plugins → Convex**
   (`DeploymentUrl`, e.g. `https://your-deployment.convex.cloud`).

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

Auth: `Client->SetUserAuth(Jwt)` / `SetAdminAuth(DeployKey)` / `ClearAuth()`.
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
