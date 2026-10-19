param([Parameter(Mandatory)][string]$QtRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$noticeRoot = Join-Path (Split-Path $PSScriptRoot) 'packaging/licenses/Qt'
foreach ($module in @('qtbase', 'qtdeclarative', 'qtsvg', 'qtimageformats')) {
    $source = Join-Path $QtRoot "sbom/$module-6.10.2.spdx.json"
    $document = Get-Content -LiteralPath $source -Raw | ConvertFrom-Json
    $attribution = [ordered]@{
        name = $document.name
        source = "Qt 6.10.2 distribution sbom/$module-6.10.2.spdx.json"
        packages = @($document.packages | Select-Object name, versionInfo, downloadLocation,
            licenseConcluded, licenseDeclared, copyrightText, homepage)
        licenseTexts = $document.hasExtractedLicensingInfos
    }
    $attribution | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $noticeRoot "$module-attribution.json") -Encoding utf8NoBOM
}
