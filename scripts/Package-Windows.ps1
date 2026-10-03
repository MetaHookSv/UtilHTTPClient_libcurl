param([string]$InstallRoot)
$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $InstallRoot) { $InstallRoot = Join-Path $repositoryRoot 'install/x86/Release' }
$installDirectory = (Resolve-Path -LiteralPath $InstallRoot).Path
foreach ($required in @('libcurl.dll', 'svencoop/metahook/dlls/UtilHTTPClient_libcurl.dll',
    'svencoop/metahook/dlls/UtilHTTPClient_libcurl.pdb', 'include/Interface/IUtilHTTPClient.h',
    'include/HLSDK/common/interface.h', 'README.md', 'README.zh-CN.md', 'LICENSE',
    'THIRD-PARTY-NOTICES.md', 'licenses/curl/COPYING', 'licenses/ScopeExit/LICENSE',
    'licenses/MetaHook/LICENSE', 'licenses/HLSDK/interface.h', 'licenses/VC-LTL/LICENSE')) {
    if (-not (Test-Path -LiteralPath (Join-Path $installDirectory $required) -PathType Leaf)) {
        throw "Install tree is missing $required."
    }
}
$sevenZip = (Get-Command 7z -ErrorAction Stop).Source
$archiveDirectory = Join-Path $repositoryRoot 'build/artifacts'
New-Item -ItemType Directory -Path $archiveDirectory -Force | Out-Null
$archivePath = Join-Path $archiveDirectory 'UtilHTTPClient_libcurl-windows-x86.7z'
if (Test-Path -LiteralPath $archivePath -PathType Leaf) { Remove-Item -LiteralPath $archivePath }
Push-Location -LiteralPath $installDirectory
try {
    & $sevenZip a -t7z $archivePath '.'
    if ($LASTEXITCODE -ne 0) { throw "7z packaging failed ($LASTEXITCODE)." }
    & $sevenZip t $archivePath
    if ($LASTEXITCODE -ne 0) { throw "7z integrity check failed ($LASTEXITCODE)." }
} finally { Pop-Location }
if ($env:GITHUB_OUTPUT) { "archive-path=$archivePath" >> $env:GITHUB_OUTPUT }
Write-Output "Archive: $archivePath"
