#ifndef AEROSYNC_DISCOVERY_ENGINE_HPP
#define AEROSYNC_DISCOVERY_ENGINE_HPP

#include "types.hpp"
#include "protocol_serializer.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace aerosync {

using PeerDiscoveredCallback = std::function<void(const std::vector<PeerInfo>&)>;

class DiscoveryEngine {
public:
    DiscoveryEngine(const std::string& localDeviceId,
                    const std::string& localDeviceName,
                    DeviceType localDeviceType,
                    uint16_t listenPort = CONTROL_TCP_PORT);
    ~DiscoveryEngine();

    bool start();
    void stop();

    void setPeerCallback(PeerDiscoveredCallback cb);
    std::vector<PeerInfo> getDiscoveredPeers() const;

private:
    void broadcastLoop();
    void listenLoop();
    void parseBeacon(const std::string& data, const std::string& senderIp);
    uint64_t getCurrentTimeMs() const;

    std::string m_localDeviceId;
    std::string m_localDeviceName;
    DeviceType m_localDeviceType;
    uint16_t m_listenPort;

    std::atomic<bool> m_running{false};
    std::thread m_broadcastThread;
    std::thread m_listenThread;

    mutable std::mutex m_peersMutex;
    std::unordered_map<std::string, PeerInfo> m_peersMap;
    PeerDiscoveredCallback m_callback;

    std::atomic<int> m_udpSocket{-1};
};

} // namespace aerosync

#endif // AEROSYNC_DISCOVERY_ENGINE_HPP
