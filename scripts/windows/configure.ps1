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

# --- find a real, complete MSVC toolset (has cl.exe) - no hardcoded version ---
$msvcToolset = Get-ChildItem "$vsPath\VC\Tools\MSVC" -Directory -ErrorAction SilentlyContinue |
Where-Object { Test-Path (Join-Path $_.FullName "bin\Hostx64\x64\cl.exe") } |
Sort-Object Name -Descending |
Select-Object -First 1

if (-not $msvcToolset) {
    Write-Host "No MSVC toolset with cl.exe found under $vsPath\VC\Tools\MSVC" -ForegroundColor Red
    Pop-Location; exit 1
}
$msvcRoot = $msvcToolset.FullName
Write-Host "==> MSVC toolset: $($msvcToolset.Name)" -ForegroundColor Cyan

# --- find newest Windows SDK that actually has headers+libs - no hardcoded version ---
$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdkVersion = Get-ChildItem "$sdkRoot\Include" -Directory -ErrorAction SilentlyContinue |
Where-Object { Test-Path (Join-Path $_.FullName "um\windows.h") } |
Sort-Object Name -Descending |
Select-Object -First 1

if (-not $sdkVersion) {
    Write-Host "No usable Windows SDK found under $sdkRoot\Include" -ForegroundColor Red
    Pop-Location; exit 1
}
$sdkVer = $sdkVersion.Name
Write-Host "==> Windows SDK: $sdkVer" -ForegroundColor Cyan

# --- assemble env vars manually, bypassing vcvars.bat entirely ---
$binDir = Join-Path $msvcRoot "bin\Hostx64\x64"
$incDirs = @(
    (Join-Path $msvcRoot "include"),
    (Join-Path $sdkRoot "Include\$sdkVer\ucrt"),
    (Join-Path $sdkRoot "Include\$sdkVer\um"),
    (Join-Path $sdkRoot "Include\$sdkVer\shared"),
    (Join-Path $sdkRoot "Include\$sdkVer\winrt")
)
$libDirs = @(
    (Join-Path $msvcRoot "lib\x64"),
    (Join-Path $sdkRoot "Lib\$sdkVer\ucrt\x64"),
    (Join-Path $sdkRoot "Lib\$sdkVer\um\x64")
)

$sdkBinDir = Join-Path $sdkRoot "bin\$sdkVer\x64"
$env:PATH = "$binDir;$sdkBinDir;$env:PATH"
$env:INCLUDE = ($incDirs -join ";")
$env:LIB = ($libDirs -join ";")
$env:LIBPATH = $env:LIB

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    Write-Host "cl.exe still not resolvable after manual env setup" -ForegroundColor Red
    Pop-Location; exit 1
}
Write-Host "Using compiler: $($cl.Source)" -ForegroundColor DarkGray

Write-Host "==> Running conan install..." -ForegroundColor Cyan
conan install . --profile=./profiles/windows-x64-$buildType.txt --build=missing
if ($LASTEXITCODE -ne 0) { Write-Host "conan install failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Running cmake configure..." -ForegroundColor Cyan
cmake --preset conan2-ninja
if ($LASTEXITCODE -ne 0) { Write-Host "cmake configure failed" -ForegroundColor Red; Pop-Location; exit 1 }

Write-Host "==> Done. Run: ./scripts/build.ps1" -ForegroundColor Green
Pop-Location