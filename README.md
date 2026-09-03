# AeroSync — High-Speed Cross-Platform File Transfer

<div align="center">

<img src="platform/android/app/src/main/res/drawable/aerosync_logo.png" width="150" alt="AeroSync Logo" />

### High-speed peer-to-peer file sharing between Windows, Linux, and Android over local Wi-Fi or Hotspot.

**Zero Cloud • Zero Compression • Direct Local Transfer**

---

[![GitHub Release](https://img.shields.io/github/v/release/Atulsain011/Aero_sync?style=for-the-badge&color=00D2FF&label=Latest%20Version)](https://github.com/Atulsain011/Aero_sync/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Android-3A7BD5?style=for-the-badge&logo=linux&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases)
[![C++20 Engine](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](core/)
[![Tauri v2](https://img.shields.io/badge/Tauri-v2%20Rust%20%2B%20React-FFC131?style=for-the-badge&logo=tauri&logoColor=black)](platform/windows/desktop_tauri)
[![License](https://img.shields.io/badge/License-MIT-blueviolet?style=for-the-badge)](LICENSE)

---

## 🚀 Downloads

| Platform | Package | Download Link | Version |
| :--- | :--- | :--- | :--- |
| **Linux (AppImage Portable)** | `AeroSync-v1.0.7-x86_64.AppImage` | [**Download Linux AppImage (Recommended)**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-v1.0.7-x86_64.AppImage) | `v1.0.7` |
| **Linux (Debian / Ubuntu / Mint)** | `aerosync_1.0.7_amd64.deb` | [**Download Linux DEB Package**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/aerosync_1.0.7_amd64.deb) | `v1.0.7` |
| **Linux (Portable Archive)** | `AeroSync-Linux-Portable.tar.gz` | [**Download Linux Portable (.tar.gz)**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Linux-Portable.tar.gz) | `v1.0.7` |
| **Windows (Installer)** | `AeroSync-Setup-v1.0.7.exe` | [**Download Windows Installer (Recommended)**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Setup-v1.0.7.exe) | `v1.0.7` |
| **Windows (Portable)** | `AeroSync-Windows-Portable.zip` | [**Download Windows Portable (.zip)**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Windows-Portable.zip) | `v1.0.7` |
| **Windows (Standalone)** | `AeroSync.exe` | [**Download Windows Executable**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync.exe) | `v1.0.7` |
| **Android (APK)** | `AeroSync.apk` | [**Download Android APK**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync.apk) | `v1.0.7` |
| **All Releases** | GitHub Releases | [**Browse All Releases**](https://github.com/Atulsain011/Aero_sync/releases) | `Latest` |

<br>

[![Download Linux AppImage](https://img.shields.io/badge/Download_Linux_AppImage-AeroSync-FCC624?style=for-the-badge&logo=linux&logoColor=black)](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-v1.0.7-x86_64.AppImage)
[![Download Linux DEB Package](https://img.shields.io/badge/Download_Linux_DEB-AeroSync-A81D33?style=for-the-badge&logo=debian&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/aerosync_1.0.7_amd64.deb)
[![Download Windows Installer](https://img.shields.io/badge/Download_Windows_Installer-AeroSync-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Setup-v1.0.7.exe)
[![Download Android App](https://img.shields.io/badge/Download_Android_App-AeroSync-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync.apk)

</div>

---

## 📖 Overview

**AeroSync** is a peer-to-peer file transfer application designed for fast file sharing between Windows PCs and Android devices over local Wi-Fi networks or mobile hotspots.

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
```

---

## 💡 Why AeroSync?

Traditional file-transfer methods can involve cloud uploads, internet bandwidth, Bluetooth limitations, USB cables, or third-party services.

AeroSync is designed around direct local communication:

* **No cloud storage required**
* **No external transfer server required for local transfers**
* **Works over local Wi-Fi**
* **Works through mobile hotspots**
* **Designed for large files and folders**
* **Direct device-to-device communication**
* **Native C++ transfer engine**
* **Windows and Android support**

---

## ✨ Key Features

### ⚡ High-Speed Local Transfer
AeroSync is optimized for high-throughput file transfers over local networks.

It is suitable for:
* Large videos & Movies
* Archives & ZIPs
* Software packages & ISOs
* Backups & System images
* Documents & Photos
* Multiple files & Deep folder hierarchies

*Actual transfer performance depends on Wi-Fi hardware, network configuration, device capabilities, storage performance, CPU utilization, network congestion, and transfer direction.*

### 🔎 Automatic Device Discovery
Nearby AeroSync devices can be discovered over the local network using network discovery mechanisms such as UDP-based discovery, eliminating manual IP-address configuration.

### 🔐 Device Pairing
AeroSync provides secure device pairing before allowing transfers. Pairing mechanisms include:
* PIN-based pairing
* User confirmation prompts
* QR-based connection

### 📁 File and Folder Transfer
AeroSync supports transferring:
* Individual files
* Multiple files in bulk
* Large multi-gigabyte files
* Nested directory structures
* Transfer integrity verification using chunked checksums

### 🖥️ Windows Desktop Application
The Windows client uses:
* React 18
* TypeScript
* Tauri v2
* Rust runtime
* Native C++20 components

### 📱 Android Application
The Android client uses:
* Kotlin
* Jetpack Compose
* Material 3
* JNI (Java Native Interface)
* Native C++20 components

### 🌐 Local Network Operation
AeroSync is designed to work without requiring an internet connection for local transfers. Both devices can communicate through:
* The same Wi-Fi network
* A phone mobile hotspot
* A local access point / switch

---

## 🔄 Transfer Queue & State Synchronization

AeroSync includes real-time transfer queue management. A transfer transitions through the following states:

```text
QUEUED ───► READY ───► TRANSFERRING ───► COMPLETED
                             │
                             ├───► CANCELLED
                             └───► FAILED
```

Each transfer is tracked with a unique transfer ID so that Android and Windows maintain consistent state in real time.

### Transfer Cancellation Synchronization
When a transfer is cancelled from either device, the cancellation event is instantly synchronized with the connected peer:

```text
Android                                  Windows
   │                                        │
   │─────── TRANSFER_CANCEL_EVENT ─────────►│
   │                                        │
[State: Cancelled]                      [State: Cancelled]
```

The active file stream is cleanly aborted on both endpoints and the queue state updates immediately.

---

## 🏗️ System Architecture

```mermaid
flowchart TD
    subgraph UI["Frontend / Presentation Layer"]
        W_UI["Windows: React + TypeScript + Tauri v2"]
        A_UI["Android: Kotlin + Jetpack Compose"]
    end

    subgraph NativeBridge["Platform Interop"]
        W_Bridge["Tauri IPC / Native Components"]
        A_Bridge["Android JNI C++ Wrapper"]
    end

    subgraph CoreEngine["AeroSync C++20 Core Engine"]
        DE["Discovery Engine (UDP)"]
        PSM["Pairing and Authentication"]
        CM["Connection Manager (TCP)"]
        TE["Transfer Engine (Chunked Streaming)"]
        PS["Protocol Layer"]
        QS["Queue / Transfer State"]
    end

    W_UI --> W_Bridge
    A_UI --> A_Bridge

    W_Bridge --> CoreEngine
    A_Bridge --> CoreEngine
```

---

## 🛠️ Technology Stack

| Component | Technology |
| :--- | :--- |
| **Windows UI** | React 18, TypeScript |
| **Desktop Framework** | Tauri v2 |
| **Desktop Runtime** | Rust |
| **Core Engine** | C++20 |
| **Android UI** | Kotlin, Jetpack Compose, Material 3 |
| **Android Native Layer** | JNI + C++20 |
| **Network Communication** | TCP / UDP Sockets |
| **Build System** | CMake, Gradle, Cargo, Ninja |
| **Android Build** | Gradle 8.10+, NDK 25.1+ |
| **Windows Build** | CMake + Tauri + NSIS |
| **Version Control** | Git |

---

## 🌍 Future: Remote File Transfer (Roadmap)

The long-term goal of AeroSync is to allow users to send files between devices across different networks over the internet:

```text
Android (Jaipur) ══════════════ [Internet P2P] ══════════════ Windows (Delhi)
```

### Planned Remote Architecture:

```text
                  Internet
                     │
              ┌──────▼──────┐
              │ Signaling   │
              │   Server    │
              └──────┬──────┘
                     │
               Connection Setup
                     │
              NAT Traversal (STUN/TURN)
                     │
          ┌──────────┴──────────┐
          │                     │
     ┌────▼────┐           ┌────▼────┐
     │ Sender  │◄─────────►│ Receiver│
     └─────────┘    P2P    └─────────┘
```

The planned remote-transfer system will feature:
* **Signaling Server**: Session exchange, authentication, and endpoint negotiation.
* **STUN / NAT Traversal**: Direct peer-to-peer punching through home routers and firewalls.
* **TURN Relay Fallback**: Guaranteed delivery even on symmetric NATs.
* **Resumable Transfers**: Splitting large 10GB+ files into hash-verified chunks with resume capability upon network disruption.

> [!NOTE]
> *Remote internet transfer is a planned capability on the project roadmap. The current release is focused on local Wi-Fi and hotspot transfers.*

---

## 🚀 Quick Start Guide

### Linux
#### Recommended: AppImage (Portable Container)
1. Download [**`AeroSync-v1.0.7-x86_64.AppImage`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-v1.0.7-x86_64.AppImage) from GitHub Releases (`v1.0.7`).
2. Make it executable: `chmod +x AeroSync-v1.0.7-x86_64.AppImage`
3. Launch AeroSync: `./AeroSync-v1.0.7-x86_64.AppImage` or double-click in your file manager.

#### Debian / Ubuntu / Mint Package (.deb)
1. Download [**`aerosync_1.0.7_amd64.deb`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/aerosync_1.0.7_amd64.deb) from GitHub Releases (`v1.0.7`).
2. Install package: `sudo apt install ./aerosync_1.0.7_amd64.deb` or `sudo dpkg -i aerosync_1.0.7_amd64.deb`
3. Launch **AeroSync** directly from your Application Launcher / Desktop Menu.

#### Portable Archive (.tar.gz)
1. Download [**`AeroSync-Linux-Portable.tar.gz`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Linux-Portable.tar.gz) from GitHub Releases (`v1.0.7`).
2. Extract the archive: `tar -xvf AeroSync-Linux-Portable.tar.gz`
3. Launch AeroSync executable from terminal or file manager.

### Windows
#### Recommended: Installer
1. Download [**`AeroSync-Setup-v1.0.7.exe`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Setup-v1.0.7.exe) from GitHub Releases (`v1.0.7`).
2. Run the installer and complete setup.
3. Launch AeroSync from the Desktop shortcut or Start Menu.
4. Allow network access through Windows Firewall if prompted.
5. Connect your Windows PC, Linux machine, and Android device to the same Wi-Fi or hotspot.

#### Portable Version
1. Download [**`AeroSync-Windows-Portable.zip`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync-Windows-Portable.zip).
2. Extract the ZIP archive.
3. Run `AeroSync.exe`.

### Android
1. Download [**`AeroSync.apk`**](https://github.com/Atulsain011/Aero_sync/releases/download/v1.0.7/AeroSync.apk).
2. Tap the APK to install (*enable "Install unknown apps" if prompted*).
3. Grant requested permissions (Nearby devices & storage access).
4. Open AeroSync on both devices, discover, pair, and transfer!

---

## 💻 Building From Source

### Prerequisites

#### Windows
* Windows 10 or Windows 11 (x64)
* CMake 3.22+ and Ninja
* MSVC 2022+ or Clang/LLVM
* Rust 1.80+ (`rustup default stable`)
* Node.js 18+ and npm

#### Android
* Android Studio Hedgehog or newer
* Android SDK (API 34)
* Android NDK 25.1.8937393
* JDK 17+
* Gradle 8.10+

---

### Linux AppImage & DEB Package Build

```bash
# Make scripts executable
chmod +x build_linux.sh package_linux.sh

# Run the complete Linux production build pipeline (.AppImage & .deb)
./build_linux.sh
```

Generated Linux packages will be located at:
```text
release/AeroSync-v1.0.7-x86_64.AppImage   # Portable AppImage Container (Recommended)
release/aerosync_1.0.7_amd64.deb          # Debian / Ubuntu / Mint Installer Package
```

#### Running & Installing on Linux

- **AppImage (Portable Container - Recommended)**:
  ```bash
  chmod +x AeroSync-v1.0.7-x86_64.AppImage
  ./AeroSync-v1.0.7-x86_64.AppImage
  ```

- **Debian / Ubuntu / Mint Package**:
  ```bash
  sudo dpkg -i aerosync_1.0.7_amd64.deb
  # or
  sudo apt install ./aerosync_1.0.7_amd64.deb
  ```

---

### Windows Desktop & Installer Build

```powershell
# Unified build and NSIS installer packaging script
powershell -ExecutionPolicy Bypass -File .\build_desktop_and_installer.ps1
```

---

### Android Build

```powershell
cd platform/android
.\gradlew.bat assembleDebug
```

The generated APK will be at:
```text
platform/android/app/build/outputs/apk/debug/app-debug.apk
```

---

## 📂 Repository Structure

```text
AeroSync/
├── core/                               # C++20 Core Engine
│   ├── include/aerosync/               # Public C++ headers
│   └── src/                            # Transfer & discovery logic
├── proto/                              # Protobuf specifications
│   └── aerosync.proto
├── platform/
│   ├── windows/                        # Windows Desktop
│   │   ├── desktop_tauri/              # Tauri v2 + React Frontend & Rust Backend
│   │   ├── assets/                     # Application Icons & Resources
│   │   └── src/
│   └── android/                        # Android App
│       └── app/
│           └── src/main/               # Jetpack Compose UI & JNI bindings
├── release/                            # Built release binaries & setup packages
├── build_desktop_and_installer.ps1     # Automated Windows build script
├── build_all.ps1                       # Unified build script
├── LICENSE                             # MIT License
└── README.md                           # Documentation
```

---

## ⚡ Performance

AeroSync is designed for high-speed local-network file transfers.

| Test | Network | File Size | Direction | Speed |
| :--- | :--- | :--- | :--- | :--- |
| 1 | Wi-Fi 5 / Hotspot | 1 GB | Android → Windows | ~35–55 MB/s |
| 2 | Wi-Fi 5 / Hotspot | 1 GB | Windows → Android | ~35–55 MB/s |
| 3 | Wi-Fi 6 (5GHz) | 5 GB | Android → Windows | ~75–110 MB/s |
| 4 | Wi-Fi 6 (5GHz) | 5 GB | Windows → Android | ~75–110 MB/s |

*Speeds vary based on Wi-Fi generation, signal strength, flash storage write speeds, and device thermal throttling.*

---

## 🎨 Branding & Icons

AeroSync v1.0.7 uses standardized branding across all platforms:
* **Background**: Sleek dark navy squircle (`#0F172A`)
* **Glyph**: Vibrant glowing cyan/blue cloud with bi-directional transfer arrows
* **Transparency**: 100% transparent corners for desktop, taskbar, start menu, and launcher icons.
* **Consistency**: Identical branding across Android launcher, Android APK, Windows EXE, Windows Installer, taskbar, and documentation.

---

## 🗺️ Project Roadmap

### Current (v1.0.7)
- [x] Windows application (Tauri v2 + React)
- [x] Linux application (AppImage container & DEB package with system launcher)
- [x] Android application (Jetpack Compose + Material 3)
- [x] Local Wi-Fi & Mobile Hotspot transfer
- [x] UDP automatic device discovery
- [x] Secure PIN pairing
- [x] Multiple file & folder transfers
- [x] Real-time transfer queue & live ETA/progress
- [x] Instant bi-directional transfer cancellation
- [x] Windows NSIS setup installer & Portable ZIP
- [x] Linux system application launcher integration & native desktop actions
- [x] Android APK release automation
- [x] Standardized application branding & multi-res icons

### Planned
- [ ] Remote cross-network transfers
- [ ] Signaling server architecture
- [ ] STUN / TURN NAT traversal integration
- [ ] Resumable remote transfers with chunk verification
- [ ] QR-code based remote transfer pairing
- [ ] End-to-end encrypted remote sockets

---

## 📄 License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

<div align="center">

### AeroSync — Fast. Direct. Private.

Crafted with ❤️ by [Atul Kumar](https://github.com/Atulsain011)

⭐ If you find AeroSync useful, please consider giving the repository a star!

</div>
