@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0build_all.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed with error code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)
endlocal
