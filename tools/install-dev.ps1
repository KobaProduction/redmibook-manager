param([string]$Installer, [switch]$OpenManager)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'Installed development updates require Windows.' }
$projectRoot = Split-Path $PSScriptRoot
$version = (Get-Content (Join-Path $projectRoot 'VERSION.txt') -Raw).Trim()
if (-not $Installer) {
    $Installer = Join-Path $projectRoot "dist/RedmibookManager-$version-Windows-x86_64.exe"
}
$Installer = (Resolve-Path -LiteralPath $Installer).Path
Write-Host "Updating installed RedmiBook Manager to $version. Windows may request administrator consent."
# Elevate the installer itself, never a PowerShell helper or the GUI.
$setup = Start-Process -FilePath $Installer -ArgumentList '/S' -Verb RunAs -PassThru
$deadline = [Threading.CancellationTokenSource]::new([TimeSpan]::FromMinutes(2))
try { $null = $setup.WaitForExitAsync($deadline.Token).GetAwaiter().GetResult() }
finally { $deadline.Dispose() }
if ($setup.ExitCode -ne 0) { throw "Installer failed with exit code $($setup.ExitCode)." }
$installedBin = Join-Path $env:ProgramW6432 'Koba Production/Redmibook Manager/bin'
& (Join-Path $PSScriptRoot 'wait-installed-runtime.ps1') -BinDirectory $installedBin
$probePath = Join-Path ([IO.Path]::GetTempPath()) "Redmibook-dev-$([guid]::NewGuid().ToString('N')).json"
try {
    Write-Host 'Checking installed session/core compatibility...'
    $probe = Start-Process -FilePath (Join-Path $installedBin 'redmibook-session.exe') `
        -ArgumentList @('--probe', '--output', ('"' + $probePath + '"')) -WindowStyle Hidden -PassThru
    $deadline = [Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(15))
    try { $null = $probe.WaitForExitAsync($deadline.Token).GetAwaiter().GetResult() }
    finally { $deadline.Dispose() }
    if ($probe.ExitCode -ne 0) { throw "Installed session probe failed: $($probe.ExitCode)." }
    $health = Get-Content -LiteralPath $probePath -Raw | ConvertFrom-Json
    if (-not $health.compatible -or $health.sessionVersion -ne $version) {
        throw "Installed components are unavailable or incompatible: $($health | ConvertTo-Json -Compress)"
    }
    Write-Host "Installed $($health.sessionVersion); session/core compatible."
    if ($OpenManager) {
        Start-Process -FilePath (Join-Path $installedBin 'redmibook-manager.exe') `
            -ArgumentList '--overview' -WindowStyle Normal
    }
}
finally { Remove-Item -LiteralPath $probePath -ErrorAction SilentlyContinue }
