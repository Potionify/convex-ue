<#
.SYNOPSIS
    Junctions the plugin (repo root) into Example/Plugins/Convex.

.DESCRIPTION
    The plugin lives at the repo root (Source/, Convex.uplugin). The Example
    host project needs it under Example/Plugins/Convex. Rather than duplicate
    files, this creates a directory junction (mklink /J). The junction is
    git-ignored and must not be committed.
#>
param()

$ErrorActionPreference = "Stop"

$PluginRoot  = Split-Path -Parent $PSScriptRoot
$LinkPath    = Join-Path $PluginRoot "Example\Plugins\Convex"
$PluginsDir  = Split-Path -Parent $LinkPath

if (-not (Test-Path $PluginsDir)) {
    New-Item -ItemType Directory -Force -Path $PluginsDir | Out-Null
}

if (Test-Path $LinkPath) {
    Write-Host "Removing existing link/dir: $LinkPath"
    # Remove junction without deleting the target contents.
    (Get-Item $LinkPath).Delete()
}

Write-Host "Creating junction:"
Write-Host "  $LinkPath  ->  $PluginRoot"

# Use cmd mklink /J for a directory junction (no admin rights required).
cmd /c mklink /J "`"$LinkPath`"" "`"$PluginRoot`"" | Out-Null

if (Test-Path $LinkPath) {
    Write-Host "Junction created."
} else {
    throw "Failed to create junction at $LinkPath"
}
