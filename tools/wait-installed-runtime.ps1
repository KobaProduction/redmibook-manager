param([Parameter(Mandatory)][string]$BinDirectory)
$ErrorActionPreference = 'Stop'
$info = [Diagnostics.ProcessStartInfo]::new((Join-Path $BinDirectory 'redmibook-session.exe'), '--endpoint')
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardOutput = $true
$info.Environment['QT_QPA_PLATFORM'] = 'offscreen'
$endpointProcess = [Diagnostics.Process]::Start($info)
$endpointText = $endpointProcess.StandardOutput.ReadToEndAsync()
$deadline = [Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(8))
try { $null = $endpointProcess.WaitForExitAsync($deadline.Token).GetAwaiter().GetResult() }
finally { $deadline.Dispose() }
if ($endpointProcess.ExitCode -ne 0) { throw 'Could not resolve installed session endpoint.' }
$endpoint = $endpointText.GetAwaiter().GetResult().Trim()
if ($endpoint -notmatch '^KobaProduction\.Redmibook\.Session\.[a-f0-9]{24}$') {
    throw 'Installed session returned an invalid endpoint.'
}
$pipe = [IO.Pipes.NamedPipeClientStream]::new('.', $endpoint, [IO.Pipes.PipeDirection]::InOut,
                                           [IO.Pipes.PipeOptions]::Asynchronous)
$deadline = [Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(15))
try { $null = $pipe.ConnectAsync($deadline.Token).GetAwaiter().GetResult() }
finally { $deadline.Dispose(); $pipe.Dispose() }
