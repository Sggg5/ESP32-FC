param(
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1",
    [string]$ProjectVersion = "unknown"
)

$ErrorActionPreference = "Stop"

if (Test-Path $IdfProfile) {
    & $IdfProfile
}
else {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location (Join-Path $RepoRoot "retro-core")

idf.py app `
    -DRG_PROJECT_APP=retro-core `
    -DRG_PROJECT_VER=$ProjectVersion `
    -DRG_BUILD_TARGET=RG_TARGET_ATIAN_S3 `
    -DRG_BUILD_RELEASE=0 `
    -DRG_ENABLE_PROFILING=0 `
    -DRG_ENABLE_NETWORKING=1
