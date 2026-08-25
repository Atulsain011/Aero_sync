#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <shlobj.h>

#include "app_window.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace aerosync_win {

AppWindow::AppWindow()
    : m_selectedTab(0),
      m_isDarkTheme(true),
      m_storageUsedPct(0),
      m_freeSpaceText("-- GB"),
      m_transferRateText("0.0 MB/s"),
      m_networkInfoText("Local Network") {}

AppWindow::~AppWindow() {
    if (m_app) m_app->shutdown();
}

void AppWindow::updateStorageStats() {
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        uint64_t total = totalNumberOfBytes.QuadPart;
        uint64_t freeB = freeBytesAvailable.QuadPart;
        uint64_t used = (total > freeB) ? (total - freeB) : 0;
        int usedPct = (total > 0) ? static_cast<int>((used * 100) / total) : 0;
        uint64_t freeGb = freeB / (1024ULL * 1024ULL * 1024ULL);
        m_storageUsedPct = usedPct;
        m_freeSpaceText = std::to_string(freeGb) + " GB";
    }
}

void AppWindow::updateNetworkInfo() {
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    std::string detected = "Local Network";
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    }

    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != NULL; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            for (PIP_ADAPTER_UNICAST_ADDRESS ua = curr->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
                if (ua->Address.lpSockaddr && ua->Address.lpSockaddr->sa_family == AF_INET) {
                    char ipStr[INET_ADDRSTRLEN] = {0};
                    sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                    std::string ip(ipStr);
                    if (ip.rfind("127.", 0) == 0 || ip.rfind("169.254.", 0) == 0) continue;

                    std::wstring desc(curr->Description ? curr->Description : L"");
                    std::string descA(desc.begin(), desc.end());

                    if (ip.rfind("192.168.43.", 0) == 0 || ip.rfind("192.168.137.", 0) == 0 ||
                        descA.find("Hotspot") != std::string::npos || descA.find("Wi-Fi Direct") != std::string::npos) {
                        m_networkInfoText = "USB Hotspot • " + ip;
                        return;
                    } else if (curr->IfType == IF_TYPE_IEEE80211 || descA.find("Wireless") != std::string::npos || descA.find("Wi-Fi") != std::string::npos) {
                        m_networkInfoText = "Wi-Fi • " + ip;
                        return;
                    } else if (curr->IfType == IF_TYPE_ETHERNET_CSMACD) {
                        detected = "Ethernet • " + ip;
                    }
                }
            }
        }
    }
    m_networkInfoText = detected;
}

bool AppWindow::initialize(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    HICON hIconBig = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(101), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
    if (!hIconBig) hIconBig = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hIconSmall) hIconSmall = LoadIcon(hInstance, MAKEINTRESOURCE(101));

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = hIconBig;
    wc.hIconSm = hIconSmall;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "AeroSyncWinClass";

    if (!RegisterClassEx(&wc)) return false;

    m_hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES, "AeroSyncWinClass", "AeroSync - Ultra High-Speed P2P Transfer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 980, 780,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) return false;

    SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    DragAcceptFiles(m_hwnd, TRUE);
    SetTimer(m_hwnd, 1, 50, NULL);

    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    GetComputerName(computerName, &size);

    m_deviceName = std::string(computerName) + " (Windows PC)";
    
    char userProfile[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH) > 0) {
        m_downloadLocation = std::string(userProfile) + "\\Downloads\\AeroSync";
    } else {
        m_downloadLocation = "C:\\AeroSync_Downloads";
    }
    CreateDirectoryA(m_downloadLocation.c_str(), NULL);

    updateStorageStats();
    updateNetworkInfo();

    m_app = std::make_unique<aerosync::AeroSyncApp>(
        "win-" + std::string(computerName),
        m_deviceName,
        aerosync::DeviceType::DEVICE_WINDOWS
    );
    m_app->setDownloadDirectory(m_downloadLocation);

    m_app->setPeerDiscoveredCallback([this](const std::vector<aerosync::PeerInfo>& peers) {
        {
            std::lock_guard<std::mutex> lock(m_uiMutex);
            m_peers = peers;
            if (!peers.empty()) {
                m_statusMessage = "Discovered " + std::to_string(peers.size()) + " active device(s)";
            }
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    });

    m_app->setIncomingConnectCallback([this](const aerosync::ConnectRequest& req, std::function<void(bool)> respondCb) {
        {
            std::lock_guard<std::mutex> lock(m_uiMutex);
            m_incomingSenderId = req.senderId;
            m_incomingSenderName = req.senderName.empty() ? "Mobile Device" : req.senderName;
            m_incomingSenderIp = req.senderIp;
            m_incomingPin = req.pairingPin;
            m_showPairingModal = false;
            m_statusMessage = "Connected with " + m_incomingSenderName;
        }
        respondCb(true); // Instant connection & pairing without waiting 30 seconds!
        InvalidateRect(m_hwnd, NULL, FALSE);
    });

    m_app->setIncomingTransferCallback([this](const aerosync::TransferManifest& manifest, std::function<void(bool)> respondCb) {
        {
            std::lock_guard<std::mutex> lock(m_uiMutex);
            m_isTransferring = true;
            std::string firstName = manifest.files.empty() ? "files" : manifest.files[0].relativePath;
            if (manifest.totalFiles > 1) {
                m_statusMessage = "Receiving " + std::to_string(manifest.totalFiles) + " files (" + firstName + " + " + std::to_string(manifest.totalFiles - 1) + " more)...";
            } else {
                m_statusMessage = "Receiving: " + firstName;
            }
        }
        respondCb(true); // Instant transfer without wasting a second!
        InvalidateRect(m_hwnd, NULL, FALSE);
    });

    m_app->setIncomingTransferProgressCallback([this](const aerosync::TransferProgress& prog) {
        {
            std::lock_guard<std::mutex> lock(m_uiMutex);
            m_activeProgress = prog;
            m_isTransferring = (prog.state == aerosync::TransferState::TRANSFERRING);
            double mbSec = prog.speedBytesPerSec / (1024.0 * 1024.0);
            if (mbSec >= 0.1) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f MB/s", mbSec);
                m_transferRateText = buf;
            } else if (prog.speedMbps >= 0.1) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f Mbps", prog.speedMbps);
                m_transferRateText = buf;
            }
            if (prog.state == aerosync::TransferState::COMPLETED ||
                (prog.fileBytesTransferred >= prog.fileSize && prog.fileSize > 0)) {
                bool exists = false;
                for (const auto& h : m_history) {
                    if (h == prog.currentFileName) { exists = true; break; }
                }
                if (!exists && !prog.currentFileName.empty()) {
                    m_history.push_back(prog.currentFileName);
                }
                m_statusMessage = "Completed: " + prog.currentFileName;
            } else {
                m_statusMessage = "Receiving: " + prog.currentFileName;
            }
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    });

    m_app->initialize();

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    return true;
}

int AppWindow::runEventLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK AppWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (AppWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (AppWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->handleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void AppWindow::processNextInQueue() {
    if (m_isQueueWorkerActive.exchange(true)) {
        return; // Worker thread already running and processing queue
    }

    std::thread([this]() {
        while (true) {
            std::vector<std::string> filePaths;
            std::vector<std::string> fileNames;
            aerosync::PeerInfo target;

            {
                std::lock_guard<std::mutex> lock(m_uiMutex);
                if (m_isPaused) {
                    m_isTransferring = false;
                    m_transferRateText = "0.0 MB/s";
                    break;
                }

                if (!m_hasSelection) {
                    if (!m_peers.empty()) {
                        m_selectedPeer = m_peers[0];
                        m_hasSelection = true;
                    } else {
                        m_isTransferring = false;
                        m_transferRateText = "0.0 MB/s";
                        m_statusMessage = "Waiting for remote device to connect...";
                        break;
                    }
                }
                target = m_selectedPeer;

                for (auto& item : m_transferQueue) {
                    if (!item.completed) {
                        item.active = true;
                        filePaths.push_back(item.path);
                        fileNames.push_back(item.name);
                    }
                }

                if (filePaths.empty()) {
                    m_isTransferring = false;
                    m_transferRateText = "0.0 MB/s";
                    break;
                }

                m_isTransferring = true;
                m_statusMessage = "Transferring " + std::to_string(filePaths.size()) + " file(s) in parallel to " + target.deviceName + "...";
            }

            InvalidateRect(m_hwnd, NULL, FALSE);

            bool ok = m_app->sendFiles(target, filePaths, [this](const aerosync::TransferProgress& prog) {
                {
                    std::lock_guard<std::mutex> lock(m_uiMutex);
                    m_activeProgress = prog;
                    double mbSec = prog.speedBytesPerSec / (1024.0 * 1024.0);
                    if (mbSec >= 0.1) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.1f MB/s", mbSec);
                        m_transferRateText = buf;
                    } else if (prog.speedMbps >= 0.1) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.1f Mbps", prog.speedMbps);
                        m_transferRateText = buf;
                    }
                    m_statusMessage = "Streaming: " + prog.currentFileName + " (" + m_transferRateText + ")";
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
            });

            {
                std::lock_guard<std::mutex> lock(m_uiMutex);
                for (auto& item : m_transferQueue) {
                    item.completed = true;
                    item.active = false;
                    bool exists = false;
                    for (const auto& h : m_history) {
                        if (h == item.name) { exists = true; break; }
                    }
                    if (!exists && !item.name.empty()) {
                        m_history.push_back(item.name);
                    }
                }
                m_isTransferring = false;
                m_transferRateText = "0.0 MB/s";
                m_statusMessage = ok ? "Completed transfer of " + std::to_string(filePaths.size()) + " file(s)!" : "Transfer finished with errors";
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            break;
        }
        m_isQueueWorkerActive = false;
    }).detach();
}

void AppWindow::clearTransferHistory() {
    std::lock_guard<std::mutex> lock(m_uiMutex);
    m_history.clear();
    m_statusMessage = "Transfer history cleared.";
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void AppWindow::removeQueueItem(size_t index) {
    std::lock_guard<std::mutex> lock(m_uiMutex);
    if (index < m_transferQueue.size()) {
        if (m_transferQueue[index].active) {
            m_app->cancelTransfer();
            m_isTransferring = false;
        }
        m_transferQueue.erase(m_transferQueue.begin() + index);
        m_statusMessage = "Removed file from queue.";
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void AppWindow::togglePauseResume() {
    std::lock_guard<std::mutex> lock(m_uiMutex);
    m_isPaused = !m_isPaused;
    if (m_isPaused) {
        m_app->cancelTransfer();
        m_isTransferring = false;
        m_statusMessage = "Transfer paused by user.";
    } else {
        m_statusMessage = "Resuming parallel transfer...";
        processNextInQueue();
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

LRESULT AppWindow::handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER: {
            if (IsIconic(m_hwnd)) return 0;
            m_radarPulse = (m_radarPulse + 1) % 60;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);

            RECT rect;
            GetClientRect(m_hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            renderUI(memDC);

            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(m_hwnd, &ps);
            return 0;
        }
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            std::vector<std::string> droppedFiles;
            for (UINT i = 0; i < count; ++i) {
                char path[MAX_PATH];
                if (DragQueryFile(hDrop, i, path, MAX_PATH)) {
                    droppedFiles.push_back(std::string(path));
                }
            }
            DragFinish(hDrop);

            if (!droppedFiles.empty()) {
                {
                    std::lock_guard<std::mutex> lock(m_uiMutex);
                    if (!m_hasSelection && !m_peers.empty()) {
                        m_selectedPeer = m_peers[0];
                        m_hasSelection = true;
                    }
                    for (const auto& path : droppedFiles) {
                        QueueItem qi;
                        qi.path = path;
                        size_t pos = path.find_last_of("\\/");
                        qi.name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
                        WIN32_FILE_ATTRIBUTE_DATA fad;
                        if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
                            qi.size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                        }
                        m_transferQueue.push_back(qi);
                    }
                    m_statusMessage = "Transferring " + std::to_string(droppedFiles.size()) + " file(s) immediately...";
                }
                processNextInQueue();
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // 1. Navigation Tabs Header Click
            if (y >= 12 && y <= 42) {
                if (x >= 154 && x <= 218) { m_selectedTab = 0; InvalidateRect(m_hwnd, NULL, FALSE); return 0; }
                if (x >= 222 && x <= 292) { m_selectedTab = 1; InvalidateRect(m_hwnd, NULL, FALSE); return 0; }
                if (x >= 296 && x <= 375) { m_selectedTab = 2; InvalidateRect(m_hwnd, NULL, FALSE); return 0; }
                if (x >= 840 && x <= 915) {
                    m_isDarkTheme = !m_isDarkTheme;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // 2. Modals Handling
            if (m_showDirectIpModal) {
                if (x >= 300 && x <= 420 && y >= 360 && y <= 400) {
                    m_showDirectIpModal = false;
                    aerosync::PeerInfo directPeer;
                    directPeer.deviceId = "direct-" + m_directIpInput;
                    directPeer.deviceName = "Direct Peer (" + m_directIpInput + ")";
                    directPeer.ipAddress = m_directIpInput;
                    directPeer.port = 48124;
                    directPeer.deviceType = aerosync::DeviceType::DEVICE_WINDOWS;
                    directPeer.lastSeenMs = 1;

                    {
                        std::lock_guard<std::mutex> lock(m_uiMutex);
                        m_selectedPeer = directPeer;
                        m_hasSelection = true;
                        m_isWaitingForAcceptance = true;
                        m_outgoingPin = std::to_string(100000 + (rand() % 900000));
                        m_statusMessage = "Connecting to " + m_directIpInput + "...";
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);

                    std::thread([this, directPeer]() {
                        bool success = m_app->connectToPeer(directPeer, m_outgoingPin);
                        {
                            std::lock_guard<std::mutex> lock(m_uiMutex);
                            m_isWaitingForAcceptance = false;
                            m_isPaired = success;
                            m_statusMessage = success ? "Connected to " + directPeer.deviceName : "Direct connection established";
                        }
                        InvalidateRect(m_hwnd, NULL, FALSE);
                    }).detach();
                    return 0;
                }
                if (x >= 440 && x <= 560 && y >= 360 && y <= 400) {
                    m_showDirectIpModal = false;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            if (m_showPairingModal) {
                if (x >= 300 && x <= 430 && y >= 360 && y <= 400) {
                    m_showPairingModal = false;
                    auto cb = m_pendingConnectCb;
                    m_pendingConnectCb = nullptr;
                    m_isPaired = true;
                    if (cb) cb(true);
                    m_statusMessage = "Pairing verified!";
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= 450 && x <= 580 && y >= 360 && y <= 400) {
                    m_showPairingModal = false;
                    auto cb = m_pendingConnectCb;
                    m_pendingConnectCb = nullptr;
                    if (cb) cb(false);
                    m_statusMessage = "Pairing request declined";
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // 3. Tab 0 (Files View) Actions
            if (m_selectedTab == 0) {
                // Browse Files Button
                if (x >= 220 && x <= 400 && y >= 185 && y <= 235) {
                    OPENFILENAME ofn = {0};
                    std::vector<char> szFile(65536, 0);
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = m_hwnd;
                    ofn.lpstrFile = szFile.data();
                    ofn.nMaxFile = static_cast<DWORD>(szFile.size());
                    ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

                    if (GetOpenFileName(&ofn)) {
                        std::vector<std::string> files;
                        char* ptr = szFile.data();
                        std::string dir = ptr;
                        ptr += dir.length() + 1;
                        if (*ptr == '\0') {
                            files.push_back(dir);
                        } else {
                            while (*ptr != '\0') {
                                std::string file = ptr;
                                if (!dir.empty() && dir.back() == '\\') files.push_back(dir + file);
                                else files.push_back(dir + "\\" + file);
                                ptr += file.length() + 1;
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(m_uiMutex);
                            if (!m_hasSelection && !m_peers.empty()) {
                                m_selectedPeer = m_peers[0];
                                m_hasSelection = true;
                            }
                            for (const auto& path : files) {
                                QueueItem qi;
                                qi.path = path;
                                size_t pos = path.find_last_of("\\/");
                                qi.name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
                                WIN32_FILE_ATTRIBUTE_DATA fad;
                                if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
                                    qi.size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                                }
                                m_transferQueue.push_back(qi);
                            }
                            m_statusMessage = "Transferring " + std::to_string(files.size()) + " file(s) immediately...";
                        }
                        processNextInQueue();
                        InvalidateRect(m_hwnd, NULL, FALSE);
                    }
                    return 0;
                }

                // Clear History Button
                if (x >= 730 && x <= 850 && y >= 450 && y <= 480) {
                    clearTransferHistory();
                    return 0;
                }

                // Manage Devices Button -> Switches to Tab 1
                if (x >= 636 && x <= 928 && y >= 375 && y <= 415) {
                    m_selectedTab = 1;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                // View All Transfers -> Switches to Tab 2
                if (x >= 850 && x <= 930 && y >= 450 && y <= 480) {
                    m_selectedTab = 2;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                // Active Transfer Controls (Pause / Resume / Cancel)
                if (m_isTransferring && y >= 485 && y <= 555) {
                    if (x >= 720 && x <= 800) {
                        togglePauseResume();
                        return 0;
                    }
                    if (x >= 805 && x <= 880) {
                        m_app->cancelTransfer();
                        m_isTransferring = false;
                        m_statusMessage = "Transfer cancelled.";
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                }

                // Queued Item Remove Buttons
                int qTy = m_isTransferring ? 561 : 485;
                for (size_t i = 0; i < m_transferQueue.size(); ++i) {
                    if (!m_transferQueue[i].completed && !m_transferQueue[i].active) {
                        if (x >= 820 && x <= 900 && y >= qTy && y <= qTy + 46) {
                            removeQueueItem(i);
                            return 0;
                        }
                        qTy += 52;
                    }
                }

                // Quick Peer Select
                int dy = 100;
                for (const auto& peer : m_peers) {
                    if (x >= 636 && x <= 928 && y >= dy && y <= dy + 48) {
                        {
                            std::lock_guard<std::mutex> lock(m_uiMutex);
                            m_selectedPeer = peer;
                            m_hasSelection = true;
                            m_isWaitingForAcceptance = true;
                            m_outgoingPin = std::to_string(100000 + (rand() % 900000));
                            m_statusMessage = "Pairing with " + peer.deviceName + " (PIN: " + m_outgoingPin + ")...";
                        }
                        InvalidateRect(m_hwnd, NULL, FALSE);

                        std::thread([this, peer]() {
                            bool paired = m_app->connectToPeer(peer, m_outgoingPin);
                            {
                                std::lock_guard<std::mutex> lock(m_uiMutex);
                                m_isWaitingForAcceptance = false;
                                m_isPaired = paired;
                                m_statusMessage = paired ? "Connected to " + peer.deviceName : "Ready for transfers";
                            }
                            if (!m_transferQueue.empty()) {
                                processNextInQueue();
                            }
                            InvalidateRect(m_hwnd, NULL, FALSE);
                        }).detach();
                        return 0;
                    }
                    dy += 54;
                }
            }

            // 4. Tab 1 (Devices View) Actions
            if (m_selectedTab == 1) {
                // Direct IP Connect Card Action
                if (x >= 40 && x <= 940 && y >= 490 && y <= 550) {
                    m_showDirectIpModal = true;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                int dy = 230;
                for (const auto& peer : m_peers) {
                    if (x >= 40 && x <= 940 && y >= dy && y <= dy + 56) {
                        {
                            std::lock_guard<std::mutex> lock(m_uiMutex);
                            m_selectedPeer = peer;
                            m_hasSelection = true;
                            m_isWaitingForAcceptance = true;
                            m_outgoingPin = std::to_string(100000 + (rand() % 900000));
                            m_statusMessage = "Pairing with " + peer.deviceName + " (PIN: " + m_outgoingPin + ")...";
                        }
                        InvalidateRect(m_hwnd, NULL, FALSE);

                        std::thread([this, peer]() {
                            bool paired = m_app->connectToPeer(peer, m_outgoingPin);
                            {
                                std::lock_guard<std::mutex> lock(m_uiMutex);
                                m_isWaitingForAcceptance = false;
                                m_isPaired = paired;
                                m_statusMessage = paired ? "Connected to " + peer.deviceName : "Ready for transfers";
                            }
                            if (!m_transferQueue.empty()) {
                                processNextInQueue();
                            }
                            InvalidateRect(m_hwnd, NULL, FALSE);
                        }).detach();
                        return 0;
                    }
                    dy += 66;
                }
            }

            // 5. Tab 2 (Activity View) Actions
            if (m_selectedTab == 2) {
                // Clear History Button
                if (x >= 750 && x <= 930 && y >= 50 && y <= 90) {
                    clearTransferHistory();
                    return 0;
                }
                // Queued Item Remove Buttons
                int qTy = m_isTransferring ? 200 : 95;
                for (size_t i = 0; i < m_transferQueue.size(); ++i) {
                    if (!m_transferQueue[i].completed && !m_transferQueue[i].active) {
                        if (x >= 820 && x <= 900 && y >= qTy && y <= qTy + 46) {
                            removeQueueItem(i);
                            return 0;
                        }
                        qTy += 60;
                    }
                }
                // Change Download Location Button
                if (x >= 740 && x <= 920 && y >= 560 && y <= 610) {
                    chooseDownloadLocation();
                    return 0;
                }
                // Open Downloads Folder Button
                if (x >= 40 && x <= 260 && y >= 640 && y <= 680) {
                    ShellExecuteA(NULL, "open", m_downloadLocation.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    return 0;
                }
            }

            return 0;
        }
        case WM_DESTROY:
            KillTimer(m_hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void AppWindow::chooseDownloadLocation() {
    BROWSEINFOA bi = {0};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = "Select AeroSync Download Folder Location";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != 0) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            {
                std::lock_guard<std::mutex> lock(m_uiMutex);
                m_downloadLocation = std::string(path);
                m_statusMessage = "Download location changed to: " + m_downloadLocation;
                if (m_app) {
                    m_app->setDownloadDirectory(m_downloadLocation);
                }
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
        CoTaskMemFree(pidl);
    }
}

void AppWindow::renderHeader(HDC hdc, const RECT& rect, bool isDark) {
    COLORREF cardAltColor = isDark ? RGB(24, 24, 27) : RGB(241, 245, 249);
    COLORREF borderColor = isDark ? RGB(39, 39, 42) : RGB(226, 232, 240);
    COLORREF textPrimary = isDark ? RGB(255, 255, 255) : RGB(9, 9, 11);
    COLORREF textSecondary = isDark ? RGB(161, 161, 170) : RGB(100, 116, 139);
    COLORREF brandBlue = RGB(56, 189, 248);
    COLORREF greenDot = RGB(34, 197, 94);

    HFONT fontBrand = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBodyBold = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontSmall = CreateFont(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

    // Brand Name
    SelectObject(hdc, fontBrand);
    SetTextColor(hdc, brandBlue);
    TextOut(hdc, 24, 16, "AeroSync", 8);

    // Navigation Tabs: [Files] [Devices] [Activity]
    SelectObject(hdc, fontBodyBold);
    const char* tabs[] = {"Files", "Devices", "Activity"};
    int tabX = 160;
    for (int i = 0; i < 3; ++i) {
        bool selected = (m_selectedTab == i);
        if (selected) {
            HBRUSH tabBg = CreateSolidBrush(isDark ? RGB(39, 39, 42) : RGB(226, 232, 240));
            RECT tabR = {tabX - 6, 14, tabX + (i == 2 ? 64 : 54), 38};
            FillRect(hdc, &tabR, tabBg);
            DeleteObject(tabBg);
            SetTextColor(hdc, textPrimary);
        } else {
            SetTextColor(hdc, textSecondary);
        }
        TextOut(hdc, tabX, 18, tabs[i], static_cast<int>(strlen(tabs[i])));
        tabX += (i == 2 ? 70 : 64);
    }

    // Dynamic Network Status Pill (Wi-Fi / USB Hotspot)
    HBRUSH pillBrush = CreateSolidBrush(cardAltColor);
    RECT pillRect = {620, 14, 820, 38};
    FillRect(hdc, &pillRect, pillBrush);
    DeleteObject(pillBrush);

    HBRUSH dotBrush = CreateSolidBrush(greenDot);
    RECT dotRect = {632, 23, 640, 31};
    FillRect(hdc, &dotRect, dotBrush);
    DeleteObject(dotBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 648, 19, m_networkInfoText.c_str(), static_cast<int>(m_networkInfoText.length()));

    // Theme Toggle
    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 840, 18, isDark ? "[DARK]" : "[LIGHT]", isDark ? 6 : 7);

    // Header Divider Line
    HPEN linePen = CreatePen(PS_SOLID, 1, borderColor);
    SelectObject(hdc, linePen);
    MoveToEx(hdc, 24, 48, NULL);
    LineTo(hdc, rect.right - 24, 48);
    DeleteObject(linePen);

    DeleteObject(fontBrand);
    DeleteObject(fontBodyBold);
    DeleteObject(fontSmall);
}

void AppWindow::renderFilesTab(HDC hdc, const RECT& rect, bool isDark) {
    COLORREF cardColor = isDark ? RGB(17, 17, 19) : RGB(255, 255, 255);
    COLORREF cardAltColor = isDark ? RGB(24, 24, 27) : RGB(241, 245, 249);
    COLORREF borderColor = isDark ? RGB(39, 39, 42) : RGB(226, 232, 240);
    COLORREF textPrimary = isDark ? RGB(255, 255, 255) : RGB(9, 9, 11);
    COLORREF textSecondary = isDark ? RGB(161, 161, 170) : RGB(100, 116, 139);
    COLORREF brandBlue = RGB(56, 189, 248);
    COLORREF greenDot = RGB(34, 197, 94);

    HFONT fontBrand = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontHeading = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBody = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBodyBold = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontSmall = CreateFont(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

    // 1. Upload Center (Left: 24, 60, 600, 280)
    SelectObject(hdc, fontHeading);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 24, 58, "Upload Center", 13);

    RECT upRect = {24, 84, 600, 280};
    HBRUSH upBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &upRect, upBrush);
    DeleteObject(upBrush);

    HPEN stripePen = CreatePen(PS_SOLID, 1, isDark ? RGB(28, 28, 32) : RGB(235, 238, 242));
    SelectObject(hdc, stripePen);
    for (int sx = upRect.left - 200; sx < upRect.right; sx += 24) {
        MoveToEx(hdc, sx, upRect.top, NULL);
        LineTo(hdc, sx + 200, upRect.bottom);
    }
    DeleteObject(stripePen);

    HPEN dashPen = CreatePen(PS_DOT, 1, brandBlue);
    SelectObject(hdc, dashPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, upRect.left + 12, upRect.top + 12, upRect.right - 12, upRect.bottom - 12);
    DeleteObject(dashPen);

    SelectObject(hdc, fontHeading);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 220, 120, "Drag and drop files here", 24);

    SelectObject(hdc, fontBody);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 120, 150, "Supports all file types. Maximum file size 50GB per transfer over local connection.", 83);

    HBRUSH btnBrush = CreateSolidBrush(brandBlue);
    RECT btnR = {230, 190, 390, 230};
    FillRect(hdc, &btnR, btnBrush);
    DeleteObject(btnBrush);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, 262, 200, "Browse Files", 12);

    // 2. CONNECTED DEVICES Quick Section (Right: 624, 60, 940, 430)
    RECT devBox = {624, 60, 940, 430};
    HBRUSH devBoxBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &devBox, devBoxBrush);
    DeleteObject(devBoxBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 640, 72, "CONNECTED DEVICES", 17);

    HBRUSH activeCountBrush = CreateSolidBrush(brandBlue);
    RECT actCountR = {850, 70, 920, 88};
    FillRect(hdc, &actCountR, activeCountBrush);
    DeleteObject(activeCountBrush);
    SetTextColor(hdc, RGB(255, 255, 255));
    std::string actTxt = std::to_string(m_peers.size()) + " Active";
    TextOut(hdc, 858, 72, actTxt.c_str(), static_cast<int>(actTxt.length()));

    int dy = 100;
    if (m_peers.empty()) {
        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 648, 140, "Scanning for nearby devices on Wi-Fi / Hotspot...", 49);
        TextOut(hdc, 648, 165, "Ensure AeroSync is running on other device.", 43);
    } else {
        for (const auto& peer : m_peers) {
            bool sel = (m_hasSelection && m_selectedPeer.deviceId == peer.deviceId);
            HBRUSH devItemBrush = CreateSolidBrush(sel ? cardAltColor : cardColor);
            RECT itemR = {636, dy, 928, dy + 48};
            FillRect(hdc, &itemR, devItemBrush);
            DeleteObject(devItemBrush);

            SelectObject(hdc, fontBodyBold);
            SetTextColor(hdc, textPrimary);
            std::string dName = peer.deviceName.empty() ? ("Device (" + peer.ipAddress + ")") : peer.deviceName;
            TextOut(hdc, 648, dy + 6, dName.c_str(), static_cast<int>(dName.length()));

            SelectObject(hdc, fontSmall);
            SetTextColor(hdc, textSecondary);
            std::string sub = "WiFi • " + peer.ipAddress;
            TextOut(hdc, 648, dy + 26, sub.c_str(), static_cast<int>(sub.length()));

            HBRUSH devDot = CreateSolidBrush(greenDot);
            RECT dR = {908, dy + 18, 916, dy + 26};
            FillRect(hdc, &dR, devDot);
            DeleteObject(devDot);

            dy += 54;
            if (dy > 360) break;
        }
    }

    // Manage Devices Button (dashed)
    HPEN managePen = CreatePen(PS_DOT, 1, brandBlue);
    SelectObject(hdc, managePen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 636, 375, 928, 415);
    DeleteObject(managePen);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, brandBlue);
    TextOut(hdc, 735, 385, "Manage Devices", 14);

    // 3. SYSTEM STATUS Section (Left: 24, 295, 600, 430)
    RECT statRect = {24, 295, 600, 430};
    HBRUSH statBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &statRect, statBrush);
    DeleteObject(statBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 40, 305, "SYSTEM STATUS", 13);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 40, 325, "Storage Used", 12);
    std::string pctStr = std::to_string(m_storageUsedPct) + "%";
    TextOut(hdc, 550, 325, pctStr.c_str(), static_cast<int>(pctStr.length()));

    HBRUSH barTrack = CreateSolidBrush(isDark ? RGB(39, 39, 42) : RGB(226, 232, 240));
    RECT barR = {40, 345, 584, 351};
    FillRect(hdc, &barR, barTrack);
    DeleteObject(barTrack);

    HBRUSH barFill = CreateSolidBrush(brandBlue);
    int fillW = (544 * m_storageUsedPct) / 100;
    RECT fillR = {40, 345, 40 + fillW, 351};
    FillRect(hdc, &fillR, barFill);
    DeleteObject(barFill);

    HBRUSH subCard1 = CreateSolidBrush(cardAltColor);
    RECT subR1 = {40, 360, 300, 415};
    FillRect(hdc, &subR1, subCard1);
    DeleteObject(subCard1);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 52, 368, "Free Space", 10);
    SelectObject(hdc, fontBrand);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 52, 384, m_freeSpaceText.c_str(), static_cast<int>(m_freeSpaceText.length()));

    HBRUSH subCard2 = CreateSolidBrush(cardAltColor);
    RECT subR2 = {316, 360, 584, 415};
    FillRect(hdc, &subR2, subCard2);
    DeleteObject(subCard2);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 328, 368, "Transfer Rate", 13);
    SelectObject(hdc, fontBrand);
    SetTextColor(hdc, greenDot);
    TextOut(hdc, 328, 384, m_transferRateText.c_str(), static_cast<int>(m_transferRateText.length()));

    // 4. RECENT TRANSFERS Section (Bottom: 24, 445, 940, 720)
    RECT recRect = {24, 445, 940, 720};
    HBRUSH recBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &recRect, recBrush);
    DeleteObject(recBrush);

    SelectObject(hdc, fontHeading);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 40, 455, "Recent Transfers", 16);

    // Clear History Button
    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, RGB(239, 68, 68));
    TextOut(hdc, 745, 458, "Clear History", 13);

    // View All Button
    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, brandBlue);
    TextOut(hdc, 870, 458, "View All", 8);

    int ty = 485;
    if (m_isTransferring) {
        HBRUSH activeCardBrush = CreateSolidBrush(cardAltColor);
        RECT actR = {40, ty, 924, ty + 68};
        FillRect(hdc, &actR, activeCardBrush);
        DeleteObject(activeCardBrush);

        SelectObject(hdc, fontBodyBold);
        SetTextColor(hdc, textPrimary);
        std::string fname = m_activeProgress.currentFileName;
        TextOut(hdc, 56, ty + 10, fname.c_str(), static_cast<int>(fname.length()));

        int pct = (m_activeProgress.fileSize > 0) ? static_cast<int>((m_activeProgress.fileBytesTransferred * 100) / m_activeProgress.fileSize) : 0;
        std::string pctTxt = std::to_string(pct) + "% (" + m_transferRateText + ")";
        SetTextColor(hdc, brandBlue);
        TextOut(hdc, 580, ty + 10, pctTxt.c_str(), static_cast<int>(pctTxt.length()));

        // Pause / Resume Button
        HBRUSH pauseBrush = CreateSolidBrush(cardColor);
        RECT pR = {720, ty + 8, 795, ty + 30};
        FillRect(hdc, &pR, pauseBrush);
        DeleteObject(pauseBrush);
        SelectObject(hdc, fontSmall);
        SetTextColor(hdc, textPrimary);
        TextOut(hdc, 730, ty + 11, m_isPaused ? "▶ Resume" : "⏸ Pause", m_isPaused ? 8 : 7);

        // Cancel Button
        HBRUSH cancelBrush = CreateSolidBrush(RGB(239, 68, 68));
        RECT cR = {805, ty + 8, 875, ty + 30};
        FillRect(hdc, &cR, cancelBrush);
        DeleteObject(cancelBrush);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOut(hdc, 816, ty + 11, "✕ Cancel", 8);

        HBRUSH transTrack = CreateSolidBrush(isDark ? RGB(39, 39, 42) : RGB(226, 232, 240));
        RECT tbarR = {56, ty + 38, 908, ty + 44};
        FillRect(hdc, &tbarR, transTrack);
        DeleteObject(transTrack);

        HBRUSH transFill = CreateSolidBrush(brandBlue);
        RECT tfillR = {56, ty + 38, 56 + (852 * pct) / 100, ty + 44};
        FillRect(hdc, &tfillR, transFill);
        DeleteObject(transFill);

        ty += 76;
    }

    for (const auto& item : m_transferQueue) {
        if (!item.completed && !item.active) {
            HBRUSH qBrush = CreateSolidBrush(cardAltColor);
            RECT qR = {40, ty, 924, ty + 46};
            FillRect(hdc, &qR, qBrush);
            DeleteObject(qBrush);

            SelectObject(hdc, fontBodyBold);
            SetTextColor(hdc, textPrimary);
            TextOut(hdc, 56, ty + 6, item.name.c_str(), static_cast<int>(item.name.length()));

            SelectObject(hdc, fontSmall);
            SetTextColor(hdc, textSecondary);
            TextOut(hdc, 56, ty + 24, "Queued • Parallel Batch Stream", 30);

            // Remove Button
            HBRUSH rmvBrush = CreateSolidBrush(cardColor);
            RECT rmvR = {820, ty + 8, 895, ty + 32};
            FillRect(hdc, &rmvR, rmvBrush);
            DeleteObject(rmvBrush);
            SetTextColor(hdc, RGB(239, 68, 68));
            TextOut(hdc, 830, ty + 11, "✕ Remove", 8);

            ty += 52;
            if (ty > 660) break;
        }
    }

    if (!m_isTransferring && m_history.empty() && m_transferQueue.empty()) {
        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 56, 520, "No recent transfers. Select or drop files to begin.", 51);
    } else {
        for (const auto& hfile : m_history) {
            HBRUSH hBrush = CreateSolidBrush(cardAltColor);
            RECT hR = {40, ty, 924, ty + 46};
            FillRect(hdc, &hR, hBrush);
            DeleteObject(hBrush);

            SelectObject(hdc, fontBodyBold);
            SetTextColor(hdc, textPrimary);
            TextOut(hdc, 56, ty + 6, hfile.c_str(), static_cast<int>(hfile.length()));

            SelectObject(hdc, fontSmall);
            SetTextColor(hdc, greenDot);
            TextOut(hdc, 56, ty + 24, "✓ Completed", 11);

            ty += 52;
            if (ty > 660) break;
        }
    }

    DeleteObject(fontBrand);
    DeleteObject(fontHeading);
    DeleteObject(fontBody);
    DeleteObject(fontBodyBold);
    DeleteObject(fontSmall);
}

void AppWindow::renderDevicesTab(HDC hdc, const RECT& rect, bool isDark) {
    COLORREF cardColor = isDark ? RGB(17, 17, 19) : RGB(255, 255, 255);
    COLORREF cardAltColor = isDark ? RGB(24, 24, 27) : RGB(241, 245, 249);
    COLORREF borderColor = isDark ? RGB(39, 39, 42) : RGB(226, 232, 240);
    COLORREF textPrimary = isDark ? RGB(255, 255, 255) : RGB(9, 9, 11);
    COLORREF textSecondary = isDark ? RGB(161, 161, 170) : RGB(100, 116, 139);
    COLORREF brandBlue = RGB(56, 189, 248);
    COLORREF greenDot = RGB(34, 197, 94);

    HFONT fontHeading = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBody = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBodyBold = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontSmall = CreateFont(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

    // Local Host Device Info Card (40, 60, 940, 140)
    RECT hostCard = {40, 60, 940, 140};
    HBRUSH hostBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &hostCard, hostBrush);
    DeleteObject(hostBrush);

    SelectObject(hdc, fontHeading);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 60, 75, m_deviceName.c_str(), static_cast<int>(m_deviceName.length()));

    SelectObject(hdc, fontBody);
    SetTextColor(hdc, textSecondary);
    std::string hostSub = "Local Device • " + m_networkInfoText + " • UDP Port 48123";
    TextOut(hdc, 60, 105, hostSub.c_str(), static_cast<int>(hostSub.length()));

    // Discovered Devices Section (40, 155, 940, 470)
    RECT devCard = {40, 155, 940, 470};
    HBRUSH devBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &devCard, devBrush);
    DeleteObject(devBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 60, 170, "DISCOVERED PEERS", 16);

    HBRUSH countBrush = CreateSolidBrush(brandBlue);
    RECT countR = {840, 168, 920, 188};
    FillRect(hdc, &countR, countBrush);
    DeleteObject(countBrush);
    SetTextColor(hdc, RGB(255, 255, 255));
    std::string cntStr = std::to_string(m_peers.size()) + " Found";
    TextOut(hdc, 850, 170, cntStr.c_str(), static_cast<int>(cntStr.length()));

    int dy = 205;
    if (m_peers.empty()) {
        SelectObject(hdc, fontHeading);
        SetTextColor(hdc, textPrimary);
        TextOut(hdc, 60, 260, "Searching for nearby devices on local network...", 49);

        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 60, 290, "Ensure other devices have AeroSync open on the same Wi-Fi or Mobile Hotspot / USB Tethering.", 91);
    } else {
        for (const auto& peer : m_peers) {
            bool isSelected = m_hasSelection && m_selectedPeer.deviceId == peer.deviceId;
            HBRUSH pBrush = CreateSolidBrush(isSelected ? cardAltColor : cardColor);
            RECT pRect = {60, dy, 920, dy + 56};
            FillRect(hdc, &pRect, pBrush);
            DeleteObject(pBrush);

            SelectObject(hdc, fontBodyBold);
            SetTextColor(hdc, textPrimary);
            std::string pName = peer.deviceName.empty() ? ("Device (" + peer.ipAddress + ")") : peer.deviceName;
            TextOut(hdc, 80, dy + 10, pName.c_str(), static_cast<int>(pName.length()));

            SelectObject(hdc, fontSmall);
            SetTextColor(hdc, textSecondary);
            std::string pSub = peer.ipAddress + ":" + std::to_string(peer.port) + " • " + peer.platform;
            TextOut(hdc, 80, dy + 32, pSub.c_str(), static_cast<int>(pSub.length()));

            HBRUSH dot = CreateSolidBrush(greenDot);
            RECT dR = {890, dy + 22, 898, dy + 30};
            FillRect(hdc, &dR, dot);
            DeleteObject(dot);

            dy += 66;
            if (dy > 420) break;
        }
    }

    // Direct IP Connect Card (40, 485, 940, 560)
    RECT ipCard = {40, 485, 940, 560};
    HBRUSH ipBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &ipCard, ipBrush);
    DeleteObject(ipBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 60, 498, "DIRECT IP CONNECTION", 20);

    SelectObject(hdc, fontBody);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 60, 520, "Connect directly via IP address (e.g. 192.168.43.1 for Mobile Hotspot gateway).", 79);

    HBRUSH connBtnBrush = CreateSolidBrush(brandBlue);
    RECT connR = {740, 505, 920, 545};
    FillRect(hdc, &connR, connBtnBrush);
    DeleteObject(connBtnBrush);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, 765, 515, "Connect via IP", 14);

    DeleteObject(fontHeading);
    DeleteObject(fontBody);
    DeleteObject(fontBodyBold);
    DeleteObject(fontSmall);
}

void AppWindow::renderActivityTab(HDC hdc, const RECT& rect, bool isDark) {
    COLORREF cardColor = isDark ? RGB(17, 17, 19) : RGB(255, 255, 255);
    COLORREF cardAltColor = isDark ? RGB(24, 24, 27) : RGB(241, 245, 249);
    COLORREF borderColor = isDark ? RGB(39, 39, 42) : RGB(226, 232, 240);
    COLORREF textPrimary = isDark ? RGB(255, 255, 255) : RGB(9, 9, 11);
    COLORREF textSecondary = isDark ? RGB(161, 161, 170) : RGB(100, 116, 139);
    COLORREF brandBlue = RGB(56, 189, 248);
    COLORREF greenDot = RGB(34, 197, 94);

    HFONT fontHeading = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBody = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBodyBold = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontSmall = CreateFont(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

    // Header Title
    SelectObject(hdc, fontHeading);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 40, 60, "Transfer Queue & Activity", 25);

    // Clear All History Button
    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, RGB(239, 68, 68));
    TextOut(hdc, 800, 64, "Clear All History", 17);

    int ty = 95;

    // Active Transfer Card
    if (m_isTransferring) {
        RECT actCard = {40, ty, 940, ty + 90};
        HBRUSH actBrush = CreateSolidBrush(cardColor);
        FillRect(hdc, &actCard, actBrush);
        DeleteObject(actBrush);

        SelectObject(hdc, fontBodyBold);
        SetTextColor(hdc, textPrimary);
        std::string fname = "Active Transfer: " + m_activeProgress.currentFileName;
        TextOut(hdc, 60, ty + 15, fname.c_str(), static_cast<int>(fname.length()));

        int pct = (m_activeProgress.fileSize > 0) ? static_cast<int>((m_activeProgress.fileBytesTransferred * 100) / m_activeProgress.fileSize) : 0;
        std::string spd = std::to_string(pct) + "% (" + m_transferRateText + ")";
        SetTextColor(hdc, brandBlue);
        TextOut(hdc, 820, ty + 15, spd.c_str(), static_cast<int>(spd.length()));

        HBRUSH barBg = CreateSolidBrush(isDark ? RGB(39, 39, 42) : RGB(226, 232, 240));
        RECT barR = {60, ty + 45, 920, ty + 53};
        FillRect(hdc, &barR, barBg);
        DeleteObject(barBg);

        HBRUSH barFill = CreateSolidBrush(brandBlue);
        RECT fillR = {60, ty + 45, 60 + (860 * pct) / 100, ty + 53};
        FillRect(hdc, &fillR, barFill);
        DeleteObject(barFill);

        ty += 105;
    }

    // Queued Items (Parallel Batch)
    for (const auto& item : m_transferQueue) {
        if (!item.completed && !item.active) {
            RECT qCard = {40, ty, 940, ty + 50};
            HBRUSH qBrush = CreateSolidBrush(cardColor);
            FillRect(hdc, &qCard, qBrush);
            DeleteObject(qBrush);

            SelectObject(hdc, fontBodyBold);
            SetTextColor(hdc, textPrimary);
            TextOut(hdc, 60, ty + 8, item.name.c_str(), static_cast<int>(item.name.length()));

            SelectObject(hdc, fontSmall);
            SetTextColor(hdc, textSecondary);
            TextOut(hdc, 60, ty + 28, "Queued • Parallel Batch Stream", 30);

            // Remove Button
            HBRUSH rmvBrush = CreateSolidBrush(cardAltColor);
            RECT rmvR = {820, ty + 10, 895, ty + 34};
            FillRect(hdc, &rmvR, rmvBrush);
            DeleteObject(rmvBrush);
            SetTextColor(hdc, RGB(239, 68, 68));
            TextOut(hdc, 830, ty + 14, "✕ Remove", 8);

            ty += 60;
            if (ty > 580) break;
        }
    }

    // Completed History Items
    for (const auto& hfile : m_history) {
        RECT hCard = {40, ty, 940, ty + 50};
        HBRUSH hBrush = CreateSolidBrush(cardColor);
        FillRect(hdc, &hCard, hBrush);
        DeleteObject(hBrush);

        SelectObject(hdc, fontBodyBold);
        SetTextColor(hdc, textPrimary);
        TextOut(hdc, 60, ty + 8, hfile.c_str(), static_cast<int>(hfile.length()));

        SelectObject(hdc, fontSmall);
        SetTextColor(hdc, greenDot);
        TextOut(hdc, 60, ty + 28, "✓ Transfer Completed Successfully", 33);

        ty += 60;
        if (ty > 580) break;
    }

    if (!m_isTransferring && m_history.empty() && m_transferQueue.empty()) {
        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 60, ty + 40, "No transfer activity yet. Drop files or select devices to begin.", 64);
    }

    // Download Location Setting Card (40, 560, 940, 620)
    RECT dlCard = {40, 560, 940, 620};
    HBRUSH dlBrush = CreateSolidBrush(cardColor);
    FillRect(hdc, &dlCard, dlBrush);
    DeleteObject(dlBrush);

    SelectObject(hdc, fontSmall);
    SetTextColor(hdc, textSecondary);
    TextOut(hdc, 60, 570, "DOWNLOAD LOCATION", 17);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, textPrimary);
    std::string dlDisplay = m_downloadLocation.empty() ? "Downloads\\AeroSync" : m_downloadLocation;
    if (dlDisplay.length() > 65) dlDisplay = dlDisplay.substr(0, 62) + "...";
    TextOut(hdc, 60, 592, dlDisplay.c_str(), static_cast<int>(dlDisplay.length()));

    HBRUSH changeBtn = CreateSolidBrush(cardAltColor);
    RECT cR = {740, 570, 920, 610};
    FillRect(hdc, &cR, changeBtn);
    DeleteObject(changeBtn);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, brandBlue);
    TextOut(hdc, 770, 582, "Change Folder", 13);

    // Action Button: Open Downloads Folder
    HBRUSH folderBtn = CreateSolidBrush(cardAltColor);
    RECT fRect = {40, 640, 240, 680};
    FillRect(hdc, &fRect, folderBtn);
    DeleteObject(folderBtn);

    SelectObject(hdc, fontBodyBold);
    SetTextColor(hdc, textPrimary);
    TextOut(hdc, 55, 652, "Open Active Folder", 18);

    DeleteObject(fontHeading);
    DeleteObject(fontBody);
    DeleteObject(fontBodyBold);
    DeleteObject(fontSmall);
}

void AppWindow::renderModals(HDC hdc, const RECT& rect, bool isDark) {
    COLORREF cardColor = isDark ? RGB(17, 17, 19) : RGB(255, 255, 255);
    COLORREF cardAltColor = isDark ? RGB(24, 24, 27) : RGB(241, 245, 249);
    COLORREF textPrimary = isDark ? RGB(255, 255, 255) : RGB(9, 9, 11);
    COLORREF textSecondary = isDark ? RGB(161, 161, 170) : RGB(100, 116, 139);
    COLORREF brandBlue = RGB(56, 189, 248);
    COLORREF greenDot = RGB(34, 197, 94);

    HFONT fontHeading = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBody = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontBodyBold = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    HFONT fontSmall = CreateFont(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

    // Direct IP Modal Dialog
    if (m_showDirectIpModal) {
        HBRUSH modalOverlay = CreateSolidBrush(RGB(5, 8, 15));
        FillRect(hdc, &rect, modalOverlay);
        DeleteObject(modalOverlay);

        HBRUSH modalBg = CreateSolidBrush(cardColor);
        RECT modalRect = {240, 180, 700, 440};
        FillRect(hdc, &modalRect, modalBg);
        DeleteObject(modalBg);

        SelectObject(hdc, fontHeading);
        SetTextColor(hdc, textPrimary);
        TextOut(hdc, 270, 205, "Connect to Direct IP", 20);

        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 270, 245, "Target IP: 192.168.43.1 (Default Hotspot Gateway)", 49);

        HBRUSH accBrush = CreateSolidBrush(brandBlue);
        RECT accRect = {300, 360, 420, 400};
        FillRect(hdc, &accRect, accBrush);
        DeleteObject(accBrush);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, fontBodyBold);
        TextOut(hdc, 335, 372, "Connect", 7);

        HBRUSH decBrush = CreateSolidBrush(RGB(239, 68, 68));
        RECT decRect = {440, 360, 560, 400};
        FillRect(hdc, &decRect, decBrush);
        DeleteObject(decBrush);
        TextOut(hdc, 480, 372, "Cancel", 6);
    }

    // 6-Digit PIN Pairing Modal Dialog
    if (m_showPairingModal) {
        HBRUSH modalOverlay = CreateSolidBrush(RGB(5, 8, 15));
        FillRect(hdc, &rect, modalOverlay);
        DeleteObject(modalOverlay);

        HBRUSH modalBg = CreateSolidBrush(cardColor);
        RECT modalRect = {220, 140, 720, 430};
        FillRect(hdc, &modalRect, modalBg);
        DeleteObject(modalBg);

        SelectObject(hdc, fontHeading);
        SetTextColor(hdc, textPrimary);
        TextOut(hdc, 250, 165, "PAIRING REQUEST WITH VERIFICATION PIN", 38);

        SelectObject(hdc, fontBody);
        SetTextColor(hdc, textSecondary);
        std::string fromStr = "Remote Device: " + m_incomingSenderName;
        TextOut(hdc, 250, 200, fromStr.c_str(), static_cast<int>(fromStr.length()));

        HBRUSH pinBoxBrush = CreateSolidBrush(cardAltColor);
        RECT pinBoxRect = {250, 230, 690, 305};
        FillRect(hdc, &pinBoxRect, pinBoxBrush);
        DeleteObject(pinBoxBrush);

        SetTextColor(hdc, brandBlue);
        HFONT pinFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, "Consolas");
        SelectObject(hdc, pinFont);
        std::string pinDisplay = "PIN: " + m_incomingPin;
        TextOut(hdc, 360, 245, pinDisplay.c_str(), static_cast<int>(pinDisplay.length()));
        DeleteObject(pinFont);

        SelectObject(hdc, fontSmall);
        SetTextColor(hdc, textSecondary);
        TextOut(hdc, 250, 320, "Verify that this 6-digit PIN matches the sender screen before confirming.", 73);

        HBRUSH acceptBrush = CreateSolidBrush(greenDot);
        RECT acceptRect = {300, 360, 430, 400};
        FillRect(hdc, &acceptRect, acceptBrush);
        DeleteObject(acceptBrush);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, fontBodyBold);
        TextOut(hdc, 325, 372, "Confirm & Pair", 14);

        HBRUSH declineBrush = CreateSolidBrush(RGB(239, 68, 68));
        RECT declineRect = {450, 360, 580, 400};
        FillRect(hdc, &declineRect, declineBrush);
        DeleteObject(declineBrush);
        TextOut(hdc, 495, 372, "Decline", 7);
    }

    DeleteObject(fontHeading);
    DeleteObject(fontBody);
    DeleteObject(fontBodyBold);
    DeleteObject(fontSmall);
}

void AppWindow::renderUI(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    bool isDark = m_isDarkTheme;
    COLORREF bgColor = isDark ? RGB(0, 0, 0) : RGB(248, 250, 252);

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);

    // 1. Header (Always rendered)
    renderHeader(hdc, rect, isDark);

    // 2. Active Tab Content
    if (m_selectedTab == 0) {
        renderFilesTab(hdc, rect, isDark);
    } else if (m_selectedTab == 1) {
        renderDevicesTab(hdc, rect, isDark);
    } else if (m_selectedTab == 2) {
        renderActivityTab(hdc, rect, isDark);
    }

    // 3. Modals Overlay (if active)
    renderModals(hdc, rect, isDark);
}

} // namespace aerosync_win
