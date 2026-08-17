#include "aerosync/connection_manager.hpp"
#include "aerosync/socket_transport.hpp"
#include "aerosync/transfer_engine.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace aerosync {

struct ConsentState {
    std::mutex mtx;
    std::condition_variable cv;
    bool responded{false};
    bool accepted{false};
};

ConnectionManager::ConnectionManager(uint16_t port) : m_serverPort(port) {}

ConnectionManager::~ConnectionManager() {
    stopServer();
}

PairingStateMachine& ConnectionManager::getPairingStateMachine() {
    return m_pairingStateMachine;
}

bool ConnectionManager::startServer() {
    if (m_running) return true;
    m_running = true;

    m_serverSockFd = SocketTransport::createTcpServer(m_serverPort);
    if (m_serverSockFd < 0) {
        m_running = false;
        return false;
    }

    m_serverThread = std::thread(&ConnectionManager::serverLoop, this);
    return true;
}

void ConnectionManager::stopServer() {
    if (!m_running) return;
    m_running = false;

    if (m_serverSockFd >= 0) {
        CLOSE_SOCKET(m_serverSockFd);
        m_serverSockFd = -1;
    }

    if (m_serverThread.joinable()) m_serverThread.join();
}

void ConnectionManager::setIncomingConnectCallback(IncomingConnectCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectCb = cb;
}

void ConnectionManager::setIncomingTransferCallback(IncomingTransferCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_incomingCb = cb;
}

void ConnectionManager::setTransferProgressCallback(TransferProgressCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCb = cb;
}

void ConnectionManager::setDownloadDirectory(const std::filesystem::path& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_downloadDirectory = dir;
}

std::filesystem::path ConnectionManager::getDownloadDirectory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_downloadDirectory.empty()) {
        return m_downloadDirectory;
    }
    return std::filesystem::current_path() / "Downloads";
}

void ConnectionManager::cancelActiveTransfer() {
    m_cancelRequested = true;
    int sock = m_activeTransferSock.exchange(-1);
    if (sock >= 0) {
#ifdef _WIN32
        shutdown(static_cast<SOCKET>(sock), SD_BOTH);
        closesocket(static_cast<SOCKET>(sock));
#else
        shutdown(sock, SHUT_RDWR);
        close(sock);
#endif
    }
}

void ConnectionManager::serverLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int len = sizeof(clientAddr);
#else
        socklen_t len = sizeof(clientAddr);
#endif
        socket_t clientSock = accept(m_serverSockFd, (sockaddr*)&clientAddr, &len);
        if (clientSock == INVALID_SOCKET) {
            if (!m_running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        SocketTransport::configureHighThroughputSocket(static_cast<int>(clientSock));
        std::thread(&ConnectionManager::handleClientConnection, this, static_cast<int>(clientSock)).detach();
    }
}

void ConnectionManager::handleClientConnection(int clientSockFd) {
    ControlMessageType msgType = ControlMessageType::UNKNOWN;
    uint32_t seq = 0;
    std::string payload;

    // Read control frame
    if (!SocketTransport::recvControlFrame(clientSockFd, msgType, seq, payload)) {
        // Fallback for raw framing
        std::string rawFrame;
        if (!SocketTransport::recvFrame(clientSockFd, rawFrame)) {
            CLOSE_SOCKET(clientSockFd);
            return;
        }
        if (!rawFrame.empty()) {
            msgType = static_cast<ControlMessageType>(rawFrame[0]);
            payload = rawFrame.substr(1);
        }
    }

    // 1. Handle CONNECT_REQUEST (Type 0x01 or 0x04)
    if (msgType == ControlMessageType::CONNECT_REQUEST || msgType == ControlMessageType::PAIRING_REQUEST) {
        ConnectRequest req;
        if (!ProtocolSerializer::deserializePairingRequest(payload, req)) {
            CLOSE_SOCKET(clientSockFd);
            return;
        }

        // Populate sender IP from socket address
        sockaddr_in peerAddr{};
#ifdef _WIN32
        int peerAddrLen = sizeof(peerAddr);
#else
        socklen_t peerAddrLen = sizeof(peerAddr);
#endif
        if (getpeername(clientSockFd, (sockaddr*)&peerAddr, &peerAddrLen) == 0) {
            char clientIp[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &(peerAddr.sin_addr), clientIp, INET_ADDRSTRLEN)) {
                req.senderIp = clientIp;
            }
        }

        m_pairingStateMachine.handleIncomingRequest(req.senderId, req.pairingPin);

        IncomingConnectCallback cbCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cbCopy = m_connectCb;
        }

        bool accepted = false;
        if (cbCopy) {
            auto consent = std::make_shared<ConsentState>();

            cbCopy(req, [consent](bool accept) {
                std::lock_guard<std::mutex> lock(consent->mtx);
                consent->accepted = accept;
                consent->responded = true;
                consent->cv.notify_all();
            });

            std::unique_lock<std::mutex> lock(consent->mtx);
            if (!consent->cv.wait_for(lock, std::chrono::seconds(PAIRING_TIMEOUT_SEC), [consent] { return consent->responded; })) {
                accepted = false; // Timeout = Declined
                m_pairingStateMachine.declineIncomingRequest("Pairing confirmation timed out");
            } else {
                accepted = consent->accepted;
            }
        }

        if (accepted) {
            m_pairingStateMachine.confirmPinMatch(req.pairingPin);

            PairingResponseMsg resp;
            resp.status = PairingStatus::PAIRING_ACCEPTED;
            resp.sessionToken = m_pairingStateMachine.getSessionToken();
            resp.responderId = "local-device";
            resp.responderName = "Local Device";
            resp.maxStreams = PARALLEL_STREAMS;
            resp.chunkSize = LARGE_CHUNK_SIZE;
            resp.reason = "Pairing accepted";

            std::string respPayload = ProtocolSerializer::serializePairingResponse(resp);
            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::CONNECT_ACCEPT, seq + 1, respPayload);

            // Keep control socket open for subsequent FILE_OFFER transfer negotiation
            ControlMessageType nextType = ControlMessageType::UNKNOWN;
            uint32_t nextSeq = 0;
            std::string nextPayload;

            if (SocketTransport::recvControlFrame(clientSockFd, nextType, nextSeq, nextPayload) ||
                SocketTransport::recvFrame(clientSockFd, nextPayload)) {
                
                if (nextType == ControlMessageType::FILE_OFFER || (!nextPayload.empty() && nextPayload[0] == static_cast<char>(ControlMessageType::FILE_OFFER))) {
                    std::string offerPayload = (nextType == ControlMessageType::FILE_OFFER) ? nextPayload : nextPayload.substr(1);
                    TransferManifest manifest;
                    if (ProtocolSerializer::deserializeTransferManifest(offerPayload, manifest)) {
                        std::filesystem::path downloadDir;
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            downloadDir = !m_downloadDirectory.empty() ? m_downloadDirectory : (std::filesystem::current_path() / "Downloads");
                        }

                        uint64_t resumeOffset = 0;
                        uint32_t resumeChunkIdx = 0;
                        if (!manifest.files.empty()) {
                            std::filesystem::path partPath = downloadDir / (manifest.files[0].relativePath + ".aerosync.part");
                            std::filesystem::path journalPath = downloadDir / (manifest.files[0].relativePath + ".aerosync.journal");
                            if (std::filesystem::exists(partPath) && std::filesystem::exists(journalPath)) {
                                std::ifstream jFile(journalPath, std::ios::binary);
                                std::set<uint32_t> completed;
                                uint32_t idx;
                                while (jFile.read(reinterpret_cast<char*>(&idx), sizeof(idx))) {
                                    completed.insert(idx);
                                }
                                uint32_t exp = 0;
                                while (completed.count(exp) > 0) {
                                    exp++;
                                }
                                size_t cSize = manifest.chunkSize > 0 ? manifest.chunkSize : LARGE_CHUNK_SIZE;
                                uint64_t verified = static_cast<uint64_t>(exp) * cSize;
                                uint64_t partSize = std::filesystem::file_size(partPath);
                                resumeOffset = std::min(verified, partSize);
                                resumeChunkIdx = exp;
                            }
                        }

                        std::string acceptPayload = manifest.batchId + "|" + std::to_string(resumeChunkIdx) + "|" + std::to_string(resumeOffset);
                        SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ACCEPT, nextSeq + 1, acceptPayload);

                        m_cancelRequested = false;
                        m_activeTransferSock.store(clientSockFd);
                        m_pairingStateMachine.startTransfer();

                        TransferProgressCallback progCb;
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            progCb = m_progressCb;
                        }

                        bool ok = TransferEngine::receiveFileBatch(clientSockFd, manifest, downloadDir, progCb, m_cancelRequested, resumeOffset);

                        m_activeTransferSock.store(-1);
                        m_pairingStateMachine.finishTransfer(ok);

                        if (ok) {
                            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_COMPLETE, nextSeq + 2, manifest.batchId + "|OK");
                        } else {
                            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ERROR, nextSeq + 2, manifest.batchId + "|FAILED");
                        }
                    }
                }
            }
        } else {
            m_pairingStateMachine.declineIncomingRequest("User declined or pairing timed out");
            PairingResponseMsg resp;
            resp.status = PairingStatus::PAIRING_DECLINED;
            resp.reason = "User declined pairing or timed out";
            std::string respPayload = ProtocolSerializer::serializePairingResponse(resp);
            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::CONNECT_DECLINE, seq + 1, respPayload);
            CLOSE_SOCKET(clientSockFd);
        }
        return;
    }

    // 2. Handle FILE_OFFER directly if pre-paired
    if (msgType == ControlMessageType::FILE_OFFER) {
        TransferManifest manifest;
        if (!ProtocolSerializer::deserializeTransferManifest(payload, manifest)) {
            CLOSE_SOCKET(clientSockFd);
            return;
        }

        IncomingTransferCallback cbCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cbCopy = m_incomingCb;
        }

        if (!cbCopy) {
            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ERROR, seq + 1, "No incoming listener registered");
            CLOSE_SOCKET(clientSockFd);
            return;
        }

        auto consent = std::make_shared<ConsentState>();
        bool accepted = false;

        cbCopy(manifest, [consent](bool accept) {
            std::lock_guard<std::mutex> lock(consent->mtx);
            consent->accepted = accept;
            consent->responded = true;
            consent->cv.notify_all();
        });

        std::unique_lock<std::mutex> lock(consent->mtx);
        if (!consent->cv.wait_for(lock, std::chrono::seconds(PAIRING_TIMEOUT_SEC), [consent] { return consent->responded; })) {
            accepted = false;
        } else {
            accepted = consent->accepted;
        }

        if (accepted) {
            std::filesystem::path downloadDir;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                downloadDir = !m_downloadDirectory.empty() ? m_downloadDirectory : (std::filesystem::current_path() / "Downloads");
            }

            uint64_t resumeOffset = 0;
            uint32_t resumeChunkIdx = 0;
            if (!manifest.files.empty()) {
                std::filesystem::path partPath = downloadDir / (manifest.files[0].relativePath + ".aerosync.part");
                std::filesystem::path journalPath = downloadDir / (manifest.files[0].relativePath + ".aerosync.journal");
                if (std::filesystem::exists(partPath) && std::filesystem::exists(journalPath)) {
                    std::ifstream jFile(journalPath, std::ios::binary);
                    std::set<uint32_t> completed;
                    uint32_t idx;
                    while (jFile.read(reinterpret_cast<char*>(&idx), sizeof(idx))) {
                        completed.insert(idx);
                    }
                    uint32_t exp = 0;
                    while (completed.count(exp) > 0) {
                        exp++;
                    }
                    size_t cSize = manifest.chunkSize > 0 ? manifest.chunkSize : LARGE_CHUNK_SIZE;
                    uint64_t verified = static_cast<uint64_t>(exp) * cSize;
                    uint64_t partSize = std::filesystem::file_size(partPath);
                    resumeOffset = std::min(verified, partSize);
                    resumeChunkIdx = exp;
                }
            }

            std::string acceptPayload = manifest.batchId + "|" + std::to_string(resumeChunkIdx) + "|" + std::to_string(resumeOffset);
            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ACCEPT, seq + 1, acceptPayload);
            m_cancelRequested = false;
            m_activeTransferSock.store(clientSockFd);
            m_pairingStateMachine.startTransfer();

            TransferProgressCallback progCb;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                progCb = m_progressCb;
            }

            bool ok = TransferEngine::receiveFileBatch(clientSockFd, manifest, downloadDir, progCb, m_cancelRequested, resumeOffset);

            m_activeTransferSock.store(-1);
            m_pairingStateMachine.finishTransfer(ok);

            if (ok) {
                SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_COMPLETE, seq + 2, manifest.batchId + "|OK");
            } else {
                SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ERROR, seq + 2, manifest.batchId + "|FAILED");
            }
        } else {
            SocketTransport::sendControlFrame(clientSockFd, ControlMessageType::FILE_ERROR, seq + 1, "Transfer declined");
        }
    }

    CLOSE_SOCKET(clientSockFd);
}

bool ConnectionManager::connectToPeer(const PeerInfo& targetPeer, const ConnectRequest& localReq) {
    int clientSock = SocketTransport::connectTcpClient(targetPeer.ipAddress, targetPeer.port, 3000);
    if (clientSock < 0) return false;

    m_pairingStateMachine.initiatePairing(targetPeer.deviceId, localReq.pairingPin);

    std::string reqPayload = ProtocolSerializer::serializePairingRequest(localReq);
    if (!SocketTransport::sendControlFrame(clientSock, ControlMessageType::CONNECT_REQUEST, 1, reqPayload)) {
        CLOSE_SOCKET(clientSock);
        m_pairingStateMachine.disconnect("Failed to send connect request");
        return false;
    }

    ControlMessageType respType = ControlMessageType::UNKNOWN;
    uint32_t respSeq = 0;
    std::string respPayload;
    if (!SocketTransport::recvControlFrame(clientSock, respType, respSeq, respPayload)) {
        CLOSE_SOCKET(clientSock);
        m_pairingStateMachine.disconnect("Failed to receive response frame");
        return false;
    }

    PairingResponseMsg resp;
    if (!ProtocolSerializer::deserializePairingResponse(respPayload, resp)) {
        // Fallback for simple payload
        resp.status = (respType == ControlMessageType::CONNECT_ACCEPT) ? PairingStatus::PAIRING_ACCEPTED : PairingStatus::PAIRING_DECLINED;
        resp.sessionToken = respPayload;
    }

    bool success = m_pairingStateMachine.onPairingResponseReceived(resp.status, resp.sessionToken, resp.reason);
    CLOSE_SOCKET(clientSock);
    return success;
}

bool ConnectionManager::requestTransfer(const PeerInfo& targetPeer,
                                        const TransferManifest& manifest,
                                        const std::vector<std::filesystem::path>& localFilePaths,
                                        TransferProgressCallback progressCb) {
    int clientSock = SocketTransport::connectTcpClient(targetPeer.ipAddress, targetPeer.port);
    if (clientSock < 0) return false;

    std::string offerPayload = ProtocolSerializer::serializeTransferManifest(manifest);
    if (!SocketTransport::sendControlFrame(clientSock, ControlMessageType::FILE_OFFER, 1, offerPayload)) {
        CLOSE_SOCKET(clientSock);
        return false;
    }

    ControlMessageType respType = ControlMessageType::UNKNOWN;
    uint32_t respSeq = 0;
    std::string respPayload;
    if (!SocketTransport::recvControlFrame(clientSock, respType, respSeq, respPayload)) {
        CLOSE_SOCKET(clientSock);
        return false;
    }

    if (respType != ControlMessageType::FILE_ACCEPT) {
        CLOSE_SOCKET(clientSock);
        return false;
    }

    uint64_t resumeByteOffset = 0;
    size_t p1 = respPayload.find('|');
    if (p1 != std::string::npos) {
        size_t p2 = respPayload.find('|', p1 + 1);
        if (p2 != std::string::npos) {
            try {
                resumeByteOffset = std::stoull(respPayload.substr(p2 + 1));
            } catch (...) {
                resumeByteOffset = 0;
            }
        }
    }

    m_cancelRequested = false;
    m_activeTransferSock.store(clientSock);
    m_pairingStateMachine.startTransfer();

    bool result = TransferEngine::sendFileBatch(clientSock, manifest, localFilePaths, progressCb, m_cancelRequested, resumeByteOffset);

    m_activeTransferSock.store(-1);
    m_pairingStateMachine.finishTransfer(result);

    if (result) {
        ControlMessageType compType = ControlMessageType::UNKNOWN;
        uint32_t compSeq = 0;
        std::string compPayload;
        SocketTransport::recvControlFrame(clientSock, compType, compSeq, compPayload);
    }

    CLOSE_SOCKET(clientSock);
    return result;
}

} // namespace aerosync
