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
$dllsDirectory = Join-Path $installDirectory 'svencoop/metahook/dlls'
$curlName = if ($Configuration -eq 'Debug') { 'libcurl-d.dll' } else { 'libcurl.dll' }
$curlDll = Join-Path $dllsDirectory $curlName
$copied = $false
$previousPath = $env:PATH
try {
    # Model the game executable in the install root and the MetaHook loader, which appends
    # <game>/metahook/dlls to PATH before loading plugins; the client resolves libcurl from there.
    [System.IO.File]::Copy($tester, $testHost, $false)
    $copied = $true
    $env:PATH = "$dllsDirectory;$previousPath"
    & $testHost $clientDll $curlDll Installed
    if ($LASTEXITCODE -ne 0) { throw "Installed runtime test failed ($LASTEXITCODE)." }
} finally {
    $env:PATH = $previousPath
    if ($copied) { Remove-Item -LiteralPath $testHost }
}
