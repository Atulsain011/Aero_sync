$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$toolBin = "$root\.tools\llvm-mingw-20260616-ucrt-x86_64\bin"
$env:PATH = "C:\Users\Atul\.cargo\bin;$toolBin;$env:PATH"
$env:CARGO_TARGET_DIR = "$env:TEMP\aerosync_cargo_target"

# Terminate any running AeroSync instance so binaries can be overwritten
Get-Process "AeroSync", "aerosync-desktop" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Clean target cache
Remove-Item -Recurse -Force "$env:TEMP\aerosync_cargo_target\release\build\aerosync-desktop-*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:TEMP\aerosync_cargo_target\release\deps\aerosync_desktop-*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:TEMP\aerosync_cargo_target\release\aerosync-desktop.exe" -ErrorAction SilentlyContinue

Write-Host "1. Building Web Assets..." -ForegroundColor Cyan
Push-Location "$root\platform\windows\desktop_tauri"
try {
    & npm run build
} finally {
    Pop-Location
}

Write-Host "2. Building Tauri Release Executable..." -ForegroundColor Cyan
Push-Location "$root\platform\windows\desktop_tauri\src-tauri"
try {
    & cargo build --release -j 2
} finally {
    Pop-Location
}

$exeSrc = "$env:TEMP\aerosync_cargo_target\release\aerosync-desktop.exe"
$dllSrc = "$env:TEMP\aerosync_cargo_target\release\WebView2Loader.dll"

Write-Host "3. Copying Binary to Target Directories..." -ForegroundColor Cyan
Copy-Item $exeSrc "$root\build_windows\AeroSync.exe" -Force
Copy-Item $exeSrc "$root\platform\windows\desktop_tauri\AeroSync.exe" -Force
Copy-Item $exeSrc "$root\release\AeroSync.exe" -Force
Copy-Item $exeSrc "$root\AeroSync.exe" -Force

if (Test-Path $dllSrc) {
    Copy-Item $dllSrc "$root\build_windows\WebView2Loader.dll" -Force
    Copy-Item $dllSrc "$root\platform\windows\desktop_tauri\WebView2Loader.dll" -Force
    Copy-Item $dllSrc "$root\release\WebView2Loader.dll" -Force
}

Write-Host "4. Building NSIS Setup Installer with Bundled WebView2Loader.dll..." -ForegroundColor Cyan
$makensisPath = "C:\Users\Atul\AppData\Local\tauri\NSIS\Bin\makensis.exe"
if (Test-Path $makensisPath) {
    & $makensisPath "$root\platform\windows\installer\AeroSync_Installer.nsi"
    Copy-Item "$root\release\AeroSync-Setup-v1.0.6.exe" "$root\release\AeroSync-Setup-v1.0.5.exe" -Force
    Copy-Item "$root\release\AeroSync-Setup-v1.0.6.exe" "$root\release\AeroSync-Setup.exe" -Force
    Write-Host "Successfully generated AeroSync-Setup-v1.0.6.exe with all runtime dependencies." -ForegroundColor Green
}

Write-Host "5. Creating Windows Portable Zip..." -ForegroundColor Cyan
$portableDir = "$root\release\AeroSync-Windows-Portable"
if (!(Test-Path $portableDir)) { New-Item -ItemType Directory -Path $portableDir -Force }
Copy-Item "$root\release\AeroSync.exe" "$portableDir\AeroSync.exe" -Force
Copy-Item "$root\release\WebView2Loader.dll" "$portableDir\WebView2Loader.dll" -Force
Compress-Archive -Path "$portableDir\*" -DestinationPath "$root\release\AeroSync-Windows-Portable.zip" -Force

Write-Host "BUILD AND PACKAGING COMPLETED SUCCESSFULLY!" -ForegroundColor Green
