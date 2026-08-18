#ifndef AEROSYNC_APP_HPP
#define AEROSYNC_APP_HPP

#include "types.hpp"
#include "discovery_engine.hpp"
#include "connection_manager.hpp"
#include "pairing_state_machine.hpp"
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace aerosync {

class AeroSyncApp {
public:
    AeroSyncApp(const std::string& deviceId,
                const std::string& deviceName,
                DeviceType deviceType);
    ~AeroSyncApp();

    bool initialize();
    void shutdown();

    void setPeerDiscoveredCallback(PeerDiscoveredCallback cb);
    void setIncomingConnectCallback(IncomingConnectCallback cb);
    void setIncomingTransferCallback(IncomingTransferCallback cb);
    void setIncomingTransferProgressCallback(TransferProgressCallback cb);
    void setPairingStateChangedCallback(PairingStateChangedCallback cb);
    void setDownloadDirectory(const std::filesystem::path& dir);
    std::filesystem::path getDownloadDirectory() const;
    void addBroadcastTarget(const std::string& targetIp);

    std::vector<PeerInfo> getPeers() const;

    bool connectToPeer(const PeerInfo& targetPeer, const std::string& customPin = "");

    bool sendFiles(const PeerInfo& targetPeer,
                   const std::vector<std::string>& filePaths,
                   TransferProgressCallback progressCb);

    void cancelTransfer();
    void disconnect();

    PairingStateMachine& getPairingStateMachine();

private:
    std::string m_deviceId;
    std::string m_deviceName;
    DeviceType m_deviceType;

    std::unique_ptr<DiscoveryEngine> m_discovery;
    std::unique_ptr<ConnectionManager> m_connection;
};

} // namespace aerosync

#endif // AEROSYNC_APP_HPP
