# ConfigureWorkspace.ps1
# Configures ALL four platform/config combinations at once.
# Usage: .\ConfigureWorkspace.ps1 [-Platforms windows,linux] [-Configs Debug,Release]

param(
    [string[]]$Platforms = @("windows"),
    [string[]]$Configs = @("Debug", "Release")
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=== SF Configure Workspace ===" -ForegroundColor Cyan
Write-Host "Platforms : $($Platforms -join ', ')" -ForegroundColor Yellow
Write-Host "Configs   : $($Configs -join ', ')"   -ForegroundColor Yellow
Write-Host ""

$Failed = @()
$Success = @()

foreach ($Platform in $Platforms) {
    foreach ($Config in $Configs) {
        Write-Host "----------------------------------------" -ForegroundColor DarkGray
        Write-Host "Configuring $Platform / $Config ..." -ForegroundColor Cyan
        Write-Host "----------------------------------------" -ForegroundColor DarkGray

        & "$PSScriptRoot/ConfigureProject.ps1" -Platform $Platform -Config $Config

        if ($LASTEXITCODE -ne 0) {
            $Failed += "$Platform-$Config"
            Write-Host "FAILED: $Platform-$Config" -ForegroundColor Red
        }
        else {
            $Success += "$Platform-$Config"
            Write-Host "OK: $Platform-$Config" -ForegroundColor Green
        }

        Write-Host ""
    }
}

Write-Host "=== Workspace Configure Summary ===" -ForegroundColor Cyan
if ($Success.Count -gt 0) {
    Write-Host "Succeeded:" -ForegroundColor Green
    $Success | ForEach-Object { Write-Host "  + $_" -ForegroundColor Green }
}
if ($Failed.Count -gt 0) {
    Write-Host "Failed:" -ForegroundColor Red
    $Failed | ForEach-Object { Write-Host "  x $_" -ForegroundColor Red }
    exit 1
}

Write-Host ""
Write-Host "All configurations ready. Use .\BuildProject.ps1 -Config Debug|Release to build." -ForegroundColor Green