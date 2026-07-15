param(
    [Parameter(Mandatory = $true)]
    [string]$XiaozhiBuildDirectory,

    [Parameter(Mandatory = $true)]
    [string]$VfsImage,

    [string]$AssetsBin,
    [string]$Port = "COM4",
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

& $IdfProfile
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Bootloader = Join-Path $XiaozhiBuildDirectory "bootloader\bootloader.bin"
$PartitionTable = Join-Path $RepoRoot "launcher\build\partition_table\partition-table.bin"
$OtaData = Join-Path $XiaozhiBuildDirectory "ota_data_initial.bin"
$Xiaozhi = Join-Path $XiaozhiBuildDirectory "xiaozhi.bin"
$Launcher = Join-Path $RepoRoot "launcher\build\launcher.bin"
$Core = Join-Path $RepoRoot "retro-core\build\retro-core.bin"

if (-not $AssetsBin) {
    $AssetsBin = Join-Path $XiaozhiBuildDirectory "generated_assets.bin"
}

$Files = @($Bootloader, $PartitionTable, $OtaData, $Xiaozhi, $AssetsBin, $Launcher, $Core, $VfsImage)
foreach ($File in $Files) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        throw "Required binary not found: $File"
    }
}

Write-Warning "This writes all application, asset and game partitions. Back up the board first."
python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
    --chip esp32s3 `
    --port $Port `
    write_flash `
    0x000000 $Bootloader `
    0x008000 $PartitionTable `
    0x00D000 $OtaData `
    0x020000 $Xiaozhi `
    0x800000 $AssetsBin `
    0xA00000 $Launcher `
    0xAC0000 $Core `
    0xBC0000 $VfsImage
