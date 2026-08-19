$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$toolBin = "$root\.tools\llvm-mingw-20260616-ucrt-x86_64\bin"
$env:PATH = "C:\Users\Atul\.cargo\bin;$toolBin;$env:PATH"
$env:CARGO_TARGET_DIR = "$env:TEMP\aerosync_cargo_target"

# Clean target dir so resource.rc with new transparent icon is cleanly compiled
Remove-Item -Recurse -Force "$env:TEMP\aerosync_cargo_target\release\build\aerosync-desktop-*" -ErrorAction SilentlyContinue

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

Write-Host "4. Building NSIS Setup Installer..." -ForegroundColor Cyan
Push-Location "$root\platform\windows\desktop_tauri"
try {
    & npx @tauri-apps/cli build --bundles nsis
} finally {
    Pop-Location
}

$setupExe = Get-ChildItem "$env:TEMP\aerosync_cargo_target\release\bundle\nsis\*-setup.exe", "$root\platform\windows\desktop_tauri\src-tauri\target\release\bundle\nsis\*-setup.exe" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if ($setupExe) {
    Write-Host "Found NSIS installer: $($setupExe.FullName)" -ForegroundColor Green
    Copy-Item $setupExe.FullName "$root\release\AeroSync-Setup-v1.0.5.exe" -Force
    Copy-Item $setupExe.FullName "$root\release\AeroSync-Setup.exe" -Force
} else {
    Write-Host "Warning: NSIS installer not found in expected folder." -ForegroundColor Yellow
}

Write-Host "BUILD AND PACKAGING COMPLETED SUCCESSFULLY!" -ForegroundColor Green
