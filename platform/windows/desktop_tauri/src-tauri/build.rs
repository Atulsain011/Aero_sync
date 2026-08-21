fn main() {
    #[cfg(windows)]
    {
        println!("cargo:rustc-link-arg=-Wl,-delayload,WebView2Loader.dll");
        println!("cargo:rustc-link-arg=-ldelayimp");
    }
    tauri_build::build();
}
