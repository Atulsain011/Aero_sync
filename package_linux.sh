#!/usr/bin/env bash
# ==============================================================================
# AeroSync Standalone Linux Packaging Script (.deb & AppImage)
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
if [ -f "$RELEASE_DIR/AeroSync" ]; then
    MAIN_BIN="$RELEASE_DIR/AeroSync"
elif [ -f "$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop" ]; then
    MAIN_BIN="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/target/release/aerosync-desktop"
fi

DAEMON_BIN=""
if [ -f "$RELEASE_DIR/aerosync_daemon" ]; then
    DAEMON_BIN="$RELEASE_DIR/aerosync_daemon"
elif [ -f "$ROOT_DIR/build_linux/aerosync_daemon" ]; then
    DAEMON_BIN="$ROOT_DIR/build_linux/aerosync_daemon"
fi

ICON_SRC="$ROOT_DIR/platform/windows/desktop_tauri/src-tauri/icons/256x256.png"

# ------------------------------------------------------------------------------
# 1. Build Debian Package (.deb)
# ------------------------------------------------------------------------------
if command -v dpkg-deb >/dev/null 2>&1 && [ -n "$MAIN_BIN" ]; then
    echo -e "\n[1/2] Building Debian Package (.deb)..."
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
Depends: libwebkit2gtk-4.1-0 | libwebkit2gtk-4.0-37, libgtk-3-0, libayatana-appindicator3-1 | libappindicator3-1
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

    dpkg-deb --build "$DEB_DIR" "$RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
    echo "Generated Debian Package: $RELEASE_DIR/aerosync_${VERSION}_amd64.deb"
else
    echo "Skipping .deb generation (dpkg-deb tool or AeroSync executable not found)."
fi

# ------------------------------------------------------------------------------
# 2. Build AppImage (.AppImage) Container
# ------------------------------------------------------------------------------
if [ -n "$MAIN_BIN" ]; then
    echo -e "\n[2/3] Preparing AppImage Structure (AppDir)..."
    APPDIR="$ROOT_DIR/build_linux_pkg/appimage/AeroSync.AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/bin"

    # AppRun Entrypoint Script
    cat <<'EOF' > "$APPDIR/AppRun"
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="$HERE/usr/bin:$PATH"
export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"

# Launch background C++ daemon if present and executable
if [ -f "$HERE/usr/bin/aerosync_daemon" ]; then
    chmod +x "$HERE/usr/bin/aerosync_daemon" 2>/dev/null || true
    "$HERE/usr/bin/aerosync_daemon" >/dev/null 2>&1 &
fi

exec "$HERE/usr/bin/aerosync" "$@"
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

    if command -v appimagetool >/dev/null 2>&1; then
        appimagetool "$APPDIR" "$RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
        echo "Generated AppImage: $RELEASE_DIR/AeroSync-v${VERSION}-x86_64.AppImage"
    else
        echo "AppDir structure prepared at: $APPDIR"
        echo "Install 'appimagetool' or use Tauri CLI to build final single-file .AppImage binary."
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

    # Create portable launcher script
    cat <<'EOF' > "$PORTABLE_DIR/launch_aerosync.sh"
#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$SCRIPT_DIR:$PATH"
export LD_LIBRARY_PATH="$SCRIPT_DIR:$LD_LIBRARY_PATH"

if [ -f "$SCRIPT_DIR/aerosync_daemon" ]; then
    chmod +x "$SCRIPT_DIR/aerosync_daemon"
    "$SCRIPT_DIR/aerosync_daemon" >/dev/null 2>&1 &
fi

chmod +x "$SCRIPT_DIR/AeroSync"
exec "$SCRIPT_DIR/AeroSync" "$@"
EOF
    chmod +x "$PORTABLE_DIR/launch_aerosync.sh"

    cat <<EOF > "$PORTABLE_DIR/aerosync.desktop"
[Desktop Entry]
Name=AeroSync
Comment=Ultra High-Speed P2P File Transfer
Exec=/bin/bash -c "cd '%k' && ./launch_aerosync.sh %U"
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

