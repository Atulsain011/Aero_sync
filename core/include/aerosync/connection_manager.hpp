#ifndef AEROSYNC_CONNECTION_MANAGER_HPP
#define AEROSYNC_CONNECTION_MANAGER_HPP

#include "types.hpp"
#include "protocol_serializer.hpp"
#include "pairing_state_machine.hpp"
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <condition_variable>
#include <filesystem>
#include <vector>

namespace aerosync {

using IncomingConnectCallback = std::function<void(const ConnectRequest& req, std::function<void(bool accept)> respondCb)>;
using IncomingTransferCallback = std::function<void(const TransferManifest& manifest, std::function<void(bool accept)> respondCb)>;

class ConnectionManager {
public:
    ConnectionManager(uint16_t port = CONTROL_TCP_PORT);
    ~ConnectionManager();

    bool startServer();
    void stopServer();

    void setIncomingConnectCallback(IncomingConnectCallback cb);
    void setIncomingTransferCallback(IncomingTransferCallback cb);
    void setTransferProgressCallback(TransferProgressCallback cb);
    void setDownloadDirectory(const std::filesystem::path& dir);
    std::filesystem::path getDownloadDirectory() const;
    
    // Pairing & Control API
    bool connectToPeer(const PeerInfo& targetPeer, const ConnectRequest& localReq);
    
    // File Transfer Negotiation
    bool requestTransfer(const PeerInfo& targetPeer,
                         const TransferManifest& manifest,
                         const std::vector<std::filesystem::path>& localFilePaths,
                         TransferProgressCallback progressCb);

    void cancelActiveTransfer();

    PairingStateMachine& getPairingStateMachine();

private:
    void serverLoop();
    void handleClientConnection(int clientSockFd);

    uint16_t m_serverPort;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    int m_serverSockFd{-1};

    IncomingConnectCallback m_connectCb;
    IncomingTransferCallback m_incomingCb;
    TransferProgressCallback m_progressCb;
    std::filesystem::path m_downloadDirectory;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<int> m_activeTransferSock{-1};

    PairingStateMachine m_pairingStateMachine;
};

} // namespace aerosync

#endif // AEROSYNC_CONNECTION_MANAGER_HPP
