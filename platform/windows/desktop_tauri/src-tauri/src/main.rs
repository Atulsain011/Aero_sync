#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Manager, Window};

#[cfg(windows)]
use std::os::windows::process::CommandExt;

struct DaemonRuntime {
    child: Option<Child>,
    daemon_path: Option<PathBuf>,
    last_error: Option<String>,
    log_path: PathBuf,
}

#[derive(Clone)]
struct DaemonState {
    inner: Arc<Mutex<DaemonRuntime>>,
}

// Windows constant for hidden console
#[cfg(windows)]
const CREATE_NO_WINDOW: u32 = 0x08000000;

fn get_daemon_log_path() -> PathBuf {
    #[cfg(windows)]
    {
        let mut p = std::env::var_os("LOCALAPPDATA")
            .map(PathBuf::from)
            .unwrap_or_else(|| std::env::temp_dir());
        p.push("AeroSync");
        if let Err(e) = std::fs::create_dir_all(&p) {
            eprintln!("[AeroSync] Warning: Failed to create log directory {}: {}", p.display(), e);
        }
        p.push("daemon.log");
        p
    }
    #[cfg(not(windows))]
    {
        let mut p = std::env::var_os("XDG_CONFIG_HOME")
            .map(PathBuf::from)
            .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".config")))
            .unwrap_or_else(|| std::env::temp_dir());
        p.push("AeroSync");
        if let Err(e) = std::fs::create_dir_all(&p) {
            eprintln!("[AeroSync] Warning: Failed to create log directory {}: {}", p.display(), e);
        }
        p.push("daemon.log");
        p
    }
}

fn read_last_log_lines(path: &Path, max_lines: usize) -> String {
    if let Ok(content) = std::fs::read_to_string(path) {
        let lines: Vec<&str> = content.lines().filter(|l| !l.trim().is_empty()).collect();
        if lines.is_empty() {
            return "No output in log file.".to_string();
        }
        let start = if lines.len() > max_lines {
            lines.len() - max_lines
        } else {
            0
        };
        lines[start..].join("\n")
    } else {
        "Unable to read log file.".to_string()
    }
}

fn get_daemon_port() -> u16 {
    if let Ok(port_str) = std::env::var("AEROSYNC_IPC_PORT") {
        if let Ok(p) = port_str.parse::<u16>() {
            if p > 1024 {
                return p;
            }
        }
    }
    48126
}

fn is_daemon_running(port: u16) -> bool {
    use std::net::{SocketAddr, TcpStream};
    use std::time::Duration;
    if let Ok(addr) = format!("127.0.0.1:{}", port).parse::<SocketAddr>() {
        if let Ok(mut stream) = TcpStream::connect_timeout(&addr, Duration::from_millis(150)) {
            use std::io::{Read, Write};
            let _ = stream.set_read_timeout(Some(Duration::from_millis(150)));
            let _ = stream.set_write_timeout(Some(Duration::from_millis(150)));
            if stream.write_all(b"GET /api/health HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").is_ok() {
                let mut buf = [0u8; 128];
                if let Ok(n) = stream.read(&mut buf) {
                    if n > 0 && (buf.starts_with(b"HTTP/1.1 200") || buf.starts_with(b"HTTP/1.0 200") || buf.starts_with(b"HTTP/")) {
                        return true;
                    }
                }
            }
            return true; // TCP port open
        }
    }
    false
}

fn find_daemon_executable() -> Option<PathBuf> {
    let exe_name = if cfg!(windows) {
        "aerosync_daemon.exe"
    } else {
        "aerosync_daemon"
    };

    let is_valid_daemon = |p: &Path| -> bool {
        if !p.is_file() {
            return false;
        }
        #[cfg(not(windows))]
        {
            if let Ok(mut f) = std::fs::File::open(p) {
                use std::io::Read;
                let mut buf = [0u8; 4096];
                if let Ok(n) = f.read(&mut buf) {
                    if n >= 20 && &buf[0..4] == b"\x7fELF" && buf[4] == 2 {
                        // Reject Android Bionic binaries
                        let slice = &buf[..n];
                        if !slice.windows(18).any(|w| w == b"/system/bin/linker")
                            && !slice.windows(9).any(|w| w == b"liblog.so")
                        {
                            return true;
                        }
                    }
                }
            }
            false
        }
        #[cfg(windows)]
        {
            if let Ok(mut f) = std::fs::File::open(p) {
                use std::io::Read;
                let mut header = [0u8; 2];
                if f.read_exact(&mut header).is_ok() && &header == b"MZ" {
                    return true;
                }
            }
            false
        }
    };

    // 0. Explicit environment variable override
    if let Some(explicit) = std::env::var_os("AEROSYNC_DAEMON_PATH") {
        let p = PathBuf::from(explicit);
        if is_valid_daemon(&p) {
            return Some(p);
        }
    }

    // 1. Next to current running executable (Standalone / Portable / AppImage mount location)
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(current_dir) = current_exe.parent() {
            let candidates = [
                current_dir.join(exe_name),
                current_dir.join("bin").join(exe_name),
                current_dir.join("resources").join(exe_name),
                current_dir.join("..").join("bin").join(exe_name),
                current_dir.join("..").join("resources").join(exe_name),
                current_dir.join("..").join("lib").join("aerosync").join(exe_name),
                current_dir.join("..").join("lib").join("aerosync").join("resources").join(exe_name),
            ];
            for c in candidates.iter() {
                if is_valid_daemon(c) {
                    return Some(c.clone());
                }
            }
        }
    }

    // 2. Platform-specific installed paths
    #[cfg(windows)]
    {
        let mut runtime_dir = std::env::var_os("LOCALAPPDATA")
            .map(PathBuf::from)
            .unwrap_or_else(|| std::env::temp_dir());
        runtime_dir.push("AeroSync");
        runtime_dir.push("bin");
        let embedded_daemon = runtime_dir.join("aerosync_daemon.exe");
        if is_valid_daemon(&embedded_daemon) {
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
                appdir_path.join("bin").join("aerosync_daemon"),
                appdir_path.join("aerosync_daemon"),
                appdir_path.join("usr").join("lib").join("aerosync").join("resources").join("aerosync_daemon"),
                appdir_path.join("usr").join("lib").join("aerosync").join("aerosync_daemon"),
            ];
            for path in appdir_candidates.iter() {
                if is_valid_daemon(path) {
                    return Some(path.clone());
                }
            }
        }

        // 2b. Standard Linux System & User locations
        let mut candidate_dirs = Vec::new();
        if let Some(home) = std::env::var_os("HOME") {
            let h = PathBuf::from(home);
            candidate_dirs.push(h.join(".local").join("bin"));
            candidate_dirs.push(h.join(".local").join("share").join("AeroSync").join("bin"));
            candidate_dirs.push(h.join("bin"));
        }
        candidate_dirs.push(PathBuf::from("/opt/aerosync/bin"));
        candidate_dirs.push(PathBuf::from("/opt/aerosync"));
        candidate_dirs.push(PathBuf::from("/usr/bin"));
        candidate_dirs.push(PathBuf::from("/usr/local/bin"));
        candidate_dirs.push(PathBuf::from("/usr/lib/aerosync"));
        candidate_dirs.push(PathBuf::from("/usr/share/aerosync/resources"));

        for dir in candidate_dirs {
            let p = dir.join(exe_name);
            if is_valid_daemon(&p) {
                return Some(p);
            }
        }
    }

    // 3. Search upward in project/build trees (for development and CI)
    if let Ok(current_exe) = std::env::current_exe() {
        let mut curr = current_exe.parent();
        for _ in 0..6 {
            if let Some(dir) = curr {
                let check_paths = [
                    dir.join(exe_name),
                    dir.join("build_windows").join(exe_name),
                    dir.join("build_linux").join(exe_name),
                    dir.join("release").join(exe_name),
                    dir.join("src-tauri").join(exe_name),
                    dir.join("platform").join("windows").join("desktop_tauri").join("src-tauri").join(exe_name),
                ];
                for p in check_paths.iter() {
                    if is_valid_daemon(p) {
                        return Some(p.clone());
                    }
                }
                curr = dir.parent();
            } else {
                break;
            }
        }
    }

    None
}

fn start_daemon_process(state: &DaemonState) {
    let port = get_daemon_port();
    if is_daemon_running(port) {
        println!("[AeroSync] AeroSync Core Daemon is already active and responding on 127.0.0.1:{}.", port);
        let mut guard = state.inner.lock().unwrap();
        guard.last_error = None;
        return;
    }

    println!("[AeroSync] Starting native daemon...");

    let inner_arc = state.inner.clone();
    std::thread::spawn(move || {
        let daemon_path = match find_daemon_executable() {
            Some(p) => {
                println!("[AeroSync] Daemon path: {}", p.display());
                p
            }
            None => {
                let err_msg = "Could not locate aerosync_daemon binary! Searched current exe directory, AppImage bundles, system directories (/usr/lib/aerosync, /opt/aerosync/bin, /usr/bin), and ~/.local/share/AeroSync/bin.".to_string();
                eprintln!("[AeroSync] Error: {}", err_msg);
                let mut guard = inner_arc.lock().unwrap();
                guard.last_error = Some(err_msg);
                return;
            }
        };

        let log_path = get_daemon_log_path();
        println!("[AeroSync] Daemon logs will be written to: {}", log_path.display());

        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            match std::fs::metadata(&daemon_path) {
                Ok(meta) => {
                    let mut perms = meta.permissions();
                    if perms.mode() & 0o111 == 0 {
                        perms.set_mode(perms.mode() | 0o755);
                        if let Err(e) = std::fs::set_permissions(&daemon_path, perms) {
                            eprintln!("[AeroSync] Error setting executable permissions (0755) on {}: {}", daemon_path.display(), e);
                        } else {
                            println!("[AeroSync] Set executable permissions (0755) on {}", daemon_path.display());
                        }
                    }
                }
                Err(e) => {
                    eprintln!("[AeroSync] Error inspecting daemon metadata at {}: {}", daemon_path.display(), e);
                }
            }
        }

        let mut cmd = Command::new(&daemon_path);
        cmd.arg("--port").arg(port.to_string());
        cmd.env("AEROSYNC_IPC_PORT", port.to_string());
        cmd.stdout(std::process::Stdio::piped());
        cmd.stderr(std::process::Stdio::piped());

        #[cfg(windows)]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let mut child = match cmd.spawn() {
            Ok(c) => c,
            Err(e) => {
                let err_msg = format!("Failed to spawn daemon process ({}): {}. Check file permissions and system dependencies.", daemon_path.display(), e);
                eprintln!("[AeroSync] Error: {}", err_msg);
                let mut guard = inner_arc.lock().unwrap();
                guard.last_error = Some(err_msg);
                return;
            }
        };

        let child_id = child.id();
        println!("[AeroSync] Daemon process spawned (PID: {})", child_id);

        // Tee daemon stdout & stderr to both daemon.log and terminal stdout/stderr
        let stdout_opt = child.stdout.take();
        let stderr_opt = child.stderr.take();
        let log_path_out = log_path.clone();
        let log_path_err = log_path.clone();

        if let Some(stdout_pipe) = stdout_opt {
            std::thread::spawn(move || {
                use std::fs::OpenOptions;
                use std::io::{BufRead, BufReader, Write};
                let reader = BufReader::new(stdout_pipe);
                let mut log_file = OpenOptions::new().create(true).append(true).open(&log_path_out).ok();
                for line_res in reader.lines() {
                    if let Ok(line) = line_res {
                        println!("{}", line);
                        if let Some(ref mut f) = log_file {
                            let _ = writeln!(f, "{}", line);
                        }
                    }
                }
            });
        }

        if let Some(stderr_pipe) = stderr_opt {
            std::thread::spawn(move || {
                use std::fs::OpenOptions;
                use std::io::{BufRead, BufReader, Write};
                let reader = BufReader::new(stderr_pipe);
                let mut log_file = OpenOptions::new().create(true).append(true).open(&log_path_err).ok();
                for line_res in reader.lines() {
                    if let Ok(line) = line_res {
                        eprintln!("{}", line);
                        if let Some(ref mut f) = log_file {
                            let _ = writeln!(f, "{}", line);
                        }
                    }
                }
            });
        }

        {
            let mut guard = inner_arc.lock().unwrap();
            guard.child = Some(child);
            guard.daemon_path = Some(daemon_path);
            guard.log_path = log_path.clone();
            guard.last_error = None;
        }

        // Poll up to 3 seconds for daemon to initialize HTTP socket
        for i in 0..30 {
            // Check if child exited prematurely
            let premature_exit = {
                let mut guard = inner_arc.lock().unwrap();
                if let Some(ref mut c) = guard.child {
                    match c.try_wait() {
                        Ok(Some(status)) => Some(status),
                        _ => None,
                    }
                } else {
                    None
                }
            };

            if let Some(status) = premature_exit {
                let log_tail = read_last_log_lines(&log_path, 8);
                let err_msg = format!("Daemon exited prematurely (exit code {}).\nLog output:\n{}", status, log_tail);
                eprintln!("[AeroSync] Error: {}", err_msg);
                let mut guard = inner_arc.lock().unwrap();
                guard.last_error = Some(err_msg);
                return;
            }

            if is_daemon_running(port) {
                println!("[AeroSync] Daemon started successfully (PID: {}, listening on 127.0.0.1:{}, verified in {}ms)", child_id, port, (i + 1) * 100);
                let mut guard = inner_arc.lock().unwrap();
                guard.last_error = None;
                return;
            }
            std::thread::sleep(std::time::Duration::from_millis(100));
        }

        let log_tail = read_last_log_lines(&log_path, 8);
        let err_msg = format!("Daemon started (PID: {}) but port {} did not respond to /api/health within 3.0s.\nLog output:\n{}", child_id, port, log_tail);
        eprintln!("[AeroSync] Warning: {}", err_msg);
        let mut guard = inner_arc.lock().unwrap();
        guard.last_error = Some(err_msg);
    });
}

fn stop_daemon_process(state: &DaemonState) {
    let port = get_daemon_port();

    // 1. Graceful HTTP shutdown request
    use std::net::{SocketAddr, TcpStream};
    use std::time::Duration;
    if let Ok(addr) = format!("127.0.0.1:{}", port).parse::<SocketAddr>() {
        if let Ok(mut stream) = TcpStream::connect_timeout(&addr, Duration::from_millis(200)) {
            use std::io::Write;
            if let Err(e) = stream.write_all(b"GET /api/shutdown HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n") {
                eprintln!("[AeroSync] Notice: Could not send HTTP shutdown to daemon: {}", e);
            }
        }
    }

    // 2. Kill tracked child process
    {
        let mut guard = state.inner.lock().unwrap();
        if let Some(mut child) = guard.child.take() {
            let pid = child.id();
            println!("[AeroSync] Shutting down background daemon process (PID: {})...", pid);
            if let Err(e) = child.kill() {
                eprintln!("[AeroSync] Notice: child.kill() on PID {}: {}", pid, e);
            }
            if let Err(e) = child.wait() {
                eprintln!("[AeroSync] Notice: child.wait() on PID {}: {}", pid, e);
            }
        }
    }

    // 3. Platform fallback kill
    #[cfg(windows)]
    {
        if let Err(e) = Command::new("taskkill")
            .args(["/F", "/IM", "aerosync_daemon.exe"])
            .creation_flags(CREATE_NO_WINDOW)
            .status() {
            eprintln!("[AeroSync] Notice: taskkill command failed: {}", e);
        }
    }
    #[cfg(not(windows))]
    {
        if let Err(e) = Command::new("pkill")
            .args(["-f", "aerosync_daemon"])
            .status() {
            eprintln!("[AeroSync] Notice: pkill command failed: {}", e);
        }
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
    // Verify that the daemon executable is present in the deterministic package layout
    if let Some(daemon_path) = find_daemon_executable() {
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            match std::fs::metadata(&daemon_path) {
                Ok(meta) => {
                    let mut perms = meta.permissions();
                    if perms.mode() & 0o111 == 0 {
                        perms.set_mode(perms.mode() | 0o755);
                        if let Err(e) = std::fs::set_permissions(&daemon_path, perms) {
                            eprintln!("[AeroSync] Error setting execute permissions on {}: {}", daemon_path.display(), e);
                        } else {
                            println!("[AeroSync] Verified executable permissions on {}", daemon_path.display());
                        }
                    }
                }
                Err(e) => {
                    eprintln!("[AeroSync] Error inspecting daemon metadata at {}: {}", daemon_path.display(), e);
                }
            }
        }
        if let Some(parent) = daemon_path.parent() {
            if let Ok(current_path) = std::env::var("PATH") {
                let new_path = format!("{}:{}", parent.display(), current_path);
                std::env::set_var("PATH", new_path);
            }
        }
    } else {
        eprintln!("[AeroSync] Notice: aerosync_daemon will be located via deterministic package search upon daemon startup.");
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
                std::fs::create_dir_all(&autostart_dir)
                    .map_err(|e| format!("Failed to create autostart directory {}: {}", autostart_dir.display(), e))?;
                if let Ok(current_exe) = std::env::current_exe() {
                    let content = format!(
                        "[Desktop Entry]\nType=Application\nName=AeroSync\nExec=\"{}\"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n",
                        current_exe.to_string_lossy()
                    );
                    std::fs::write(&desktop_file, content)
                        .map_err(|e| format!("Failed to write autostart desktop file {}: {}", desktop_file.display(), e))?;
                }
            } else {
                if desktop_file.exists() {
                    std::fs::remove_file(&desktop_file)
                        .map_err(|e| format!("Failed to remove autostart desktop file {}: {}", desktop_file.display(), e))?;
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
        match Command::new("notify-send")
            .args([&title, &body])
            .status() {
            Ok(s) => {
                if !s.success() {
                    eprintln!("[AeroSync] Notice: notify-send exited with status: {}", s);
                }
            }
            Err(e) => {
                eprintln!("[AeroSync] Notice: notify-send failed: {}", e);
            }
        }
        Ok(())
    }
}

#[derive(serde::Serialize, Clone)]
struct DaemonStatusInfo {
    is_running: bool,
    port_listening: bool,
    daemon_path: Option<String>,
    error: Option<String>,
    log_path: Option<String>,
    port: u16,
}

#[tauri::command]
fn get_daemon_status(state: tauri::State<DaemonState>) -> DaemonStatusInfo {
    let port = get_daemon_port();
    let port_listening = is_daemon_running(port);
    let mut guard = state.inner.lock().unwrap();

    let is_child_running = if let Some(ref mut child) = guard.child {
        match child.try_wait() {
            Ok(None) => true,
            Ok(Some(status)) => {
                if guard.last_error.is_none() {
                    let log_tail = read_last_log_lines(&guard.log_path, 8);
                    guard.last_error = Some(format!("Daemon process terminated (exit code {}).\nLog output:\n{}", status, log_tail));
                }
                false
            }
            _ => false,
        }
    } else {
        false
    };

    let is_running = port_listening || is_child_running;
    let error = if port_listening {
        None
    } else {
        guard.last_error.clone().or_else(|| {
            Some(format!(
                "AeroSync Core Daemon is offline (port {} not listening). Click 'Restart Core Engine' to relaunch.",
                port
            ))
        })
    };

    DaemonStatusInfo {
        is_running,
        port_listening,
        daemon_path: guard.daemon_path.as_ref().map(|p| p.to_string_lossy().to_string()),
        error,
        log_path: Some(guard.log_path.to_string_lossy().to_string()),
        port,
    }
}

#[tauri::command]
fn restart_daemon(state: tauri::State<DaemonState>) -> bool {
    stop_daemon_process(&state);
    std::thread::sleep(std::time::Duration::from_millis(200));
    start_daemon_process(&state);

    let port = get_daemon_port();
    for _ in 0..25 {
        if is_daemon_running(port) {
            return true;
        }
        std::thread::sleep(std::time::Duration::from_millis(100));
    }
    false
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
        inner: Arc::new(Mutex::new(DaemonRuntime {
            child: None,
            daemon_path: None,
            last_error: None,
            log_path: get_daemon_log_path(),
        })),
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
            send_notification,
            get_daemon_status,
            restart_daemon
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
