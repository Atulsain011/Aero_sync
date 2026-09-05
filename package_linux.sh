#!/usr/bin/env bash
# ==============================================================================
# AeroSync Standalone Linux Packaging Pipeline
# Produces:
#   1. AeroSync-Linux-x86_64.AppImage (Linux — AppImage (Recommended))
#   2. aerosync_1.0.8_amd64.deb      (Linux — Debian/Ubuntu (.deb))
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"
RELEASE_DIR="$ROOT_DIR/release"
VERSION="1.0.8"

mkdir -p "$RELEASE_DIR"

echo "=========================================================="
echo " AEROSYNC LINUX SELF-CONTAINED PACKAGING TOOL (v$VERSION)"
echo "=========================================================="

# 1. Locate desktop executable & daemon
MAIN_BIN=""
if [ -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop" ]; then
    MAIN_BIN="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop"
elif [ -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/x86_64-unknown-linux-gnu/release/aerosync-desktop" ]; then
    MAIN_BIN="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/x86_64-unknown-linux-gnu/release/aerosync-desktop"
elif [ -f "$RELEASE_DIR/AeroSync" ] && file "$RELEASE_DIR/AeroSync" 2>/dev/null | grep -q "ELF"; then
    MAIN_BIN="$RELEASE_DIR/AeroSync"
fi

if [ -z "$MAIN_BIN" ] || [ ! -f "$MAIN_BIN" ]; then
    echo "Error: AeroSync Linux desktop binary not found!"
    echo "Expected at: platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop"
    exit 1
fi

verify_daemon_arch() {
    local bin="$1"
    if [ ! -f "$bin" ]; then
        return 1
    fi
    local file_out
    file_out=$(file "$bin" 2>/dev/null || true)
    
    # Must be 64-bit ELF x86-64
    if ! echo "$file_out" | grep -q "ELF"; then
        return 1
    fi
    if ! echo "$file_out" | grep -q -E "x86-64|x86_64"; then
        return 1
    fi
    
    # Reject Android / Bionic executables
    if strings "$bin" 2>/dev/null | grep -q -E "/system/bin/linker|liblog.so"; then
        echo "Warning: Rejecting Android Bionic binary at $bin" >&2
        return 1
    fi
    
    return 0
}

DAEMON_BIN=""
for cand in \
    "$ROOT_DIR/build_linux/aerosync_daemon" \
    "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/aerosync_daemon" \
    "$ROOT_DIR/platform/windows/desktop_tauri/aerosync_daemon" \
    "$RELEASE_DIR/aerosync_daemon"; do
    if verify_daemon_arch "$cand"; then
        DAEMON_BIN="$cand"
        break
    fi
done

if [ -z "$DAEMON_BIN" ]; then
    echo "==========================================================" >&2
    echo "ERROR: Valid Linux x86_64 aerosync_daemon binary not found!" >&2
    echo "The Linux package must contain the native Linux x86_64 aerosync_daemon binary." >&2
    echo "It must NOT be an .exe file and NOT an Android Bionic binary." >&2
    echo "Please build it with: mkdir -p build_linux && cd build_linux && cmake ../platform/windows -DCMAKE_BUILD_TYPE=Release && cmake --build ." >&2
    echo "==========================================================" >&2
    exit 1
fi

chmod 755 "$DAEMON_BIN"

ICON_SRC=""
for icon_candidate in \
    "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons/256x256.png" \
    "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons/128x128@2x.png" \
    "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons/icon.png" \
    "$ROOT_DIR/platform/android/app/src/main/res/drawable/aerosync_logo.png"; do
    if [ -f "$icon_candidate" ]; then
        ICON_SRC="$icon_candidate"
        break
    fi
done

echo "Found Main Binary:   $MAIN_BIN"
echo "Found Daemon Binary: $DAEMON_BIN"
echo "Daemon Arch:         $(file "$DAEMON_BIN" 2>/dev/null || echo "ELF x86_64")"
if command -v ldd >/dev/null 2>&1; then
    echo "Daemon Dynamic Deps:"
    ldd "$DAEMON_BIN" || true
fi
echo "Found Icon Source:   $ICON_SRC"

# ------------------------------------------------------------------------------
# 2. Collect WebKitGTK 4.1 Shared Libraries & Helper Binaries for Bundling
# ------------------------------------------------------------------------------
echo -e "\n[1/3] Collecting WebKitGTK 4.1 and Runtime Dependencies..."

STAGING_LIB_DIR="$ROOT_DIR/build_linux_pkg/bundled_libs"
rm -rf "$STAGING_LIB_DIR"
mkdir -p "$STAGING_LIB_DIR"
mkdir -p "$STAGING_LIB_DIR/webkit2gtk-4.1"
mkdir -p "$STAGING_LIB_DIR/webkit2gtk-4.1/injected-bundle"
mkdir -p "$STAGING_LIB_DIR/gio/modules"

# WebKitGTK helper processes (WebKitWebProcess, WebKitNetworkProcess) and GIO modules
# are host system components and must match the host's installed WebKitGTK 4.1 runtime.

# Library Exclude List: Core base system libraries and GPU/display driver interfaces
# that MUST be resolved dynamically by the host Linux distribution.
should_exclude() {
    local lib="$1"
    case "$lib" in
        # Core standard C/POSIX runtime (must match host kernel/glibc)
        libc.so*|libm.so*|libdl.so*|librt.so*|libpthread.so*|libresolv.so*|libutil.so*) return 0 ;;
        ld-linux*|libnss_*|libnsl.so*) return 0 ;;
        # GPU drivers & low-level hardware display server bindings
        libGL.so*|libGLX.so*|libEGL.so*|libGLdispatch.so*|libOpenGL.so*) return 0 ;;
        libdrm.so*|libgbm.so*|libvulkan.so*) return 0 ;;
        libX11.so*|libX11-xcb.so*|libxcb*.so*|libXext.so*|libXfixes.so*|libXi.so*|libXdamage.so*|libXcomposite.so*|libXrandr.so*|libXcursor.so*|libXrender.so*|libXinerama.so*) return 0 ;;
        libwayland-client.so*|libwayland-server.so*|libwayland-cursor.so*|libwayland-egl.so*) return 0 ;;
        *) return 1 ;;
    esac
}

# Note on WebKitGTK: WebKitGTK 4.1, GTK3, GIO, Mesa, and Wayland/X11 are core host system
# dependencies. Bundling a conflicting build-host WebKitGTK shared library causes fatal
# ABI and IPC collisions with the host's WebKitWebProcess and GIO event loop.
SEED_LIBS=()

for seed in "${SEED_LIBS[@]}"; do
    found_path=""
    for search_dir in "/usr/lib/x86_64-linux-gnu" "/usr/lib64" "/usr/lib"; do
        if [ -f "$search_dir/$seed" ]; then
            found_path="$search_dir/$seed"
            break
        fi
    done
    if [ -n "$found_path" ]; then
        echo "Bundling seed library: $found_path"
        cp -L "$found_path" "$STAGING_LIB_DIR/" 2>/dev/null || true
        # Also copy real so-name if symlinked
        real_target=$(readlink -f "$found_path" 2>/dev/null || true)
        if [ -n "$real_target" ] && [ -f "$real_target" ]; then
            cp -L "$real_target" "$STAGING_LIB_DIR/" 2>/dev/null || true
        fi
    fi
done

# Scan dependencies of collected libraries and binaries recursively
echo "Resolving recursive dependencies..."
MAX_DEPTH=3
current_depth=0
while [ $current_depth -lt $MAX_DEPTH ]; do
    new_found=0
    targets=($(find "$STAGING_LIB_DIR" "$MAIN_BIN" -type f \( -name "*.so*" -o -perm /111 \) 2>/dev/null))
    for target in "${targets[@]}"; do
        deps=$(ldd "$target" 2>/dev/null | grep "=>" | awk '{print $3}' | grep "^/" || true)
        for dep in $deps; do
            base_dep=$(basename "$dep")
            if should_exclude "$base_dep"; then
                continue
            fi
            if [ ! -f "$STAGING_LIB_DIR/$base_dep" ]; then
                cp -L "$dep" "$STAGING_LIB_DIR/$base_dep" 2>/dev/null || true
                new_found=1
            fi
        done
    done
    if [ $new_found -eq 0 ]; then
        break
    fi
    current_depth=$((current_depth + 1))
done

echo "Total bundled shared libraries: $(ls -1 "$STAGING_LIB_DIR"/*.so* 2>/dev/null | wc -l)"

# Apply RPATH with patchelf if available
if command -v patchelf >/dev/null 2>&1; then
    echo "Applying RPATH to bundled libraries..."
    for sofile in "$STAGING_LIB_DIR"/*.so*; do
        if [ -f "$sofile" ] && [ ! -L "$sofile" ]; then
            patchelf --set-rpath '$ORIGIN' "$sofile" 2>/dev/null || true
        fi
    done
    if [ -f "$STAGING_LIB_DIR/webkit2gtk-4.1/WebKitWebProcess" ]; then
        patchelf --set-rpath '$ORIGIN/..:$ORIGIN' "$STAGING_LIB_DIR/webkit2gtk-4.1/WebKitWebProcess" 2>/dev/null || true
    fi
    if [ -f "$STAGING_LIB_DIR/webkit2gtk-4.1/WebKitNetworkProcess" ]; then
        patchelf --set-rpath '$ORIGIN/..:$ORIGIN' "$STAGING_LIB_DIR/webkit2gtk-4.1/WebKitNetworkProcess" 2>/dev/null || true
    fi
fi

# ------------------------------------------------------------------------------
# 3. Build Self-Contained AppImage (.AppImage)
# ------------------------------------------------------------------------------
echo -e "\n[2/3] Building Truly Self-Contained AppImage Container..."

APPDIR="$ROOT_DIR/build_linux_pkg/appimage/AeroSync.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/pixmaps"

# Copy main desktop binary into deterministic layout
cp "$MAIN_BIN" "$APPDIR/usr/bin/aerosync"
chmod 755 "$APPDIR/usr/bin/aerosync"

# Copy verified Linux daemon binary into deterministic layout
cp "$DAEMON_BIN" "$APPDIR/usr/bin/aerosync_daemon"
chmod 755 "$APPDIR/usr/bin/aerosync_daemon"

# Copy bundled libraries into AppDir
cp -a "$STAGING_LIB_DIR"/* "$APPDIR/usr/lib/" 2>/dev/null || true

# Apply RPATH to both desktop binary and daemon
if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN' "$APPDIR/usr/bin/aerosync" 2>/dev/null || true
    patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN' "$APPDIR/usr/bin/aerosync_daemon" 2>/dev/null || true
fi

# Aggressively ensure NO Windows .exe files exist in AppDir
find "$APPDIR" -type f -name "*.exe" -delete

# Install authentic AeroSync icons across all standard hicolor theme resolutions
ICONS_SRC_DIR="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons"
for sz in 16x16 24x24 32x32 48x48 64x64 128x128 256x256; do
    mkdir -p "$APPDIR/usr/share/icons/hicolor/$sz/apps"
    if [ -f "$ICONS_SRC_DIR/$sz.png" ]; then
        cp "$ICONS_SRC_DIR/$sz.png" "$APPDIR/usr/share/icons/hicolor/$sz/apps/aerosync.png"
        chmod 644 "$APPDIR/usr/share/icons/hicolor/$sz/apps/aerosync.png"
    fi
done

# 512x512 master resolution
mkdir -p "$APPDIR/usr/share/icons/hicolor/512x512/apps"
if [ -f "$ICONS_SRC_DIR/icon.png" ]; then
    cp "$ICONS_SRC_DIR/icon.png" "$APPDIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
    chmod 644 "$APPDIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
elif [ -f "$ICONS_SRC_DIR/256x256.png" ]; then
    cp "$ICONS_SRC_DIR/256x256.png" "$APPDIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
    chmod 644 "$APPDIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
fi

# AppDir root icons for AppImage runtime, desktop environments and file managers (.DirIcon)
if [ -f "$ICONS_SRC_DIR/icon.png" ]; then
    cp "$ICONS_SRC_DIR/icon.png" "$APPDIR/aerosync.png"
    cp "$ICONS_SRC_DIR/icon.png" "$APPDIR/.DirIcon"
    cp "$ICONS_SRC_DIR/icon.png" "$APPDIR/usr/share/pixmaps/aerosync.png"
elif [ -f "$ICONS_SRC_DIR/256x256.png" ]; then
    cp "$ICONS_SRC_DIR/256x256.png" "$APPDIR/aerosync.png"
    cp "$ICONS_SRC_DIR/256x256.png" "$APPDIR/.DirIcon"
    cp "$ICONS_SRC_DIR/256x256.png" "$APPDIR/usr/share/pixmaps/aerosync.png"
elif [ -n "$ICON_SRC" ] && [ -f "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$APPDIR/aerosync.png"
    cp "$ICON_SRC" "$APPDIR/.DirIcon"
    cp "$ICON_SRC" "$APPDIR/usr/share/pixmaps/aerosync.png"
fi

# Create canonical .desktop file
cat <<'EOF' > "$APPDIR/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
GenericName=File Transfer
Comment=Ultra High-Speed Cross-Platform Peer-to-Peer File Transfer
Exec=aerosync %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
MimeType=x-scheme-handler/aerosync;
EOF
chmod 644 "$APPDIR/aerosync.desktop"
cp "$APPDIR/aerosync.desktop" "$APPDIR/AppRun.desktop" 2>/dev/null || true
cp "$APPDIR/aerosync.desktop" "$APPDIR/usr/share/applications/aerosync.desktop" 2>/dev/null || true

# Create Smart AppRun Launcher with Auto-Desktop Registration & WebKitGTK Workarounds
cat <<'EOF' > "$APPDIR/AppRun"
#!/usr/bin/env bash
set -e

# Determine real AppDir location
HERE="$(dirname "$(readlink -f "${0}")")"
if [ -z "$APPDIR" ]; then
    export APPDIR="$HERE"
fi

# Support --help and --version cleanly
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "AeroSync - Ultra High-Speed Peer-to-Peer File Transfer"
    echo ""
    echo "Usage: AeroSync [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --install           Integrate AeroSync into your system application menu and desktop"
    echo "  --remove            Remove AeroSync desktop and application menu entries"
    echo "  --software-render   Force software rendering fallback (use if graphics driver fails)"
    echo "  --disable-gpu       Disable hardware GPU acceleration"
    echo "  --help, -h          Show this help message"
    echo "  --version, -v       Show version information"
    echo ""
    exit 0
fi

if [ "$1" = "--version" ] || [ "$1" = "-v" ]; then
    echo "AeroSync v1.0.8"
    exit 0
fi

# 1. Desktop Integration Handler
# Registers .desktop launcher and application menu entry automatically on launch
APP_EXEC_TARGET="${APPIMAGE:-$HERE/AppRun}"
USER_APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
USER_ICONS_BASE="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
USER_PIXMAPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/pixmaps"

install_desktop_integration() {
    mkdir -p "$USER_APPS_DIR" "$USER_PIXMAPS_DIR"
    # Copy all icon resolutions to user hicolor theme directory
    if [ -d "$HERE/usr/share/icons/hicolor" ]; then
        for sz_dir in "$HERE/usr/share/icons/hicolor"/*; do
            if [ -d "$sz_dir/apps" ]; then
                sz=$(basename "$sz_dir")
                mkdir -p "$USER_ICONS_BASE/$sz/apps"
                if [ -f "$sz_dir/apps/aerosync.png" ]; then
                    cp -f "$sz_dir/apps/aerosync.png" "$USER_ICONS_BASE/$sz/apps/aerosync.png" 2>/dev/null || true
                fi
            fi
        done
    fi
    if [ -f "$HERE/aerosync.png" ]; then
        mkdir -p "$USER_ICONS_BASE/256x256/apps"
        cp -f "$HERE/aerosync.png" "$USER_ICONS_BASE/256x256/apps/aerosync.png" 2>/dev/null || true
        cp -f "$HERE/aerosync.png" "$USER_PIXMAPS_DIR/aerosync.png" 2>/dev/null || true
    fi

    cat <<DESKTOP_ENTRY > "$USER_APPS_DIR/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
GenericName=File Transfer
Comment=Ultra High-Speed Cross-Platform Peer-to-Peer File Transfer
Exec="$APP_EXEC_TARGET" %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
MimeType=x-scheme-handler/aerosync;
DESKTOP_ENTRY
    chmod 644 "$USER_APPS_DIR/aerosync.desktop" 2>/dev/null || true

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$USER_APPS_DIR" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t -f "$USER_ICONS_BASE" 2>/dev/null || true
    fi
}

remove_desktop_integration() {
    rm -f "$USER_APPS_DIR/aerosync.desktop"
    for sz in 16x16 24x24 32x32 48x48 64x64 128x128 256x256 512x512; do
        rm -f "$USER_ICONS_BASE/$sz/apps/aerosync.png" 2>/dev/null || true
    done
    rm -f "$USER_PIXMAPS_DIR/aerosync.png"
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$USER_APPS_DIR" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t -f "$USER_ICONS_BASE" 2>/dev/null || true
    fi
    echo "AeroSync desktop integration removed successfully."
}

if [ "$1" = "--install" ]; then
    install_desktop_integration
    echo "AeroSync successfully integrated into your application menu!"
    exit 0
fi

if [ "$1" = "--remove" ] || [ "$1" = "--uninstall" ]; then
    remove_desktop_integration
    exit 0
fi

# Auto-register on first double-click if not already present
if [ ! -f "$USER_APPS_DIR/aerosync.desktop" ]; then
    install_desktop_integration 2>/dev/null || true
fi

# 2. Pre-flight Check for WebKitGTK 4.1 Runtime (libwebkit2gtk-4.1.so.0)
check_webkit_runtime() {
    if [ -f "$HERE/usr/lib/libwebkit2gtk-4.1.so.0" ]; then
        return 0
    fi
    if command -v ldconfig >/dev/null 2>&1; then
        if /sbin/ldconfig -p 2>/dev/null | grep -q "libwebkit2gtk-4.1.so.0"; then
            return 0
        fi
        if ldconfig -p 2>/dev/null | grep -q "libwebkit2gtk-4.1.so.0"; then
            return 0
        fi
    fi
    for p in \
        "/usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.1.so.0" \
        "/usr/lib64/libwebkit2gtk-4.1.so.0" \
        "/usr/lib/libwebkit2gtk-4.1.so.0" \
        "/usr/local/lib/libwebkit2gtk-4.1.so.0" \
        "/lib/x86_64-linux-gnu/libwebkit2gtk-4.1.so.0" \
        "/lib64/libwebkit2gtk-4.1.so.0"; do
        if [ -f "$p" ]; then
            return 0
        fi
    done
    if command -v ldd >/dev/null 2>&1 && [ -f "$HERE/usr/bin/aerosync" ]; then
        if ! ldd "$HERE/usr/bin/aerosync" 2>/dev/null | grep -q "libwebkit2gtk-4.1.so.0 => not found"; then
            return 0
        fi
    fi
    return 1
}

show_missing_webkit_error() {
    local distro="Linux"
    local cmd="sudo apt install -y libwebkit2gtk-4.1-0"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        distro="${NAME:-Linux}"
        case "$ID" in
            ubuntu|debian|linuxmint|pop|elementary|zorin)
                cmd="sudo apt install -y libwebkit2gtk-4.1-0"
                ;;
            fedora|rhel|centos)
                cmd="sudo dnf install -y webkit2gtk4.1"
                ;;
            arch|manjaro|endeavouros)
                cmd="sudo pacman -S webkit2gtk-4.1"
                ;;
            opensuse*|suse)
                cmd="sudo zypper in libwebkit2gtk-4_1-0"
                ;;
            alpine)
                cmd="apk add webkit2gtk-4.1"
                ;;
            *)
                if [ -n "$ID_LIKE" ]; then
                    case "$ID_LIKE" in
                        *debian*|*ubuntu*) cmd="sudo apt install -y libwebkit2gtk-4.1-0" ;;
                        *fedora*|*rhel*) cmd="sudo dnf install -y webkit2gtk4.1" ;;
                        *arch*) cmd="sudo pacman -S webkit2gtk-4.1" ;;
                    esac
                fi
                ;;
        esac
    fi

    local msg="AeroSync requires the WebKitGTK 4.1 runtime library (libwebkit2gtk-4.1.so.0) to render its graphical interface.\n\nTo install it on $distro, run:\n\n    $cmd\n\n(On Debian/Ubuntu systems, installing our official .deb package also resolves all dependencies automatically via apt)."

    if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
        if command -v zenity >/dev/null 2>&1; then
            zenity --error --title="AeroSync - Missing Dependency" --width=500 --text="$(printf "$msg")" 2>/dev/null && exit 1
        elif command -v kdialog >/dev/null 2>&1; then
            kdialog --error "$(printf "$msg")" --title "AeroSync - Missing Dependency" 2>/dev/null && exit 1
        elif command -v xmessage >/dev/null 2>&1; then
            xmessage -center -title "AeroSync" "$(printf "$msg")" 2>/dev/null && exit 1
        fi
    fi

    echo "================================================================================" >&2
    echo " AEROSYNC RUNTIME DEPENDENCY CHECK FAILED" >&2
    echo "================================================================================" >&2
    echo " AeroSync requires the WebKitGTK 4.1 runtime library (libwebkit2gtk-4.1.so.0)." >&2
    echo "" >&2
    echo " To install it on $distro, execute the following command:" >&2
    echo "" >&2
    echo "     $cmd" >&2
    echo "" >&2
    echo " Note: On Debian / Ubuntu systems, you can also install the official .deb package:" >&2
    echo "     sudo apt install ./AeroSync-Linux-x86_64.deb" >&2
    echo " which automatically resolves and installs all required dependencies." >&2
    echo "================================================================================" >&2
    exit 1
}

if ! check_webkit_runtime; then
    show_missing_webkit_error
fi

# 3. Configure Environment for WebKitGTK & System Integration
export PATH="$HERE/usr/bin:$HERE:$PATH"

# Ubuntu 24.04/23.10 and Debian 12 AppArmor unprivileged user namespace fix
export WEBKIT_FORCE_SANDBOX=0

# NVIDIA driver Wayland explicit sync fix
export __NV_DISABLE_EXPLICIT_SYNC=1

# Software rendering fallback if GPU acceleration fails or is forced
if [ "$AEROSYNC_FORCE_SOFTWARE_RENDER" = "1" ] || [[ "$*" == *"--software-render"* ]] || [[ "$*" == *"--disable-gpu"* ]]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export WEBKIT_GRAPHICS_POLICY=software
    export GSK_RENDERER=cairo
    export WEBKIT_DISABLE_COMPOSITING_MODE=1
    export WEBKIT_DISABLE_DMABUF_RENDERER=1
fi

# 4. Start C++ Daemon in Background if not active
if [ -f "$HERE/usr/bin/aerosync_daemon" ]; then
    chmod +x "$HERE/usr/bin/aerosync_daemon" 2>/dev/null || true
    if ! pgrep -f "aerosync_daemon" >/dev/null 2>&1; then
        DAEMON_LOG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/AeroSync"
        mkdir -p "$DAEMON_LOG_DIR" 2>/dev/null || true
        "$HERE/usr/bin/aerosync_daemon" >> "$DAEMON_LOG_DIR/daemon.log" 2>&1 &
    fi
fi

# 5. Launch Main Executable
MAIN_EXEC="$HERE/usr/bin/aerosync"
chmod +x "$MAIN_EXEC" 2>/dev/null || true
exec "$MAIN_EXEC" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Build Real ELF AppImage with appimagetool or runtime-x86_64 + mksquashfs
APPIMAGE_OUTPUT="$RELEASE_DIR/AeroSync-Linux-x86_64.AppImage"

build_appimage_tool() {
    local tool_path="$ROOT_DIR/build_linux_pkg/appimagetool-x86_64.AppImage"
    local tool_extracted="$ROOT_DIR/build_linux_pkg/squashfs-root"
    if [ ! -f "$tool_path" ]; then
        echo "Fetching official appimagetool..."
        mkdir -p "$ROOT_DIR/build_linux_pkg"
        if command -v curl >/dev/null 2>&1; then
            curl -sSL "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-x86_64.AppImage" -o "$tool_path" || true
        elif command -v wget >/dev/null 2>&1; then
            wget -q "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-x86_64.AppImage" -O "$tool_path" || true
        fi
        chmod +x "$tool_path" 2>/dev/null || true
    fi

    # Extract if not extracted, ensuring execution works even on systems without FUSE
    if [ -f "$tool_path" ] && [ ! -d "$tool_extracted" ]; then
        echo "Extracting appimagetool for reliable execution without FUSE..."
        (cd "$ROOT_DIR/build_linux_pkg" && "$tool_path" --appimage-extract >/dev/null 2>&1) || true
    fi

    if [ -d "$tool_extracted" ] && [ -f "$tool_extracted/AppRun" ]; then
        echo "Packaging AppImage with extracted appimagetool..."
        ARCH=x86_64 "$tool_extracted/AppRun" --no-appstream "$APPDIR" "$APPIMAGE_OUTPUT" && return 0
    fi

    if [ -f "$tool_path" ] && [ -x "$tool_path" ]; then
        echo "Packaging AppImage with appimagetool --appimage-extract-and-run..."
        ARCH=x86_64 "$tool_path" --appimage-extract-and-run --no-appstream "$APPDIR" "$APPIMAGE_OUTPUT" && return 0
        ARCH=x86_64 "$tool_path" --no-appstream "$APPDIR" "$APPIMAGE_OUTPUT" && return 0
    fi

    if command -v appimagetool >/dev/null 2>&1; then
        echo "Packaging AppImage with system appimagetool..."
        ARCH=x86_64 appimagetool --no-appstream "$APPDIR" "$APPIMAGE_OUTPUT" && return 0
    fi

    # Fallback: Construct real Type 2 ELF AppImage using official runtime-x86_64 + mksquashfs
    if command -v mksquashfs >/dev/null 2>&1; then
        echo "Constructing genuine Type 2 ELF AppImage using runtime-x86_64 and mksquashfs..."
        local runtime_bin="$ROOT_DIR/build_linux_pkg/runtime-x86_64"
        if [ ! -f "$runtime_bin" ]; then
            if [ -f "$tool_extracted/usr/lib/appimagekit/runtime-x86_64" ]; then
                cp "$tool_extracted/usr/lib/appimagekit/runtime-x86_64" "$runtime_bin"
            elif [ -f "$tool_extracted/usr/bin/runtime" ]; then
                cp "$tool_extracted/usr/bin/runtime" "$runtime_bin"
            elif command -v curl >/dev/null 2>&1; then
                curl -sSL "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64" -o "$runtime_bin" || true
            elif command -v wget >/dev/null 2>&1; then
                wget -q "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64" -O "$runtime_bin" || true
            fi
        fi
        if [ -f "$runtime_bin" ] && [ -s "$runtime_bin" ]; then
            chmod +x "$runtime_bin"
            local squash_tmp="$ROOT_DIR/build_linux_pkg/app.squashfs"
            rm -f "$squash_tmp"
            mksquashfs "$APPDIR" "$squash_tmp" -root-owned -noappend -comp gzip 2>/dev/null || \
            mksquashfs "$APPDIR" "$squash_tmp" -root-owned -noappend 2>/dev/null
            if [ -f "$squash_tmp" ] && [ -s "$squash_tmp" ]; then
                cat "$runtime_bin" "$squash_tmp" > "$APPIMAGE_OUTPUT"
                # Write Type 2 AppImage magic bytes (0x41 0x49 0x02) at offset 8
                printf '\x41\x49\x02' | dd of="$APPIMAGE_OUTPUT" bs=1 seek=8 count=3 conv=notrunc 2>/dev/null || true
                chmod +x "$APPIMAGE_OUTPUT"
                rm -f "$squash_tmp"
                return 0
            fi
        fi
    fi

    return 1
}

if ! build_appimage_tool; then
    echo "==========================================================" >&2
    echo "ERROR: Failed to create genuine ELF AppImage!" >&2
    echo "AeroSync will NOT create a fake shell script with an .AppImage extension." >&2
    echo "Please ensure appimagetool or mksquashfs is installed on your Linux build system." >&2
    echo "==========================================================" >&2
    exit 1
fi

# Sanity check: Ensure AppImage is a real ELF binary and exceeds minimum size
MIN_APPIMAGE_SIZE=5000000 # 5 MB minimum
ACTUAL_SIZE=$(wc -c < "$APPIMAGE_OUTPUT" 2>/dev/null || stat -c %s "$APPIMAGE_OUTPUT" 2>/dev/null || echo 0)
if [ "$ACTUAL_SIZE" -lt "$MIN_APPIMAGE_SIZE" ]; then
    echo "Error: Generated AppImage is only $ACTUAL_SIZE bytes (expected >= $MIN_APPIMAGE_SIZE bytes)!" >&2
    echo "The AppImage build was incomplete or failed." >&2
    exit 1
fi

FILE_TYPE=$(file "$APPIMAGE_OUTPUT" 2>/dev/null || true)
if ! echo "$FILE_TYPE" | grep -q "ELF"; then
    echo "Error: Generated AppImage $APPIMAGE_OUTPUT is NOT a valid ELF AppImage binary!" >&2
    echo "File type detected: $FILE_TYPE" >&2
    exit 1
fi

chmod +x "$APPIMAGE_OUTPUT"
echo "Generated Real ELF AppImage: $APPIMAGE_OUTPUT ($ACTUAL_SIZE bytes)"

# ------------------------------------------------------------------------------
# 4. Build Self-Contained Debian / Ubuntu / Kubuntu Package (.deb)
# ------------------------------------------------------------------------------
echo -e "\n[3/3] Building Self-Contained Debian Package (.deb)..."

DEB_DIR="$ROOT_DIR/build_linux_pkg/deb/aerosync_${VERSION}_amd64"
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/lib/aerosync"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/pixmaps"

# Control file: Universal baseline dependencies available on every Ubuntu/Kubuntu/Debian release
cat <<EOF > "$DEB_DIR/DEBIAN/control"
Package: aerosync
Version: $VERSION
Architecture: amd64
Maintainer: AeroSync Team <support@aerosync.com>
Depends: libc6 (>= 2.34), libwebkit2gtk-4.1-0, libgtk-3-0 | libgtk-3-0t64, libglib2.0-0 | libglib2.0-0t64, libayatana-appindicator3-1 | libappindicator3-1, librsvg2-2 | librsvg2-2t64
Section: utils
Priority: optional
Homepage: https://github.com/Atulsain011/Aero_sync
Description: AeroSync High-Speed Cross-Platform File Transfer
 AeroSync is an ultra high-speed peer-to-peer file transfer desktop application
 designed for zero-cloud, direct Wi-Fi and mobile hotspot file sharing.
 Requires WebKitGTK 4.1 runtime (libwebkit2gtk-4.1-0).
EOF

# Install binaries into /usr/lib/aerosync (proper Linux FHS layout)
cp "$MAIN_BIN" "$DEB_DIR/usr/lib/aerosync/aerosync"
chmod 755 "$DEB_DIR/usr/lib/aerosync/aerosync"

cp "$DAEMON_BIN" "$DEB_DIR/usr/lib/aerosync/aerosync_daemon"
chmod 755 "$DEB_DIR/usr/lib/aerosync/aerosync_daemon"

# Create launcher in /usr/bin/aerosync
cat <<'EOF' > "$DEB_DIR/usr/bin/aerosync"
#!/usr/bin/env bash
set -e

if [ -d "/usr/lib/aerosync" ]; then
    LIB_DIR="/usr/lib/aerosync"
else
    SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    if [ -d "$SELF_DIR/../lib/aerosync" ]; then
        LIB_DIR="$(cd "$SELF_DIR/../lib/aerosync" && pwd)"
    else
        LIB_DIR="/usr/lib/aerosync"
    fi
fi

# Pre-flight check for WebKitGTK runtime
if command -v ldd >/dev/null 2>&1 && [ -f "$LIB_DIR/aerosync" ]; then
    if ldd "$LIB_DIR/aerosync" 2>/dev/null | grep -q "libwebkit2gtk-4.1.so.0 => not found"; then
        echo "================================================================================" >&2
        echo " AEROSYNC DEPENDENCY ERROR: Missing libwebkit2gtk-4.1.so.0" >&2
        echo " Please install the required WebKitGTK 4.1 package via your package manager:" >&2
        echo "     sudo apt install -y libwebkit2gtk-4.1-0" >&2
        echo "================================================================================" >&2
        if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
            if command -v zenity >/dev/null 2>&1; then
                zenity --error --title="AeroSync - Missing Dependency" --width=480 --text="AeroSync requires libwebkit2gtk-4.1-0.\n\nPlease install it using:\n  sudo apt install -y libwebkit2gtk-4.1-0" 2>/dev/null || true
            fi
        fi
        exit 1
    fi
fi

export PATH="$LIB_DIR:$PATH"

# Ubuntu 24.04/23.10 and Debian 12 AppArmor unprivileged user namespace fix
export WEBKIT_FORCE_SANDBOX=0

# NVIDIA driver Wayland explicit sync fix
export __NV_DISABLE_EXPLICIT_SYNC=1

if [ "$AEROSYNC_FORCE_SOFTWARE_RENDER" = "1" ] || [[ "$*" == *"--software-render"* ]] || [[ "$*" == *"--disable-gpu"* ]]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export WEBKIT_GRAPHICS_POLICY=software
    export GSK_RENDERER=cairo
    export WEBKIT_DISABLE_COMPOSITING_MODE=1
    export WEBKIT_DISABLE_DMABUF_RENDERER=1
fi

# Start native daemon in background if not running
if [ -f "$LIB_DIR/aerosync_daemon" ]; then
    chmod +x "$LIB_DIR/aerosync_daemon" 2>/dev/null || true
    if ! pgrep -f "aerosync_daemon" >/dev/null 2>&1; then
        DAEMON_LOG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/AeroSync"
        mkdir -p "$DAEMON_LOG_DIR" 2>/dev/null || true
        "$LIB_DIR/aerosync_daemon" >> "$DAEMON_LOG_DIR/daemon.log" 2>&1 &
    fi
fi

exec "$LIB_DIR/aerosync" "$@"
EOF
chmod 755 "$DEB_DIR/usr/bin/aerosync"

# Create /usr/bin/aerosync_daemon symlink
ln -sf "/usr/lib/aerosync/aerosync_daemon" "$DEB_DIR/usr/bin/aerosync_daemon"

# Desktop Entry in /usr/share/applications/aerosync.desktop
cat <<'EOF' > "$DEB_DIR/usr/share/applications/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
GenericName=File Transfer
Comment=Ultra High-Speed Cross-Platform Peer-to-Peer File Transfer
Exec=aerosync %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
MimeType=x-scheme-handler/aerosync;
EOF
chmod 644 "$DEB_DIR/usr/share/applications/aerosync.desktop"

# Install authentic AeroSync icons across all standard hicolor theme resolutions
ICONS_SRC_DIR="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons"
for sz in 16x16 24x24 32x32 48x48 64x64 128x128 256x256; do
    mkdir -p "$DEB_DIR/usr/share/icons/hicolor/$sz/apps"
    if [ -f "$ICONS_SRC_DIR/$sz.png" ]; then
        cp "$ICONS_SRC_DIR/$sz.png" "$DEB_DIR/usr/share/icons/hicolor/$sz/apps/aerosync.png"
        chmod 644 "$DEB_DIR/usr/share/icons/hicolor/$sz/apps/aerosync.png"
    fi
done

# 512x512 master resolution
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/512x512/apps"
if [ -f "$ICONS_SRC_DIR/icon.png" ]; then
    cp "$ICONS_SRC_DIR/icon.png" "$DEB_DIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
    chmod 644 "$DEB_DIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
elif [ -f "$ICONS_SRC_DIR/256x256.png" ]; then
    cp "$ICONS_SRC_DIR/256x256.png" "$DEB_DIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
    chmod 644 "$DEB_DIR/usr/share/icons/hicolor/512x512/apps/aerosync.png"
fi

# Pixmaps fallback
mkdir -p "$DEB_DIR/usr/share/pixmaps"
if [ -f "$ICONS_SRC_DIR/256x256.png" ]; then
    cp "$ICONS_SRC_DIR/256x256.png" "$DEB_DIR/usr/share/pixmaps/aerosync.png"
    chmod 644 "$DEB_DIR/usr/share/pixmaps/aerosync.png"
elif [ -f "$ICONS_SRC_DIR/icon.png" ]; then
    cp "$ICONS_SRC_DIR/icon.png" "$DEB_DIR/usr/share/pixmaps/aerosync.png"
    chmod 644 "$DEB_DIR/usr/share/pixmaps/aerosync.png"
elif [ -n "$ICON_SRC" ] && [ -f "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$DEB_DIR/usr/share/pixmaps/aerosync.png"
    chmod 644 "$DEB_DIR/usr/share/pixmaps/aerosync.png"
fi

# Post-install & Post-remove hooks
cat <<'EOF' > "$DEB_DIR/DEBIAN/postinst"
#!/bin/sh
set -e
chmod 755 /usr/bin/aerosync 2>/dev/null || true
chmod 755 /usr/bin/aerosync_daemon 2>/dev/null || true
chmod 755 /usr/lib/aerosync/* 2>/dev/null || true

if [ -x "$(command -v update-desktop-database)" ]; then
    update-desktop-database -q /usr/share/applications || true
fi
if [ -x "$(command -v gtk-update-icon-cache)" ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "$DEB_DIR/DEBIAN/postinst"

cat <<'EOF' > "$DEB_DIR/DEBIAN/postrm"
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    rm -f /usr/bin/aerosync
    rm -f /usr/bin/aerosync_daemon
fi
if [ -x "$(command -v update-desktop-database)" ]; then
    update-desktop-database -q /usr/share/applications || true
fi
if [ -x "$(command -v gtk-update-icon-cache)" ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "$DEB_DIR/DEBIAN/postrm"

# Aggressively ensure NO Windows .exe files exist in DEB_DIR
find "$DEB_DIR" -type f -name "*.exe" -delete

DEB_OUTPUT="$RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb --build "$DEB_DIR" "$DEB_OUTPUT"
    echo "Generated Debian Package: $DEB_OUTPUT"
else
    # Cross-platform Python fallback for building .deb archive
    PY_CMD="python3"
    if ! command -v python3 >/dev/null 2>&1; then
        if command -v python >/dev/null 2>&1; then PY_CMD="python"; else PY_CMD="python.exe"; fi
    fi
    $PY_CMD -c "
import io, os, tarfile

def make_deb(deb_dir, output_deb):
    os.makedirs(os.path.dirname(output_deb), exist_ok=True)
    deb_binary = b'2.0\n'
    control_buf = io.BytesIO()
    with tarfile.open(fileobj=control_buf, mode='w:gz') as tar:
        dp = os.path.join(deb_dir, 'DEBIAN')
        if os.path.exists(dp):
            for root, dirs, files in os.walk(dp):
                for f in files:
                    fp = os.path.join(root, f)
                    rp = './' + os.path.relpath(fp, dp).replace('\\\\', '/')
                    tar.add(fp, arcname=rp)
    c_bytes = control_buf.getvalue()

    data_buf = io.BytesIO()
    with tarfile.open(fileobj=data_buf, mode='w:gz') as tar:
        for root, dirs, files in os.walk(deb_dir):
            if 'DEBIAN' in root.split(os.sep): continue
            for f in files:
                fp = os.path.join(root, f)
                rp = './' + os.path.relpath(fp, deb_dir).replace('\\\\', '/')
                tar.add(fp, arcname=rp)
    d_bytes = data_buf.getvalue()

    def ar_hdr(name, sz):
        return name.ljust(16).encode('ascii') + str(0).ljust(12).encode('ascii') + str(0).ljust(6).encode('ascii') + str(0).ljust(6).encode('ascii') + b'100644  ' + str(sz).ljust(10).encode('ascii') + b'\x60\n'

    with open(output_deb, 'wb') as f:
        f.write(b'!<arch>\n')
        f.write(ar_hdr('debian-binary', len(deb_binary)))
        f.write(deb_binary)
        if len(deb_binary) % 2 != 0: f.write(b'\n')
        f.write(ar_hdr('control.tar.gz', len(c_bytes)))
        f.write(c_bytes)
        if len(c_bytes) % 2 != 0: f.write(b'\n')
        f.write(ar_hdr('data.tar.gz', len(d_bytes)))
        f.write(d_bytes)
        if len(d_bytes) % 2 != 0: f.write(b'\n')

make_deb('$DEB_DIR', '$DEB_OUTPUT')
"
    echo "Generated Debian Package via Python fallback: $DEB_OUTPUT"
fi
cp -f "$DEB_OUTPUT" "$RELEASE_DIR/AeroSync-Linux-x86_64.deb" 2>/dev/null || true

# ------------------------------------------------------------------------------
# Clean Up Obsolete / Broken Release Artifacts
# ------------------------------------------------------------------------------
# Strictly avoid publishing broken AppImages, raw binaries, launcher scripts, or incomplete archives
rm -rf "$ROOT_DIR/build_linux_pkg/portable"
rm -f "$RELEASE_DIR/AeroSync-Linux-Portable.tar.gz"
rm -f "$RELEASE_DIR/Start_AeroSync_Linux.sh"
rm -f "$RELEASE_DIR/aerosync"
rm -f "$RELEASE_DIR/AeroSync"

echo -e "\n=========================================================="
echo " LINUX PACKAGING COMPLETE! OFFICIAL RELEASE ARTIFACTS:"
echo " 1. Linux — AppImage (Recommended): $APPIMAGE_OUTPUT"
echo " 2. Linux — Debian/Ubuntu (.deb):   $DEB_OUTPUT"
echo "=========================================================="
