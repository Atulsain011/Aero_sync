# AeroSync Unified Complete Build Script
# Builds: Windows GUI Client (AeroSync.exe), Android APK (app-debug.apk), and C++ Test Executables

$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$toolBin = "$root\.tools\llvm-mingw-20260616-ucrt-x86_64\bin"
$jdkPath = "$root\.tools\jdk-17.0.14+7"
$ndkPath = "C:\Users\Atul\AppData\Local\Android\Sdk\ndk\25.1.8937393"
$sdkPath = "C:\Users\Atul\AppData\Local\Android\Sdk"
$cmake   = "C:\Program Files\CMake\bin\cmake.exe"
$ninja   = "C:\Users\Atul\AppData\Local\Android\Sdk\cmake\3.22.1\bin\ninja.exe"
$gradle  = "$root\platform\android\gradlew.bat"
if (-not (Test-Path $gradle)) {
    $gradle = "C:\Users\Atul\.gradle\wrapper\dists\gradle-8.10.2-all\7iv73wktx1xtkvlq19urqw1wm\gradle-8.10.2\bin\gradle.bat"
}

$ninjaDir = Split-Path -Parent $ninja
$env:PATH = "$ninjaDir;$toolBin;$env:PATH"
$env:JAVA_HOME = $jdkPath
$env:ANDROID_HOME = $sdkPath
$env:ANDROID_NDK_HOME = $ndkPath

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " AEROSYNC UNIFIED PRODUCTION BUILD PIPELINE" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Build Windows Desktop Native Daemon & Binaries
Write-Host "`n[1/5] Building Windows Native Core Daemon (aerosync_daemon.exe)..." -ForegroundColor Yellow
& $cmake -B "$root\build_windows" -S "$root\platform\windows" -G "Ninja" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_CXX_COMPILER=$toolBin\clang++.exe" `
    "-DCMAKE_C_COMPILER=$toolBin\clang.exe" `
    "-DCMAKE_BUILD_TYPE=Release"
& $cmake --build "$root\build_windows"
Copy-Item -Path "$toolBin\*.dll" -Destination "$root\build_windows" -Force -ErrorAction SilentlyContinue
Copy-Item "$root\build_windows\aerosync_daemon.exe" "$root\platform\windows\desktop_tauri\aerosync_daemon.exe" -Force
Copy-Item "$root\build_windows\aerosync_daemon.exe" "$root\aerosync_daemon.exe" -Force
Copy-Item -Path "$toolBin\*.dll" -Destination "$root\platform\windows\desktop_tauri" -Force -ErrorAction SilentlyContinue

# 2. Build Tauri + React + TypeScript Desktop App (AeroSync.exe)
Write-Host "`n[2/5] Building Tauri + React + TypeScript Desktop App (AeroSync.exe)..." -ForegroundColor Yellow
Push-Location "$root\platform\windows\desktop_tauri"
try {
    & npm run build
    $env:PATH = "C:\Users\Atul\.cargo\bin;$toolBin;$env:PATH"
    $env:CARGO_TARGET_DIR = "$env:TEMP\aerosync_cargo_target"
    Push-Location "src-tauri"
    try {
        & cargo build --release -j 2
        Copy-Item "$env:TEMP\aerosync_cargo_target\release\aerosync-desktop.exe" "$root\build_windows\AeroSync.exe" -Force
        Copy-Item "$env:TEMP\aerosync_cargo_target\release\aerosync-desktop.exe" "$root\platform\windows\desktop_tauri\AeroSync.exe" -Force
        if (Test-Path "$env:TEMP\aerosync_cargo_target\release\WebView2Loader.dll") {
            Copy-Item "$env:TEMP\aerosync_cargo_target\release\WebView2Loader.dll" "$root\build_windows\WebView2Loader.dll" -Force
            Copy-Item "$env:TEMP\aerosync_cargo_target\release\WebView2Loader.dll" "$root\platform\windows\desktop_tauri\WebView2Loader.dll" -Force
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
}

# 3. Build Test Suite & Throughput Benchmark
Write-Host "`n[3/5] Building C++ Test Suite & Benchmark Harness..." -ForegroundColor Yellow
& $cmake -B "$root\build_tests" -S "$root\tests" -G "Ninja" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_CXX_COMPILER=$toolBin\clang++.exe" `
    "-DCMAKE_C_COMPILER=$toolBin\clang.exe" `
    "-DCMAKE_BUILD_TYPE=Release"
& $cmake --build "$root\build_tests"
Copy-Item -Path "$toolBin\*.dll" -Destination "$root\build_tests" -Force -ErrorAction SilentlyContinue

# 4. Configure Windows Firewall Inbound Rules
Write-Host "`n[+] Configuring Inbound Windows Firewall Rules..." -ForegroundColor Yellow
try {
    & powershell -ExecutionPolicy Bypass -File "$root\platform\windows\installer_firewall_rule.ps1" -AppPath "$root\build_windows\AeroSync.exe"
} catch {
    Write-Host "Firewall rule setup note: Run as administrator if prompt appears." -ForegroundColor Gray
}

# 5. Build Android Mobile App APK (with JNI C++ Core for 4 ABIs)
Write-Host "`n[5/5] Building Android APK (AeroSync.apk & app-debug.apk)..." -ForegroundColor Yellow
Push-Location "$root\platform\android"
try {
    & $gradle assembleRelease assembleDebug --quiet
} finally {
    Pop-Location
}

$releaseApkSrc = "$root\platform\android\app\build\outputs\apk\release\app-release.apk"
$debugApkSrc   = "$root\platform\android\app\build\outputs\apk\debug\app-debug.apk"
$targetApkSrc  = if (Test-Path $debugApkSrc) { $debugApkSrc } else { $releaseApkSrc }

if (Test-Path $targetApkSrc) {
    if (-not (Test-Path "$root\release")) { New-Item -ItemType Directory -Path "$root\release" -Force }
    Copy-Item $targetApkSrc "$root\AeroSync.apk" -Force
    Copy-Item $targetApkSrc "$root\release\AeroSync.apk" -Force
    Copy-Item $targetApkSrc "$root\release\AeroSync-v1.0.7.apk" -Force
    if (Test-Path $debugApkSrc) {
        Copy-Item $debugApkSrc "$root\release\app-debug.apk" -Force
    }
}

# Create 1-click launcher batch script in root and release directory
$launcherBat = "@echo off`r`nsetlocal`r`nset `"SCRIPT_DIR=%~dp0`"`r`nif exist `"%SCRIPT_DIR%AeroSync.exe`" (`r`n    start `"`" `"%SCRIPT_DIR%AeroSync.exe`" %*`r`n) else if exist `"%SCRIPT_DIR%release\AeroSync.exe`" (`r`n    start `"`" `"%SCRIPT_DIR%release\AeroSync.exe`" %*`r`n) else if exist `"%SCRIPT_DIR%build_windows\AeroSync.exe`" (`r`n    start `"`" `"%SCRIPT_DIR%build_windows\AeroSync.exe`" %*`r`n) else if exist `"%SCRIPT_DIR%platform\windows\desktop_tauri\AeroSync.exe`" (`r`n    start `"`" `"%SCRIPT_DIR%platform\windows\desktop_tauri\AeroSync.exe`" %*`r`n) else (`r`n    echo [AeroSync] Error: AeroSync.exe not found in %SCRIPT_DIR%`r`n    pause`r`n)`r`n"
Set-Content -Path "$root\Start_AeroSync_Desktop.bat" -Value $launcherBat
if (Test-Path "$root\release") {
    Set-Content -Path "$root\release\Start_AeroSync_Desktop.bat" -Value $launcherBat
    if (Test-Path "$root\Start_AeroSync_Linux.sh") {
        Copy-Item "$root\Start_AeroSync_Linux.sh" "$root\release\Start_AeroSync_Linux.sh" -Force
    }
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " BUILD SUCCESSFUL! ALL PRODUCTION ARTIFACTS READY:" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
Write-Host " 1. Production Native Desktop App: $root\build_windows\AeroSync.exe" -ForegroundColor White
Write-Host " 2. One-Click Launcher:            $root\Start_AeroSync_Desktop.bat" -ForegroundColor White
Write-Host " 3. Native C++ Daemon:             $root\build_windows\aerosync_daemon.exe" -ForegroundColor White
Write-Host " 4. Android Mobile APK (Root):     $root\AeroSync.apk" -ForegroundColor White
Write-Host " 5. Android Mobile APK (Release):  $root\release\AeroSync.apk" -ForegroundColor White
Write-Host " 6. Test Runner:                   $root\build_tests\test_core_engine.exe" -ForegroundColor White
Write-Host " 7. Benchmark:                     $root\build_tests\aerosync_benchmark.exe" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Green
