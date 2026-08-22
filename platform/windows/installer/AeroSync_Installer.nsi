; ==============================================================================
; AeroSync NSIS Modern Installer Script
; Packages AeroSync.exe + WebView2Loader.dll with Start Menu & Desktop Shortcuts
; ==============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ------------------------------------------------------------------------------
; General Definitions
; ------------------------------------------------------------------------------
!define PRODUCT_NAME "AeroSync"
!define PRODUCT_VERSION "1.0.7"
!define PRODUCT_PUBLISHER "AeroSync"
!define PRODUCT_WEB_SITE "https://github.com/Atulsain011/Aero_sync"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\AeroSync.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKCU"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\..\..\release\AeroSync-Setup-v1.0.7.exe"
InstallDir "$LOCALAPPDATA\Programs\AeroSync"
InstallDirRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" ""
RequestExecutionLevel user
SetCompressor zlib

; ------------------------------------------------------------------------------
; Version Information Resource (PE Header)
; ------------------------------------------------------------------------------
VIProductVersion "1.0.7.0"
VIAddVersionKey "ProductName" "AeroSync"
VIAddVersionKey "Comments" "High-Speed Peer-to-Peer Local Network File Transfer"
VIAddVersionKey "CompanyName" "AeroSync"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 AeroSync"
VIAddVersionKey "FileDescription" "AeroSync Setup Installer"
VIAddVersionKey "FileVersion" "1.0.7.0"
VIAddVersionKey "ProductVersion" "1.0.7.0"

; ------------------------------------------------------------------------------
; Interface Configuration & Branding
; ------------------------------------------------------------------------------
!define MUI_ICON "..\assets\icon.ico"
!define MUI_UNICON "..\assets\icon.ico"
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\AeroSync.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch AeroSync"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ------------------------------------------------------------------------------
; Installer Section
; ------------------------------------------------------------------------------
Section "MainSection" SEC01
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; 1. Copy Main Executable and Critical Runtime DLLs
    File "..\..\..\release\AeroSync.exe"
    File "..\..\..\release\WebView2Loader.dll"
    File "..\..\..\release\aerosync_daemon.exe"
    File "..\..\..\release\libc++.dll"
    File "..\..\..\release\libunwind.dll"
    File "..\..\..\release\libwinpthread-1.dll"

    ; 2. Create Start Menu Shortcuts
    CreateDirectory "$SMPROGRAMS\AeroSync"
    CreateShortcut "$SMPROGRAMS\AeroSync\AeroSync.lnk" "$INSTDIR\AeroSync.exe" "" "$INSTDIR\AeroSync.exe" 0
    CreateShortcut "$SMPROGRAMS\AeroSync\Uninstall AeroSync.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

    ; 3. Create Desktop Shortcut
    CreateShortcut "$DESKTOP\AeroSync.lnk" "$INSTDIR\AeroSync.exe" "" "$INSTDIR\AeroSync.exe" 0

    ; 4. Write Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; 5. Registry Keys for Windows Add/Remove Programs
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\AeroSync.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\AeroSync.exe,0"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
SectionEnd

; ------------------------------------------------------------------------------
; Uninstaller Section
; ------------------------------------------------------------------------------
Section Uninstall
    ; Remove Shortcuts
    Delete "$DESKTOP\AeroSync.lnk"
    Delete "$SMPROGRAMS\AeroSync\AeroSync.lnk"
    Delete "$SMPROGRAMS\AeroSync\Uninstall AeroSync.lnk"
    RMDir "$SMPROGRAMS\AeroSync"

    ; Remove Files
    Delete "$INSTDIR\AeroSync.exe"
    Delete "$INSTDIR\WebView2Loader.dll"
    Delete "$INSTDIR\aerosync_daemon.exe"
    Delete "$INSTDIR\libc++.dll"
    Delete "$INSTDIR\libunwind.dll"
    Delete "$INSTDIR\libwinpthread-1.dll"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove Directory
    RMDir "$INSTDIR"

    ; Clean Registry
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}"
    SetAutoClose true
SectionEnd
