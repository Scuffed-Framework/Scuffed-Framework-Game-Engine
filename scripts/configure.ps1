# configure.ps1 : run this after deleting the build folder or adding new .cpp files
# Usage: ./scripts/configure.ps1

$root = Split-Path $PSScriptRoot -Parent
Push-Location $root

Write-Host "==> Running conan install..." -ForegroundColor Cyan
conan install . --profile=./profiles/windows-x64-debug.txt --build=missing
if ($LASTEXITCODE -ne 0) { Write-Host "conan install failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Running cmake configure..." -ForegroundColor Cyan
cmake --preset conan-default
if ($LASTEXITCODE -ne 0) { Write-Host "cmake configure failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Done. Run: ./scripts/build.ps1" -ForegroundColor Green
Pop-Location
