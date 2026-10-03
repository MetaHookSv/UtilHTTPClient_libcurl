@echo off
setlocal
if not "%Configuration%"=="Debug" if not "%Configuration%"=="Release" exit /b 2
for %%I in ("%~dp0..") do set "TaskSourceDir=%%~fI"
set "TaskBuildDir=%TaskSourceDir%\build\x86\%Configuration%"
set "TaskInstallDir=%TaskSourceDir%\install\x86\%Configuration%"

cmake -G "Visual Studio 17 2022" -A Win32 -S "%TaskSourceDir%" -B "%TaskBuildDir%" -DCMAKE_INSTALL_PREFIX="%TaskInstallDir%" %*
if errorlevel 1 exit /b %errorlevel%
cmake --build "%TaskBuildDir%" --config %Configuration% --parallel
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir "%TaskBuildDir%" -C %Configuration% --output-on-failure
if errorlevel 1 exit /b %errorlevel%
cmake --install "%TaskBuildDir%" --config %Configuration%
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Test-Installed.ps1" -BuildRoot "%TaskBuildDir%" -InstallRoot "%TaskInstallDir%" -Configuration %Configuration%
exit /b %errorlevel%
