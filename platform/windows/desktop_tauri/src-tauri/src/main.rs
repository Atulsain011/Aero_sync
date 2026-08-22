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
    // 1. Check in same directory as current executable (Standalone / Portable location)
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(current_dir) = current_exe.parent() {
            let candidate1 = current_dir.join("aerosync_daemon.exe");
            if candidate1.exists() {
                return Some(candidate1);
            }
        }
    }

    // 2. Check embedded unpack directory in LocalAppData/AeroSync/bin
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

    // 3. Check in relative build directories
    let current_dir = std::env::current_exe().ok()?.parent()?.to_path_buf();
    let candidates = [
        current_dir.join("build_windows").join("aerosync_daemon.exe"),
        current_dir.join("..").join("build_windows").join("aerosync_daemon.exe"),
        current_dir.join("..").join("..").join("build_windows").join("aerosync_daemon.exe"),
    ];

    for path in candidates.iter() {
        if path.exists() {
            return Some(path.clone());
        }
    }

    None
}

fn start_daemon_process(state: &DaemonState) {
    let child_arc = state.child.clone();
    std::thread::spawn(move || {
        if let Some(daemon_path) = find_daemon_executable() {
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
        Err("Unsupported platform".into())
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
        Err("Unsupported platform".into())
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
    static WEBVIEW2_LOADER_BYTES: &[u8] = include_bytes!("../WebView2Loader.dll");
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

    // 2. Unpack current static aerosync_daemon.exe and WebView2Loader.dll
    let files: &[(&str, &[u8])] = &[
        ("WebView2Loader.dll", WEBVIEW2_LOADER_BYTES),
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

fn main() {
    #[cfg(windows)]
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
            get_files_metadata
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
