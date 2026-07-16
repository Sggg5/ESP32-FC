param(
    [Parameter(Mandatory = $true)]
    [string]$XiaozhiPath
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $XiaozhiPath).Path
$main = Join-Path $root "main"
$cmakePath = Join-Path $main "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw "Not a Xiaozhi source tree: $root"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$boardSource = Join-Path $scriptRoot "atian-s3"
$overlaySource = Join-Path $scriptRoot "overlay\main"
$boardTarget = Join-Path $main "boards\atian-s3"
$appTarget = Join-Path $main "apps\screen_stream"
$displayTarget = Join-Path $main "display"

New-Item -ItemType Directory -Force -Path $boardTarget, $appTarget | Out-Null
Copy-Item -Path (Join-Path $boardSource "*") -Destination $boardTarget -Recurse -Force
Copy-Item -Path (Join-Path $overlaySource "apps\screen_stream\*") -Destination $appTarget -Force
Copy-Item -LiteralPath (Join-Path $overlaySource "display\lcd_display.h") -Destination $displayTarget -Force
Copy-Item -LiteralPath (Join-Path $overlaySource "display\lcd_display.cc") -Destination $displayTarget -Force

$cmake = Get-Content -LiteralPath $cmakePath -Raw
if ($cmake -notmatch "ATIAN_SCREEN_STREAM_BEGIN") {
    $snippet = Get-Content -LiteralPath (Join-Path $scriptRoot "overlay\CMakeLists.snippet.cmake") -Raw
    $marker = "idf_component_register"
    $index = $cmake.IndexOf($marker)
    if ($index -lt 0) {
        throw "Could not find idf_component_register in $cmakePath"
    }
    $cmake = $cmake.Insert($index, "$snippet`r`n`r`n")
    [System.IO.File]::WriteAllText($cmakePath, $cmake, [System.Text.UTF8Encoding]::new($false))
}

Write-Host "ATIAN-S3 board and PC screen streaming overlay installed in $root"
