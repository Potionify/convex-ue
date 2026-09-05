# Changelog

Notable changes per release. Versions follow [semantic versioning](https://semver.org/),
with the caveat that 0.x means the API may still move between minor versions.
The vendored convex-cpp commit is recorded in `Source/ConvexCore/convex-cpp.version.txt`.

## [0.2.0] — Unreleased

### Added

- Script mixins: on the AngelScript fork, `FConvexValue`, `FConvexResult`
  and `FConvexPaginatedSnapshot` gain methods such as
  `Result.Value.Field("player.hp").AsFloat()`, `Get`, `HasField`, `At`,
  `Keys`, `ToJson` and `Describe`. Blueprint is unchanged.
- Codegen emits a script struct for every declared object shape, a
  `ConvexApi::Types` namespace that decodes and encodes them, and a typed
  delegate per function with a declared return. Needs convex-ue-codegen
  0.2.0; regenerate the API after upgrading.
- `SetUserAuthWithRefreshEvent` and the `OnAuthRefreshRequested` event: the
  client presents the last token given to `SetUserAuth` on every reconnect
  and asks game code for a fresh one, from Blueprint or script.
- `Initialize` and `Shutdown` are script-callable, so script can own a
  client without the subsystem.
- `AttachListener` on both subscription classes keeps an adapter object
  alive for the life of the subscription.
- Live script tests against the local backend, run by
  `convex-ue-build.bat angelscript`, and a second generated script file
  in the Example project that covers typed returns.
- A chat demo in the Example project, C++ only: `UConvexChatWidget`
  subscribes to `messages:list` and sends with `messages:send`, and
  `AConvexChatGameMode` puts it on screen in Play In Editor. It is the
  scene in the README's demo video.

### Changed

- The one-shot dynamic-delegate calls (`Query`, `Mutation`, `Action`, the
  HTTP trio, `UploadFile`, `DownloadFile`) keep the delegate's target object
  alive until the callback fires.
- Generated script wrappers for functions with a declared return take the
  typed delegate instead of `FConvexResultDelegate`, and declared object
  arguments take the struct instead of `FConvexValue`.

## [0.1.1] — 2026-09-04

### Added

- AngelScript support for the Hazelight UnrealEngine-Angelscript fork: the
  client and its value, result, and subscription types are callable from
  script, and codegen can emit typed `.as` wrappers.
- `ConvexVersion.h` with `CONVEX_UE_VERSION`, the one place the plugin
  version lives in code. The `Convex-Client` header strings use it.

### Changed

- Vendored convex-cpp updated to v0.1.2. `UConvexPaginatedSubscription` now
  splits an oversized page in place instead of resetting the whole list. A
  page whose range the server reports incomplete is held back from
  `Results`, and `Status` returns to `LoadingMore` until the split repairs
  it, so the list never shows a hole. Only stale cursors still reset.
- `convex-ue-build.bat` and `Tools/generate-convex-api.bat` read the engine
  root from `CONVEX_UE_ENGINE` and stop with a message when it is unset,
  instead of falling back to a fixed path.

## [0.1.0] — 2026-07-12

First version. Runtime client over UE WebSockets and HTTP (live query
subscriptions, mutations, actions, file storage, auto-reconnect), game-thread
delegate delivery, Blueprint async nodes and function library,
`UConvexPaginatedSubscription`, and the in-editor Convex tab with a function
runner, data browser, schema viewer, log tail, wire-traffic inspector, and
Generate API button backed by a vendored convex-ue-codegen.

[0.1.1]: https://github.com/Potionify/convex-ue/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Potionify/convex-ue/releases/tag/v0.1.0
