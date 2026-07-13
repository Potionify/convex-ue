<#
.SYNOPSIS
    Vendors the pure convex-ue-codegen emission core into the ConvexEditor module.

.DESCRIPTION
    Copies the emission core from a sibling convex-ue-codegen checkout into
    ConvexEditor/Private (headers + sources compiled directly into the editor
    module, mirroring how convex-cpp is vendored into ConvexClient). This is
    what makes the plugin's Generate API button and the ConvexCodegen
    commandlet self-contained: no external executable or sibling checkout is
    needed at use time.

    The core expects <convex_codegen/...> and <nlohmann/json.hpp> on the
    include path; ConvexEditor.Build.cs adds both vendored directories.

    Destination directories are wiped first for a clean sync. The source
    repo's git HEAD and the sync date are written to
    convex-ue-codegen.version.txt.

    The CLI, WASM bridge, web app and tests are NOT copied.

.PARAMETER SourceRepo
    Path to the convex-ue-codegen checkout. Defaults to the sibling repo.
#>
param(
    [string]$SourceRepo = "F:\GitHub-Potionify\potionify-workspace\convex-ue-codegen"
)

$ErrorActionPreference = "Stop"

# Plugin root = parent of this Tools directory.
$PluginRoot = Split-Path -Parent $PSScriptRoot
$EditorRoot = Join-Path $PluginRoot "Source\ConvexEditor"

$SrcInclude    = Join-Path $SourceRepo "core\include\convex_codegen"
$SrcSrc        = Join-Path $SourceRepo "core\src"
$SrcThirdParty = Join-Path $SourceRepo "third_party\nlohmann\json.hpp"

$DstInclude    = Join-Path $EditorRoot "Private\convex-ue-codegen\include\convex_codegen"
$DstSrc        = Join-Path $EditorRoot "Private\convex-ue-codegen\src"
$DstThirdParty = Join-Path $EditorRoot "Private\convex-ue-codegen\third_party\nlohmann"

# --- Validate source ------------------------------------------------------
if (-not (Test-Path $SrcInclude))    { throw "Core headers not found: $SrcInclude" }
if (-not (Test-Path $SrcSrc))        { throw "Core sources not found: $SrcSrc" }
if (-not (Test-Path $SrcThirdParty)) { throw "nlohmann/json.hpp not found: $SrcThirdParty" }

Write-Host "Syncing convex-ue-codegen core from: $SourceRepo"

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

Write-Host "  Copying headers -> $DstInclude"
Copy-Item -Path (Join-Path $SrcInclude "*.h") -Destination $DstInclude -Force

Write-Host "  Copying sources -> $DstSrc"
Copy-Item -Path (Join-Path $SrcSrc "*.cpp") -Destination $DstSrc -Force

Write-Host "  Copying nlohmann/json.hpp -> $DstThirdParty"
Copy-Item -Path $SrcThirdParty -Destination $DstThirdParty -Force

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
    "convex-ue-codegen vendored sync (emission core only)",
    "source: $SourceRepo",
    "commit: $commit",
    "synced: $stamp"
) -join "`n"

$versionPath = Join-Path $EditorRoot "convex-ue-codegen.version.txt"
Set-Content -Path $versionPath -Value $version -Encoding UTF8

Write-Host ""
Write-Host "Sync complete."
Write-Host "  commit: $commit"
Write-Host "  wrote:  $versionPath"
