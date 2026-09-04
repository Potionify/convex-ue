# Changelog

Notable changes per release. Versions follow [semantic versioning](https://semver.org/),
with the caveat that 0.x means the API may still move between minor versions.
The vendored convex-cpp commit is recorded in `Source/ConvexCore/convex-cpp.version.txt`.

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

## [0.1.0] — 2026-07-12

First version. Runtime client over UE WebSockets and HTTP (live query
subscriptions, mutations, actions, file storage, auto-reconnect), game-thread
delegate delivery, Blueprint async nodes and function library,
`UConvexPaginatedSubscription`, and the in-editor Convex tab with a function
runner, data browser, schema viewer, log tail, wire-traffic inspector, and
Generate API button backed by a vendored convex-ue-codegen.

[0.1.1]: https://github.com/Potionify/convex-ue/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Potionify/convex-ue/releases/tag/v0.1.0
