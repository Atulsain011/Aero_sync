#include "aerosync/discovery_engine.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>

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
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

#ifdef __ANDROID__
    #include <android/log.h>
    #define AERO_LOG_I(...) __android_log_print(ANDROID_LOG_INFO, "AeroSync", __VA_ARGS__)
    #define AERO_LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "AeroSync", __VA_ARGS__)
#else
    #define AERO_LOG_I(...) do { printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while(0)
    #define AERO_LOG_E(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while(0)
#endif

namespace aerosync {

constexpr uint64_t PEER_OFFLINE_TIMEOUT_MS = 5000; // 5 seconds timeout
static const char* DISCOVERY_MULTICAST_IP = "239.255.48.123";

static void initSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

static void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

static std::vector<std::string> getLocalIpAddresses() {
    std::vector<std::string> ips;
    ips.push_back("127.0.0.1");
#ifdef _WIN32
    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS uni = curr->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
                if (uni->Address.lpSockaddr && uni->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(uni->Address.lpSockaddr);
                    char ipStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &(sin->sin_addr), ipStr, INET_ADDRSTRLEN)) {
                        ips.push_back(ipStr);
                    }
                }
            }
        }
    }
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(sin->sin_addr), ipStr, INET_ADDRSTRLEN)) {
                ips.push_back(ipStr);
            }
        }
        freeifaddrs(ifaddr);
    }
#endif
    std::sort(ips.begin(), ips.end());
    ips.erase(std::unique(ips.begin(), ips.end()), ips.end());
    return ips;
}

static std::vector<in_addr> getLocalInterfaceAddresses() {
    std::vector<in_addr> addrs;
#ifdef _WIN32
    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS uni = curr->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
                if (uni->Address.lpSockaddr && uni->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(uni->Address.lpSockaddr);
                    addrs.push_back(sin->sin_addr);
                }
            }
        }
    }
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            addrs.push_back(sin->sin_addr);
        }
        freeifaddrs(ifaddr);
    }
#endif
    return addrs;
}

static std::vector<std::string> getBroadcastAndGatewayTargets() {
    std::vector<std::string> targets;
    targets.push_back("255.255.255.255");
    targets.push_back(DISCOVERY_MULTICAST_IP); // 239.255.48.123

    // Common mobile hotspot & tethering subnet broadcasts and gateways
    targets.push_back("192.168.43.255");  // Android default hotspot broadcast
    targets.push_back("192.168.43.1");    // Android default hotspot gateway
    targets.push_back("192.168.49.255");  // Android Wi-Fi Direct broadcast
    targets.push_back("192.168.49.1");    // Android Wi-Fi Direct gateway
    targets.push_back("10.42.0.255");     // Linux NetworkManager Hotspot broadcast
    targets.push_back("10.42.0.1");       // Linux NetworkManager Hotspot gateway
    targets.push_back("192.168.12.255");  // Alternative Linux/Android Hotspot
    targets.push_back("192.168.12.1");
    targets.push_back("192.168.137.255"); // Windows Mobile Hotspot broadcast
    targets.push_back("192.168.137.1");   // Windows Mobile Hotspot gateway
    targets.push_back("192.168.42.255");  // Android USB Tethering (RNDIS/NCM) broadcast
    targets.push_back("192.168.42.1");    // Android USB Tethering gateway
    targets.push_back("192.168.42.129");  // Android USB Tethering host IP
    targets.push_back("172.20.10.15");    // iOS / Mobile Hotspot broadcast
    targets.push_back("172.20.10.1");     // iOS / Mobile Hotspot gateway
    targets.push_back("192.168.1.255");   // Common LAN broadcast
    targets.push_back("192.168.1.1");
    targets.push_back("192.168.0.255");   // Common LAN broadcast
    targets.push_back("192.168.0.1");
    targets.push_back("192.168.2.255");
    targets.push_back("192.168.2.1");
    targets.push_back("10.0.0.255");
    targets.push_back("10.0.0.1");

#ifdef _WIN32
    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
    DWORD ret = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen);
    }

    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (curr->OperStatus != IfOperStatusUp) continue;

            // 1. Unicast addresses and calculate subnet broadcast (e.g. 192.168.43.255)
            for (PIP_ADAPTER_UNICAST_ADDRESS uni = curr->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
                if (uni->Address.lpSockaddr && uni->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(uni->Address.lpSockaddr);
                    uint32_t ip = ntohl(sin->sin_addr.s_addr);
                    uint8_t prefixLen = uni->OnLinkPrefixLength;
                    if (prefixLen > 0 && prefixLen < 32) {
                        uint32_t mask = (~0u << (32 - prefixLen));
                        uint32_t bcast = (ip & mask) | (~mask);
                        in_addr bcastAddr;
                        bcastAddr.s_addr = htonl(bcast);
                        char bcastStr[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &bcastAddr, bcastStr, INET_ADDRSTRLEN)) {
                            targets.push_back(bcastStr);
                        }
                    }
                }
            }

            // 2. Gateway addresses (e.g. 192.168.43.1 on Mobile Hotspot)
            for (PIP_ADAPTER_GATEWAY_ADDRESS_LH gw = curr->FirstGatewayAddress; gw != nullptr; gw = gw->Next) {
                if (gw->Address.lpSockaddr && gw->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(gw->Address.lpSockaddr);
                    char gwStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &(sin->sin_addr), gwStr, INET_ADDRSTRLEN)) {
                        targets.push_back(gwStr);
                    }
                }
            }
        }
    }
#else
    // Linux and Android: Dynamically discover all interfaces (wlp*, wlan*, enp*, eth*, etc.)
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            uint32_t ip = ntohl(sin->sin_addr.s_addr);

            // 1. If broadcast address is directly populated
            if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr) {
                sockaddr_in* bsin = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr);
                char bcastStr[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &(bsin->sin_addr), bcastStr, INET_ADDRSTRLEN)) {
                    targets.push_back(bcastStr);
                }
            }

            // 2. Compute subnet broadcast from netmask
            if (ifa->ifa_netmask) {
                sockaddr_in* nsin = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
                uint32_t mask = ntohl(nsin->sin_addr.s_addr);
                if (mask > 0 && mask < 0xFFFFFFFFu) {
                    uint32_t bcast = (ip & mask) | (~mask);
                    in_addr bcastAddr;
                    bcastAddr.s_addr = htonl(bcast);
                    char bcastStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &bcastAddr, bcastStr, INET_ADDRSTRLEN)) {
                        targets.push_back(bcastStr);
                    }
                }
            } else {
                // Fallback default /24 subnet broadcast
                uint32_t bcast = ip | 0x000000FFu;
                in_addr bcastAddr;
                bcastAddr.s_addr = htonl(bcast);
                char bcastStr[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &bcastAddr, bcastStr, INET_ADDRSTRLEN)) {
                    targets.push_back(bcastStr);
                }
            }
        }
        freeifaddrs(ifaddr);
    }

#if defined(__linux__) && !defined(__ANDROID__)
    // 1. Parse Linux routing table /proc/net/route to find default gateways (Destination == 00000000)
    std::ifstream routeFile("/proc/net/route");
    if (routeFile.is_open()) {
        std::string line;
        std::getline(routeFile, line); // Skip header line
        while (std::getline(routeFile, line)) {
            std::istringstream iss(line);
            std::string iface, dest, gateway;
            if (iss >> iface >> dest >> gateway) {
                if (dest == "00000000" && gateway != "00000000") {
                    try {
                        uint32_t gwHex = static_cast<uint32_t>(std::stoul(gateway, nullptr, 16));
                        in_addr gwAddr;
                        gwAddr.s_addr = gwHex; // already in network byte order in /proc/net/route
                        char gwStr[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &gwAddr, gwStr, INET_ADDRSTRLEN)) {
                            targets.push_back(gwStr);
                        }
                    } catch (...) {}
                }
            }
        }
    }

    // 2. Parse Linux ARP cache /proc/net/arp to discover active neighbors on the local subnet & hotspot
    std::ifstream arpFile("/proc/net/arp");
    if (arpFile.is_open()) {
        std::string line;
        std::getline(arpFile, line); // Skip header line
        while (std::getline(arpFile, line)) {
            std::istringstream iss(line);
            std::string ip, hwType, flags, hwAddr, mask, device;
            if (iss >> ip >> hwType >> flags >> hwAddr >> mask >> device) {
                if (flags != "0x0" && flags != "0x00" && ip.find("127.") != 0 && ip.find("169.254.") != 0) {
                    targets.push_back(ip);
                }
            }
        }
    }
#endif
#endif

    // Deduplicate targets
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
}

DiscoveryEngine::DiscoveryEngine(const std::string& localDeviceId,
                                 const std::string& localDeviceName,
                                 DeviceType localDeviceType,
                                 uint16_t listenPort)
    : m_localDeviceId(localDeviceId),
      m_localDeviceName(localDeviceName),
      m_localDeviceType(localDeviceType),
      m_listenPort(listenPort) {
    initSockets();
}

DiscoveryEngine::~DiscoveryEngine() {
    stop();
    cleanupSockets();
}

uint64_t DiscoveryEngine::getCurrentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void DiscoveryEngine::logNetworkInterfaces() {
#ifndef _WIN32
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        size_t ifaceCount = 0;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(sin->sin_addr), ipStr, INET_ADDRSTRLEN);

            char bcastStr[INET_ADDRSTRLEN] = {0};
            if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr) {
                sockaddr_in* bsin = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr);
                inet_ntop(AF_INET, &(bsin->sin_addr), bcastStr, INET_ADDRSTRLEN);
            }

            if (bcastStr[0] != '\0') {
                AERO_LOG_I("[AeroSync] Network interface: %s (%s, broadcast: %s)",
                           ifa->ifa_name, ipStr, bcastStr);
            } else {
                AERO_LOG_I("[AeroSync] Network interface: %s (%s)",
                           ifa->ifa_name, ipStr);
            }
            ifaceCount++;
        }
        freeifaddrs(ifaddr);

        if (ifaceCount == 0) {
            AERO_LOG_I("[AeroSync] Notice: No active physical network interfaces found (loopback only).");
        }
    } else {
        int err = errno;
        AERO_LOG_E("[AeroSync] Error: Could not enumerate network interfaces (errno %d: %s)",
                   err, strerror(err));
    }
#else
    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            std::wstring descW(curr->FriendlyName ? curr->FriendlyName : (curr->Description ? curr->Description : L""));
            std::string desc(descW.begin(), descW.end());
            for (PIP_ADAPTER_UNICAST_ADDRESS uni = curr->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
                if (uni->Address.lpSockaddr && uni->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(uni->Address.lpSockaddr);
                    char ipStr[INET_ADDRSTRLEN] = {0};
                    if (inet_ntop(AF_INET, &(sin->sin_addr), ipStr, INET_ADDRSTRLEN)) {
                        AERO_LOG_I("[AeroSync] Network interface: %s (%s)", desc.c_str(), ipStr);
                    }
                }
            }
        }
    }
#endif
}

bool DiscoveryEngine::start() {
    if (m_running) return true;
    m_running = true;

    AERO_LOG_I("[AeroSync] Discovery initialized");
    logNetworkInterfaces();

    m_listenThread = std::thread(&DiscoveryEngine::listenLoop, this);
    m_broadcastThread = std::thread(&DiscoveryEngine::broadcastLoop, this);
    return true;
}

void DiscoveryEngine::stop() {
    if (!m_running) return;
    m_running = false;

    int sock = m_udpSocket.exchange(-1);
    if (sock != -1 && sock != static_cast<int>(INVALID_SOCKET)) {
        CLOSE_SOCKET(sock);
    }

    if (m_listenThread.joinable()) m_listenThread.join();
    if (m_broadcastThread.joinable()) m_broadcastThread.join();
}

void DiscoveryEngine::setPeerCallback(PeerDiscoveredCallback cb) {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_callback = cb;
}

void DiscoveryEngine::addBroadcastTarget(const std::string& targetIp) {
    if (targetIp.empty()) return;
    std::lock_guard<std::mutex> lock(m_targetsMutex);
    if (std::find(m_customTargets.begin(), m_customTargets.end(), targetIp) == m_customTargets.end()) {
        m_customTargets.push_back(targetIp);
        AERO_LOG_I("[DISCOVERY_TARGET_ADDED] Registered dynamic broadcast target: %s", targetIp.c_str());
    }
}

std::vector<PeerInfo> DiscoveryEngine::getDiscoveredPeers() const {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    std::vector<PeerInfo> list;
    uint64_t now = getCurrentTimeMs();
    for (const auto& kv : m_peersMap) {
        if (now - kv.second.lastSeenMs <= PEER_OFFLINE_TIMEOUT_MS) {
            list.push_back(kv.second);
        }
    }
    return list;
}

void DiscoveryEngine::broadcastLoop() {
    socket_t sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSock == INVALID_SOCKET) {
#ifdef _WIN32
        int err = WSAGetLastError();
        AERO_LOG_E("[AeroSync] Error: Failed to create UDP broadcast socket (WSA error: %d)", err);
#else
        int err = errno;
        AERO_LOG_E("[AeroSync] Error: Failed to create UDP broadcast socket (errno %d: %s)", err, strerror(err));
#endif
        return;
    }

    int broadcastEnable = 1;
    setsockopt(sendSock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastEnable, sizeof(broadcastEnable));

    // Allow multicast TTL = 4 for local network traversal
    int mcTtl = 4;
    setsockopt(sendSock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&mcTtl, sizeof(mcTtl));

    int mcLoop = 1;
    setsockopt(sendSock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&mcLoop, sizeof(mcLoop));

    // mDNS multicast address fallback: 224.0.0.251:5353
    sockaddr_in mdnsAddr{};
    mdnsAddr.sin_family = AF_INET;
    mdnsAddr.sin_port = htons(5353);
    inet_pton(AF_INET, "224.0.0.251", &mdnsAddr.sin_addr);

    PeerInfo localPeer;
    localPeer.deviceId = m_localDeviceId;
    localPeer.deviceName = m_localDeviceName;
    localPeer.deviceType = m_localDeviceType;
    localPeer.platform = deviceTypeToString(m_localDeviceType);
    localPeer.appVersion = "1.0.8";
    localPeer.port = m_listenPort;

    AERO_LOG_I("[AeroSync] Beacon loop active for device %s (%s)", m_localDeviceName.c_str(), m_localDeviceId.c_str());

    while (m_running) {
        localPeer.lastSeenMs = getCurrentTimeMs();
        std::string jsonPacket = ProtocolSerializer::serializeDiscoveryBeacon(localPeer);

        // 1. Send UDP beacons across all active network adapters, subnets, gateways, and ARP neighbors
        auto targets = getBroadcastAndGatewayTargets();
        {
            std::lock_guard<std::mutex> lock(m_targetsMutex);
            for (const auto& ct : m_customTargets) {
                targets.push_back(ct);
            }
        }
        const char* allowLoopbackEnv = getenv("AEROSYNC_ALLOW_LOOPBACK_DISCOVERY");
        bool allowLoopback = (allowLoopbackEnv && std::string(allowLoopbackEnv) == "1");
        if (allowLoopback) {
            targets.push_back("127.0.0.1");
        }
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

        for (const auto& targetIp : targets) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(DISCOVERY_UDP_PORT);
            if (inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr) > 0) {
                sendto(sendSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                       (sockaddr*)&addr, sizeof(addr));
            }
        }

        // 2. Send multicast fallback across ALL active network interfaces via IP_MULTICAST_IF
        sockaddr_in mdnsPort48123{};
        mdnsPort48123.sin_family = AF_INET;
        mdnsPort48123.sin_port = htons(DISCOVERY_UDP_PORT);
        inet_pton(AF_INET, "224.0.0.251", &mdnsPort48123.sin_addr);

        sockaddr_in mcAddr{};
        mcAddr.sin_family = AF_INET;
        mcAddr.sin_port = htons(DISCOVERY_UDP_PORT);
        inet_pton(AF_INET, DISCOVERY_MULTICAST_IP, &mcAddr.sin_addr);

        auto localIfAddrs = getLocalInterfaceAddresses();
        if (localIfAddrs.empty()) {
            in_addr defaultAny{};
            defaultAny.s_addr = htonl(INADDR_ANY);
            localIfAddrs.push_back(defaultAny);
        }

        for (const auto& ifAddr : localIfAddrs) {
            setsockopt(sendSock, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&ifAddr, sizeof(ifAddr));

            sendto(sendSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                   (sockaddr*)&mcAddr, sizeof(mcAddr));
            sendto(sendSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                   (sockaddr*)&mdnsPort48123, sizeof(mdnsPort48123));
            sendto(sendSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                   (sockaddr*)&mdnsAddr, sizeof(mdnsAddr));
        }

        AERO_LOG_I("[DISCOVERY_BEACON_SENT] sent to %zu subnet/gateway targets across %zu interfaces",
                   targets.size(), localIfAddrs.size());

        // Maintain peer table and enforce 5-second timeout for offline peers
        PeerDiscoveredCallback cbToCall = nullptr;
        std::vector<PeerInfo> activePeers;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            uint64_t now = getCurrentTimeMs();
            std::vector<std::string> toRemove;
            for (auto& kv : m_peersMap) {
                if (now - kv.second.lastSeenMs > PEER_OFFLINE_TIMEOUT_MS) {
                    toRemove.push_back(kv.first);
                    AERO_LOG_I("[AeroSync] Device lost: %s (%s) [id: %s, last seen %llu ms ago]",
                               kv.second.deviceName.c_str(), kv.second.platform.c_str(),
                               kv.first.c_str(), static_cast<unsigned long long>(now - kv.second.lastSeenMs));
                }
            }
            bool changed = false;
            for (const auto& id : toRemove) {
                m_peersMap.erase(id);
                changed = true;
            }
            if (changed && m_callback) {
                cbToCall = m_callback;
                for (const auto& kv : m_peersMap) activePeers.push_back(kv.second);
            }
        }
        if (cbToCall) {
            cbToCall(activePeers);
        }

        for (int s = 0; s < 10 && m_running; ++s) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    CLOSE_SOCKET(sendSock);
}

void DiscoveryEngine::listenLoop() {
    socket_t listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenSock == INVALID_SOCKET) {
#ifdef _WIN32
        int err = WSAGetLastError();
        AERO_LOG_E("[AeroSync] Error: Failed to create UDP discovery listen socket (WSA error: %d)", err);
#else
        int err = errno;
        AERO_LOG_E("[AeroSync] Error: Failed to create UDP discovery listen socket (errno %d: %s)", err, strerror(err));
#endif
        return;
    }

    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#ifndef _WIN32
    if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) {
        int err = errno;
        AERO_LOG_E("[AeroSync] Warning: setsockopt SO_REUSEPORT failed (errno %d: %s)", err, strerror(err));
    }
#endif
    int bcastEnable = 1;
    if (setsockopt(listenSock, SOL_SOCKET, SO_BROADCAST, (const char*)&bcastEnable, sizeof(bcastEnable)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        AERO_LOG_E("[AeroSync] Warning: setsockopt SO_BROADCAST failed (WSA error: %d)", err);
#else
        int err = errno;
        AERO_LOG_E("[AeroSync] Warning: setsockopt SO_BROADCAST failed (errno %d: %s)", err, strerror(err));
#endif
    }

#ifdef _WIN32
    DWORD timeout = 500;
    setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in listenAddr{};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(DISCOVERY_UDP_PORT);
    listenAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr)) == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        AERO_LOG_E("[AeroSync] Error: Failed to bind discovery UDP port %d (WSA error: %d). Cause: Port may already be in use by another application.", DISCOVERY_UDP_PORT, err);
#else
        int err = errno;
        AERO_LOG_E("[AeroSync] Error: Failed to bind discovery UDP port %d (errno %d: %s). Cause: Port may already be in use by another application.", DISCOVERY_UDP_PORT, err, strerror(err));
#endif
        CLOSE_SOCKET(listenSock);
        return;
    }

    m_udpSocket = static_cast<int>(listenSock);
    AERO_LOG_I("[AeroSync] Discovery listening on 0.0.0.0:%d (multicast: %s)", DISCOVERY_UDP_PORT, DISCOVERY_MULTICAST_IP);

    // Join local multicast groups for robust discovery across all interfaces
    ip_mreq mreq{};
    inet_pton(AF_INET, DISCOVERY_MULTICAST_IP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(listenSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));

    ip_mreq mdnsMreq{};
    inet_pton(AF_INET, "224.0.0.251", &mdnsMreq.imr_multiaddr);
    mdnsMreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(listenSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mdnsMreq, sizeof(mdnsMreq));

    // Also join on each specific local IP adapter to guarantee reception across secondary interfaces
    auto localIps = getLocalIpAddresses();
    for (const auto& lip : localIps) {
        if (lip == "127.0.0.1") continue;
        in_addr ifAddr{};
        if (inet_pton(AF_INET, lip.c_str(), &ifAddr) > 0) {
            ip_mreq ifMreq{};
            inet_pton(AF_INET, DISCOVERY_MULTICAST_IP, &ifMreq.imr_multiaddr);
            ifMreq.imr_interface = ifAddr;
            setsockopt(listenSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&ifMreq, sizeof(ifMreq));

            ip_mreq ifMdns{};
            inet_pton(AF_INET, "224.0.0.251", &ifMdns.imr_multiaddr);
            ifMdns.imr_interface = ifAddr;
            setsockopt(listenSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&ifMdns, sizeof(ifMdns));
        }
    }

    char buffer[2048];

    while (m_running) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        int bytesRecv = recvfrom(listenSock, buffer, sizeof(buffer) - 1, 0,
                                 (sockaddr*)&clientAddr, &clientLen);

        if (bytesRecv > 0) {
            buffer[bytesRecv] = '\0';
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
            parseBeacon(std::string(buffer, bytesRecv), std::string(ipStr));
        }
    }

    int sock = m_udpSocket.exchange(-1);
    if (sock != -1 && sock != static_cast<int>(INVALID_SOCKET)) {
        CLOSE_SOCKET(sock);
    }
}

void DiscoveryEngine::sendDirectResponse(const std::string& targetIp, uint16_t targetPort) {
    const char* allowLoopbackEnv = getenv("AEROSYNC_ALLOW_LOOPBACK_DISCOVERY");
    bool allowLoopback = (allowLoopbackEnv && std::string(allowLoopbackEnv) == "1");
    if (targetIp.empty() || (!allowLoopback && targetIp == "127.0.0.1")) return;

    // Rate-limit responses to 1 every 1.5 seconds per IP to prevent reply storms
    uint64_t now = getCurrentTimeMs();
    {
        std::lock_guard<std::mutex> lock(m_responseMutex);
        auto it = m_lastResponseTime.find(targetIp);
        if (it != m_lastResponseTime.end() && (now - it->second < 1500)) {
            return;
        }
        m_lastResponseTime[targetIp] = now;
    }

    // Auto-register peer IP as a custom broadcast target for ongoing heartbeats
    addBroadcastTarget(targetIp);

    PeerInfo replyPeer;
    replyPeer.deviceId = m_localDeviceId;
    replyPeer.deviceName = m_localDeviceName;
    replyPeer.deviceType = m_localDeviceType;
    replyPeer.platform = deviceTypeToString(m_localDeviceType);
    replyPeer.appVersion = "1.0.8";
    replyPeer.port = m_listenPort;
    replyPeer.lastSeenMs = now;
    replyPeer.isResponse = true;

    std::string jsonPacket = ProtocolSerializer::serializeDiscoveryBeacon(replyPeer);

    sockaddr_in targetAddr{};
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(DISCOVERY_UDP_PORT);
    if (inet_pton(AF_INET, targetIp.c_str(), &targetAddr.sin_addr) > 0) {
        socket_t sock = m_udpSocket.load();
        if (sock != -1 && sock != static_cast<int>(INVALID_SOCKET)) {
            sendto(sock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                   (sockaddr*)&targetAddr, sizeof(targetAddr));
            AERO_LOG_I("[DISCOVERY_RESPONSE_SENT] Immediate unicast response sent to %s:%d",
                       targetIp.c_str(), DISCOVERY_UDP_PORT);
        } else {
            socket_t tempSock = socket(AF_INET, SOCK_DGRAM, 0);
            if (tempSock != INVALID_SOCKET) {
                int bcastEnable = 1;
                setsockopt(tempSock, SOL_SOCKET, SO_BROADCAST, (const char*)&bcastEnable, sizeof(bcastEnable));
                sendto(tempSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
                       (sockaddr*)&targetAddr, sizeof(targetAddr));
                CLOSE_SOCKET(tempSock);
            }
        }
    }
}

void DiscoveryEngine::parseBeacon(const std::string& data, const std::string& senderIp) {
    PeerInfo peer;
    if (!ProtocolSerializer::deserializeDiscoveryBeacon(data, senderIp, peer)) {
        return;
    }

    if (peer.deviceId.empty() || peer.deviceId == m_localDeviceId) {
        return; // Skip self
    }

    // Skip loopback addresses unless explicitly allowed for testing
    const char* allowLoopbackEnv = getenv("AEROSYNC_ALLOW_LOOPBACK_DISCOVERY");
    bool allowLoopback = (allowLoopbackEnv && std::string(allowLoopbackEnv) == "1");

    if (!allowLoopback) {
        if (senderIp == "127.0.0.1" || senderIp == "localhost" || peer.ipAddress == "127.0.0.1") {
            return;
        }

        // Skip if sender matches any local host IP address and matches local device type
        auto localIps = getLocalIpAddresses();
        for (const auto& lip : localIps) {
            if ((senderIp == lip || peer.ipAddress == lip) && peer.deviceType == m_localDeviceType) {
                return; // Ignore local machine adapter loopback
            }
        }
    }

    // If incoming beacon is an initial broadcast (not a reply), send an immediate direct unicast response!
    if (!peer.isResponse) {
        sendDirectResponse(senderIp, peer.port);
    } else {
        addBroadcastTarget(senderIp);
    }

    peer.lastSeenMs = getCurrentTimeMs();

    PeerDiscoveredCallback cbToCall = nullptr;
    std::vector<PeerInfo> activePeers;
    {
        std::lock_guard<std::mutex> lock(m_peersMutex);

        // Prune any old entry that had the exact same IP and port but different deviceId
        for (auto it = m_peersMap.begin(); it != m_peersMap.end(); ) {
            if (it->first != peer.deviceId && it->second.ipAddress == peer.ipAddress && it->second.port == peer.port) {
                AERO_LOG_I("[PEER_PRUNED] Replacing obsolete device entry %s with %s at %s:%d",
                           it->first.c_str(), peer.deviceId.c_str(), peer.ipAddress.c_str(), peer.port);
                it = m_peersMap.erase(it);
            } else {
                ++it;
            }
        }

        auto it = m_peersMap.find(peer.deviceId);
        bool isNew = (it == m_peersMap.end());
        bool changed = isNew || (it->second.deviceName != peer.deviceName) ||
                       (it->second.ipAddress != peer.ipAddress) ||
                       (it->second.port != peer.port) ||
                       (it->second.deviceType != peer.deviceType);

        if (isNew) {
            AERO_LOG_I("[AeroSync] Device discovered: %s (%s) at %s:%d [id: %s]",
                       peer.deviceName.c_str(), peer.platform.c_str(),
                       peer.ipAddress.c_str(), peer.port, peer.deviceId.c_str());
        } else if (changed) {
            AERO_LOG_I("[AeroSync] Device updated: %s (%s) at %s:%d [id: %s]",
                       peer.deviceName.c_str(), peer.platform.c_str(),
                       peer.ipAddress.c_str(), peer.port, peer.deviceId.c_str());
        }

        m_peersMap[peer.deviceId] = peer;

        if (changed && m_callback) {
            cbToCall = m_callback;
            for (const auto& kv : m_peersMap) activePeers.push_back(kv.second);
        }
    }
    if (cbToCall) {
        cbToCall(activePeers);
    }
}

} // namespace aerosync
