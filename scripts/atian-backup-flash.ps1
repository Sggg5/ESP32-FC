param(
    [string]$Port = "COM4",
    [string]$OutputFile = ("atian-s3-backup-{0}.bin" -f (Get-Date -Format "yyyyMMdd-HHmmss")),
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

& $IdfProfile
python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
    --chip esp32s3 `
    --port $Port `
    read_flash 0x0 0x1000000 $OutputFile

Write-Host "Saved the complete 16 MB flash backup to: $OutputFile"
