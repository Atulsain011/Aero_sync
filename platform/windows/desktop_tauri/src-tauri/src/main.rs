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
    let current_exe = std::env::current_exe().ok()?;
    let current_dir = current_exe.parent()?;

    // 1. Check in same directory as current executable (fast path)
    let candidate1 = current_dir.join("aerosync_daemon.exe");
    if candidate1.exists() {
        return Some(candidate1);
    }

    // 2. Check in build_windows directory relative to workspace
    let candidates = [
        current_dir.join("build_windows").join("aerosync_daemon.exe"),
        current_dir.join("..").join("build_windows").join("aerosync_daemon.exe"),
        current_dir.join("..").join("..").join("build_windows").join("aerosync_daemon.exe"),
        PathBuf::from("C:\\Users\\Atul\\Desktop\\Aerosync\\build_windows\\aerosync_daemon.exe"),
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
        let arg = format!("/select,\"{}\"", clean_path);
        Command::new("explorer.exe")
            .raw_arg(&arg)
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

fn main() {
    let daemon_state = DaemonState {
        child: Arc::new(Mutex::new(None)),
    };

    start_daemon_process(&daemon_state);

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_shell::init())
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
