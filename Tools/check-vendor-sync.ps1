<#
.SYNOPSIS
    Verifies the vendored convex-cpp copy is byte-identical to the canonical
    checkout. Exits 1 on any drift; wire into CI so a core change can never
    ship without re-running sync-convex-cpp.ps1.

.DESCRIPTION
    Compares, by SHA-256:
      * ConvexCore/Public/convex-cpp/include/convex/*.h   vs  <repo>/include/convex/*.h
      * ConvexClient/Private/convex-cpp/src/**            vs  <repo>/src/**
      * ConvexClient/Private/convex-cpp/third_party/nlohmann/json.hpp
    Reports files that differ, exist only vendored (stale), or exist only in
    the canonical repo (missing from the vendor copy).

.PARAMETER SourceRepo
    Path to the convex-cpp checkout. Defaults to the sibling repo.
#>
param(
    [string]$SourceRepo = "F:\GitHub-Potionify\potionify-workspace\convex-cpp"
)

$ErrorActionPreference = "Stop"

$PluginRoot = Split-Path -Parent $PSScriptRoot
$Pairs = @(
    @{ Canonical = Join-Path $SourceRepo "include\convex"
       Vendored  = Join-Path $PluginRoot "Source\ConvexCore\Public\convex-cpp\include\convex" },
    @{ Canonical = Join-Path $SourceRepo "src"
       Vendored  = Join-Path $PluginRoot "Source\ConvexClient\Private\convex-cpp\src" },
    @{ Canonical = Join-Path $SourceRepo "third_party\nlohmann"
       Vendored  = Join-Path $PluginRoot "Source\ConvexClient\Private\convex-cpp\third_party\nlohmann" }
)

$Drift = @()
foreach ($Pair in $Pairs) {
    if (-not (Test-Path $Pair.Vendored)) {
        $Drift += "MISSING DIR: $($Pair.Vendored)"
        continue
    }
    $CanonicalFiles = Get-ChildItem -Recurse -File $Pair.Canonical
    foreach ($File in $CanonicalFiles) {
        $Relative = $File.FullName.Substring($Pair.Canonical.Length).TrimStart('\')
        $VendoredFile = Join-Path $Pair.Vendored $Relative
        if (-not (Test-Path $VendoredFile)) {
            $Drift += "NOT VENDORED: $Relative (from $($Pair.Canonical))"
        }
        elseif ((Get-FileHash $File.FullName -Algorithm SHA256).Hash -ne
                (Get-FileHash $VendoredFile -Algorithm SHA256).Hash) {
            $Drift += "DIFFERS: $VendoredFile"
        }
    }
    $VendoredFiles = Get-ChildItem -Recurse -File $Pair.Vendored
    foreach ($File in $VendoredFiles) {
        $Relative = $File.FullName.Substring($Pair.Vendored.Length).TrimStart('\')
        if (-not (Test-Path (Join-Path $Pair.Canonical $Relative))) {
            $Drift += "STALE (no canonical counterpart): $($File.FullName)"
        }
    }
}

if ($Drift.Count -gt 0) {
    Write-Host "Vendored convex-cpp has drifted from canonical ($($Drift.Count) issue(s)):" -ForegroundColor Red
    $Drift | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host "Run Tools\sync-convex-cpp.ps1 to re-vendor." -ForegroundColor Yellow
    exit 1
}

Write-Host "Vendored convex-cpp is in sync with $SourceRepo." -ForegroundColor Green
exit 0
