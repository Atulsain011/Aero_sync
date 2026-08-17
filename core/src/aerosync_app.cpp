#include "aerosync/aerosync_app.hpp"
#include <filesystem>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace aerosync {

static std::string generateRandomId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
    return ss.str();
}

AeroSyncApp::AeroSyncApp(const std::string& deviceId,
                         const std::string& deviceName,
                         DeviceType deviceType)
    : m_deviceId(deviceId), m_deviceName(deviceName), m_deviceType(deviceType) {
    m_discovery = std::make_unique<DiscoveryEngine>(m_deviceId, m_deviceName, m_deviceType);
    m_connection = std::make_unique<ConnectionManager>();
}

AeroSyncApp::~AeroSyncApp() {
    shutdown();
}

bool AeroSyncApp::initialize() {
    if (!m_connection->startServer()) return false;
    if (!m_discovery->start()) return false;
    return true;
}

void AeroSyncApp::shutdown() {
    if (m_discovery) m_discovery->stop();
    if (m_connection) m_connection->stopServer();
}

void AeroSyncApp::setPeerDiscoveredCallback(PeerDiscoveredCallback cb) {
    if (m_discovery) m_discovery->setPeerCallback(cb);
}

void AeroSyncApp::setIncomingConnectCallback(IncomingConnectCallback cb) {
    if (m_connection) m_connection->setIncomingConnectCallback(cb);
}

void AeroSyncApp::setIncomingTransferCallback(IncomingTransferCallback cb) {
    if (m_connection) m_connection->setIncomingTransferCallback(cb);
}

void AeroSyncApp::setIncomingTransferProgressCallback(TransferProgressCallback cb) {
    if (m_connection) m_connection->setTransferProgressCallback(cb);
}

void AeroSyncApp::setPairingStateChangedCallback(PairingStateChangedCallback cb) {
    if (m_connection) {
        m_connection->getPairingStateMachine().setStateChangedCallback(cb);
    }
}

void AeroSyncApp::setDownloadDirectory(const std::filesystem::path& dir) {
    if (m_connection) m_connection->setDownloadDirectory(dir);
}

std::filesystem::path AeroSyncApp::getDownloadDirectory() const {
    if (m_connection) return m_connection->getDownloadDirectory();
    return std::filesystem::current_path() / "Downloads";
}

PairingStateMachine& AeroSyncApp::getPairingStateMachine() {
    return m_connection->getPairingStateMachine();
}

std::vector<PeerInfo> AeroSyncApp::getPeers() const {
    if (m_discovery) return m_discovery->getDiscoveredPeers();
    return {};
}

bool AeroSyncApp::connectToPeer(const PeerInfo& targetPeer, const std::string& customPin) {
    if (!m_connection) return false;
    ConnectRequest req;
    req.senderId = m_deviceId;
    req.senderName = m_deviceName;
    req.platform = deviceTypeToString(m_deviceType);
    req.appVersion = "1.0.0";
    req.pairingPin = customPin.empty() ? PairingStateMachine::generateRandom6DigitPin() : customPin;
    req.sessionNonce = std::chrono::steady_clock::now().time_since_epoch().count();

    return m_connection->connectToPeer(targetPeer, req);
}

bool AeroSyncApp::sendFiles(const PeerInfo& targetPeer,
                            const std::vector<std::string>& filePaths,
                            TransferProgressCallback progressCb) {
    if (!m_connection) return false;

    TransferManifest manifest;
    manifest.batchId = generateRandomId();
    manifest.senderId = m_deviceId;
    manifest.senderName = m_deviceName;
    manifest.sessionToken = m_connection->getPairingStateMachine().getSessionToken();
    manifest.totalFiles = static_cast<uint32_t>(filePaths.size());
    manifest.chunkSize = LARGE_CHUNK_SIZE;
    manifest.streamCount = PARALLEL_STREAMS;

    uint64_t totalBytes = 0;
    std::vector<std::filesystem::path> localPaths;
    for (size_t i = 0; i < filePaths.size(); ++i) {
        std::filesystem::path p(filePaths[i]);
        uint64_t sz = std::filesystem::exists(p) ? std::filesystem::file_size(p) : 0;

        FileMetadata fm;
        fm.fileIndex = static_cast<uint32_t>(i);
        fm.relativePath = p.filename().string();
        fm.fileSize = sz;

        totalBytes += sz;
        manifest.files.push_back(fm);
        localPaths.push_back(p);
    }
    manifest.totalBytes = totalBytes;

    return m_connection->requestTransfer(targetPeer, manifest, localPaths, progressCb);
}

void AeroSyncApp::cancelTransfer() {
    if (m_connection) m_connection->cancelActiveTransfer();
}

} // namespace aerosync
