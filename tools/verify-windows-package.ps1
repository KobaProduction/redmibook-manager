param(
    [string]$PackageDirectory = (Join-Path (Split-Path $PSScriptRoot) 'dist'),
    [switch]$SkipInstaller
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path $PSScriptRoot
$version = (Get-Content (Join-Path $projectRoot 'VERSION.txt') -Raw).Trim()
$packageName = "RedmibookManager-$version-Windows-x86_64"
$portable = Join-Path $PackageDirectory "$packageName.zip"
$installer = Join-Path $PackageDirectory "$packageName.exe"
$previewRoot = Join-Path $PackageDirectory 'preview'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('rmb package ' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$installRoot = Join-Path $env:ProgramW6432 ('Koba Production/Redmibook Manager Package Test ' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$registryKey = 'HKLM:/Software/Microsoft/Windows/CurrentVersion/Uninstall/Redmibook Manager'
$environmentNames = @('PATH', 'QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH',
    'QT_QPA_PLATFORM', 'QT_QUICK_BACKEND', 'QT_QUICK_CONTROLS_STYLE', 'QT_QPA_FONTDIR')
$savedEnvironment = @{}
$results = [ordered]@{ version = $version; portable = $false; installer = 'not-run'; screenshot = '' }

function Invoke-BoundedProcess {
    param([string]$Executable, [string]$Arguments, [string]$WorkingDirectory = $testRoot)
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) {
        $process.Kill()
        throw "Timed out: $Executable"
    }
    if ($process.ExitCode -ne 0) { throw "Exit $($process.ExitCode): $Executable" }
}

function Assert-MaintenanceLock {
    param([string]$Executable, [string]$Arguments)
    $mutex = [Threading.Mutex]::new($false, 'Global\KobaProduction.RedmibookManager.Maintenance')
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            throw 'Concurrent maintenance did not reject promptly'
        }
        if ($process.ExitCode -ne 2) { throw "Expected maintenance lock rejection, got $($process.ExitCode)" }
    }
    finally { $mutex.Dispose() }
}

function Assert-Bundle {
    param([string]$Directory)
    foreach ($relative in @('bin/redmibook-manager.exe', 'bin/redmibook-service.exe',
            'bin/redmibook-session.exe', 'bin/redmibook-tray.exe', 'bin/Qt6Core.dll', 'bin/Qt6Widgets.dll',
            'plugins/platforms/qwindows.dll', 'licenses/Qt/LGPL-3.0-only.txt',
            'licenses/THIRD_PARTY_NOTICES.md')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Directory $relative))) {
            throw "Missing deployed dependency or notice: $relative"
        }
    }
}

function Remove-TestInstallation {
    $uninstaller = Join-Path $installRoot 'Uninstall.exe'
    if (Test-Path -LiteralPath $uninstaller) {
        # _?= disables NSIS's detached temp-copy launcher. Wait for the real
        # uninstaller, running outside its install tree so it can remove itself.
        $helper = Join-Path $testRoot 'RedmibookManagerUninstall.exe'
        Copy-Item -LiteralPath $uninstaller -Destination $helper -Force
        Invoke-BoundedProcess $helper "/S _?=$installRoot"
        if (Test-Path -LiteralPath (Join-Path $installRoot 'bin/redmibook-manager.exe')) {
            throw 'Uninstaller left the application executable in place'
        }
        if (Test-Path -LiteralPath $registryKey) { throw 'Uninstaller left the machine uninstall registration' }
        if (Get-Service RedmibookManager -ErrorAction SilentlyContinue) { throw 'Uninstaller left the service registration' }
    }
}

if (-not $IsWindows) { throw 'This verifier requires PowerShell 7 on Windows.' }
if (-not $SkipInstaller) {
    $principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not [Environment]::Is64BitProcess -or
        -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Installer verification requires elevated 64-bit PowerShell. Use -SkipInstaller for portable-only checks.'
    }
}
if (-not $SkipInstaller -and (Test-Path -LiteralPath $registryKey)) {
    throw 'A machine installation already exists. Use -SkipInstaller to preserve it.'
}
if (-not $SkipInstaller -and (Get-Service RedmibookManager -ErrorAction SilentlyContinue)) {
    throw 'A service registration already exists. Use -SkipInstaller to preserve it.'
}
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    foreach ($name in $environmentNames) {
        $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }
    $env:PATH = "$env:SystemRoot/system32;$env:SystemRoot"
    Remove-Item Env:QT_PLUGIN_PATH, Env:QML_IMPORT_PATH, Env:QML2_IMPORT_PATH -ErrorAction SilentlyContinue
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_BACKEND = 'software'
    $env:QT_QUICK_CONTROLS_STYLE = 'Fusion'
    $env:QT_QPA_FONTDIR = Join-Path $env:SystemRoot 'Fonts'
    Expand-Archive -LiteralPath $portable -DestinationPath (Join-Path $testRoot 'portable app')
    $portableExe = Get-ChildItem (Join-Path $testRoot 'portable app') -Recurse -Filter redmibook-manager.exe | Select-Object -First 1
    if (-not $portableExe) { throw 'Portable executable was not found' }
    $bundleRoot = Split-Path $portableExe.DirectoryName
    Assert-Bundle $bundleRoot
    $screenshot = Join-Path $PackageDirectory 'manager-overview.png'
    Invoke-BoundedProcess $portableExe.FullName "--smoke-test --screenshot `"$screenshot`""
    $results.portable = $true
    $results.screenshot = $screenshot
    if (-not $SkipInstaller) {
        Assert-MaintenanceLock $installer "/S /D=$installRoot"
        Invoke-BoundedProcess $installer "/S /D=$installRoot"
        Assert-Bundle $installRoot
        Invoke-BoundedProcess (Join-Path $installRoot 'bin/redmibook-service.exe') '--probe'
        Invoke-BoundedProcess (Join-Path $installRoot 'bin/redmibook-manager.exe') '--smoke-test'
        Invoke-BoundedProcess $installer "/S /D=$installRoot"
        Assert-Bundle $installRoot
        Invoke-BoundedProcess (Join-Path $installRoot 'bin/redmibook-service.exe') '--probe'
        Invoke-BoundedProcess (Join-Path $installRoot 'bin/redmibook-manager.exe') '--smoke-test'
        Assert-MaintenanceLock (Join-Path $installRoot 'Uninstall.exe') "/S _?=$installRoot"
        $results.maintenanceLock = 'setup-and-uninstall-rejected-while-locked'
        Remove-TestInstallation
        if (Test-Path -LiteralPath $registryKey) { throw 'Uninstaller left the machine uninstall registration' }
        $results.installer = 'install-run-reinstall-run-uninstall-pass'
        $results.upgrade = 'A-to-B not exercised by this same-build package check'
    }
    New-Item -ItemType Directory -Force $previewRoot | Out-Null
    Copy-Item -LiteralPath $bundleRoot -Destination $previewRoot -Recurse -Force
    $results | ConvertTo-Json | Set-Content (Join-Path $PackageDirectory 'verification.json') -Encoding utf8NoBOM
    Write-Output ($results | ConvertTo-Json)
}
finally {
    try { Remove-TestInstallation }
    finally {
        foreach ($name in $environmentNames) {
            [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
        }
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if (-not $resolvedTestRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Split-Path $resolvedTestRoot -Leaf).StartsWith('rmb package ')) {
            throw 'Refusing cleanup outside the owned temporary package-test directory'
        }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
