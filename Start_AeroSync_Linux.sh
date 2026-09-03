#!/usr/bin/env bash
# ==============================================================================
# AeroSync Linux 1-Click Direct Standalone Launcher
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$SCRIPT_DIR:$SCRIPT_DIR/release:$SCRIPT_DIR/build_linux:$PATH"

# Fix 1: Ubuntu 24.04/23.10 and Debian 12 AppArmor sandbox fix (prevents WebKitWebProcess crash & white display)
export WEBKIT_FORCE_SANDBOX=0

# Fix 2: WebKitGTK DMA-BUF compatibility flags for flawless rendering across all Linux distros
export WEBKIT_DISABLE_DMABUF_RENDERER=1
export WEBKIT_DISABLE_COMPOSITING_MODE=1

# Fix 3: NVIDIA driver Wayland explicit sync fix
export __NV_DISABLE_EXPLICIT_SYNC=1

# Fix 4: Single WebProcess mode to prevent IPC disconnection
export WEBKIT_USE_SINGLE_WEB_PROCESS=1

# Software rendering fallback if GPU acceleration fails or is explicitly passed
if [ "$AEROSYNC_FORCE_SOFTWARE_RENDER" = "1" ] || [[ "$*" == *"--software-render"* ]] || [[ "$*" == *"--disable-gpu"* ]]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export WEBKIT_GRAPHICS_POLICY=software
    export GSK_RENDERER=cairo
fi

# Auto-grant execution permissions to binaries if needed
if [ -f "$SCRIPT_DIR/aerosync_daemon" ]; then
    chmod +x "$SCRIPT_DIR/aerosync_daemon" 2>/dev/null || true
fi
if [ -f "$SCRIPT_DIR/release/aerosync_daemon" ]; then
    chmod +x "$SCRIPT_DIR/release/aerosync_daemon" 2>/dev/null || true
fi

# Locate main executable or AppImage
AEROSYNC_BIN=""
if [ -f "$SCRIPT_DIR/AeroSync" ]; then
    AEROSYNC_BIN="$SCRIPT_DIR/AeroSync"
elif [ -f "$SCRIPT_DIR/release/AeroSync" ]; then
    AEROSYNC_BIN="$SCRIPT_DIR/release/AeroSync"
elif [ -f "$SCRIPT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage" ]; then
    AEROSYNC_BIN="$SCRIPT_DIR/release/AeroSync-v1.0.7-x86_64.AppImage"
elif [ -f "$SCRIPT_DIR/build_linux_pkg/appimage/AeroSync.AppDir/AppRun" ]; then
    AEROSYNC_BIN="$SCRIPT_DIR/build_linux_pkg/appimage/AeroSync.AppDir/AppRun"
elif [ -f "$SCRIPT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop" ]; then
    AEROSYNC_BIN="$SCRIPT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop"
fi

if [ -z "$AEROSYNC_BIN" ]; then
    echo "Error: AeroSync executable or AppImage not found in $SCRIPT_DIR"
    exit 1
fi

chmod +x "$AEROSYNC_BIN" 2>/dev/null || true
exec "$AEROSYNC_BIN" "$@"
