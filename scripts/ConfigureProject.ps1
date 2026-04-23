# ConfigureProject.ps1
# Usage: .\ConfigureProject.ps1 [-Platform windows|linux] [-Config Debug|Release]

param(
    [ValidateSet("windows", "linux")]
    [string]$Platform = "windows",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$ProfileMap = @{
    "windows-Debug"   = "./profiles/windows-x64-debug.txt"
    "windows-Release" = "./profiles/windows-x64-release.txt"
    "linux-Debug"     = "./profiles/linux-x64-debug.txt"
    "linux-Release"   = "./profiles/linux-x64-release.txt"
}

$ProfileKey = "$Platform-$Config"
$Profile = $ProfileMap[$ProfileKey]
$BuildDir = "build/$Config"
$Toolchain = "$BuildDir/generators/conan_toolchain.cmake"

Write-Host ""
Write-Host "=== SF Configure ===" -ForegroundColor Cyan
Write-Host "Platform : $Platform" -ForegroundColor Yellow
Write-Host "Config   : $Config"   -ForegroundColor Yellow
Write-Host "Profile  : $Profile"  -ForegroundColor Yellow
Write-Host "BuildDir : $BuildDir" -ForegroundColor Yellow
Write-Host ""

# Step 1: Conan install
Write-Host "[1/2] Running conan install..." -ForegroundColor Cyan
conan install . --profile="$Profile" --build=missing --output-folder="$BuildDir"
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: conan install failed." -ForegroundColor Red
    exit 1
}

# Step 2: CMake configure
Write-Host ""
Write-Host "[2/2] Running cmake configure..." -ForegroundColor Cyan

if ($Platform -eq "windows") {
    cmake -B "$BuildDir" -S . `
        -DCMAKE_BUILD_TYPE="$Config" `
        -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
        -G "Ninja" `
        -A x64
}
else {
    cmake -B "$BuildDir" -S . `
        -DCMAKE_BUILD_TYPE="$Config" `
        -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
        -G "Ninja"
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: cmake configure failed." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Configure complete! Run .\BuildProject.ps1 -Config $Config to build ===" -ForegroundColor Green