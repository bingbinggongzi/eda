param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$env:EDA_UPDATE_SNAPSHOTS = "1"
try {
    ctest --test-dir $BuildDir -C $Config --output-on-failure
} finally {
    Remove-Item Env:EDA_UPDATE_SNAPSHOTS -ErrorAction SilentlyContinue
}

Write-Host "UI snapshot baselines updated. Re-run tests without EDA_UPDATE_SNAPSHOTS before commit."
