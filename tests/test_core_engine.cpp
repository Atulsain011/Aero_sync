#include "aerosync/aerosync_app.hpp"
#include "aerosync/socket_transport.hpp"
#include "aerosync/pairing_state_machine.hpp"
#include "aerosync/protocol_serializer.hpp"
#include "aerosync/transfer_engine.hpp"
#include "aerosync/discovery_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <filesystem>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCK(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCK(s) close(s)
#endif

namespace fs = std::filesystem;

static bool createLoopbackSocketPair(int& outServerSock, int& outClientSock) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock < 0) return false;

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = 0;

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        CLOSE_SOCK(listenSock);
        return false;
    }

    socklen_t addrLen = sizeof(serverAddr);
    if (getsockname(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), &addrLen) < 0) {
        CLOSE_SOCK(listenSock);
        return false;
    }

    if (listen(listenSock, 1) < 0) {
        CLOSE_SOCK(listenSock);
        return false;
    }

    uint16_t port = ntohs(serverAddr.sin_port);

    int clientSock = -1;
    std::thread connector([&]() {
        clientSock = aerosync::SocketTransport::connectTcpClient("127.0.0.1", port, 3000);
    });

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int acceptedSock = static_cast<int>(accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen));
    connector.join();

    CLOSE_SOCK(listenSock);

    if (acceptedSock < 0 || clientSock < 0) {
        if (acceptedSock >= 0) CLOSE_SOCK(acceptedSock);
        if (clientSock >= 0) CLOSE_SOCK(clientSock);
        return false;
    }

    aerosync::SocketTransport::configureHighThroughputSocket(acceptedSock);
    aerosync::SocketTransport::configureHighThroughputSocket(clientSock);

    outServerSock = acceptedSock;
    outClientSock = clientSock;
    return true;
}

static void createTestFile(const fs::path& p, size_t size, uint8_t seed = 0x42) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    std::vector<uint8_t> buf(std::min<size_t>(size, 64 * 1024));
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = static_cast<uint8_t>((seed + i * 31) & 0xFF);
    }
    size_t written = 0;
    while (written < size) {
        size_t toWrite = std::min(buf.size(), size - written);
        f.write(reinterpret_cast<const char*>(buf.data()), toWrite);
        written += toWrite;
    }
}

static uint32_t computeFileCRC32C(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open()) return 0;
    std::vector<uint8_t> full((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return aerosync::ProtocolSerializer::computeCRC32C(full.data(), full.size());
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "AEROSYNC SHARED C++ CORE UNIT TEST SUITE" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Test Fast Castagnoli CRC32C Calculation
    std::string sample = "AeroSync High Speed P2P Transfer Engine (400+ Mbps)";
    uint32_t crc1 = aerosync::ProtocolSerializer::computeCRC32C(
        reinterpret_cast<const uint8_t*>(sample.data()), sample.length());
    uint32_t crc2 = aerosync::ProtocolSerializer::computeCRC32C(
        reinterpret_cast<const uint8_t*>(sample.data()), sample.length());

    assert(crc1 == crc2);
    assert(crc1 != 0);
    std::cout << "[PASS 1/5] CRC32C Checksum Calculation (CRC = 0x" << std::hex << crc1 << std::dec << ")" << std::endl;

    // 2. Test Pairing State Machine Transitions & 6-Digit PIN
    aerosync::PairingStateMachine psm;
    assert(psm.getCurrentState() == aerosync::PairingState::UNPAIRED);

    std::string testPin = "749201";
    assert(psm.initiatePairing("target-device-1", testPin));
    assert(psm.getCurrentState() == aerosync::PairingState::PAIRING_REQUESTED);
    assert(psm.getActivePin() == testPin);

    // Responder side verification
    aerosync::PairingStateMachine responderPsm;
    assert(responderPsm.handleIncomingRequest("sender-device-1", testPin));
    assert(responderPsm.getCurrentState() == aerosync::PairingState::AWAITING_PIN_CONFIRMATION);

    // Incorrect PIN -> DISCONNECTED
    aerosync::PairingStateMachine mismatchPsm;
    mismatchPsm.handleIncomingRequest("sender-device-1", testPin);
    assert(!mismatchPsm.confirmPinMatch("000000"));
    assert(mismatchPsm.getCurrentState() == aerosync::PairingState::DISCONNECTED);

    // Correct PIN -> AUTHENTICATED_SESSION
    assert(responderPsm.confirmPinMatch(testPin));
    assert(responderPsm.getCurrentState() == aerosync::PairingState::AUTHENTICATED_SESSION);
    assert(responderPsm.isSessionAuthenticated());
    assert(!responderPsm.getSessionToken().empty());

    // Initiator receives acceptance
    assert(psm.onPairingResponseReceived(aerosync::PairingStatus::PAIRING_ACCEPTED, responderPsm.getSessionToken()));
    assert(psm.getCurrentState() == aerosync::PairingState::AUTHENTICATED_SESSION);

    // Transfer State Transitions
    assert(psm.startTransfer());
    assert(psm.getCurrentState() == aerosync::PairingState::TRANSFERRING);
    assert(psm.finishTransfer(true));
    assert(psm.getCurrentState() == aerosync::PairingState::AUTHENTICATED_SESSION);

    psm.reset();
    assert(psm.getCurrentState() == aerosync::PairingState::UNPAIRED);
    std::cout << "[PASS 2/5] Pairing State Machine & 6-Digit PIN Authentication" << std::endl;

    // 3. Test Protocol Serialization & Binary Framing
    aerosync::ConnectRequest req;
    req.senderId = "dev-pixel8";
    req.senderName = "Pixel 8 Pro";
    req.platform = "android";
    req.appVersion = "1.0.0";
    req.pairingPin = "839201";
    req.sessionNonce = 123456789ULL;

    std::string serializedReq = aerosync::ProtocolSerializer::serializePairingRequest(req);
    aerosync::ConnectRequest deserializedReq;
    assert(aerosync::ProtocolSerializer::deserializePairingRequest(serializedReq, deserializedReq));
    assert(deserializedReq.senderId == req.senderId);
    assert(deserializedReq.pairingPin == req.pairingPin);

    // Control frame envelope
    std::string ctrlFrame = aerosync::ProtocolSerializer::encodeControlFrame(
        aerosync::ControlMessageType::PAIRING_REQUEST, 42, serializedReq);
    aerosync::ControlMessageType outType;
    uint32_t outSeq = 0;
    std::string outPayload;
    assert(aerosync::ProtocolSerializer::decodeControlFrame(ctrlFrame, outType, outSeq, outPayload));
    assert(outType == aerosync::ControlMessageType::PAIRING_REQUEST);
    assert(outSeq == 42);
    assert(outPayload == serializedReq);
    std::cout << "[PASS 3/5] Binary Wire Framing & Pairing Message Codec" << std::endl;

    // 4. Test Chunk Framing Envelope & Castagnoli CRC32C Integrity
    aerosync::ChunkHeader chunkHdr;
    chunkHdr.batchId = "batch-999";
    chunkHdr.fileIndex = 1;
    chunkHdr.chunkIndex = 5;
    chunkHdr.offset = 20 * 1024 * 1024;
    chunkHdr.length = 4 * 1024 * 1024;
    
    std::vector<uint8_t> dummyChunk(chunkHdr.length, 0x5E);
    chunkHdr.crc32c = aerosync::ProtocolSerializer::computeCRC32C(dummyChunk.data(), dummyChunk.size());

    auto envelope = aerosync::ProtocolSerializer::serializeChunkEnvelope(chunkHdr, dummyChunk.data(), dummyChunk.size());
    assert(envelope.size() == 24 + chunkHdr.length);

    aerosync::ChunkHeader decHdr;
    const uint8_t* decDataPtr = nullptr;
    assert(aerosync::ProtocolSerializer::deserializeChunkEnvelope(envelope.data(), envelope.size(), decHdr, decDataPtr));
    assert(decHdr.fileIndex == chunkHdr.fileIndex);
    assert(decHdr.chunkIndex == chunkHdr.chunkIndex);
    assert(decHdr.crc32c == chunkHdr.crc32c);
    assert(decDataPtr != nullptr);
    assert(decDataPtr[0] == 0x5E);
    std::cout << "[PASS 4/5] 4MB Chunk Envelope Framing & CRC32C Integrity" << std::endl;

    // 5. Test Beacon Device Name Parsing & Fallback Resilience
    aerosync::PeerInfo testPeer;
    testPeer.deviceId = "dev-pixel8";
    testPeer.deviceName = "Pixel 8 Pro";
    testPeer.deviceType = aerosync::DeviceType::DEVICE_ANDROID;
    testPeer.appVersion = "1.0.0";
    testPeer.port = 48124;

    std::string serializedBeacon = aerosync::ProtocolSerializer::serializeDiscoveryBeacon(testPeer);
    aerosync::PeerInfo parsedPeer;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(serializedBeacon, "192.168.1.10", parsedPeer));
    assert(parsedPeer.deviceId == "dev-pixel8");
    assert(parsedPeer.deviceName == "Pixel 8 Pro");

    // Test camelCase JSON beacon input
    std::string camelBeacon = "{\"deviceName\":\"Galaxy S23\",\"deviceId\":\"dev-s23\",\"platform\":\"android\",\"listeningPort\":48124}";
    aerosync::PeerInfo parsedCamel;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(camelBeacon, "192.168.1.20", parsedCamel));
    assert(parsedCamel.deviceId == "dev-s23");
    assert(parsedCamel.deviceName == "Galaxy S23");

    // Test empty device name fallback to platform & IP
    std::string emptyNameBeacon = "{\"device_name\":\"\",\"device_id\":\"dev-anon\",\"platform\":\"windows\",\"listening_port\":48124}";
    aerosync::PeerInfo parsedEmpty;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(emptyNameBeacon, "192.168.1.30", parsedEmpty));
    assert(parsedEmpty.deviceId == "dev-anon");
    assert(parsedEmpty.deviceName == "Windows (192.168.1.30)");

    // 6. Test Cross-Platform Protocol Discovery Matrix (Linux <-> Windows <-> Android)
    aerosync::PeerInfo linuxPeer;
    linuxPeer.deviceId = "linux-workstation-88a1";
    linuxPeer.deviceName = "Ubuntu Laptop (Linux)";
    linuxPeer.deviceType = aerosync::DeviceType::DEVICE_LINUX;
    linuxPeer.platform = "linux";
    linuxPeer.appVersion = "1.0.8";
    linuxPeer.port = 48124;

    aerosync::PeerInfo winPeer;
    winPeer.deviceId = "win-desktop-77b2";
    winPeer.deviceName = "Gaming Rig (Windows PC)";
    winPeer.deviceType = aerosync::DeviceType::DEVICE_WINDOWS;
    winPeer.platform = "windows";
    winPeer.appVersion = "1.0.8";
    winPeer.port = 48124;

    aerosync::PeerInfo androidPeer;
    androidPeer.deviceId = "android-pixel-99c3";
    androidPeer.deviceName = "Pixel 8 Pro";
    androidPeer.deviceType = aerosync::DeviceType::DEVICE_ANDROID;
    androidPeer.platform = "android";
    androidPeer.appVersion = "1.0.8";
    androidPeer.port = 48124;

    // Test 1: Linux -> Windows deserialization
    std::string linuxBeacon = aerosync::ProtocolSerializer::serializeDiscoveryBeacon(linuxPeer);
    aerosync::PeerInfo winParsedLinux;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(linuxBeacon, "192.168.43.150", winParsedLinux));
    assert(winParsedLinux.deviceId == linuxPeer.deviceId);
    assert(winParsedLinux.deviceType == aerosync::DeviceType::DEVICE_LINUX);
    assert(winParsedLinux.platform == "linux");
    assert(winParsedLinux.port == 48124);

    // Test 2: Linux -> Android deserialization
    aerosync::PeerInfo androidParsedLinux;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(linuxBeacon, "192.168.43.150", androidParsedLinux));
    assert(androidParsedLinux.deviceId == linuxPeer.deviceId);
    assert(androidParsedLinux.platform == "linux");

    // Test 3: Windows -> Linux deserialization
    std::string winBeacon = aerosync::ProtocolSerializer::serializeDiscoveryBeacon(winPeer);
    aerosync::PeerInfo linuxParsedWin;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(winBeacon, "192.168.43.100", linuxParsedWin));
    assert(linuxParsedWin.deviceId == winPeer.deviceId);
    assert(linuxParsedWin.deviceType == aerosync::DeviceType::DEVICE_WINDOWS);
    assert(linuxParsedWin.platform == "windows");
    assert(linuxParsedWin.port == 48124);

    // Test 4: Android -> Linux deserialization
    std::string androidBeacon = aerosync::ProtocolSerializer::serializeDiscoveryBeacon(androidPeer);
    aerosync::PeerInfo linuxParsedAndroid;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(androidBeacon, "192.168.43.1", linuxParsedAndroid));
    assert(linuxParsedAndroid.deviceId == androidPeer.deviceId);
    assert(linuxParsedAndroid.deviceType == aerosync::DeviceType::DEVICE_ANDROID);
    assert(linuxParsedAndroid.platform == "android");
    assert(linuxParsedAndroid.port == 48124);

    // Test 5: Windows <-> Android deserialization
    aerosync::PeerInfo winParsedAndroid;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(androidBeacon, "192.168.43.1", winParsedAndroid));
    assert(winParsedAndroid.deviceId == androidPeer.deviceId);

    aerosync::PeerInfo androidParsedWin;
    assert(aerosync::ProtocolSerializer::deserializeDiscoveryBeacon(winBeacon, "192.168.43.100", androidParsedWin));
    assert(androidParsedWin.deviceId == winPeer.deviceId);

    // 7. Test UDP Discovery Engine Mutual Discovery & Instant Response
    aerosync::DiscoveryEngine peerA("dev-A", "Phone-A", aerosync::DeviceType::DEVICE_ANDROID, 48124);
    aerosync::DiscoveryEngine peerB("dev-B", "PC-B", aerosync::DeviceType::DEVICE_WINDOWS, 48124);

    bool peerADiscoveredB = false;
    bool peerBDiscoveredA = false;
    peerA.setPeerCallback([&](const std::vector<aerosync::PeerInfo>& peers) {
        for (const auto& p : peers) {
            if (p.deviceId == "dev-B") peerADiscoveredB = true;
        }
    });
    peerB.setPeerCallback([&](const std::vector<aerosync::PeerInfo>& peers) {
        for (const auto& p : peers) {
            if (p.deviceId == "dev-A") peerBDiscoveredA = true;
        }
    });

    peerA.start();
    peerB.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    peerA.stop();
    peerB.stop();

    assert(peerADiscoveredB);
    assert(peerBDiscoveredA);
    std::cout << "[PASS 5/12] UDP / mDNS Discovery Engine & Cross-Platform Protocol Compatibility" << std::endl;

    // -------------------------------------------------------------------------
    // END-TO-END VERIFIED FILE TRANSFER SUITE
    // (Discovery -> Pairing -> Offer -> Approval -> TCP Stream -> Progress -> Integrity)
    // -------------------------------------------------------------------------
    std::cout << "\n--- END-TO-END FILE TRANSFER SUITE ---" << std::endl;

    fs::path tempRoot = fs::current_path() / "test_scratch_transfer";
    std::error_code ecR;
    fs::remove_all(tempRoot, ecR);
    fs::create_directories(tempRoot / "sender", ecR);
    fs::create_directories(tempRoot / "receiver", ecR);

    fs::path senderDir = tempRoot / "sender";
    fs::path receiverDir = tempRoot / "receiver";

    // Helper lambda to run loopback transfer
    auto runLoopbackTransfer = [&](const aerosync::TransferManifest& manifest,
                                   const std::vector<fs::path>& localPaths,
                                   const fs::path& destDir,
                                   std::vector<aerosync::TransferProgress>& senderProgs,
                                   std::vector<aerosync::TransferProgress>& receiverProgs,
                                   std::atomic<bool>& cancelSend,
                                   std::atomic<bool>& cancelRecv,
                                   uint64_t resumeByteOffset = 0,
                                   std::function<void(const aerosync::TransferProgress&)> customRxCb = nullptr) -> bool {
        int serverSock = -1;
        int clientSock = -1;
        if (!createLoopbackSocketPair(serverSock, clientSock)) {
            return false;
        }

        std::mutex mtx;
        auto rxCb = [&](const aerosync::TransferProgress& prog) {
            std::lock_guard<std::mutex> lk(mtx);
            receiverProgs.push_back(prog);
            if (customRxCb) customRxCb(prog);
        };

        auto txCb = [&](const aerosync::TransferProgress& prog) {
            std::lock_guard<std::mutex> lk(mtx);
            senderProgs.push_back(prog);
        };

        bool rxOk = false;
        std::thread rxThread([&]() {
            rxOk = aerosync::TransferEngine::receiveFileBatch(serverSock, manifest, destDir, rxCb, cancelRecv, resumeByteOffset);
        });

        bool txOk = aerosync::TransferEngine::sendFileBatch(clientSock, manifest, localPaths, txCb, cancelSend, resumeByteOffset);

        if (rxThread.joinable()) {
            rxThread.join();
        }

        CLOSE_SOCK(serverSock);
        CLOSE_SOCK(clientSock);

        return (txOk && rxOk);
    };

    // 8. TEST: Small File Transfer (1 KB) & Castagnoli CRC32C Verification
    {
        fs::path smallSrc = senderDir / "small.txt";
        createTestFile(smallSrc, 1024, 0x11);
        uint32_t srcCrc = computeFileCRC32C(smallSrc);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-small-1";
        manifest.totalFiles = 1;
        manifest.totalBytes = 1024;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;

        aerosync::FileMetadata fm;
        fm.fileIndex = 0;
        fm.relativePath = "small.txt";
        fm.fileSize = 1024;
        manifest.files.push_back(fm);

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        bool ok = runLoopbackTransfer(manifest, {smallSrc}, receiverDir, txP, rxP, cSend, cRecv);
        assert(ok);

        fs::path smallDst = receiverDir / "small.txt";
        assert(fs::exists(smallDst));
        assert(fs::file_size(smallDst) == 1024);
        uint32_t dstCrc = computeFileCRC32C(smallDst);
        assert(dstCrc == srcCrc);
        assert(!rxP.empty());
        assert(rxP.back().state == aerosync::TransferState::COMPLETED);
        assert(rxP.back().fileBytesTransferred == 1024);
        std::cout << "[PASS 6/12] Small File (1 KB) Transfer & Bit-for-Bit Castagnoli CRC32C" << std::endl;
    }

    // 9. TEST: Large File Multi-Chunk Transfer (8 MB) & Wire Saturation
    {
        fs::path largeSrc = senderDir / "large_data.bin";
        const size_t LARGE_SIZE = 8 * 1024 * 1024;
        createTestFile(largeSrc, LARGE_SIZE, 0x77);
        uint32_t srcCrc = computeFileCRC32C(largeSrc);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-large-2";
        manifest.totalFiles = 1;
        manifest.totalBytes = LARGE_SIZE;
        manifest.chunkSize = 1024 * 1024; // 1 MB chunks -> 8 chunks
        manifest.streamCount = 1;

        aerosync::FileMetadata fm;
        fm.fileIndex = 0;
        fm.relativePath = "large_data.bin";
        fm.fileSize = LARGE_SIZE;
        manifest.files.push_back(fm);

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        bool ok = runLoopbackTransfer(manifest, {largeSrc}, receiverDir, txP, rxP, cSend, cRecv);
        assert(ok);

        fs::path largeDst = receiverDir / "large_data.bin";
        assert(fs::exists(largeDst));
        assert(fs::file_size(largeDst) == LARGE_SIZE);
        uint32_t dstCrc = computeFileCRC32C(largeDst);
        assert(dstCrc == srcCrc);

        // Verify actual non-fake progress was emitted across chunks
        assert(txP.size() >= 3);
        uint64_t prevBytes = 0;
        for (const auto& p : txP) {
            assert(p.batchBytesTransferred >= prevBytes);
            prevBytes = p.batchBytesTransferred;
        }
        assert(rxP.back().state == aerosync::TransferState::COMPLETED);
        assert(rxP.back().batchBytesTransferred == LARGE_SIZE);
        assert(rxP.back().speedBytesPerSec > 0.0);
        std::cout << "[PASS 7/12] Large File (8 MB, 8 Chunks) Transfer & Real-time Progress" << std::endl;
    }

    // 10. TEST: Multi-File Batch Transfer (3 Distinct Files)
    {
        fs::path f1 = senderDir / "doc.pdf";
        fs::path f2 = senderDir / "photo.raw";
        fs::path f3 = senderDir / "video_clip.mp4";

        createTestFile(f1, 64 * 1024, 0xAA);
        createTestFile(f2, 256 * 1024, 0xBB);
        createTestFile(f3, 1536 * 1024, 0xCC);

        uint32_t c1 = computeFileCRC32C(f1);
        uint32_t c2 = computeFileCRC32C(f2);
        uint32_t c3 = computeFileCRC32C(f3);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-multi-3";
        manifest.totalFiles = 3;
        manifest.totalBytes = (64 + 256 + 1536) * 1024;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;

        manifest.files.push_back({0, "doc.pdf", 64 * 1024});
        manifest.files.push_back({1, "photo.raw", 256 * 1024});
        manifest.files.push_back({2, "video_clip.mp4", 1536 * 1024});

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        bool ok = runLoopbackTransfer(manifest, {f1, f2, f3}, receiverDir, txP, rxP, cSend, cRecv);
        assert(ok);

        assert(fs::exists(receiverDir / "doc.pdf") && computeFileCRC32C(receiverDir / "doc.pdf") == c1);
        assert(fs::exists(receiverDir / "photo.raw") && computeFileCRC32C(receiverDir / "photo.raw") == c2);
        assert(fs::exists(receiverDir / "video_clip.mp4") && computeFileCRC32C(receiverDir / "video_clip.mp4") == c3);
        assert(rxP.back().batchBytesTransferred == manifest.totalBytes);
        std::cout << "[PASS 8/12] Multi-File Batch (3 Files) Transfer & Cumulative Progress" << std::endl;
    }

    // 11. TEST: Recursive Folder Transfer with Nested Subdirectories
    {
        fs::path folderRoot = senderDir / "my_project";
        fs::create_directories(folderRoot / "src" / "include");
        fs::create_directories(folderRoot / "assets");

        fs::path n1 = folderRoot / "README.md";
        fs::path n2 = folderRoot / "src" / "main.cpp";
        fs::path n3 = folderRoot / "src" / "include" / "types.hpp";
        fs::path n4 = folderRoot / "assets" / "icon.png";

        createTestFile(n1, 512, 0x12);
        createTestFile(n2, 2048, 0x34);
        createTestFile(n3, 1024, 0x56);
        createTestFile(n4, 8192, 0x78);

        uint32_t cn1 = computeFileCRC32C(n1);
        uint32_t cn2 = computeFileCRC32C(n2);
        uint32_t cn3 = computeFileCRC32C(n3);
        uint32_t cn4 = computeFileCRC32C(n4);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-folder-4";
        manifest.totalFiles = 4;
        manifest.totalBytes = 512 + 2048 + 1024 + 8192;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;

        manifest.files.push_back({0, "my_project/README.md", 512});
        manifest.files.push_back({1, "my_project/src/main.cpp", 2048});
        manifest.files.push_back({2, "my_project/src/include/types.hpp", 1024});
        manifest.files.push_back({3, "my_project/assets/icon.png", 8192});

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        bool ok = runLoopbackTransfer(manifest, {n1, n2, n3, n4}, receiverDir, txP, rxP, cSend, cRecv);
        assert(ok);

        fs::path r1 = receiverDir / "my_project" / "README.md";
        fs::path r2 = receiverDir / "my_project" / "src" / "main.cpp";
        fs::path r3 = receiverDir / "my_project" / "src" / "include" / "types.hpp";
        fs::path r4 = receiverDir / "my_project" / "assets" / "icon.png";

        assert(fs::exists(r1) && computeFileCRC32C(r1) == cn1);
        assert(fs::exists(r2) && computeFileCRC32C(r2) == cn2);
        assert(fs::exists(r3) && computeFileCRC32C(r3) == cn3);
        assert(fs::exists(r4) && computeFileCRC32C(r4) == cn4);
        std::cout << "[PASS 9/12] Recursive Folder Transfer & Relative Hierarchy Preservation" << std::endl;
    }

    // 12. TEST: Mid-Stream Transfer Cancellation & Clean Teardown
    {
        fs::path cancelSrc = senderDir / "cancel_test.bin";
        const size_t CANCEL_SIZE = 10 * 1024 * 1024;
        createTestFile(cancelSrc, CANCEL_SIZE, 0x99);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-cancel-5";
        manifest.totalFiles = 1;
        manifest.totalBytes = CANCEL_SIZE;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;
        manifest.files.push_back({0, "cancel_test.bin", CANCEL_SIZE});

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        auto cancelTriggerCb = [&](const aerosync::TransferProgress& prog) {
            if (prog.fileBytesTransferred >= 1024 * 1024) {
                cSend = true;
                cRecv = true;
            }
        };

        bool ok = runLoopbackTransfer(manifest, {cancelSrc}, receiverDir, txP, rxP, cSend, cRecv, 0, cancelTriggerCb);
        assert(!ok); // Must fail because cancelled
        assert(!rxP.empty());
        assert(rxP.back().state == aerosync::TransferState::CANCELLED);
        std::cout << "[PASS 10/12] Mid-Stream Transfer Cancellation & Resource Teardown" << std::endl;
    }

    // 13. TEST: Interrupted Transfer Resumption from Journal & Byte Offset
    {
        fs::path resumeSrc = senderDir / "resumable_dataset.bin";
        const size_t RESUME_SIZE = 5 * 1024 * 1024; // 5 MB
        createTestFile(resumeSrc, RESUME_SIZE, 0x33);
        uint32_t srcCrc = computeFileCRC32C(resumeSrc);

        // Simulate pre-existing 2 MB partial download (.aerosync.part)
        fs::path partPath = receiverDir / "resumable_dataset.bin.aerosync.part";
        fs::path journalPath = receiverDir / "resumable_dataset.bin.aerosync.journal";

        const size_t PART_SIZE = 2 * 1024 * 1024; // 2 MB already received
        std::ifstream srcIn(resumeSrc, std::ios::binary);
        std::vector<char> partBuf(PART_SIZE);
        srcIn.read(partBuf.data(), PART_SIZE);
        srcIn.close();

        std::ofstream partOut(partPath, std::ios::binary);
        partOut.write(partBuf.data(), PART_SIZE);
        partOut.close();

        // Write chunk indices 0 and 1 into journal
        std::ofstream jOut(journalPath, std::ios::binary);
        uint32_t c0 = 0, c1 = 1;
        jOut.write(reinterpret_cast<const char*>(&c0), sizeof(c0));
        jOut.write(reinterpret_cast<const char*>(&c1), sizeof(c1));
        jOut.close();

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-resume-6";
        manifest.totalFiles = 1;
        manifest.totalBytes = RESUME_SIZE;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;
        manifest.files.push_back({0, "resumable_dataset.bin", RESUME_SIZE});

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        // Resume from 2 MB offset!
        bool ok = runLoopbackTransfer(manifest, {resumeSrc}, receiverDir, txP, rxP, cSend, cRecv, PART_SIZE);
        assert(ok);

        fs::path finalResumed = receiverDir / "resumable_dataset.bin";
        assert(fs::exists(finalResumed));
        assert(fs::file_size(finalResumed) == RESUME_SIZE);
        assert(computeFileCRC32C(finalResumed) == srcCrc);
        assert(!fs::exists(journalPath));
        std::cout << "[PASS 11/12] Interrupted Transfer Resume from Journal & Exact Byte Offset" << std::endl;
    }

    // 14. TEST: Duplicate Filename Conflict Handling (Non-Destructive)
    {
        fs::path originalFile = receiverDir / "report.pdf";
        createTestFile(originalFile, 1024, 0x11);
        uint32_t origCrc = computeFileCRC32C(originalFile);

        fs::path newFileSrc = senderDir / "report.pdf";
        createTestFile(newFileSrc, 2048, 0x99);
        uint32_t newCrc = computeFileCRC32C(newFileSrc);

        aerosync::TransferManifest manifest;
        manifest.batchId = "batch-dup-7";
        manifest.totalFiles = 1;
        manifest.totalBytes = 2048;
        manifest.chunkSize = 1024 * 1024;
        manifest.streamCount = 1;
        manifest.files.push_back({0, "report.pdf", 2048});

        std::vector<aerosync::TransferProgress> txP, rxP;
        std::atomic<bool> cSend{false}, cRecv{false};

        bool ok = runLoopbackTransfer(manifest, {newFileSrc}, receiverDir, txP, rxP, cSend, cRecv);
        assert(ok);

        // Verify original file is PRESERVED and NOT overwritten
        assert(fs::exists(originalFile));
        assert(fs::file_size(originalFile) == 1024);
        assert(computeFileCRC32C(originalFile) == origCrc);

        // Verify non-colliding duplicate file was created
        fs::path dupFile = receiverDir / "report (1).pdf";
        assert(fs::exists(dupFile));
        assert(fs::file_size(dupFile) == 2048);
        assert(computeFileCRC32C(dupFile) == newCrc);
        std::cout << "[PASS 12/12] Duplicate Filename Conflict Resolution (Non-Destructive rename)" << std::endl;
    }

    // Cleanup scratch directory
    fs::remove_all(tempRoot, ecR);

    std::cout << "==================================================" << std::endl;
    std::cout << "ALL SHARED C++ CORE TESTS PASSED (100%)!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
