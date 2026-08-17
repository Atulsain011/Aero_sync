#include "aerosync/aerosync_app.hpp"
#include "aerosync/socket_transport.hpp"
#include "aerosync/pairing_state_machine.hpp"
#include "aerosync/protocol_serializer.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

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

    // 5. Test UDP Broadcast/Multicast Peer Discovery Loopback
    aerosync::DiscoveryEngine peerA("dev-A", "Phone-A", aerosync::DeviceType::DEVICE_ANDROID, 48124);
    aerosync::DiscoveryEngine peerB("dev-B", "PC-B", aerosync::DeviceType::DEVICE_WINDOWS, 48124);

    bool peerADiscoveredB = false;
    peerA.setPeerCallback([&](const std::vector<aerosync::PeerInfo>& peers) {
        for (const auto& p : peers) {
            if (p.deviceId == "dev-B") peerADiscoveredB = true;
        }
    });

    peerA.start();
    peerB.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    peerA.stop();
    peerB.stop();

    assert(peerADiscoveredB);
    std::cout << "[PASS 5/5] UDP / mDNS Discovery Engine (dev-A discovered dev-B)" << std::endl;

    std::cout << "==================================================" << std::endl;
    std::cout << "ALL SHARED C++ CORE TESTS (5/5) PASSED (100%)!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
