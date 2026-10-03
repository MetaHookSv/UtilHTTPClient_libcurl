@echo off
setlocal
set "Configuration=Debug"
call "%~dp0build-UtilHTTPClient_libcurl-x86.bat" %*
exit /b %errorlevel%
