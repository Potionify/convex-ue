// Copyright Potionify. Apache-2.0.

#pragma once

// Single source of truth for the plugin version. The Convex-Client header
// strings embed it, and Convex.uplugin's VersionName mirrors it by hand
// (the .uplugin cannot read a macro). Bump both when cutting a release.
// The vendored convex-cpp has its own version in <convex/version.h>.
#define CONVEX_UE_VERSION "0.1.1"
