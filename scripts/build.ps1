# build.ps1 : recompile (does NOT re-run conan or cmake configure)
# Usage: ./scripts/build.ps1
param(
    [switch]$d,
    [switch]$r
)

$buildType = "debug"
if ($r) { $buildType = "release" }

$root = Split-Path $PSScriptRoot -Parent
Push-Location $root

cmake --build --preset conan2-$buildType
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Build succeeded" -ForegroundColor Green
Pop-Location
