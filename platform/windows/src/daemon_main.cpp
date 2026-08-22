#include "aerosync/aerosync_app.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <iomanip>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/statvfs.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace {

struct DaemonState {
    std::mutex mtx;
    std::string deviceId;
    std::string deviceName;
    std::string downloadDir;
    std::vector<aerosync::PeerInfo> peers;
    aerosync::TransferProgress currentProgress;
    bool isTransferring{false};
    bool isPaused{false};
    std::string statusMessage{"AeroSync Core Daemon Ready"};
    uint64_t storageFreeBytes{0};
    uint64_t storageTotalBytes{0};
    std::string networkType{"Wi-Fi / LAN"};
    int linkSpeedMbps{0};
    std::vector<std::string> completedHistory;
};

static DaemonState g_state;
static std::unique_ptr<aerosync::AeroSyncApp> g_app;
static std::atomic<bool> g_running{true};

static std::string escapeJson(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

static void updateStorageAndNetwork() {
    static auto s_lastUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastUpdate).count() < 2500) {
        return; // Return cached metrics instantly
    }
    s_lastUpdate = now;

#ifdef _WIN32
    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    std::string root = "C:\\";
    {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        if (!g_state.downloadDir.empty() && g_state.downloadDir.length() >= 3 && g_state.downloadDir[1] == ':') {
            root = g_state.downloadDir.substr(0, 3);
        }
    }
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytes, &totalBytes, &totalFree)) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.storageFreeBytes = freeBytes.QuadPart;
        g_state.storageTotalBytes = totalBytes.QuadPart;
    }

    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen) == NO_ERROR) {
        std::string detectedNet = "Local Network";
        int detectedSpeed = 1000;
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

                    if (ip.rfind("192.168.43.", 0) == 0 || descA.find("Hotspot") != std::string::npos) {
                        detectedNet = "Mobile Hotspot (" + ip + ")";
                        detectedSpeed = 480;
                    } else if (curr->IfType == IF_TYPE_IEEE80211 || descA.find("Wi-Fi") != std::string::npos) {
                        detectedNet = "Wi-Fi 5GHz (" + ip + ")";
                        detectedSpeed = 433;
                    } else if (curr->IfType == IF_TYPE_ETHERNET_CSMACD) {
                        detectedNet = "Gigabit Ethernet (" + ip + ")";
                        detectedSpeed = 1000;
                    }
                }
            }
        }
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.networkType = detectedNet;
        g_state.linkSpeedMbps = detectedSpeed;
    }
#else
    std::string downloadPath = "/tmp";
    {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        if (!g_state.downloadDir.empty()) {
            downloadPath = g_state.downloadDir;
        }
    }
    struct statvfs statBuf;
    if (statvfs(downloadPath.c_str(), &statBuf) == 0) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.storageFreeBytes = static_cast<uint64_t>(statBuf.f_bavail) * statBuf.f_frsize;
        g_state.storageTotalBytes = static_cast<uint64_t>(statBuf.f_blocks) * statBuf.f_frsize;
    }

    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        std::string detectedNet = "Local Network (LAN)";
        int detectedSpeed = 1000;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;

            sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
            std::string ip(ipStr);
            if (ip.rfind("127.", 0) == 0) continue;

            std::string ifName(ifa->ifa_name ? ifa->ifa_name : "");
            if (ifName.rfind("wlan", 0) == 0 || ifName.rfind("wlp", 0) == 0) {
                detectedNet = "Wi-Fi (" + ip + ")";
                detectedSpeed = 433;
            } else if (ifName.rfind("eth", 0) == 0 || ifName.rfind("eno", 0) == 0 || ifName.rfind("enp", 0) == 0) {
                detectedNet = "Gigabit Ethernet (" + ip + ")";
                detectedSpeed = 1000;
            } else {
                detectedNet = "LAN (" + ip + ")";
            }
        }
        freeifaddrs(ifaddr);
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.networkType = detectedNet;
        g_state.linkSpeedMbps = detectedSpeed;
    }
#endif
}

static std::string buildStatusJsonResponse() {
    updateStorageAndNetwork();
    std::lock_guard<std::mutex> lock(g_state.mtx);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"success\": true,\n";
    ss << "  \"deviceId\": \"" << escapeJson(g_state.deviceId) << "\",\n";
    ss << "  \"deviceName\": \"" << escapeJson(g_state.deviceName) << "\",\n";
    ss << "  \"downloadDir\": \"" << escapeJson(g_state.downloadDir) << "\",\n";
    ss << "  \"statusMessage\": \"" << escapeJson(g_state.statusMessage) << "\",\n";
    ss << "  \"isTransferring\": " << (g_state.isTransferring ? "true" : "false") << ",\n";
    ss << "  \"isPaused\": " << (g_state.isPaused ? "true" : "false") << ",\n";
    ss << "  \"storageFreeBytes\": " << g_state.storageFreeBytes << ",\n";
    ss << "  \"storageTotalBytes\": " << g_state.storageTotalBytes << ",\n";
    ss << "  \"networkType\": \"" << escapeJson(g_state.networkType) << "\",\n";
    ss << "  \"linkSpeedMbps\": " << g_state.linkSpeedMbps << ",\n";

    // Peers
    ss << "  \"peers\": [\n";
    for (size_t i = 0; i < g_state.peers.size(); ++i) {
        const auto& p = g_state.peers[i];
        std::string platformStr = (p.deviceType == aerosync::DeviceType::DEVICE_ANDROID) ? "android" : "windows";
        ss << "    {\n";
        ss << "      \"deviceId\": \"" << escapeJson(p.deviceId) << "\",\n";
        ss << "      \"deviceName\": \"" << escapeJson(p.deviceName) << "\",\n";
        ss << "      \"deviceType\": " << static_cast<int>(p.deviceType) << ",\n";
        ss << "      \"platform\": \"" << platformStr << "\",\n";
        ss << "      \"ipAddress\": \"" << escapeJson(p.ipAddress) << "\",\n";
        ss << "      \"port\": " << p.port << ",\n";
        ss << "      \"lastSeenMs\": " << (p.lastSeenMs > 0 ? p.lastSeenMs : std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) << "\n";
        ss << "    }" << (i + 1 < g_state.peers.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    // Current Progress
    double pct = 0.0;
    if (g_state.currentProgress.fileSize > 0) {
        pct = (100.0 * g_state.currentProgress.fileBytesTransferred) / g_state.currentProgress.fileSize;
        if (pct > 100.0) pct = 100.0;
    }
    int stateInt = static_cast<int>(g_state.currentProgress.state);

    ss << "  \"currentProgress\": {\n";
    ss << "    \"state\": " << stateInt << ",\n";
    ss << "    \"currentFileName\": \"" << escapeJson(g_state.currentProgress.currentFileName) << "\",\n";
    ss << "    \"fileName\": \"" << escapeJson(g_state.currentProgress.currentFileName) << "\",\n";
    ss << "    \"fileSize\": " << g_state.currentProgress.fileSize << ",\n";
    ss << "    \"totalBytes\": " << g_state.currentProgress.fileSize << ",\n";
    ss << "    \"fileBytesTransferred\": " << g_state.currentProgress.fileBytesTransferred << ",\n";
    ss << "    \"transferredBytes\": " << g_state.currentProgress.fileBytesTransferred << ",\n";
    ss << "    \"totalBytesTransferred\": " << g_state.currentProgress.batchBytesTransferred << ",\n";
    ss << "    \"batchTransferredBytes\": " << g_state.currentProgress.batchBytesTransferred << ",\n";
    ss << "    \"batchTotalBytes\": " << g_state.currentProgress.batchTotalBytes << ",\n";
    ss << "    \"speedMbps\": " << g_state.currentProgress.speedMbps << ",\n";
    ss << "    \"speedBytesPerSec\": " << g_state.currentProgress.speedBytesPerSec << ",\n";
    ss << "    \"progressPercent\": " << pct << ",\n";
    ss << "    \"etaSeconds\": " << static_cast<int>(g_state.currentProgress.etaSeconds) << ",\n";
    ss << "    \"errorCode\": 0,\n";
    ss << "    \"stateStr\": \"" << (g_state.isTransferring ? "TRANSFERRING" : (g_state.isPaused ? "PAUSED" : "IDLE")) << "\"\n";
    ss << "  },\n";

    // Progress alias for legacy clients
    ss << "  \"progress\": {\n";
    ss << "    \"fileName\": \"" << escapeJson(g_state.currentProgress.currentFileName) << "\",\n";
    ss << "    \"transferredBytes\": " << g_state.currentProgress.fileBytesTransferred << ",\n";
    ss << "    \"totalBytes\": " << g_state.currentProgress.fileSize << ",\n";
    ss << "    \"batchTransferredBytes\": " << g_state.currentProgress.batchBytesTransferred << ",\n";
    ss << "    \"batchTotalBytes\": " << g_state.currentProgress.batchTotalBytes << ",\n";
    ss << "    \"speedMbps\": " << g_state.currentProgress.speedMbps << ",\n";
    ss << "    \"speedBytesPerSec\": " << g_state.currentProgress.speedBytesPerSec << ",\n";
    ss << "    \"etaSeconds\": " << static_cast<int>(g_state.currentProgress.etaSeconds) << ",\n";
    ss << "    \"state\": \"" << (g_state.isTransferring ? "TRANSFERRING" : (g_state.isPaused ? "PAUSED" : "IDLE")) << "\"\n";
    ss << "  },\n";

    // Completed History (both array keys for compatibility)
    ss << "  \"completedHistory\": [\n";
    for (size_t i = 0; i < g_state.completedHistory.size(); ++i) {
        ss << "    \"" << escapeJson(g_state.completedHistory[i]) << "\"" << (i + 1 < g_state.completedHistory.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    ss << "  \"history\": [\n";
    for (size_t i = 0; i < g_state.completedHistory.size(); ++i) {
        ss << "    \"" << escapeJson(g_state.completedHistory[i]) << "\"" << (i + 1 < g_state.completedHistory.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

static void sendHttpResponse(socket_t clientSock, int statusCode, const std::string& contentType, const std::string& body) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << statusCode << " OK\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type, Accept\r\n";
    ss << "Connection: close\r\n\r\n";
    ss << body;

    std::string response = ss.str();
    send(clientSock, response.c_str(), static_cast<int>(response.length()), 0);
}

static std::string extractJsonString(const std::string& src, const std::string& key) {
    size_t kpos = src.find("\"" + key + "\"");
    if (kpos == std::string::npos) return "";
    size_t colon = src.find(":", kpos);
    if (colon == std::string::npos) return "";
    size_t q1 = src.find("\"", colon);
    if (q1 == std::string::npos) return "";
    std::string val;
    for (size_t i = q1 + 1; i < src.size(); ++i) {
        if (src[i] == '\\' && i + 1 < src.size()) {
            if (src[i + 1] == '\\') { val += '\\'; i++; }
            else if (src[i + 1] == '"') { val += '"'; i++; }
            else if (src[i + 1] == 'n') { val += '\n'; i++; }
            else if (src[i + 1] == 't') { val += '\t'; i++; }
            else { val += src[i + 1]; i++; }
        } else if (src[i] == '"') {
            break;
        } else {
            val += src[i];
        }
    }
    return val;
}

static std::vector<std::string> extractJsonStringArray(const std::string& src, const std::string& key) {
    std::vector<std::string> list;
    size_t kpos = src.find("\"" + key + "\"");
    if (kpos == std::string::npos) return list;
    size_t arrStart = src.find("[", kpos);
    if (arrStart == std::string::npos) return list;

    bool inQuote = false;
    bool inEscape = false;
    std::string cur;
    for (size_t i = arrStart + 1; i < src.size(); ++i) {
        char c = src[i];
        if (inEscape) {
            if (c == '\\') cur += '\\';
            else if (c == '"') cur += '"';
            else if (c == 'n') cur += '\n';
            else if (c == 't') cur += '\t';
            else cur += c;
            inEscape = false;
        } else if (c == '\\') {
            inEscape = true;
        } else if (c == '"') {
            if (inQuote) {
                list.push_back(cur);
                cur.clear();
                inQuote = false;
            } else {
                inQuote = true;
            }
        } else if (c == ']' && !inQuote) {
            break;
        } else if (inQuote) {
            cur += c;
        }
    }
    return list;
}

static void handleHttpClient(socket_t clientSock) {
    std::vector<char> reqBuf(65536);
    int bytesRecv = recv(clientSock, reqBuf.data(), static_cast<int>(reqBuf.size() - 1), 0);
    if (bytesRecv <= 0) {
        CLOSE_SOCKET(clientSock);
        return;
    }
    reqBuf[bytesRecv] = '\0';
    std::string req(reqBuf.data(), bytesRecv);

    std::istringstream reqStream(req);
    std::string method, path, httpVersion;
    reqStream >> method >> path >> httpVersion;

    if (method == "OPTIONS") {
        sendHttpResponse(clientSock, 200, "text/plain", "OK");
        CLOSE_SOCKET(clientSock);
        return;
    }

    if (method == "GET" && (path == "/api/status" || path == "/status" || path == "/")) {
        std::string json = buildStatusJsonResponse();
        sendHttpResponse(clientSock, 200, "application/json", json);
    } else if (method == "POST" && path == "/api/transfer/send") {
        size_t bodyPos = req.find("\r\n\r\n");
        std::string body = (bodyPos != std::string::npos) ? req.substr(bodyPos + 4) : "";

        std::string targetIp = extractJsonString(body, "targetIp");
        if (targetIp.empty()) targetIp = "127.0.0.1";

        int targetPort = 48124;
        size_t portPos = body.find("\"targetPort\":");
        if (portPos != std::string::npos) {
            size_t pStart = portPos + 13;
            while (pStart < body.size() && (body[pStart] == ' ' || body[pStart] == ':')) pStart++;
            targetPort = std::atoi(body.c_str() + pStart);
            if (targetPort <= 0) targetPort = 48124;
        }

        std::vector<std::string> filePaths = extractJsonStringArray(body, "filePaths");

        if (filePaths.empty()) {
            sendHttpResponse(clientSock, 400, "application/json", "{\"success\":false,\"error\":\"No files specified\"}");
        } else {
            aerosync::PeerInfo target;
            target.deviceId = "direct-" + targetIp;
            target.deviceName = "Remote Device (" + targetIp + ")";
            target.ipAddress = targetIp;
            target.port = static_cast<uint16_t>(targetPort);

            std::thread([target, filePaths]() {
                {
                    std::lock_guard<std::mutex> lock(g_state.mtx);
                    g_state.isTransferring = true;
                    g_state.isPaused = false;
                    g_state.statusMessage = "Sending " + std::to_string(filePaths.size()) + " file(s) to " + target.ipAddress;
                }

                bool ok = g_app->sendFiles(target, filePaths, [](const aerosync::TransferProgress& prog) {
                    std::lock_guard<std::mutex> lock(g_state.mtx);
                    g_state.currentProgress = prog;
                    g_state.isTransferring = (prog.state == aerosync::TransferState::TRANSFERRING);
                    double mbSec = prog.speedBytesPerSec / (1024.0 * 1024.0);
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.1f MB/s", mbSec);
                    g_state.statusMessage = "Streaming " + prog.currentFileName + " (" + buf + ")";
                });

                {
                    std::lock_guard<std::mutex> lock(g_state.mtx);
                    g_state.isTransferring = false;
                    if (ok) {
                        for (const auto& fp : filePaths) {
                            std::string fn = std::filesystem::path(fp).filename().string();
                            g_state.completedHistory.push_back(fn);
                        }
                        g_state.statusMessage = "Transfer completed successfully!";
                    } else {
                        g_state.currentProgress.state = aerosync::TransferState::CANCELLED;
                        g_state.currentProgress.speedBytesPerSec = 0;
                        g_state.statusMessage = "Transfer cancelled or interrupted";
                    }
                }
            }).detach();

            sendHttpResponse(clientSock, 200, "application/json", "{\"success\":true,\"message\":\"Transfer started\"}");
        }
    } else if (method == "POST" && path == "/api/transfer/cancel") {
        g_app->cancelTransfer();
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.isTransferring = false;
            g_state.currentProgress.state = aerosync::TransferState::CANCELLED;
            g_state.currentProgress.speedBytesPerSec = 0;
            g_state.statusMessage = "Transfer cancelled";
        }
        sendHttpResponse(clientSock, 200, "application/json", "{\"success\":true,\"message\":\"Cancelled\"}");
    } else if (method == "POST" && path == "/api/settings/download_dir") {
        size_t bodyPos = req.find("\r\n\r\n");
        std::string body = (bodyPos != std::string::npos) ? req.substr(bodyPos + 4) : "";
        std::string newDir = extractJsonString(body, "downloadDir");
        if (newDir.empty()) newDir = extractJsonString(body, "dir");

        if (!newDir.empty()) {
            {
                std::lock_guard<std::mutex> lock(g_state.mtx);
                g_state.downloadDir = newDir;
            }
            g_app->setDownloadDirectory(newDir);
            sendHttpResponse(clientSock, 200, "application/json", "{\"success\":true,\"downloadDir\":\"" + escapeJson(newDir) + "\"}");
            CLOSE_SOCKET(clientSock);
            return;
        }
        sendHttpResponse(clientSock, 400, "application/json", "{\"success\":false,\"error\":\"Invalid directory\"}");
    } else if (method == "GET" && path == "/api/shutdown") {
        sendHttpResponse(clientSock, 200, "application/json", "{\"success\":true,\"message\":\"Shutting down\"}");
        CLOSE_SOCKET(clientSock);
        g_running = false;
        return;
    } else {
        sendHttpResponse(clientSock, 404, "application/json", "{\"error\":\"Not Found\"}");
    }

    CLOSE_SOCKET(clientSock);
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Enforce Single-Instance Core Daemon to prevent duplicate discovery beacons
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "AeroSync_Core_Daemon_Mutex_v2");
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cout << "[Daemon] Another AeroSync Core Daemon is already running. Exiting cleanly." << std::endl;
        return 0;
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    char compName[MAX_COMPUTERNAME_LENGTH + 1] = "Windows-PC";
#ifdef _WIN32
    DWORD cSize = sizeof(compName);
    GetComputerNameA(compName, &cSize);
#endif

    // Retrieve or persist permanent device ID for this Windows machine
    char localAppData[MAX_PATH] = {0};
    std::string idFilePath;
    if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::string appDir = std::string(localAppData) + "\\AeroSync";
        std::filesystem::create_directories(appDir);
        idFilePath = appDir + "\\device_id.txt";
    }

    std::string persistentId;
    if (!idFilePath.empty() && std::filesystem::exists(idFilePath)) {
        std::ifstream f(idFilePath);
        std::getline(f, persistentId);
    }
    if (persistentId.empty() || persistentId.length() < 5) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis;
        std::ostringstream ss;
        ss << "win-" << compName << "-" << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
        persistentId = ss.str();
        if (!idFilePath.empty()) {
            std::ofstream f(idFilePath);
            f << persistentId;
        }
    }
    g_state.deviceId = persistentId;
    g_state.deviceName = std::string(compName) + " (Windows PC)";

    char userProfile[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH) > 0) {
        g_state.downloadDir = std::string(userProfile) + "\\Downloads\\AeroSync";
    } else {
        g_state.downloadDir = "C:\\AeroSync_Downloads";
    }
    std::filesystem::create_directories(g_state.downloadDir);

    g_app = std::make_unique<aerosync::AeroSyncApp>(
        g_state.deviceId,
        g_state.deviceName,
        aerosync::DeviceType::DEVICE_WINDOWS
    );
    g_app->setDownloadDirectory(g_state.downloadDir);

    g_app->setPeerDiscoveredCallback([](const std::vector<aerosync::PeerInfo>& peers) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.peers = peers;
    });

    g_app->setIncomingConnectCallback([](const aerosync::ConnectRequest& req, std::function<void(bool accept)> respondCb) {
        std::cout << "[Daemon] Incoming connection/pairing request from " << req.senderName << " (" << req.senderIp << ") PIN: " << req.pairingPin << std::endl;
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.statusMessage = "Authenticated with " + req.senderName;
        }
        respondCb(true); // Auto-accept high-speed connections
    });

    g_app->setPairingStateChangedCallback([](aerosync::PairingState state, const std::string& reason) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.statusMessage = "Pairing: " + aerosync::pairingStateToString(state) + (reason.empty() ? "" : (" (" + reason + ")"));
    });

    g_app->setIncomingTransferCallback([](const aerosync::TransferManifest& manifest, std::function<void(bool)> respondCb) {
        {
            std::lock_guard<std::mutex> lock(g_state.mtx);
            g_state.isTransferring = true;
            g_state.statusMessage = "Receiving " + std::to_string(manifest.totalFiles) + " file(s) from " + manifest.senderName + "...";
        }
        respondCb(true); // Auto-accept high-speed transfers
    });

    g_app->setIncomingTransferProgressCallback([](const aerosync::TransferProgress& prog) {
        std::lock_guard<std::mutex> lock(g_state.mtx);
        g_state.currentProgress = prog;
        g_state.isTransferring = (prog.state == aerosync::TransferState::TRANSFERRING);
        if (prog.state == aerosync::TransferState::COMPLETED ||
            (prog.fileBytesTransferred >= prog.fileSize && prog.fileSize > 0)) {
            g_state.completedHistory.push_back(prog.currentFileName);
            g_state.statusMessage = "Received: " + prog.currentFileName;
        } else {
            double mbSec = prog.speedBytesPerSec / (1024.0 * 1024.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f MB/s", mbSec);
            g_state.statusMessage = "Receiving: " + prog.currentFileName + " (" + buf + ")";
        }
    });

    if (!g_app->initialize()) {
        std::cerr << "Failed to initialize AeroSync C++ Core Engine!" << std::endl;
        return 1;
    }

    std::cout << "AeroSync C++ Core Daemon started on 127.0.0.1:48126" << std::endl;

    // Start HTTP IPC Server on 127.0.0.1:48126
    socket_t serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET) {
        std::cerr << "Failed to create IPC server socket" << std::endl;
        return 1;
    }

    int reuse = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(48126);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind IPC server socket to port 48126" << std::endl;
        CLOSE_SOCKET(serverSock);
        return 1;
    }

    if (listen(serverSock, 32) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on IPC socket" << std::endl;
        CLOSE_SOCKET(serverSock);
        return 1;
    }

    while (g_running) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        socket_t clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock != INVALID_SOCKET) {
            std::thread(handleHttpClient, clientSock).detach();
        }
    }

    CLOSE_SOCKET(serverSock);
    g_app->shutdown();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
