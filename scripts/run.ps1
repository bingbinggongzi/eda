param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "$Config\eda_ui_prototype.exe"
if (-not (Test-Path $exe)) {
    throw "Executable not found: $exe. Run scripts/build.ps1 first."
}

Write-Host "Starting: $exe"
Start-Process -FilePath $exe
