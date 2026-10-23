param(
    [ValidateSet('toggle', 'mute', 'unmute', 'read')][string]$Action = 'read',
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Explicit, elevated diagnostic only. Does not register a production firmware command.
$report = [ordered]@{ action = $Action; phase = 'qualification'; before = $null; targetMuted = $null; after = $null; setAccepted = $false; error = $null }
function Save-Report { $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Output -Encoding utf8 }
function Invoke-MicrophonePacket([bool]$Write, [bool]$Muted) {
    $packet = [byte[]]::new(32)
    $packet[1] = if ($Write) { 0xFB } else { 0xFA }
    $packet[3] = 0x0A
    $packet[4] = 5
    if ($Write -and -not $Muted) { $packet[6] = 1 }
    $reply = Invoke-CimMethod -InputObject $script:mifs -MethodName MiInterface -Arguments @{ InData = $packet } -OperationTimeoutSec 10
    $bytes = [byte[]]$reply.OutData
    if ($bytes.Length -ne 30 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x8000) {
        throw 'Unexpected firmware reply; no subsequent write is permitted.'
    }
    if ($Write) { return }
    if ([BitConverter]::ToUInt16($bytes, 2) -ne 0x0A00 -or
        [BitConverter]::ToUInt16($bytes, 4) -ne 5 -or
        [BitConverter]::ToUInt32($bytes, 6) -gt 1) { throw 'Invalid microphone GET payload.' }
    return ([BitConverter]::ToUInt32($bytes, 6) -eq 0)
}
try {
    if (Test-Path -LiteralPath $Output) { throw 'Use a fresh report path; preserve previous diagnostic evidence.' }
    $board = Get-CimInstance Win32_BaseBoard -OperationTimeoutSec 10
    $bios = Get-CimInstance Win32_BIOS -OperationTimeoutSec 10
    if ($board.Manufacturer -ne 'XIAOMI' -or $board.Product -ne 'TM2309' -or $bios.SMBIOSBIOSVersion -ne 'RMAMT6B0P0A0A') {
        throw 'This diagnostic is qualified only for XIAOMI TM2309 / RMAMT6B0P0A0A.'
    }
    $instances = @(Get-CimInstance -Namespace root/wmi -ClassName MICommonInterface -Filter 'Active=TRUE' -OperationTimeoutSec 10 |
        Where-Object { $_.InstanceName -match '(?i)(^|[\\/])MIFS(_.*)?$' })
    if ($instances.Count -ne 1) { throw 'Expected exactly one active MIFS instance.' }
    $script:mifs = $instances[0]
    $report.before = Invoke-MicrophonePacket $false $false
    $report.phase = 'baseline-saved'
    Save-Report
    if ($Action -ne 'read') {
        $report.targetMuted = if ($Action -eq 'toggle') { -not $report.before } else { $Action -eq 'mute' }
        $report.phase = 'write-dispatched-outcome-unknown'
        Save-Report
        Invoke-MicrophonePacket $true $report.targetMuted
        $report.setAccepted = $true
        $report.after = Invoke-MicrophonePacket $false $false
        $report.phase = if ($report.after -eq $report.targetMuted) { 'firmware-flag-matched-led-unverified' } else { 'firmware-flag-mismatch' }
    } else {
        $report.after = $report.before
        $report.phase = 'read-complete'
    }
    Save-Report
} catch {
    $report.error = $_.Exception.Message
    Save-Report
    exit 1
}
