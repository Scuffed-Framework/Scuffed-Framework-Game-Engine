# BuildProject.ps1
# Usage: .\BuildProject.ps1 [-Config Debug|Release] [-Jobs 8]

param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"

$BuildDir = "build/$Config"
$Toolchain = "$BuildDir/generators/conan_toolchain.cmake"

Write-Host ""
Write-Host "=== SF Build ===" -ForegroundColor Cyan
Write-Host "Config   : $Config"   -ForegroundColor Yellow
Write-Host "BuildDir : $BuildDir" -ForegroundColor Yellow
Write-Host "Jobs     : $Jobs"     -ForegroundColor Yellow
Write-Host ""

if (-not (Test-Path "$BuildDir/CMakeCache.txt")) {
    Write-Host "ERROR: Project not configured yet. Run .\ConfigureProject.ps1 first." -ForegroundColor Red
    exit 1
}

cmake --build "$BuildDir" --config "$Config" --parallel $Jobs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Build complete! Output in 000-Build-$Config-x64\ ===" -ForegroundColor Green