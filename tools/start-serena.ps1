param([switch]$Foreground)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
    [Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant()))).Substring(0, 16)
if (-not $Foreground) {
    $logDirectory = Join-Path $projectRoot '.serena/server'
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    $listener = Get-NetTCPConnection -State Listen -LocalPort 8767 -ErrorAction SilentlyContinue
    if ($listener) {
        $owner = Get-CimInstance Win32_Process -Filter "ProcessId = $($listener[0].OwningProcess)"
        if (-not $owner.CommandLine.Contains($projectRoot)) {
            throw 'Port 8767 belongs to another process/project; it will not be stopped.'
        }
        Write-Output 'Reusing RedmiBook Manager Serena at http://127.0.0.1:8767/mcp'
        exit 0
    }
    $shellPath = (Get-Process -Id $PID).Path
    $worker = Start-Process -FilePath $shellPath -WindowStyle Hidden -WorkingDirectory $projectRoot `
        -ArgumentList @('-NoProfile', '-File', ('"{0}"' -f $PSCommandPath), '-Foreground') -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    while ([DateTime]::UtcNow -lt $deadline) {
        $socket = [Net.Sockets.TcpClient]::new()
        try {
            $connection = $socket.ConnectAsync('127.0.0.1', 8767)
            if ($connection.Wait(200) -and $socket.Connected) {
                $listener = Get-NetTCPConnection -State Listen -LocalPort 8767
                @{ project = $projectRoot; wrapperPid = $worker.Id; serverPid = $listener[0].OwningProcess;
                   endpoint = 'http://127.0.0.1:8767/mcp' } | ConvertTo-Json |
                    Set-Content (Join-Path $logDirectory 'instance.json') -Encoding utf8
                Write-Output 'Shared Serena is listening at http://127.0.0.1:8767/mcp'
                exit 0
            }
        }
        catch [AggregateException] { }
        finally { $socket.Dispose() }
        if ($worker.HasExited -and $worker.ExitCode -ne 0) {
            throw "Serena startup failed; see $logDirectory"
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Serena startup timed out; see $logDirectory"
}
$logDirectory = Join-Path $projectRoot '.serena/server'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Start-Transcript -Path (Join-Path $logDirectory "$PID.log") -Force | Out-Null
$mutex = [Threading.Mutex]::new($false, "Local\RedmibookManager.Serena.$projectHash")
$ownsMutex = $false
try {
    try { $ownsMutex = $mutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $ownsMutex = $true }
    if (-not $ownsMutex) {
        Write-Output 'The shared RedmibookManager Serena server is already running.'
        exit 0
    }
    $serena = (Get-Command serena -ErrorAction Stop).Source
    $contextPath = Join-Path $projectRoot '.codex/serena-full.yml'
    $editingModePath = Join-Path $projectRoot '.codex/serena-full-editing.yml'
    if (-not (Test-Path -LiteralPath $contextPath -PathType Leaf)) {
        throw "Serena context not found: $contextPath"
    }
    if (-not (Test-Path -LiteralPath $editingModePath -PathType Leaf)) {
        throw "Serena editing mode not found: $editingModePath"
    }
    $env:PYTHONUTF8 = '1'
    $env:PYTHONIOENCODING = 'utf-8'
    $env:PYTHONUNBUFFERED = '1'
    $env:PSModulePath = [Environment]::GetEnvironmentVariable('PSModulePath', 'Machine')
    & $serena start-mcp-server --project $projectRoot --context $contextPath `
        --add-mode $editingModePath `
        --language-backend LSP --agent-interface tools --transport streamable-http --host 127.0.0.1 --port 8767 `
        --enable-web-dashboard false --enable-gui-log-window false --open-web-dashboard false
    if ($LASTEXITCODE -ne 0) { throw "Serena exited with code $LASTEXITCODE" }
}
finally {
    if ($ownsMutex) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
    Stop-Transcript | Out-Null
}
