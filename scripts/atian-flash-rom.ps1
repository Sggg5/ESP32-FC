param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$Port = "COM4",
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1",
    [string]$RomOffset = "0x220000"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $RomPath)) {
    throw "ROM file not found: $RomPath"
}

if (Test-Path $IdfProfile) {
    & $IdfProfile
}
else {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
    --chip esp32s3 `
    --port $Port `
    write_flash $RomOffset $RomPath
