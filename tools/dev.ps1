param(
    [ValidateSet('verify', 'configure', 'build', 'test', 'format', 'package', 'package-test', 'run')]
    [string]$Action = 'verify',
    [string]$QtRoot = $env:QT_ROOT,
    [string]$QtTools = $env:QT_TOOLS
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

if (-not $QtRoot) { throw 'Set QT_ROOT to the Qt 6.10.2 mingw_64 directory or pass -QtRoot.' }
if (-not $QtTools) { $QtTools = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools' }
$env:QT_ROOT = (Resolve-Path -LiteralPath $QtRoot).Path
$toolPaths = @(
    (Join-Path $QtRoot 'bin'),
    (Join-Path $QtTools 'mingw1310_64/bin'),
    (Join-Path $QtTools 'CMake_64/bin'),
    (Join-Path $QtTools 'Ninja'),
    (Join-Path $QtTools 'QtCreator/bin/clang/bin')
)
$env:PATH = ($toolPaths -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $env:PATH
Push-Location (Split-Path $PSScriptRoot)
try {
    switch ($Action) {
        'verify' { Invoke-Checked cmake @('--workflow', '--preset', 'verify') }
        'configure' { Invoke-Checked cmake @('--preset', 'release') }
        'build' { Invoke-Checked cmake @('--build', '--preset', 'release', '--parallel', '4') }
        'test' { Invoke-Checked ctest @('--preset', 'release') }
        'format' { Invoke-Checked cmake @('--build', 'build/release', '--target', 'format') }
        'package' {
            Invoke-Checked cmake @('--workflow', '--preset', 'verify')
            Invoke-Checked cpack @('--config', 'build/release/CPackConfig.cmake')
        }
        'package-test' { & (Join-Path $PSScriptRoot 'verify-windows-package.ps1') }
        'run' {
            $app = Get-ChildItem (Join-Path (Split-Path $PSScriptRoot) 'dist/preview') -Recurse -Filter redmibook-manager.exe |
                Select-Object -First 1
            if (-not $app) { throw 'Run package-test first to prepare a verified portable preview.' }
            Start-Process -FilePath $app.FullName -WorkingDirectory $app.DirectoryName -WindowStyle Normal
        }
    }
}
finally { Pop-Location }
