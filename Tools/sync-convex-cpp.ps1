<#
.SYNOPSIS
    Vendors the pure C++ convex-cpp library into the ConvexCore module.

.DESCRIPTION
    Copies the library from a sibling convex-cpp checkout:
      * headers -> ConvexCore/Public   (consumers #include <convex/client.h>)
      * sources + third_party/nlohmann -> ConvexClient/Private (compiled there:
        the vendored classes carry no UE *_API export macros, so they cannot
        cross a DLL boundary; ConvexCore only publishes the include path)
    Vendored .cpp expect "detail/..." (relative to src) and
    <nlohmann/json.hpp> (relative to third_party) on the include path.

    Destination directories are wiped first for a clean sync. The source
    repo's git HEAD and the sync date are written to convex-cpp.version.txt.

    Transports, tests, examples, integration and build output are NOT copied.

.PARAMETER SourceRepo
    Path to the convex-cpp checkout. Defaults to the sibling repo.
#>
param(
    [string]$SourceRepo = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) "convex-cpp")
)

$ErrorActionPreference = "Stop"

# Plugin root = parent of this Tools directory.
$PluginRoot = Split-Path -Parent $PSScriptRoot
$CoreRoot   = Join-Path $PluginRoot "Source\ConvexCore"
$ClientRoot = Join-Path $PluginRoot "Source\ConvexClient"

$SrcInclude     = Join-Path $SourceRepo "include\convex"
$SrcSrc         = Join-Path $SourceRepo "src"
$SrcThirdParty  = Join-Path $SourceRepo "third_party\nlohmann"

$DstInclude     = Join-Path $CoreRoot "Public\convex-cpp\include\convex"
$DstSrc         = Join-Path $ClientRoot "Private\convex-cpp\src"
$DstThirdParty  = Join-Path $ClientRoot "Private\convex-cpp\third_party\nlohmann"

# --- Validate source ------------------------------------------------------
if (-not (Test-Path $SrcInclude))    { throw "Source headers not found: $SrcInclude" }
if (-not (Test-Path $SrcSrc))        { throw "Source sources not found: $SrcSrc" }
if (-not (Test-Path $SrcThirdParty)) { throw "third_party/nlohmann not found: $SrcThirdParty" }

Write-Host "Syncing convex-cpp from: $SourceRepo"

# --- Clean destinations ---------------------------------------------------
foreach ($dir in @($DstInclude, $DstSrc, $DstThirdParty)) {
    if (Test-Path $dir) {
        Write-Host "  Removing $dir"
        Remove-Item -Recurse -Force $dir
    }
}

# --- Recreate and copy ----------------------------------------------------
New-Item -ItemType Directory -Force -Path $DstInclude    | Out-Null
New-Item -ItemType Directory -Force -Path $DstSrc        | Out-Null
New-Item -ItemType Directory -Force -Path $DstThirdParty | Out-Null

# Headers: include/convex/*.h
Write-Host "  Copying headers -> $DstInclude"
Copy-Item -Path (Join-Path $SrcInclude "*.h") -Destination $DstInclude -Force

# Sources: src/** including detail/
Write-Host "  Copying sources -> $DstSrc"
Copy-Item -Path (Join-Path $SrcSrc "*") -Destination $DstSrc -Recurse -Force

# third_party single-header
Write-Host "  Copying third_party/nlohmann -> $DstThirdParty"
Copy-Item -Path (Join-Path $SrcThirdParty "*") -Destination $DstThirdParty -Force

# --- Record provenance ----------------------------------------------------
$commit = "unknown"
try {
    Push-Location $SourceRepo
    $commit = (git rev-parse HEAD).Trim()
} catch {
    Write-Warning "Could not read git HEAD of source repo: $_"
} finally {
    Pop-Location
}

$stamp   = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
$version = @(
    "convex-cpp vendored sync",
    "source: https://github.com/Potionify/convex-cpp",
    "commit: $commit",
    "synced: $stamp"
) -join "`n"

$versionPath = Join-Path $CoreRoot "convex-cpp.version.txt"
Set-Content -Path $versionPath -Value $version -Encoding UTF8

Write-Host ""
Write-Host "Sync complete."
Write-Host "  commit: $commit"
Write-Host "  wrote:  $versionPath"
