# Contributing

Thanks for your interest in contributing to convex-ue.

convex-ue is a community Unreal Engine plugin for [Convex](https://convex.dev),
maintained by Potionify. It is not an official Convex product. For questions
about the Convex platform itself, the
[Convex Discord Community](https://convex.dev/community) is the right place.

## Questions and feature requests

Please open a GitHub issue on this repository.

## Building and testing

Requirements: Unreal Engine 5.8 on Windows with the Visual Studio 2022
toolchain the engine expects. Set `CONVEX_UE_ENGINE` to the engine root, then:

```
convex-ue-build.bat test
```

That builds the Example editor target and runs every `Convex.*` automation
test. The offline tests need nothing else. The live tests talk to the
self-hosted backend from the convex-cpp repository (`docker compose up -d`
in `integration/backend` there) and skip themselves with a warning when it
is not reachable. Close the Unreal Editor first; a running editor locks the
plugin DLLs.

For the Hazelight UnrealEngine-Angelscript fork, set `CONVEX_UEAS_ENGINE` and
run `convex-ue-build.bat angelscript`. It builds a copy of the plugin against
the fork and runs the script tests in `Example/Script`.

## Vendored code

`Source/ConvexCore` and `Source/ConvexClient/Private/convex-cpp` are a copy
of [convex-cpp](https://github.com/Potionify/convex-cpp), and
`Source/ConvexEditor/Private/convex-ue-codegen` is a copy of the
[convex-ue-codegen](https://github.com/Potionify/convex-ue-codegen) emission
core. Do not edit those directories here. Change the upstream repository,
then run `Tools/sync-convex-cpp.ps1` or `Tools/sync-convex-ue-codegen.ps1`,
which also records the source commit. CI fails a pull request whose vendored
convex-cpp differs from the recorded commit.

## Pull requests

Community PRs are welcome. A few things to be aware of:

- Small, focused PRs (bug fixes, documentation, test coverage) are the
  easiest to review and integrate.
- For anything larger, open an issue first to check the direction before you
  put in too much work.
- Protocol and sync behaviour belong in convex-cpp. This repository holds
  the transports, UObject wrappers, Blueprint bindings and editor tooling.
- New UE-layer code should come with an automation test. The offline suite
  in `Source/ConvexClient/Private/Tests` runs without a backend.
- There is no enforced formatter. Match the style of the surrounding code.
