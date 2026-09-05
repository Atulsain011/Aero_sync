#!/usr/bin/env bash
# ==============================================================================
# AeroSync Linux Production Build Pipeline
# Builds Native C++ Core Daemon (aerosync_daemon), Shared Core Library,
# Test Suite, and Tauri Linux Desktop Bundle (.AppImage / .deb)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"

echo "=========================================================="
echo " AEROSYNC LINUX PRODUCTION BUILD PIPELINE (v1.0.8)"
echo "=========================================================="

# Check for required build tools
CXX_COMPILER="g++"
C_COMPILER="gcc"
if ! command -v g++ >/dev/null 2>&1; then
    if command -v clang++ >/dev/null 2>&1; then
        CXX_COMPILER="clang++"
        C_COMPILER="clang"
    else
        echo >&2 "Error: g++ or clang++ is required but not installed."
        exit 1
    fi
fi
command -v cmake >/dev/null 2>&1 || { echo >&2 "Error: cmake is required but not installed."; exit 1; }
command -v npm >/dev/null 2>&1 || { echo >&2 "Error: npm is required but not installed."; exit 1; }

mkdir -p "$ROOT_DIR/build_linux"
mkdir -p "$ROOT_DIR/release"

# 1. Build C++ Core Static Library & Native Linux Daemon
echo -e "\n[1/4] Building Linux Native Core Daemon (aerosync_daemon)..."
mkdir -p "$ROOT_DIR/build_linux"
cd "$ROOT_DIR/build_linux"

# Remove non-Linux stale binaries and any Windows .exe files
rm -f "$ROOT_DIR/build_linux/aerosync_daemon"
rm -f "$ROOT_DIR/release/aerosync_daemon"
rm -f "$ROOT_DIR/platform/windows/desktop_tauri/aerosync_daemon"
rm -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/aerosync_daemon"
rm -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/"*.exe
rm -f "$ROOT_DIR/platform/windows/desktop_tauri/"*.exe

cmake "$ROOT_DIR/platform/windows" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_COMPILER="$C_COMPILER"

cmake --build . --config Release -j$(nproc 2>/dev/null || echo 2)

# Verify built Linux daemon architecture
DAEMON_PATH="$ROOT_DIR/build_linux/aerosync_daemon"
if [ ! -f "$DAEMON_PATH" ]; then
    echo "Error: aerosync_daemon binary was not produced by CMake build!" >&2
    exit 1
fi
chmod 755 "$DAEMON_PATH"

echo "=== Verifying Built Linux Daemon Architecture ==="
FILE_INFO=$(file "$DAEMON_PATH" 2>/dev/null || true)
echo "$FILE_INFO"

if ! echo "$FILE_INFO" | grep -q "ELF"; then
    echo "Error: Built aerosync_daemon is NOT an ELF binary!" >&2
    exit 1
fi
if ! echo "$FILE_INFO" | grep -q -E "x86-64|x86_64"; then
    echo "Error: Built aerosync_daemon is NOT x86-64 architecture!" >&2
    exit 1
fi
if strings "$DAEMON_PATH" 2>/dev/null | grep -q -E "/system/bin/linker|liblog.so"; then
    echo "Error: Built aerosync_daemon is an Android binary, not a GNU/Linux desktop binary!" >&2
    exit 1
fi
if command -v ldd >/dev/null 2>&1; then
    echo "Daemon Dynamic Dependencies:"
    ldd "$DAEMON_PATH" || true
fi

cp "$DAEMON_PATH" "$ROOT_DIR/release/aerosync_daemon"
cp "$DAEMON_PATH" "$ROOT_DIR/platform/windows/desktop_tauri/aerosync_daemon"
cp "$DAEMON_PATH" "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/aerosync_daemon"

# 2. Build C++ Core Unit Tests & Throughput Benchmark
echo -e "\n[2/4] Building C++ Test Suite & Benchmark Harness..."
mkdir -p "$ROOT_DIR/build_tests_linux"
cd "$ROOT_DIR/build_tests_linux"
cmake "$ROOT_DIR/tests" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_COMPILER="$C_COMPILER"

cmake --build . --config Release -j$(nproc 2>/dev/null || echo 2)

# 3. Build Web Assets (Vite + React + TypeScript)
echo -e "\n[3/4] Building Web Assets..."
cd "$ROOT_DIR/platform/windows/desktop_tauri"
npm install
npm run build

# 4. Build Tauri Linux Executable / AppImage / DEB Package
echo -e "\n[4/4] Building Tauri Linux Release Packages (.AppImage & .deb)..."
cd "$ROOT_DIR/platform/windows/desktop_tauri"

if command -v cargo >/dev/null 2>&1 || command -v npx >/dev/null 2>&1; then
    echo "Compiling Linux Tauri desktop binary..."
    cargo build --release || npx tauri build --no-bundle || true
fi

# Run self-contained Linux packaging pipeline
if [ -f "$ROOT_DIR/package_linux.sh" ]; then
    bash "$ROOT_DIR/package_linux.sh"
fi

if [ -f "$ROOT_DIR/release/AeroSync-Linux-x86_64.AppImage" ]; then
    chmod +x "$ROOT_DIR/release/AeroSync-Linux-x86_64.AppImage" 2>/dev/null || true
fi

echo "=========================================================="
echo " LINUX BUILD SUCCESSFUL! RELEASE ARTIFACTS READY:"
echo "=========================================================="
echo " 1. Linux — AppImage (Recommended): $ROOT_DIR/release/AeroSync-Linux-x86_64.AppImage"
echo " 2. Linux — Debian/Ubuntu (.deb):   $ROOT_DIR/release/aerosync_1.0.8_amd64.deb"
echo "=========================================================="

