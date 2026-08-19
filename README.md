# AeroSync — High-Speed Cross-Platform File Transfer

<div align="center">

![AeroSync Logo](platform/android/app/src/main/res/drawable/aerosync_logo.png)

### High-speed peer-to-peer file sharing between Windows and Android over local Wi-Fi or Hotspot.

**Zero Cloud • Zero Compression • Direct Local Transfer**

---

[![GitHub Release](https://img.shields.io/github/v/release/Atulsain011/Aero_sync?style=for-the-badge&color=00D2FF&label=Latest%20Version)](https://github.com/Atulsain011/Aero_sync/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android-3A7BD5?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases)
[![C++20 Engine](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](core/)
[![Tauri v2](https://img.shields.io/badge/Tauri-v2%20Rust%20%2B%20React-FFC131?style=for-the-badge&logo=tauri&logoColor=black)](platform/windows/desktop_tauri)
[![License](https://img.shields.io/badge/License-MIT-blueviolet?style=for-the-badge)](LICENSE)

---

## Downloads

| Platform | Package | Download | Version |
| :--- | :--- | :--- | :--- |
| **Windows Installer** | `AeroSync-Setup-v1.0.6.exe` | [**Download Windows Installer**](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync-Setup-v1.0.6.exe) | `v1.0.6` |
| **Windows Portable** | `AeroSync-Windows-Portable.zip` | [**Download Windows Portable**](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync-Windows-Portable.zip) | `v1.0.6` |
| **Windows Standalone** | `AeroSync.exe` | [**Download Windows Executable**](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync.exe) | `v1.0.6` |
| **Android 8.0+** | `AeroSync.apk` | [**Download Android APK**](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync.apk) | `v1.0.6` |
| **All Releases** | GitHub Releases | [**Browse All Releases**](https://github.com/Atulsain011/Aero_sync/releases) | `Latest` |

<br>

[![Download Windows Installer](https://img.shields.io/badge/Download_Windows_Installer-AeroSync-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync-Setup-v1.0.6.exe)

[![Download Android App](https://img.shields.io/badge/Download_Android_App-AeroSync-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync.apk)

</div>

---

## Overview

AeroSync is a peer-to-peer file transfer application designed for fast file sharing between Windows PCs and Android devices over local Wi-Fi networks or mobile hotspots.

Files are transferred directly between connected devices without requiring cloud storage or an external file-transfer server.

The project combines a native C++20 transfer engine with platform-specific interfaces for Windows and Android.

```text
+-------------------+          Local Wi-Fi / Hotspot          +-------------------+
|                   | <=====================================> |                   |
|    Windows PC     |           Direct File Transfer          |   Android Device  |
|                   |                                         |                   |
| Tauri + React UI  |                                         | Jetpack Compose   |
| C++20 Core Engine |                                         | JNI + C++ Engine  |
+-------------------+                                         +-------------------+
