#ifndef AEROSYNC_WINDOWS_APP_WINDOW_HPP
#define AEROSYNC_WINDOWS_APP_WINDOW_HPP

#include "aerosync/aerosync_app.hpp"
#include <windows.h>
#include <memory>
#include <string>
#include <vector>

namespace aerosync_win {

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool initialize(HINSTANCE hInstance, int nCmdShow);
    int runEventLoop();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void renderUI(HDC hdc);

    HWND m_hwnd{NULL};
    HINSTANCE m_hInstance{NULL};
    std::string m_deviceName;

    std::unique_ptr<aerosync::AeroSyncApp> m_app;
    std::vector<aerosync::PeerInfo> m_peers;
    aerosync::PeerInfo m_selectedPeer;
    bool m_hasSelection{false};
    bool m_isWaitingForAcceptance{false};
    std::string m_outgoingPin;
    bool m_isPaired{false};

    // Pairing PIN Modal State
    bool m_showPairingModal{false};
    std::string m_incomingSenderId;
    std::string m_incomingSenderName;
    std::string m_incomingSenderIp;
    std::string m_incomingPin;
    std::function<void(bool)> m_pendingConnectCb;

    // Direct IP Connect Modal State
    bool m_showDirectIpModal{false};
    std::string m_directIpInput{"192.168.43.1"};

    // Incoming Transfer Modal State
    bool m_showIncomingModal{false};
    aerosync::TransferManifest m_pendingManifest;
    std::function<void(bool)> m_pendingRespondCb;

    // Dashboard Theme & Navigation
    bool m_isDarkTheme{true};
    int m_selectedTab{0}; // 0 = Files, 1 = Devices, 2 = Activity
    int m_storageUsedPct{0};
    std::string m_freeSpaceText{"-- GB"};
    std::string m_transferRateText{"0.0 MB/s"};
    std::string m_networkInfoText{"Local Network"};
    std::string m_downloadLocation;

    // Sequential 1-by-1 Transfer Queue
    struct QueueItem {
        std::string path;
        std::string name;
        uint64_t size{0};
        bool completed{false};
        bool active{false};
    };
    std::vector<QueueItem> m_transferQueue;
    std::vector<std::string> m_history;
    std::vector<std::string> m_stagedFiles;
    bool m_isTransferring{false};
    bool m_isPaused{false};
    std::atomic<bool> m_isQueueWorkerActive{false};
    aerosync::TransferProgress m_activeProgress;
    std::string m_statusMessage{"Ready for peer sync"};

    // Radar Animation Angle / Pulse
    int m_radarPulse{0};

    void processNextInQueue();
    void updateStorageStats();
    void updateNetworkInfo();
    void chooseDownloadLocation();
    void clearTransferHistory();
    void removeQueueItem(size_t index);
    void togglePauseResume();

    void renderHeader(HDC hdc, const RECT& rect, bool isDark);
    void renderFilesTab(HDC hdc, const RECT& rect, bool isDark);
    void renderDevicesTab(HDC hdc, const RECT& rect, bool isDark);
    void renderActivityTab(HDC hdc, const RECT& rect, bool isDark);
    void renderModals(HDC hdc, const RECT& rect, bool isDark);

    mutable std::mutex m_uiMutex;
};

} // namespace aerosync_win

#endif // AEROSYNC_WINDOWS_APP_WINDOW_HPP
