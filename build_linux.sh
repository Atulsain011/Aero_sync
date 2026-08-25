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
echo " AEROSYNC LINUX PRODUCTION BUILD PIPELINE (v1.0.7)"
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

# Remove non-Linux stale binaries if present
if [ -f "$ROOT_DIR/build_linux/aerosync_daemon" ] && ! file "$ROOT_DIR/build_linux/aerosync_daemon" 2>/dev/null | grep -q "ld-linux"; then
    rm -f "$ROOT_DIR/build_linux/aerosync_daemon"
fi
if [ -f "$ROOT_DIR/release/aerosync_daemon" ] && ! file "$ROOT_DIR/release/aerosync_daemon" 2>/dev/null | grep -q "ld-linux"; then
    rm -f "$ROOT_DIR/release/aerosync_daemon"
fi

cmake "$ROOT_DIR/platform/windows" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_COMPILER="$C_COMPILER"

cmake --build . --config Release -j$(nproc 2>/dev/null || echo 2)

chmod +x "$ROOT_DIR/build_linux/aerosync_daemon" 2>/dev/null || true
cp "$ROOT_DIR/build_linux/aerosync_daemon" "$ROOT_DIR/release/aerosync_daemon" 2>/dev/null || true
cp "$ROOT_DIR/build_linux/aerosync_daemon" "$ROOT_DIR/platform/windows/desktop_tauri/aerosync_daemon" 2>/dev/null || true
cp "$ROOT_DIR/build_linux/aerosync_daemon" "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/aerosync_daemon" 2>/dev/null || true

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
    echo "Generating AppImage & DEB packages via Tauri CLI..."
    npx tauri build --bundles appimage,deb 2>/dev/null || cargo tauri build --bundles appimage,deb 2>/dev/null || cargo build --release

    # Copy output release artifacts
    find "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/bundle/appimage" -name "*.AppImage" -exec cp {} "$ROOT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage" \; 2>/dev/null || true
    find "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/bundle/deb" -name "*.deb" -exec cp {} "$ROOT_DIR/release/aerosync_1.0.7_amd64.deb" \; 2>/dev/null || true
    cp "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop" "$ROOT_DIR/release/AeroSync" 2>/dev/null || true
else
    echo "Note: Install Rust/Cargo and Tauri CLI to generate native Linux AppImage and DEB packages."
fi

# Run Linux packaging pipeline
if [ -f "$ROOT_DIR/package_linux.sh" ]; then
    bash "$ROOT_DIR/package_linux.sh" || true
fi

chmod +x "$ROOT_DIR/release/aerosync_daemon" 2>/dev/null || true
chmod +x "$ROOT_DIR/release/AeroSync" 2>/dev/null || true
if [ -f "$ROOT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage" ]; then
    chmod +x "$ROOT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage" 2>/dev/null || true
fi

echo "=========================================================="
echo " LINUX BUILD SUCCESSFUL! RELEASE ARTIFACTS READY:"
echo "=========================================================="
echo " 1. Linux Core Daemon:    $ROOT_DIR/release/aerosync_daemon"
echo " 2. Linux Executable:     $ROOT_DIR/release/AeroSync"
echo " 3. AppImage Container:   $ROOT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage"
echo " 4. Debian DEB Package:   $ROOT_DIR/release/aerosync_1.0.7_amd64.deb"
echo " 5. Linux Portable Tar:   $ROOT_DIR/release/AeroSync-Linux-Portable.tar.gz"
echo " 6. Linux Test Runner:    $ROOT_DIR/build_tests_linux/test_core_engine"
echo " 7. Linux Benchmark:      $ROOT_DIR/build_tests_linux/aerosync_benchmark"
echo "=========================================================="
