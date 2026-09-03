#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Manager, Window};

#[cfg(windows)]
use std::os::windows::process::CommandExt;

struct DaemonState {
    child: Arc<Mutex<Option<Child>>>,
}

// Windows constant for hidden console
#[cfg(windows)]
const CREATE_NO_WINDOW: u32 = 0x08000000;

fn find_daemon_executable() -> Option<PathBuf> {
    let exe_name = if cfg!(windows) {
        "aerosync_daemon.exe"
    } else {
        "aerosync_daemon"
    };

    // 1. Check in same directory as current executable (Standalone / Portable / AppImage location)
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(current_dir) = current_exe.parent() {
            let candidate1 = current_dir.join(exe_name);
            if candidate1.exists() {
                return Some(candidate1);
            }
            let candidate_res = current_dir.join("resources").join(exe_name);
            if candidate_res.exists() {
                return Some(candidate_res);
            }
            let candidate_res_up = current_dir.join("..").join("resources").join(exe_name);
            if candidate_res_up.exists() {
                return Some(candidate_res_up);
            }
            let candidate_lib_res = current_dir.join("..").join("lib").join("aerosync").join("resources").join(exe_name);
            if candidate_lib_res.exists() {
                return Some(candidate_lib_res);
            }
        }
    }

    // 2. Platform-specific user/system runtime directories
    #[cfg(windows)]
    {
        let mut runtime_dir = std::env::var_os("LOCALAPPDATA")
            .map(PathBuf::from)
            .unwrap_or_else(|| std::env::temp_dir());
        runtime_dir.push("AeroSync");
        runtime_dir.push("bin");
        let embedded_daemon = runtime_dir.join("aerosync_daemon.exe");
        if embedded_daemon.exists() {
            return Some(embedded_daemon);
        }
    }

    #[cfg(not(windows))]
    {
        // 2a. AppImage Container ($APPDIR) Resolution
        if let Some(appdir) = std::env::var_os("APPDIR") {
            let appdir_path = PathBuf::from(appdir);
            let appdir_candidates = [
                appdir_path.join("usr").join("bin").join("aerosync_daemon"),
                appdir_path.join("usr").join("lib").join("aerosync").join("resources").join("aerosync_daemon"),
                appdir_path.join("usr").join("lib").join("aerosync").join("aerosync_daemon"),
                appdir_path.join("resources").join("aerosync_daemon"),
            ];
            for path in appdir_candidates.iter() {
                if path.exists() {
                    return Some(path.clone());
                }
            }
        }

        if let Some(home) = std::env::var_os("HOME") {
            let user_bin = PathBuf::from(&home)
                .join(".local")
                .join("bin")
                .join("aerosync_daemon");
            if user_bin.exists() {
                return Some(user_bin);
            }
            let user_share_bin = PathBuf::from(&home)
                .join(".local")
                .join("share")
                .join("AeroSync")
                .join("bin")
                .join("aerosync_daemon");
            if user_share_bin.exists() {
                return Some(user_share_bin);
            }
            let home_bin = PathBuf::from(&home).join("bin").join("aerosync_daemon");
            if home_bin.exists() {
                return Some(home_bin);
            }
        }
        let system_paths = [
            PathBuf::from("/usr/bin/aerosync_daemon"),
            PathBuf::from("/usr/local/bin/aerosync_daemon"),
            PathBuf::from("/opt/aerosync/aerosync_daemon"),
            PathBuf::from("/opt/aerosync/bin/aerosync_daemon"),
            PathBuf::from("/usr/lib/aerosync/aerosync_daemon"),
            PathBuf::from("/usr/lib/aerosync/resources/aerosync_daemon"),
            PathBuf::from("/usr/share/aerosync/resources/aerosync_daemon"),
        ];
        for sys_path in system_paths.iter() {
            if sys_path.exists() {
                return Some(sys_path.clone());
            }
        }
    }

    // 3. Check in relative build directories
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(current_dir) = current_exe.parent() {
            let candidates = [
                current_dir.join("build_windows").join(exe_name),
                current_dir.join("build_linux").join(exe_name),
                current_dir.join("..").join("build_windows").join(exe_name),
                current_dir.join("..").join("build_linux").join(exe_name),
                current_dir.join("..").join("..").join("build_windows").join(exe_name),
                current_dir.join("..").join("..").join("build_linux").join(exe_name),
            ];

            for path in candidates.iter() {
                if path.exists() {
                    return Some(path.clone());
                }
            }
        }
    }

    None
}

fn is_daemon_running() -> bool {
    use std::net::{SocketAddr, TcpStream};
    use std::time::Duration;
    if let Ok(addr) = "127.0.0.1:48126".parse::<SocketAddr>() {
        if TcpStream::connect_timeout(&addr, Duration::from_millis(150)).is_ok() {
            return true;
        }
    }
    false
}

fn start_daemon_process(state: &DaemonState) {
    let child_arc = state.child.clone();
    std::thread::spawn(move || {
        // Check if daemon is active or in startup phase
        for _ in 0..20 {
            if is_daemon_running() {
                println!("[AeroSync] AeroSync Core Daemon is already active on 127.0.0.1:48126.");
                return;
            }
            std::thread::sleep(std::time::Duration::from_millis(100));
        }

        if is_daemon_running() {
            println!("[AeroSync] AeroSync Core Daemon is already active on 127.0.0.1:48126.");
            return;
        }

        if let Some(daemon_path) = find_daemon_executable() {
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                if let Ok(meta) = std::fs::metadata(&daemon_path) {
                    let mut perms = meta.permissions();
                    if perms.mode() & 0o111 == 0 {
                        perms.set_mode(perms.mode() | 0o755);
                        let _ = std::fs::set_permissions(&daemon_path, perms);
                    }
                }
            }

            let mut cmd = Command::new(&daemon_path);

            #[cfg(windows)]
            cmd.creation_flags(CREATE_NO_WINDOW);

            match cmd.spawn() {
                Ok(child) => {
                    let mut guard = child_arc.lock().unwrap();
                    *guard = Some(child);
                }
                Err(e) => {
                    eprintln!("[AeroSync] Warning: Failed to spawn daemon process: {}", e);
                }
            }
        }
    });
}

fn stop_daemon_process(state: &DaemonState) {
    let mut guard = state.child.lock().unwrap();
    if let Some(mut child) = guard.take() {
        println!("[AeroSync] Shutting down background daemon process...");
        let _ = child.kill();
        let _ = child.wait();
    }
}

#[tauri::command]
fn minimize_window(window: Window) {
    let _ = window.minimize();
}

#[tauri::command]
fn maximize_window(window: Window) {
    if let Ok(is_maximized) = window.is_maximized() {
        if is_maximized {
            let _ = window.unmaximize();
        } else {
            let _ = window.maximize();
        }
    }
}

#[tauri::command]
fn close_window(window: Window) {
    let _ = window.close();
}

#[tauri::command]
fn show_in_folder(path: String) -> Result<(), String> {
    #[cfg(windows)]
    {
        let p = Path::new(&path);
        if !p.exists() {
            return Err("File path does not exist".into());
        }
        let clean_path = path.replace('/', "\\");
        Command::new("explorer.exe")
            .arg("/select,")
            .arg(&clean_path)
            .spawn()
            .map_err(|e| e.to_string())?;
        Ok(())
    }
    #[cfg(not(windows))]
    {
        let p = Path::new(&path);
        if !p.exists() {
            return Err("File path does not exist".into());
        }
        let target = if p.is_file() {
            p.parent().unwrap_or(p)
        } else {
            p
        };
        Command::new("xdg-open")
            .arg(target)
            .spawn()
            .map_err(|e| e.to_string())?;
        Ok(())
    }
}

#[tauri::command]
fn open_folder(path: String) -> Result<(), String> {
    #[cfg(windows)]
    {
        let clean_path = path.replace('/', "\\");
        Command::new("explorer.exe")
            .arg(&clean_path)
            .spawn()
            .map_err(|e| e.to_string())?;
        Ok(())
    }
    #[cfg(not(windows))]
    {
        let p = Path::new(&path);
        if !p.exists() {
            return Err("Folder path does not exist".into());
        }
        Command::new("xdg-open")
            .arg(p)
            .spawn()
            .map_err(|e| e.to_string())?;
        Ok(())
    }
}

#[derive(serde::Serialize)]
struct DiskSpaceInfo {
    free_bytes: u64,
    total_bytes: u64,
}

#[tauri::command]
fn get_disk_space(path: Option<String>) -> Result<DiskSpaceInfo, String> {
    #[cfg(windows)]
    {
        use std::ffi::OsStr;
        use std::os::windows::ffi::OsStrExt;

        let target_path = path.unwrap_or_else(|| "C:\\".to_string());
        let wide: Vec<u16> = OsStr::new(&target_path)
            .encode_wide()
            .chain(std::iter::once(0))
            .collect();

        let mut free_bytes_available_to_caller: u64 = 0;
        let mut total_number_of_bytes: u64 = 0;
        let mut total_number_of_free_bytes: u64 = 0;

        extern "system" {
            fn GetDiskFreeSpaceExW(
                lpDirectoryName: *const u16,
                lpFreeBytesAvailableToCaller: *mut u64,
                lpTotalNumberOfBytes: *mut u64,
                lpTotalNumberOfFreeBytes: *mut u64,
            ) -> i32;
        }

        let success = unsafe {
            GetDiskFreeSpaceExW(
                wide.as_ptr(),
                &mut free_bytes_available_to_caller,
                &mut total_number_of_bytes,
                &mut total_number_of_free_bytes,
            )
        };

        if success != 0 {
            Ok(DiskSpaceInfo {
                free_bytes: free_bytes_available_to_caller,
                total_bytes: total_number_of_bytes,
            })
        } else {
            // Fallback default
            Ok(DiskSpaceInfo {
                free_bytes: 100 * 1024 * 1024 * 1024,
                total_bytes: 512 * 1024 * 1024 * 1024,
            })
        }
    }
    #[cfg(not(windows))]
    {
        use std::ffi::CString;
        let target_path = path.unwrap_or_else(|| "/".to_string());
        if let Ok(c_path) = CString::new(target_path) {
            unsafe {
                let mut stat: libc::statvfs = std::mem::zeroed();
                if libc::statvfs(c_path.as_ptr(), &mut stat) == 0 {
                    let free_bytes = (stat.f_bavail as u64) * (stat.f_frsize as u64);
                    let total_bytes = (stat.f_blocks as u64) * (stat.f_frsize as u64);
                    return Ok(DiskSpaceInfo {
                        free_bytes,
                        total_bytes,
                    });
                }
            }
        }
        Ok(DiskSpaceInfo {
            free_bytes: 100 * 1024 * 1024 * 1024,
            total_bytes: 512 * 1024 * 1024 * 1024,
        })
    }
}

#[tauri::command]
async fn pick_files() -> Result<Vec<String>, String> {
    let files = rfd::AsyncFileDialog::new()
        .set_title("Select Files to Send via AeroSync")
        .pick_files()
        .await;

    if let Some(handles) = files {
        let paths: Vec<String> = handles
            .into_iter()
            .map(|h| h.path().to_string_lossy().to_string())
            .collect();
        Ok(paths)
    } else {
        Ok(Vec::new())
    }
}

#[tauri::command]
async fn pick_folder() -> Result<Option<String>, String> {
    let folder = rfd::AsyncFileDialog::new()
        .set_title("Select Folder to Send via AeroSync")
        .pick_folder()
        .await;

    if let Some(handle) = folder {
        Ok(Some(handle.path().to_string_lossy().to_string()))
    } else {
        Ok(None)
    }
}

#[derive(serde::Serialize)]
struct FileMetadataItem {
    path: String,
    name: String,
    size: u64,
}

#[tauri::command]
async fn get_files_metadata(paths: Vec<String>) -> Result<Vec<FileMetadataItem>, String> {
    let mut results = Vec::new();
    for p in paths {
        let path = std::path::Path::new(&p);
        let name = path
            .file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_else(|| p.clone());
        let size = std::fs::metadata(path).map(|m| m.len()).unwrap_or(0);
        results.push(FileMetadataItem { path: p, name, size });
    }
    Ok(results)
}

#[cfg(windows)]
fn ensure_runtime_assets() {
    static DAEMON_BYTES: &[u8] = include_bytes!("../aerosync_daemon.exe");

    // 1. Terminate any running daemon process so binaries can be overwritten cleanly
    let _ = Command::new("taskkill")
        .args(["/F", "/IM", "aerosync_daemon.exe"])
        .creation_flags(CREATE_NO_WINDOW)
        .output();

    let mut runtime_dir = std::env::var_os("LOCALAPPDATA")
        .map(PathBuf::from)
        .unwrap_or_else(|| std::env::temp_dir());
    runtime_dir.push("AeroSync");
    runtime_dir.push("bin");

    if let Err(e) = std::fs::create_dir_all(&runtime_dir) {
        eprintln!("[AeroSync] Warning: Failed to create runtime dir: {}", e);
        runtime_dir = std::env::temp_dir().join("AeroSync_bin");
        let _ = std::fs::create_dir_all(&runtime_dir);
    }

    // 2. Unpack current static aerosync_daemon.exe
    let files: &[(&str, &[u8])] = &[
        ("aerosync_daemon.exe", DAEMON_BYTES),
    ];

    for (filename, bytes) in files {
        let dest = runtime_dir.join(filename);
        let write_needed = match std::fs::metadata(&dest) {
            Ok(meta) => meta.len() != bytes.len() as u64,
            Err(_) => true,
        };
        if write_needed {
            let _ = std::fs::remove_file(&dest);
            let _ = std::fs::write(&dest, bytes);
        }

        // Also ensure current executable directory has aerosync_daemon.exe
        if let Ok(current_exe) = std::env::current_exe() {
            if let Some(current_dir) = current_exe.parent() {
                let local_dest = current_dir.join(filename);
                let local_write_needed = match std::fs::metadata(&local_dest) {
                    Ok(meta) => meta.len() != bytes.len() as u64,
                    Err(_) => true,
                };
                if local_write_needed {
                    let _ = std::fs::remove_file(&local_dest);
                    let _ = std::fs::write(&local_dest, bytes);
                }
            }
        }
    }

    // 3. Clean up legacy runtime DLLs if present to avoid system DLL load conflicts
    let legacy_dlls = ["libc++.dll", "libunwind.dll", "libwinpthread-1.dll"];
    for dll in legacy_dlls {
        let legacy_path = runtime_dir.join(dll);
        if legacy_path.exists() {
            let _ = std::fs::remove_file(legacy_path);
        }
        if let Ok(current_exe) = std::env::current_exe() {
            if let Some(current_dir) = current_exe.parent() {
                let local_legacy = current_dir.join(dll);
                if local_legacy.exists() {
                    let _ = std::fs::remove_file(local_legacy);
                }
            }
        }
    }

    if let Ok(current_path) = std::env::var("PATH") {
        let new_path = format!("{};{}", runtime_dir.to_string_lossy(), current_path);
        std::env::set_var("PATH", new_path);
    }
}

#[cfg(not(windows))]
fn ensure_runtime_assets() {
    static DAEMON_BYTES: &[u8] = include_bytes!("../aerosync_daemon");

    let mut runtime_dir = if let Some(home) = std::env::var_os("HOME") {
        let mut p = PathBuf::from(home);
        p.push(".local");
        p.push("share");
        p.push("AeroSync");
        p.push("bin");
        p
    } else {
        std::env::temp_dir().join("AeroSync_bin")
    };

    if let Err(e) = std::fs::create_dir_all(&runtime_dir) {
        eprintln!("[AeroSync] Warning: Failed to create runtime dir: {}", e);
        runtime_dir = std::env::temp_dir().join("AeroSync_bin");
        let _ = std::fs::create_dir_all(&runtime_dir);
    }

    let dest = runtime_dir.join("aerosync_daemon");
    let write_needed = match std::fs::metadata(&dest) {
        Ok(meta) => meta.len() != DAEMON_BYTES.len() as u64,
        Err(_) => true,
    };
    if write_needed {
        let _ = std::fs::remove_file(&dest);
        let _ = std::fs::write(&dest, DAEMON_BYTES);
    }

    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        if let Ok(meta) = std::fs::metadata(&dest) {
            let mut perms = meta.permissions();
            perms.set_mode(0o755);
            let _ = std::fs::set_permissions(&dest, perms);
        }
    }

    // Also extract to current executable directory if missing and writable
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(current_dir) = current_exe.parent() {
            let local_dest = current_dir.join("aerosync_daemon");
            let local_write_needed = match std::fs::metadata(&local_dest) {
                Ok(meta) => meta.len() != DAEMON_BYTES.len() as u64,
                Err(_) => true,
            };
            if local_write_needed {
                if let Ok(_) = std::fs::write(&local_dest, DAEMON_BYTES) {
                    #[cfg(unix)]
                    {
                        use std::os::unix::fs::PermissionsExt;
                        if let Ok(meta) = std::fs::metadata(&local_dest) {
                            let mut perms = meta.permissions();
                            perms.set_mode(0o755);
                            let _ = std::fs::set_permissions(&local_dest, perms);
                        }
                    }
                }
            }
        }
    }

    if let Ok(current_path) = std::env::var("PATH") {
        let new_path = format!("{}:{}", runtime_dir.to_string_lossy(), current_path);
        std::env::set_var("PATH", new_path);
    }
}

#[tauri::command]
fn set_autostart(enable: bool) -> Result<(), String> {
    #[cfg(windows)]
    {
        if let Ok(current_exe) = std::env::current_exe() {
            let exe_path = current_exe.to_string_lossy().to_string();
            let clean_exe = exe_path.replace('/', "\\");
            if enable {
                let status = Command::new("reg")
                    .args([
                        "add",
                        "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        "/v",
                        "AeroSync",
                        "/t",
                        "REG_SZ",
                        "/d",
                        &format!("\"{}\"", clean_exe),
                        "/f",
                    ])
                    .creation_flags(CREATE_NO_WINDOW)
                    .status();
                match status {
                    Ok(s) if s.success() => Ok(()),
                    Ok(s) => Err(format!("reg add exited with status {}", s)),
                    Err(e) => Err(e.to_string()),
                }
            } else {
                let _ = Command::new("reg")
                    .args([
                        "delete",
                        "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        "/v",
                        "AeroSync",
                        "/f",
                    ])
                    .creation_flags(CREATE_NO_WINDOW)
                    .status();
                Ok(())
            }
        } else {
            Err("Could not determine current executable path".into())
        }
    }
    #[cfg(not(windows))]
    {
        if let Ok(home) = std::env::var("HOME") {
            let autostart_dir = PathBuf::from(home).join(".config").join("autostart");
            let desktop_file = autostart_dir.join("aerosync.desktop");
            if enable {
                let _ = std::fs::create_dir_all(&autostart_dir);
                if let Ok(current_exe) = std::env::current_exe() {
                    let content = format!(
                        "[Desktop Entry]\nType=Application\nName=AeroSync\nExec=\"{}\"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n",
                        current_exe.to_string_lossy()
                    );
                    let _ = std::fs::write(&desktop_file, content);
                }
            } else {
                if desktop_file.exists() {
                    let _ = std::fs::remove_file(desktop_file);
                }
            }
        }
        Ok(())
    }
}

#[tauri::command]
fn get_autostart() -> Result<bool, String> {
    #[cfg(windows)]
    {
        let output = Command::new("reg")
            .args([
                "query",
                "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                "/v",
                "AeroSync",
            ])
            .creation_flags(CREATE_NO_WINDOW)
            .output();
        match output {
            Ok(out) => Ok(out.status.success()),
            Err(_) => Ok(false),
        }
    }
    #[cfg(not(windows))]
    {
        if let Ok(home) = std::env::var("HOME") {
            let desktop_file = PathBuf::from(home).join(".config").join("autostart").join("aerosync.desktop");
            Ok(desktop_file.exists())
        } else {
            Ok(false)
        }
    }
}

#[tauri::command]
fn send_notification(title: String, body: String) -> Result<(), String> {
    #[cfg(windows)]
    {
        let clean_title = title.replace('\'', "''").replace('"', "`\"");
        let clean_body = body.replace('\'', "''").replace('"', "`\"");
        let script = format!(
            "[reflection.assembly]::loadwithpartialname('System.Windows.Forms') | Out-Null; $n = New-Object System.Windows.Forms.NotifyIcon; $n.Icon = [System.Drawing.SystemIcons]::Information; $n.Visible = `$true; $n.ShowBalloonTip(5000, '{}', '{}', [System.Windows.Forms.ToolTipIcon]::Info); Start-Sleep -m 500; $n.Dispose()",
            clean_title, clean_body
        );
        let status = Command::new("powershell")
            .args(["-NoProfile", "-NonInteractive", "-Command", &script])
            .creation_flags(CREATE_NO_WINDOW)
            .status();
        match status {
            Ok(_) => Ok(()),
            Err(e) => Err(e.to_string()),
        }
    }
    #[cfg(not(windows))]
    {
        let _ = Command::new("notify-send")
            .args([&title, &body])
            .status();
        Ok(())
    }
}

fn main() {
    #[cfg(not(windows))]
    {
        // 1. Ubuntu 23.10+ / 24.04+ / Debian AppArmor sandbox bypass
        // Without this, bubblewrap fails to spawn WebKitWebProcess due to unprivileged user namespace restrictions,
        // which leaves the GTK window as a completely blank white screen.
        if std::env::var_os("WEBKIT_FORCE_SANDBOX").is_none() {
            std::env::set_var("WEBKIT_FORCE_SANDBOX", "0");
        }

        // 2. WebKit2GTK DMA-BUF renderer incompatibility fix (NVIDIA, Intel, Wayland)
        if std::env::var_os("WEBKIT_DISABLE_DMABUF_RENDERER").is_none() {
            std::env::set_var("WEBKIT_DISABLE_DMABUF_RENDERER", "1");
        }

        // 3. WebKit2GTK compositing mode fix (forces reliable software fallback if GPU fails)
        if std::env::var_os("WEBKIT_DISABLE_COMPOSITING_MODE").is_none() {
            std::env::set_var("WEBKIT_DISABLE_COMPOSITING_MODE", "1");
        }

        // 4. NVIDIA driver Wayland explicit sync fix
        if std::env::var_os("__NV_DISABLE_EXPLICIT_SYNC").is_none() {
            std::env::set_var("__NV_DISABLE_EXPLICIT_SYNC", "1");
        }

        // 5. WebKit single process mode to prevent IPC disconnection
        if std::env::var_os("WEBKIT_USE_SINGLE_WEB_PROCESS").is_none() {
            std::env::set_var("WEBKIT_USE_SINGLE_WEB_PROCESS", "1");
        }

        // 6. Software rendering check
        let force_sw = std::env::args().any(|arg| arg == "--software-render" || arg == "--disable-gpu")
            || std::env::var("AEROSYNC_FORCE_SOFTWARE_RENDER").map(|v| v == "1" || v == "true").unwrap_or(false);

        if force_sw {
            std::env::set_var("LIBGL_ALWAYS_SOFTWARE", "1");
            std::env::set_var("WEBKIT_GRAPHICS_POLICY", "software");
            std::env::set_var("GSK_RENDERER", "cairo");
        }
    }

    ensure_runtime_assets();

    let daemon_state = DaemonState {
        child: Arc::new(Mutex::new(None)),
    };

    start_daemon_process(&daemon_state);

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(daemon_state)
        .invoke_handler(tauri::generate_handler![
            minimize_window,
            maximize_window,
            close_window,
            show_in_folder,
            open_folder,
            get_disk_space,
            pick_files,
            pick_folder,
            get_files_metadata,
            set_autostart,
            get_autostart,
            send_notification
        ])
        .build(tauri::generate_context!())
        .expect("error while building tauri application")
        .run(|app_handle: &AppHandle, event| {
            if let tauri::RunEvent::ExitRequested { .. } = event {
                if let Some(state) = app_handle.try_state::<DaemonState>() {
                    stop_daemon_process(&state);
                }
            }
        });
}
