param(
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$InstallRoot,
    [Parameter(Mandatory = $true)][ValidateSet('Debug', 'Release')][string]$Configuration
)
$ErrorActionPreference = 'Stop'
$buildDirectory = (Resolve-Path -LiteralPath $BuildRoot).Path
$installDirectory = (Resolve-Path -LiteralPath $InstallRoot).Path
$cache = [System.IO.File]::ReadAllLines((Join-Path $buildDirectory 'CMakeCache.txt'))
if ($cache -contains 'BUILD_TESTING:BOOL=OFF') {
    Write-Output 'Installed runtime test skipped: BUILD_TESTING=OFF.'
    exit 0
}
$tester = Join-Path $buildDirectory "tests/$Configuration/UtilHTTPClientTests.exe"
$testHost = Join-Path $installDirectory 'UtilHTTPClientTests.exe'
$clientDll = Join-Path $installDirectory 'svencoop/metahook/dlls/UtilHTTPClient_libcurl.dll'
$curlName = if ($Configuration -eq 'Debug') { 'libcurl-d.dll' } else { 'libcurl.dll' }
$curlDll = Join-Path $installDirectory $curlName
$copied = $false
try {
    # Model the game executable in the install root; do not preload curl or change PATH.
    [System.IO.File]::Copy($tester, $testHost, $false)
    $copied = $true
    & $testHost $clientDll $curlDll Installed
    if ($LASTEXITCODE -ne 0) { throw "Installed runtime test failed ($LASTEXITCODE)." }
} finally {
    if ($copied) { Remove-Item -LiteralPath $testHost }
}
