param(
    [Parameter(Mandatory = $true)]
    [string]$RomDirectory,

    [string]$Port = "COM4",
    [string]$OutputImage,
    [switch]$NoFlash,
    [string]$IdfProfile = "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $RomDirectory -PathType Container)) {
    throw "ROM directory not found: $RomDirectory"
}
if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw "ESP-IDF PowerShell profile not found: $IdfProfile"
}

& $IdfProfile
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputImage) {
    $OutputImage = Join-Path $RepoRoot "flashfs_vfs_coexist.img"
}

$Staging = Join-Path ([System.IO.Path]::GetTempPath()) ("atian-vfs-" + [guid]::NewGuid().ToString("N"))
try {
    New-Item -ItemType Directory -Path (Join-Path $Staging "roms\nes") -Force | Out-Null

    $Seed = Join-Path $RepoRoot "flashfs_seed"
    foreach ($SeedFolder in @("config", "retro-go")) {
        $SeedPath = Join-Path $Seed $SeedFolder
        if (Test-Path -LiteralPath $SeedPath) {
            Copy-Item -LiteralPath $SeedPath -Destination $Staging -Recurse -Force
        }
    }
    Copy-Item -Path (Join-Path $RomDirectory "*.nes") -Destination (Join-Path $Staging "roms\nes") -Force

    $Roms = @(Get-ChildItem -LiteralPath (Join-Path $Staging "roms\nes") -Filter "*.nes" -File)
    if ($Roms.Count -eq 0) {
        throw "No .nes files found in: $RomDirectory"
    }

    $TotalBytes = ($Roms | Measure-Object -Property Length -Sum).Sum
    if ($TotalBytes -gt 3.8MB) {
        throw "ROM files use $TotalBytes bytes. Keep the total below about 3.8 MB for the 0x440000 VFS partition."
    }

    python "$env:IDF_PATH\components\fatfs\wl_fatfsgen.py" `
        $Staging `
        --output_file $OutputImage `
        --partition_size 0x440000 `
        --sector_size 4096 `
        --long_name_support

    Write-Host "Generated $OutputImage with $($Roms.Count) NES ROM(s)."
    Write-Warning "The launcher list in launcher\main\applications.c must contain the same filenames."

    if ($NoFlash) {
        Write-Host "NoFlash selected; the board was not changed."
    }
    else {
        python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
            --chip esp32s3 `
            --port $Port `
            write_flash 0xBC0000 $OutputImage
    }
}
finally {
    if (Test-Path -LiteralPath $Staging) {
        Remove-Item -LiteralPath $Staging -Recurse -Force
    }
}
