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

Why AeroSync?

Traditional file-transfer methods can involve cloud uploads, internet bandwidth, Bluetooth limitations, USB cables, or third-party services.

AeroSync is designed around direct local communication:

No cloud storage required
No external transfer server required for local transfers
Works over local Wi-Fi
Works through mobile hotspots
Designed for large files and folders
Direct device-to-device communication
Native C++ transfer engine
Windows and Android support
Key Features
⚡ High-Speed Local Transfer

AeroSync is optimized for high-throughput file transfers over local networks.

It is suitable for:

Large videos
Movies
Archives
Software packages
Backups
Documents
Photos
Multiple files
Large folders

Actual transfer performance depends on Wi-Fi hardware, network configuration, device capabilities, storage performance, CPU utilization, network congestion, and transfer direction.

🔎 Automatic Device Discovery

Nearby AeroSync devices can be discovered over the local network using network discovery mechanisms such as UDP-based discovery.

The goal is to minimize manual IP-address configuration.

🔐 Device Pairing

AeroSync provides device pairing before allowing transfers.

Pairing can use mechanisms such as:

PIN-based pairing
User confirmation
QR-based connection
📁 File and Folder Transfer

AeroSync supports transferring:

Individual files
Multiple files
Large files
Folders
Nested directory structures

Transfer integrity can be verified using checksums where supported by the implementation.

🖥️ Windows Desktop Application

The Windows client uses:

React
TypeScript
Tauri v2
Rust
Native C++ components
📱 Android Application

The Android client uses:

Kotlin
Jetpack Compose
Material 3
JNI
Native C++ components
🌐 Local Network Operation

AeroSync is designed to work without requiring an internet connection for local transfers.

Both devices can communicate through:

The same Wi-Fi network
A phone hotspot
A local access point
Transfer Queue

AeroSync includes transfer queue management.

A transfer can move through states such as:

QUEUED
   ↓
READY
   ↓
TRANSFERRING
   ↓
COMPLETED

Other possible states include:

CANCELLED
FAILED

Each transfer can be associated with a unique transfer ID so that Android and Windows can maintain consistent transfer state.

Transfer Cancellation

When a transfer is cancelled from either device, the corresponding transfer state should be synchronized with the other connected device.

Example:

Android
   │
   │ TRANSFER_CANCEL
   ▼
Windows

or:

Windows
   │
   │ TRANSFER_CANCEL
   ▼
Android

The active transfer should stop and the associated queue state should be updated.

System Architecture
Technology Stack
Component	Technology
Windows UI	React, TypeScript
Desktop Framework	Tauri v2
Desktop Runtime	Rust
Core Engine	C++20
Android UI	Kotlin, Jetpack Compose
Android Native Layer	JNI + C++
Network Communication	TCP / UDP
Build System	CMake, Gradle, Cargo
Android Build	Gradle
Windows Build	CMake + Tauri
Version Control	Git
🌍 Future: Remote File Transfer

The long-term goal of AeroSync is to allow users to send files between devices even when they are on different networks.

For example:

                 Internet


Android ───────────────────────── Windows
Jaipur                              Delhi

The intended architecture is:

                  Internet
                     │
              ┌──────▼──────┐
              │ Signaling   │
              │   Server    │
              └──────┬──────┘
                     │
               Connection Setup
                     │
              NAT Traversal
                     │
          ┌──────────┴──────────┐
          │                     │
     ┌────▼────┐           ┌────▼────┐
     │ Sender  │◄─────────►│Receiver │
     └─────────┘    P2P    └─────────┘

The planned remote-transfer system may use:

Signaling server
STUN
TURN fallback
Secure peer-to-peer transport
Chunked transfer
Transfer authentication
Resume support
Important

Remote internet transfer is a planned capability unless explicitly implemented and enabled in the current release.

The current AeroSync application is primarily focused on local Wi-Fi and hotspot transfers.

Remote Transfer — Planned Requirements
Signaling Server

The signaling service would help devices:

Create transfer sessions
Exchange connection information
Authenticate sessions
Establish peer connections
Handle device availability
STUN

STUN can help devices discover their public network endpoints and establish direct connections through NAT where possible.

TURN

If direct peer-to-peer connectivity is not possible, a TURN relay can provide a fallback connection.

This can consume server bandwidth, so direct P2P connectivity should be preferred whenever possible.

Resumable Transfers

Large remote transfers should be split into chunks:

10 GB File
    │
    ├── Chunk 1
    ├── Chunk 2
    ├── Chunk 3
    ├── Chunk 4
    └── ...

This allows:

Progress tracking
Retry
Resume
Integrity verification
Cancellation
Local vs Remote
Local Mode
Android
   │
   │ Wi-Fi / Hotspot
   ▼
Windows

Advantages:

Very high local throughput
No cloud storage
No internet required
Direct device-to-device transfer
Remote Mode — Planned
Android
   │
   │ Internet
   ▼
P2P / Relay
   │
   ▼
Windows

Remote speed will depend on:

Sender upload speed
Receiver download speed
Network latency
ISP routing
NAT/firewall conditions
Direct P2P availability
Quick Start
Windows
Recommended: Installer
Download the latest Windows installer from GitHub Releases.
Run the installer.
Follow the setup instructions.
Launch AeroSync from the Desktop or Start Menu.
Allow network access through Windows Firewall if prompted.
Connect the Windows PC and Android device to the same local network.
Portable Version
Download AeroSync-Windows-Portable.zip.
Extract the ZIP.
Open the extracted folder.
Run AeroSync.exe.
Standalone Executable

The standalone executable is also available:

AeroSync.exe

For the most reliable Windows installation, the installer package is recommended.

Android
Download AeroSync.apk.
Install the APK.
If Android asks for permission to install from the selected source, allow it.
Grant the permissions requested by AeroSync.
Connect Android and Windows to the same Wi-Fi network or mobile hotspot.
Open AeroSync on both devices.
Wait for device discovery.
Pair the devices.
Select files.
Start the transfer.
Building From Source
Prerequisites
Windows
Windows 10 or Windows 11
CMake 3.22+
Ninja
MSVC 2022+ or Clang/LLVM
Rust 1.80+
Node.js 18+
npm
Android
Android Studio
Android SDK
Android API 34
Android NDK 25.1+
JDK 17+
Gradle 8.10+
Unified Build
.\build_all.ps1
Windows Native Build
cmake -B build_windows -S platform/windows -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows
Windows Desktop Build
cd platform/windows/desktop_tauri

Install dependencies:

npm install

Build frontend:

npm run build

Build release application:

cargo build --release
Android Build
cd platform/android

Debug APK:

.\gradlew assembleDebug

Expected output:

platform/android/app/build/outputs/apk/debug/app-debug.apk

For production distribution, configure the appropriate signing configuration and release variant.

GitHub Actions

AeroSync uses GitHub Actions for automated builds and release artifacts.

The release pipeline can produce:

AeroSync-Setup-v1.0.6.exe
AeroSync-Windows-Portable.zip
AeroSync.exe
AeroSync.apk

Builds should be verified before publishing a release.

Repository Structure
AeroSync/
│
├── core/
│   ├── include/
│   │   └── aerosync/
│   └── src/
│
├── proto/
│   └── aerosync.proto
│
├── platform/
│   ├── windows/
│   │   ├── desktop_tauri/
│   │   ├── src/
│   │   └── installer_firewall_rule.ps1
│   │
│   └── android/
│       └── app/
│           └── src/
│               ├── main/
│               └── cpp/
│
├── tests/
│
├── build_all.ps1
├── Start_AeroSync_Desktop.bat
├── LICENSE
└── README.md
Performance

AeroSync is designed for high-speed local-network file transfers.

Actual throughput depends on:

Wi-Fi generation
Channel width
Signal strength
Access point
Hotspot implementation
Device hardware
CPU utilization
Storage read/write speed
Network congestion
Transfer direction

Performance should be measured using real hardware and controlled network conditions.

Benchmark Format
Test	Network	File Size	Direction	Speed
1	Wi-Fi / Hotspot	1 GB	Android → Windows	TBD
2	Wi-Fi / Hotspot	1 GB	Windows → Android	TBD
3	Wi-Fi / Hotspot	5 GB	Android → Windows	TBD
4	Wi-Fi / Hotspot	5 GB	Windows → Android	TBD

Replace TBD with measured results before publishing performance claims.

🔐 Security and Privacy
Local Transfers

Local transfers are designed to send data directly between participating devices.

AeroSync does not require cloud storage for local file transfers.

Device Pairing

Devices should be paired before transfers are initiated.

Data Integrity

Transfer integrity can be verified using checksums implemented by the transfer protocol.

Remote Transfers

When remote-transfer functionality is implemented, it should include:

Encrypted transport
Secure session IDs
Expiring transfer sessions
User approval
Device verification
Secure signaling
NAT traversal

Security guarantees depend on the exact implementation and configuration of the current release.

📦 AeroSync v1.0.6

The current release is:

v1.0.6
GitHub Release

https://github.com/Atulsain011/Aero_sync/releases/tag/v1.0.6

Direct Downloads
Windows Installer
https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync-Setup-v1.0.6.exe
Windows Portable
https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync-Windows-Portable.zip
Windows Standalone
https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync.exe
Android APK
https://github.com/Atulsain011/Aero_sync/releases/latest/download/AeroSync.apk
All Releases

https://github.com/Atulsain011/Aero_sync/releases

🎨 Branding

AeroSync v1.0.6 uses the standardized AeroSync branding:

Dark navy background
Cyan/blue cloud transfer symbol
Upload arrow
Download arrow
Consistent application icon

The same branding should be used across:

Android launcher
Android APK
Windows EXE
Windows installer
Desktop shortcut
Taskbar icon
Website
Favicon

The project should avoid legacy or placeholder AeroSync icons.

🗺️ Roadmap
Current
 Windows application
 Android application
 Local Wi-Fi transfer
 Hotspot transfer
 Device discovery
 Device pairing
 File selection
 Multiple-file transfer
 Transfer queue
 Transfer progress
 Transfer cancellation
 Activity / history
 Windows installer
 Windows portable package
 Android APK
 GitHub release automation
 Standardized application branding
Planned
 Remote cross-network transfers
 Internet-based P2P connection
 Signaling server
 STUN integration
 TURN fallback
 Resumable remote transfers
 QR-based remote transfer links
 Expiring transfer links
 Remote device authentication
 Advanced transfer analytics
 Improved transfer recovery
Development

Contributions, bug reports, performance testing, and technical feedback are welcome.

Before submitting changes:

Build the affected platform.
Test device discovery.
Test pairing.
Test both transfer directions.
Test multiple files.
Test large files.
Test transfer cancellation.
Test queue synchronization.
Verify file integrity.
Check for crashes and resource leaks.
License

This project is licensed under the MIT License.

See LICENSE for details.

<div align="center">
AeroSync
Fast. Direct. Private.

Built for high-speed device-to-device file transfer.

Crafted by Atul Kumar

⭐ If you find AeroSync useful, consider giving the repository a star.

</div> ```
