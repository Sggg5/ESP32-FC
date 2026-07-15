param(
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1",
    [string]$ProjectVersion = "unknown"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

& $IdfProfile
$RepoRoot = Split-Path -Parent $PSScriptRoot

foreach ($Project in @("launcher", "retro-core")) {
    Write-Host "Building $Project for ATIAN-S3..."
    Push-Location (Join-Path $RepoRoot $Project)
    try {
        idf.py app `
            "-DRG_PROJECT_APP=$Project" `
            "-DRG_PROJECT_VER=$ProjectVersion" `
            -DRG_BUILD_TARGET=RG_TARGET_ATIAN_S3 `
            -DRG_BUILD_RELEASE=0 `
            -DRG_ENABLE_PROFILING=0 `
            -DRG_ENABLE_NETWORKING=1
    }
    finally {
        Pop-Location
    }
}

Write-Host "Build complete:"
Write-Host "  launcher\build\launcher.bin"
Write-Host "  retro-core\build\retro-core.bin"
