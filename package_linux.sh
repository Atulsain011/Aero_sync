#!/usr/bin/env bash
# ==============================================================================
# AeroSync Standalone Linux Packaging Script (.deb, AppImage & Portable Archive)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"
RELEASE_DIR="$ROOT_DIR/release"
VERSION="1.0.7"

mkdir -p "$RELEASE_DIR"

echo "=========================================================="
echo " AEROSYNC LINUX PACKAGING TOOL (v$VERSION)"
echo "=========================================================="

# Locate desktop executable & daemon
MAIN_BIN=""
if [ -f "$RELEASE_DIR/AeroSync" ] && file "$RELEASE_DIR/AeroSync" 2>/dev/null | grep -q "ELF"; then
    MAIN_BIN="$RELEASE_DIR/AeroSync"
elif [ -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop" ]; then
    MAIN_BIN="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop"
elif [ -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/x86_64-unknown-linux-gnu/release/aerosync-desktop" ]; then
    MAIN_BIN="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/x86_64-unknown-linux-gnu/release/aerosync-desktop"
fi

DAEMON_BIN=""
if [ -f "$RELEASE_DIR/aerosync_daemon" ] && file "$RELEASE_DIR/aerosync_daemon" 2>/dev/null | grep -q "ELF"; then
    DAEMON_BIN="$RELEASE_DIR/aerosync_daemon"
elif [ -f "$ROOT_DIR/build_linux/aerosync_daemon" ] && file "$ROOT_DIR/build_linux/aerosync_daemon" 2>/dev/null | grep -q "ELF"; then
    DAEMON_BIN="$ROOT_DIR/build_linux/aerosync_daemon"
fi

ICON_SRC="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons/256x256.png"

# ------------------------------------------------------------------------------
# 1. Build Debian Package (.deb)
# ------------------------------------------------------------------------------
if [ -n "$MAIN_BIN" ]; then
    echo -e "\n[1/3] Building Debian Package (.deb)..."
    DEB_DIR="$ROOT_DIR/build_linux_pkg/deb/aerosync_${VERSION}_amd64"
    rm -rf "$DEB_DIR"
    mkdir -p "$DEB_DIR/DEBIAN"
    mkdir -p "$DEB_DIR/usr/bin"
    mkdir -p "$DEB_DIR/usr/share/applications"
    mkdir -p "$DEB_DIR/usr/share/icons/hicolor/256x256/apps"

    # Control File
    cat <<EOF > "$DEB_DIR/DEBIAN/control"
Package: aerosync
Version: $VERSION
Architecture: amd64
Maintainer: AeroSync Team <support@aerosync.com>
Depends: libwebkit2gtk-4.1-0 | libwebkit2gtk-4.0-37, libgtk-3-0, libayatana-appindicator3-1 | libappindicator3-1, librsvg2-2, libssl3 | libssl1.1
Section: utils
Priority: optional
Homepage: https://github.com/Atulsain011/Aero_sync
Description: AeroSync High-Speed Cross-Platform File Transfer
 AeroSync is an ultra high-speed peer-to-peer file transfer desktop application
 designed for zero-cloud, direct Wi-Fi and mobile hotspot file sharing.
EOF

    # Desktop Shortcut
    cat <<EOF > "$DEB_DIR/usr/share/applications/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
Comment=Ultra High-Speed P2P File Transfer
Exec=/usr/bin/aerosync %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Network;FileTransfer;Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
EOF

    # Post-installation script for desktop and icon database refresh
    cat <<EOF > "$DEB_DIR/DEBIAN/postinst"
#!/bin/sh
set -e
if [ -x "\$(command -v update-desktop-database)" ]; then
    update-desktop-database -q || true
fi
if [ -x "\$(command -v gtk-update-icon-cache)" ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
EOF
    chmod 755 "$DEB_DIR/DEBIAN/postinst"

    cat <<EOF > "$DEB_DIR/DEBIAN/postrm"
#!/bin/sh
set -e
if [ -x "\$(command -v update-desktop-database)" ]; then
    update-desktop-database -q || true
fi
if [ -x "\$(command -v gtk-update-icon-cache)" ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
EOF
    chmod 755 "$DEB_DIR/DEBIAN/postrm"

    # Install binaries
    cp "$MAIN_BIN" "$DEB_DIR/usr/bin/aerosync"
    chmod +x "$DEB_DIR/usr/bin/aerosync"

    if [ -n "$DAEMON_BIN" ]; then
        cp "$DAEMON_BIN" "$DEB_DIR/usr/bin/aerosync_daemon"
        chmod +x "$DEB_DIR/usr/bin/aerosync_daemon"
    fi

    if [ -f "$ICON_SRC" ]; then
        cp "$ICON_SRC" "$DEB_DIR/usr/share/icons/hicolor/256x256/apps/aerosync.png"
    fi

    if command -v dpkg-deb >/dev/null 2>&1; then
        dpkg-deb --build "$DEB_DIR" "$RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
        echo "Generated Debian Package via dpkg-deb: $RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
    elif command -v python.exe >/dev/null 2>&1 || command -v python3 >/dev/null 2>&1 || command -v python >/dev/null 2>&1; then
        PY_CMD="python.exe"
        if ! command -v python.exe >/dev/null 2>&1; then
            if command -v python3 >/dev/null 2>&1; then PY_CMD="python3"; else PY_CMD="python"; fi
        fi
        $PY_CMD -c "
import io, os, tarfile

def norm(p):
    if p.startswith('/c/') or p.startswith('/C/'):
        return 'C:/' + p[3:]
    return p

def make_deb(deb_dir, output_deb):
    deb_dir = norm(deb_dir)
    output_deb = norm(output_deb)
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

make_deb('$DEB_DIR', '$RELEASE_DIR/aerosync_${VERSION}_amd64.deb')
"
        echo "Generated Debian Package via Python: $RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
    fi
else
    echo "Skipping .deb generation (No native Linux AeroSync executable found)."
fi

# ------------------------------------------------------------------------------
# 2. Build AppImage (.AppImage) Container
# ------------------------------------------------------------------------------
if [ -n "$MAIN_BIN" ]; then
    echo -e "\n[2/3] Preparing AppImage Structure (AppDir)..."
    APPDIR="$ROOT_DIR/build_linux_pkg/appimage/AeroSync.AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/bin"

    # AppRun Entrypoint Script with WebKit2GTK DMA-BUF/Compositing Fix & Software Rendering Fallback
    cat <<'EOF' > "$APPDIR/AppRun"
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="$HERE/usr/bin:$HERE:$PATH"

# WebKitGTK Linux DMA-BUF & Compositing compatibility flags
export WEBKIT_DISABLE_DMABUF_RENDERER=1
export WEBKIT_DISABLE_COMPOSITING_MODE=1

# Software rendering fallback if GPU acceleration fails or is forced
if [ "$AEROSYNC_FORCE_SOFTWARE_RENDER" = "1" ] || [[ "$*" == *"--software-render"* ]] || [[ "$*" == *"--disable-gpu"* ]]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export WEBKIT_GRAPHICS_POLICY=software
    export GSK_RENDERER=cairo
fi

if [ -d "$HERE/usr/lib/aerosync" ]; then
    export LD_LIBRARY_PATH="$HERE/usr/lib/aerosync:$LD_LIBRARY_PATH"
fi

MAIN_EXEC="$HERE/usr/bin/aerosync"
if [ ! -f "$MAIN_EXEC" ]; then
    MAIN_EXEC="$HERE/AeroSync"
fi
chmod +x "$MAIN_EXEC" 2>/dev/null || true
exec "$MAIN_EXEC" "$@"
EOF
    chmod +x "$APPDIR/AppRun"

    # Desktop Entry
    cat <<EOF > "$APPDIR/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
Comment=Ultra High-Speed P2P File Transfer
Exec=aerosync %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Network;FileTransfer;Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
EOF
    cp "$APPDIR/aerosync.desktop" "$APPDIR/AppRun.desktop" 2>/dev/null || true

    cp "$MAIN_BIN" "$APPDIR/usr/bin/aerosync"
    chmod +x "$APPDIR/usr/bin/aerosync"

    if [ -n "$DAEMON_BIN" ]; then
        cp "$DAEMON_BIN" "$APPDIR/usr/bin/aerosync_daemon"
        chmod +x "$APPDIR/usr/bin/aerosync_daemon"
    fi

    if [ -f "$ICON_SRC" ]; then
        cp "$ICON_SRC" "$APPDIR/aerosync.png"
        cp "$ICON_SRC" "$APPDIR/.DirIcon"
    fi

    TAURI_APPIMAGE="$(find "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target" -name "*.AppImage" 2>/dev/null | head -n 1)"
    if command -v appimagetool >/dev/null 2>&1; then
        appimagetool "$APPDIR" "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
        echo "Generated AppImage via appimagetool: $RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
    elif [ -n "$TAURI_APPIMAGE" ] && [ -f "$TAURI_APPIMAGE" ]; then
        cp "$TAURI_APPIMAGE" "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
        chmod +x "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
        echo "Generated AppImage via Tauri CLI bundle: $RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
    else
        # AppDir Container structure prepared & launcher wrapper script
        echo "AppDir structure prepared at: $APPDIR"
        cat <<'EOF' > "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
APPDIR="$HERE/build_linux_pkg/appimage/AeroSync.AppDir"
if [ ! -d "$APPDIR" ]; then
    APPDIR="$HERE/../build_linux_pkg/appimage/AeroSync.AppDir"
fi
if [ ! -d "$APPDIR" ]; then
    APPDIR="$HERE/AeroSync.AppDir"
fi
if [ -d "$APPDIR" ]; then
    exec "$APPDIR/AppRun" "$@"
elif [ -f "$HERE/AeroSync" ]; then
    exec "$HERE/AeroSync" "$@"
elif [ -f "$HERE/usr/bin/aerosync" ]; then
    exec "$HERE/usr/bin/aerosync" "$@"
else
    echo "Error: AeroSync executable or AppDir container directory not found."
    echo "Looked at: $APPDIR and $HERE"
    exit 1
fi
EOF
        chmod +x "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
        echo "Generated AppImage Container Launcher: $RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
    fi
fi

# ------------------------------------------------------------------------------
# 3. Build Linux Portable Archive (.tar.gz)
# ------------------------------------------------------------------------------
if [ -n "$MAIN_BIN" ]; then
    echo -e "\n[3/3] Building Linux Portable Archive (.tar.gz)..."
    PORTABLE_BUILD_DIR="$ROOT_DIR/build_linux_pkg/portable"
    PORTABLE_DIR="$PORTABLE_BUILD_DIR/AeroSync-Linux-Portable"
    rm -rf "$PORTABLE_BUILD_DIR"
    mkdir -p "$PORTABLE_DIR"

    cp "$MAIN_BIN" "$PORTABLE_DIR/AeroSync"
    chmod +x "$PORTABLE_DIR/AeroSync"

    if [ -n "$DAEMON_BIN" ]; then
        cp "$DAEMON_BIN" "$PORTABLE_DIR/aerosync_daemon"
        chmod +x "$PORTABLE_DIR/aerosync_daemon"
    fi

    if [ -f "$ICON_SRC" ]; then
        cp "$ICON_SRC" "$PORTABLE_DIR/aerosync.png"
    fi

    # Create portable launcher script with WebKit2GTK compatibility and software rendering fallback
    cat <<'EOF' > "$PORTABLE_DIR/launch_aerosync.sh"
#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$SCRIPT_DIR:$PATH"
if [ -d "$SCRIPT_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
fi

# WebKitGTK Linux DMA-BUF compatibility flags
export WEBKIT_DISABLE_DMABUF_RENDERER=1

# Software rendering fallback if GPU acceleration fails or is forced
if [ "$AEROSYNC_FORCE_SOFTWARE_RENDER" = "1" ] || [[ "$*" == *"--software-render"* ]] || [[ "$*" == *"--disable-gpu"* ]]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export WEBKIT_GRAPHICS_POLICY=software
    export GSK_RENDERER=cairo
fi

if [ -f "$SCRIPT_DIR/aerosync_daemon" ]; then
    chmod +x "$SCRIPT_DIR/aerosync_daemon" 2>/dev/null || true
    "$SCRIPT_DIR/aerosync_daemon" >/dev/null 2>&1 &
fi

chmod +x "$SCRIPT_DIR/AeroSync" 2>/dev/null || true
exec "$SCRIPT_DIR/AeroSync" "$@"
EOF
    chmod +x "$PORTABLE_DIR/launch_aerosync.sh"

    cat <<EOF > "$PORTABLE_DIR/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
Comment=Ultra High-Speed P2P File Transfer
Exec=sh -c 'DIR="\$(dirname "%k")"; cd "\$DIR" && ./launch_aerosync.sh' %U
Icon=aerosync
Terminal=false
Type=Application
Categories=Network;FileTransfer;Utility;
Keywords=P2P;File;Share;Transfer;Speed;AeroSync;
StartupWMClass=aerosync
EOF

    cd "$PORTABLE_BUILD_DIR"
    tar -czf "$RELEASE_DIR/AeroSync-Linux-Portable.tar.gz" AeroSync-Linux-Portable
    echo "Generated Linux Portable Archive: $RELEASE_DIR/AeroSync-Linux-Portable.tar.gz"
fi

echo -e "\n=========================================================="
echo " LINUX PACKAGING COMPLETE!"
echo " Output artifacts in: $RELEASE_DIR"
echo "=========================================================="


