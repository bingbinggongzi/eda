$ErrorActionPreference = "Stop"

cmake --build build --config Debug --target eda_tests
if ($LASTEXITCODE -ne 0) { throw "Build tests failed with exit code $LASTEXITCODE" }

ctest --test-dir build -C Debug --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Stress tests failed with exit code $LASTEXITCODE" }

Write-Host "Stress checks completed."
