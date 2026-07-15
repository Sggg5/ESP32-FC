param(
    [string]$Port = "COM4",
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

& $IdfProfile
$RepoRoot = Split-Path -Parent $PSScriptRoot
$LauncherBinary = Join-Path $RepoRoot "launcher\build\launcher.bin"
$CoreBinary = Join-Path $RepoRoot "retro-core\build\retro-core.bin"

foreach ($File in @($LauncherBinary, $CoreBinary)) {
    if (-not (Test-Path -LiteralPath $File)) {
        throw "App binary not found: $File. Run scripts\atian-build.ps1 first."
    }
}

python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
    --chip esp32s3 `
    --port $Port `
    write_flash `
    0xA00000 $LauncherBinary `
    0xAC0000 $CoreBinary
