param(
    [string]$BuildDirectory = 'build/debug',
    [string]$OutputDirectory = '.tools/ui-preview',
    [switch]$Build,
    [switch]$TrayOnly,
    [switch]$InteractivePresets,
    [ValidateSet('Fusion', 'FluentWinUI3')][string]$Style = 'Fusion',
    [switch]$PresetSettings,
    [ValidateSet('Dark', 'Light')][string]$Theme = 'Dark'
)
$ErrorActionPreference = 'Stop'
if ($InteractivePresets) { $TrayOnly = $true }
Set-StrictMode -Version Latest
$previewRoot = Split-Path $PSScriptRoot
$buildPath = [IO.Path]::GetFullPath($BuildDirectory, $previewRoot)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory, $previewRoot)
$cache = Get-Content -LiteralPath (Join-Path $buildPath 'CMakeCache.txt')
function Read-CacheValue([string]$name) {
    $entry = $cache | Select-String ('^' + [regex]::Escape($name) + ':[^=]+=(.*)$')
    if (-not $entry) { throw "Missing $name in configured CMake cache." }
    $entry.Matches[0].Groups[1].Value
}
$qtPath = Split-Path (Split-Path (Split-Path (Read-CacheValue 'Qt6_DIR')))
$compilerPath = Split-Path (Read-CacheValue 'CMAKE_CXX_COMPILER')
$env:PATH = (Join-Path $qtPath 'bin') + ';' + $compilerPath + ';' + $env:PATH
if ($Build) {
    $cmake = Read-CacheValue 'CMAKE_COMMAND'
    $targets = @('redmibook-ui-preview')
    if (-not $TrayOnly) { $targets += 'redmibook-manager' }
    & $cmake --build $buildPath --target @targets -j 4
    if ($LASTEXITCODE -ne 0) { throw 'Preview build failed.' }
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
function Invoke-Preview([string]$name, [string[]]$previewArguments) {
    $info = [Diagnostics.ProcessStartInfo]::new((Join-Path $buildPath "bin/$name.exe"))
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.Environment['PATH'] = (Join-Path $qtPath 'bin') + ';' + $compilerPath + ';' + $env:PATH
    $info.Environment['QT_QPA_PLATFORM'] = 'offscreen'
    $info.Environment['QT_QUICK_BACKEND'] = 'software'
    $info.Environment['QT_QPA_FONTDIR'] = Join-Path $env:SystemRoot 'Fonts'
    $info.Environment['QT_QUICK_CONTROLS_STYLE'] = $Style
    $info.Environment['MANAGER_PREVIEW_THEME'] = $Theme.ToLowerInvariant()
    foreach ($argument in $previewArguments) { $info.ArgumentList.Add($argument) }
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $deadline = [Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(30))
    try { $null = $process.WaitForExitAsync($deadline.Token).GetAwaiter().GetResult() }
    finally { $deadline.Dispose() }
    ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()) |
        Set-Content -LiteralPath (Join-Path $outputPath "$name.log")
    if ($process.ExitCode -ne 0) { throw "$name failed: $($process.ExitCode). See $outputPath/$name.log" }
}
if ($InteractivePresets) {
    $info = [Diagnostics.ProcessStartInfo]::new((Join-Path $buildPath 'bin/redmibook-ui-preview.exe'))
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $null = $info.Environment.Remove('QT_QPA_PLATFORM')
    $info.ArgumentList.Add('--interactive-presets')
    $preview = [Diagnostics.Process]::Start($info)
    Write-Host "Interactive preset preview PID $($preview.Id). Isolated settings; simulated hardware only."
    return
}
Invoke-Preview 'redmibook-ui-preview' @($outputPath)
if (-not $TrayOnly) {
    $captureMode = if ($PresetSettings) { '--preset-preview' } else { '--ui-contract' }
    Invoke-Preview 'redmibook-manager' @($captureMode, '--screenshot', (Join-Path $outputPath 'manager.png'))
}
Write-Host "Preview PNGs: $outputPath (isolated preferences; no desktop input or hardware writes)."
