param(
    [ValidateSet('build', 'start', 'restart', 'status', 'stop')]
    [string]$Action = 'start',
    [ValidateSet('all', 'core', 'session', 'tray', 'manager')]
    [string]$Target = 'all',
    [string]$QtRoot = $env:QT_ROOT,
    [string]$QtTools = $env:QT_TOOLS,
    [string]$BuildDirectory = 'build/dev',
    [string]$MicrophoneHotkey = '',
    [switch]$SkipBuild,
    [switch]$NoTray,
    [switch]$Manager
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This launcher currently supports Windows only.' }
$checkout = Split-Path $PSScriptRoot
$stateDirectory = Join-Path $checkout '.tools/dev-runtime'
$stateFile = Join-Path $stateDirectory 'processes.json'
$buildPath = [IO.Path]::GetFullPath($BuildDirectory, $checkout)
$records = [Collections.Generic.List[object]]::new()
New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
# Serialize this check-out's controller operations without changing any installed hosts.
$controllerLock = [IO.File]::Open((Join-Path $stateDirectory 'controller.lock'),
    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

function Set-Toolchain {
    if (-not $QtRoot) { throw 'Pass -QtRoot or set QT_ROOT to Qt 6.10.2 mingw_64.' }
    $script:QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
    $toolsPath = $QtTools
    if (-not $toolsPath) { $toolsPath = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools' }
    $env:QT_ROOT = $QtRoot
    $paths = @((Join-Path $QtRoot 'bin'), (Join-Path $toolsPath 'mingw1310_64/bin'),
        (Join-Path $toolsPath 'CMake_64/bin'), (Join-Path $toolsPath 'Ninja'))
    $env:PATH = ($paths -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $env:PATH
}

function Build-Selected {
    $targets = @{
        all = @('redmibook-service', 'redmibook-session', 'redmibook-tray', 'redmibook-manager')
        core = @('redmibook-service'); session = @('redmibook-session')
        tray = @('redmibook-tray'); manager = @('redmibook-manager')
    }
    if (-not (Test-Path (Join-Path $buildPath 'CMakeCache.txt'))) {
        Invoke-Checked cmake @('-S', $checkout, '-B', $buildPath, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Debug', "-DCMAKE_PREFIX_PATH=$QtRoot", '-DMANAGER_BUILD_ID=dev-local')
    }
    Invoke-Checked cmake (@('--build', $buildPath, '--parallel', '4', '--target') + $targets[$Target])
}

function Get-OwnedProcess {
    param($Record)
    $process = Get-Process -Id $Record.pid -ErrorAction SilentlyContinue
    if ($process -and $process.StartTime.ToUniversalTime().Ticks -eq $Record.started -and
        $process.Path -eq $Record.path) { return $process }
    return $null
}

function Wait-ProcessExit {
    param([Diagnostics.Process]$Process, [int]$TimeoutMilliseconds)
    $cancellation = [Threading.CancellationTokenSource]::new($TimeoutMilliseconds)
    try {
        $Process.WaitForExitAsync($cancellation.Token).GetAwaiter().GetResult()
        return $true
    }
    catch [OperationCanceledException] { return $false }
    finally { $cancellation.Dispose() }
}

function Wait-PipeReady {
    param([string]$Name)
    $pipe = [IO.Pipes.NamedPipeClientStream]::new('.', $Name,
        [IO.Pipes.PipeDirection]::InOut, [IO.Pipes.PipeOptions]::Asynchronous)
    $cancellation = [Threading.CancellationTokenSource]::new(12000)
    try { $pipe.ConnectAsync($cancellation.Token).GetAwaiter().GetResult() }
    finally { $pipe.Dispose(); $cancellation.Dispose() }
}

function Stop-Owned {
    if (-not (Test-Path -LiteralPath $stateFile)) { return }
    $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
    $owned = @($state.processes)
    [array]::Reverse($owned)
    foreach ($record in $owned) {
        $process = Get-OwnedProcess $record
        if ($process) {
            # Only an exact PID/start time/executable from this launch is eligible.
            if ($record.role -ne 'core') { [void]$process.CloseMainWindow() }
            if (-not (Wait-ProcessExit $process 1000)) {
                $process.Kill()
                if (-not (Wait-ProcessExit $process 3000)) { throw 'Owned process did not stop.' }
            }
        }
    }
    Remove-Item -LiteralPath $stateFile
}

function Save-State {
    @{ scope = $env:MANAGER_DEV_SCOPE; build = $buildPath; processes = @($records.ToArray()) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $stateFile -Encoding utf8
}

function Start-Owned {
    param([string]$Role, [string]$Name, [string[]]$Arguments = @())
    $path = Join-Path $buildPath "bin/$Name.exe"
    $options = @{ FilePath = $path; WorkingDirectory = $stateDirectory; PassThru = $true
        WindowStyle = 'Hidden'; RedirectStandardOutput = (Join-Path $stateDirectory "$Role.stdout.log")
        RedirectStandardError = (Join-Path $stateDirectory "$Role.stderr.log") }
    if ($Arguments.Count) { $options.ArgumentList = $Arguments }
    $process = Start-Process @options
    $records.Add(@{ role = $Role; pid = $process.Id; started = $process.StartTime.ToUniversalTime().Ticks
        path = $path })
    Save-State
    return $process
}

function Invoke-Probe {
    param([string]$Name, [string[]]$Arguments)
    $options = @{ FilePath = (Join-Path $buildPath "bin/$Name.exe"); ArgumentList = $Arguments
        WorkingDirectory = $stateDirectory; PassThru = $true; WindowStyle = 'Hidden'
        RedirectStandardOutput = (Join-Path $stateDirectory 'probe.stdout.log')
        RedirectStandardError = (Join-Path $stateDirectory 'probe.stderr.log') }
    $process = Start-Process @options
    if (-not (Wait-ProcessExit $process 5000)) {
        $process.Kill()
        throw 'Development probe timed out.'
    }
    return $process.ExitCode -eq 0
}

function Read-Status {
    $output = Join-Path $stateDirectory 'status.json'
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
    if (-not (Invoke-Probe 'redmibook-session' @('--probe', '--output', ('"' + $output + '"')))) {
        return $null
    }
    return Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
}

$savedEnvironment = @{}
foreach ($name in @('PATH', 'QT_ROOT', 'MANAGER_DEV_SCOPE', 'MANAGER_DEV_CORE_PID')) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location $checkout
try {
    if ($Action -eq 'stop') { Stop-Owned; return }
    Set-Toolchain
    New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
    if ($Action -eq 'build') { Build-Selected; return }
    if ($Action -eq 'restart') { Stop-Owned }
    if ($Action -eq 'status') {
        if (-not (Test-Path -LiteralPath $stateFile)) { throw 'No tracked development runtime.' }
        $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
        $script:buildPath = $state.build
        $env:MANAGER_DEV_SCOPE = $state.scope
        $core = @($state.processes | Where-Object role -eq 'core')[0]
        if (-not (Get-OwnedProcess $core)) { throw 'Tracked development core is no longer running.' }
        $env:MANAGER_DEV_CORE_PID = [string]$core.pid
        $status = Read-Status
        if (-not $status) { throw 'Development session is unavailable.' }
        $status | ConvertTo-Json
        return
    }
    if (Test-Path -LiteralPath $stateFile) { throw 'Tracked runtime exists; use restart or stop first.' }
    if (-not $SkipBuild) { Build-Selected }
    $scopeFile = Join-Path $stateDirectory 'scope.txt'
    if (-not (Test-Path -LiteralPath $scopeFile)) {
        ('dev-' + [Guid]::NewGuid().ToString('N')) | Set-Content -LiteralPath $scopeFile
    }
    $env:MANAGER_DEV_SCOPE = (Get-Content -LiteralPath $scopeFile -Raw).Trim()
    $core = Start-Owned 'core' 'redmibook-service' @('--dev-console')
    $env:MANAGER_DEV_CORE_PID = [string]$core.Id
    Wait-PipeReady ('KobaProduction.RedmibookManager.Diagnostic.v1.Dev.' + $env:MANAGER_DEV_SCOPE)
    if (-not (Invoke-Probe 'redmibook-service' @('--probe'))) {
        throw 'Development core did not become ready; inspect core.stderr.log.'
    }
    if (-not (Invoke-Probe 'redmibook-session' @('--endpoint'))) {
        throw 'Could not resolve development session endpoint.'
    }
    $sessionEndpoint = (Get-Content (Join-Path $stateDirectory 'probe.stdout.log') -Raw).Trim()
    if ($sessionEndpoint -notmatch '^KobaProduction\.Redmibook\.Session\.[a-f0-9]{24}$') {
        throw 'Development session returned an invalid endpoint.'
    }
    $sessionArguments = @()
    if ($MicrophoneHotkey) {
        $sessionArguments = @('--microphone-hotkey', ('"' + $MicrophoneHotkey + '"'))
    }
    [void](Start-Owned 'session' 'redmibook-session' $sessionArguments)
    Wait-PipeReady $sessionEndpoint
    $status = Read-Status
    if (-not $status -or -not $status.daemon -or -not $status.compatible) {
        throw 'Development session/core identity or readiness failed; inspect session logs.'
    }
    if (-not $NoTray) { [void](Start-Owned 'tray' 'redmibook-tray') }
    if ($Manager) { [void](Start-Owned 'manager' 'redmibook-manager') }
    $status | ConvertTo-Json
    Write-Host "Development runtime: $stateDirectory (no installation or autostart changes)."
}
catch {
    if ($records.Count) { Stop-Owned }
    throw
}
finally {
    $controllerLock.Dispose()
    Pop-Location
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
}
