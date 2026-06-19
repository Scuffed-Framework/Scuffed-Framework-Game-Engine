# configure.ps1
param(
    [switch]$d,
    [switch]$r
)
$buildType = "Debug"
if ($r) { $buildType = "Release" }

$root = Split-Path $PSScriptRoot -Parent
Push-Location $root

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "vswhere not found - is Visual Studio installed?" -ForegroundColor Red
    Pop-Location; exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "No VS install with MSVC toolchain found" -ForegroundColor Red
    Pop-Location; exit 1
}

$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
Write-Host "==> Importing VS environment from: $vcvars" -ForegroundColor Cyan

cmd /c "`"$vcvars`" && set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
    }
}

Write-Host "==> Running conan install..." -ForegroundColor Cyan
conan install . --profile=./profiles/windows-x64-$buildType.txt --build=missing
if ($LASTEXITCODE -ne 0) { Write-Host "conan install failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Running cmake configure..." -ForegroundColor Cyan
cmake --preset conan2-ninja
if ($LASTEXITCODE -ne 0) { Write-Host "cmake configure failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Done. Run: ./scripts/build.ps1" -ForegroundColor Green
Pop-Location