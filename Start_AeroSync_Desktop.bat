@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
if exist "%SCRIPT_DIR%AeroSync.exe" (
    start "" "%SCRIPT_DIR%AeroSync.exe" %*
) else if exist "%SCRIPT_DIR%release\AeroSync.exe" (
    start "" "%SCRIPT_DIR%release\AeroSync.exe" %*
) else (
    echo [AeroSync] Error: AeroSync.exe not found in %SCRIPT_DIR%
    pause
)

