#include "aerosync/discovery_engine.hpp"
#include <iostream>
#include <sstream>
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
    #define AERO_LOG_I(...) __android_log_print(ANDROID_LOG_INFO, "AeroSyncDiscovery", __VA_ARGS__)
    #define AERO_LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "AeroSyncDiscovery", __VA_ARGS__)
#else
    #define AERO_LOG_I(...) printf(__VA_ARGS__); printf("\n")
    #define AERO_LOG_E(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
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
            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(sin->sin_addr), ipStr, INET_ADDRSTRLEN)) {
                ips.push_back(ipStr);
            }
        }
        freeifaddrs(ifaddr);
    }
#endif
    return ips;
}

static std::vector<std::string> getBroadcastAndGatewayTargets() {
    std::vector<std::string> targets;
    targets.push_back("255.255.255.255");
    targets.push_back(DISCOVERY_MULTICAST_IP); // Site-local Multicast

    // Common mobile hotspot & tethering subnet broadcasts and gateways
    targets.push_back("192.168.42.255");  // Android USB Tethering (RNDIS/NCM) broadcast
    targets.push_back("192.168.42.1");    // Android USB Tethering gateway
    targets.push_back("192.168.42.129");  // Android USB Tethering host IP
    targets.push_back("192.168.137.255"); // Windows Mobile Hotspot broadcast
    targets.push_back("192.168.137.1");   // Windows Mobile Hotspot gateway
    targets.push_back("192.168.43.255");  // Android default hotspot broadcast
    targets.push_back("192.168.43.1");    // Android default hotspot gateway
    targets.push_back("192.168.49.255");  // Wi-Fi Direct / Hotspot
    targets.push_back("192.168.49.1");
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

bool DiscoveryEngine::start() {
    if (m_running) return true;
    m_running = true;

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
        AERO_LOG_E("[DISCOVERY_ERROR] Failed to create UDP broadcast socket");
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
    localPeer.appVersion = "1.0.0";
    localPeer.port = m_listenPort;

    AERO_LOG_I("[DISCOVERY_STARTED] Beacon loop active for device %s (%s)", m_localDeviceName.c_str(), m_localDeviceId.c_str());

    while (m_running) {
        localPeer.lastSeenMs = getCurrentTimeMs();
        std::string jsonPacket = ProtocolSerializer::serializeDiscoveryBeacon(localPeer);

        // 1. Send UDP beacons across all active network adapters and gateways
        auto targets = getBroadcastAndGatewayTargets();
        {
            std::lock_guard<std::mutex> lock(m_targetsMutex);
            for (const auto& ct : m_customTargets) {
                targets.push_back(ct);
            }
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

        // 2. Send mDNS multicast fallback on 224.0.0.251:5353
        sendto(sendSock, jsonPacket.c_str(), static_cast<int>(jsonPacket.length()), 0,
               (sockaddr*)&mdnsAddr, sizeof(mdnsAddr));

        AERO_LOG_I("[DISCOVERY_BEACON_SENT] sent to %zu subnet/gateway targets", targets.size());

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
                    AERO_LOG_I("[PEER_REMOVED] %s (%s) timed out", kv.second.deviceName.c_str(), kv.first.c_str());
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

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    CLOSE_SOCKET(sendSock);
}

void DiscoveryEngine::listenLoop() {
    socket_t listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenSock == INVALID_SOCKET) {
        AERO_LOG_E("[DISCOVERY_ERROR] Failed to create UDP listen socket");
        return;
    }

    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#ifndef _WIN32
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse));
#endif
    int bcastEnable = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_BROADCAST, (const char*)&bcastEnable, sizeof(bcastEnable));

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
        AERO_LOG_E("[DISCOVERY_ERROR] Failed to bind discovery UDP port %d", DISCOVERY_UDP_PORT);
        CLOSE_SOCKET(listenSock);
        return;
    }

    AERO_LOG_I("[DISCOVERY_SOCKET_BOUND] Bound discovery UDP port %d successfully", DISCOVERY_UDP_PORT);

    // Join local multicast group for robust hotspot discovery
    ip_mreq mreq{};
    inet_pton(AF_INET, DISCOVERY_MULTICAST_IP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(listenSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));

    m_udpSocket = static_cast<int>(listenSock);
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

void DiscoveryEngine::parseBeacon(const std::string& data, const std::string& senderIp) {
    PeerInfo peer;
    if (!ProtocolSerializer::deserializeDiscoveryBeacon(data, senderIp, peer)) {
        return;
    }

    if (peer.deviceId.empty() || peer.deviceId == m_localDeviceId) {
        return; // Skip self
    }

    // Skip loopback addresses
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
            AERO_LOG_I("[PEER_FOUND] %s (%s) at %s:%d [%s]", peer.deviceName.c_str(), peer.deviceId.c_str(), peer.ipAddress.c_str(), peer.port, peer.platform.c_str());
        } else if (changed) {
            AERO_LOG_I("[PEER_UPDATED] %s (%s) at %s:%d", peer.deviceName.c_str(), peer.deviceId.c_str(), peer.ipAddress.c_str(), peer.port);
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
