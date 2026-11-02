param([string]$Artifact, [string]$BuildDirectory = (Join-Path (Split-Path $PSScriptRoot) 'build/release'))
$ErrorActionPreference = 'Stop'
$path = if ($Artifact) { "$Artifact.identity.json" } else { Join-Path $BuildDirectory 'build-identity.json' }
if (-not (Test-Path -LiteralPath $path)) { throw "Build identity is missing: $path. Configure/build/package this checkout first." }
$identity = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
if ($identity.version -notmatch '^\d+\.\d+\.\d+$' -or
    $identity.build -notmatch '^g[0-9a-f]{40}(\.dirty\.[0-9a-f]{32})?$' -or
    -not $identity.packageName) { throw "Invalid Git build identity: $path" }
return $identity
