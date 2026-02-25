param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

if (-not $env:QT_ROOT -and (Test-Path "C:\Qt\6.8.3\msvc2022_64")) {
    $env:QT_ROOT = "C:\Qt\6.8.3\msvc2022_64"
}

Write-Host "QT_ROOT=$env:QT_ROOT"

cmake -S . -B $BuildDir -DCMAKE_PREFIX_PATH="$env:QT_ROOT"
cmake --build $BuildDir --config $Config

Write-Host "Build completed: $BuildDir ($Config)"
