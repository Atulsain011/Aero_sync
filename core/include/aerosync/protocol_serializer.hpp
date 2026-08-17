#ifndef AEROSYNC_PROTOCOL_SERIALIZER_HPP
#define AEROSYNC_PROTOCOL_SERIALIZER_HPP

#include "types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace aerosync {

enum class ControlMessageType : uint8_t {
    UNKNOWN         = 0x00,
    CONNECT_REQUEST = 0x01,
    CONNECT_ACCEPT  = 0x02,
    CONNECT_DECLINE = 0x03,
    PAIRING_REQUEST = 0x04,
    PAIRING_CONFIRM = 0x05,
    PAIRING_REJECT  = 0x06,
    FILE_OFFER      = 0x07,
    FILE_ACCEPT     = 0x08,
    FILE_CANCEL     = 0x09,
    CHUNK_ACK       = 0x0A,
    FILE_COMPLETE   = 0x0B,
    FILE_ERROR      = 0x0C,
    PING            = 0x0D,
    PONG            = 0x0E
};

struct ConnectRequest {
    std::string senderId;
    std::string senderName;
    std::string platform;
    std::string appVersion{"1.0.0"};
    std::string pairingPin{"000000"};
    std::string senderIp;
    uint16_t senderPort{48124};
    uint64_t sessionNonce{0};
};

struct PairingResponseMsg {
    PairingStatus status{PairingStatus::PAIRING_PENDING};
    std::string sessionToken;
    std::string responderId;
    std::string responderName;
    std::string reason;
    uint32_t maxStreams{PARALLEL_STREAMS};
    uint32_t chunkSize{LARGE_CHUNK_SIZE};
};

class ProtocolSerializer {
public:
    // Wire Frame Encoders / Decoders
    static std::string encodeControlFrame(ControlMessageType msgType, uint32_t sequenceNum, const std::string& payload);
    static bool decodeControlFrame(const std::string& rawFrame, ControlMessageType& outType, uint32_t& outSeq, std::string& outPayload);

    // High-Efficiency Discovery Beacon
    static std::string serializeDiscoveryBeacon(const PeerInfo& peer);
    static bool deserializeDiscoveryBeacon(const std::string& data, const std::string& senderIp, PeerInfo& outPeer);

    // Pairing Protocol Serialization
    static std::string serializePairingRequest(const ConnectRequest& req);
    static bool deserializePairingRequest(const std::string& data, ConnectRequest& outReq);

    static std::string serializePairingResponse(const PairingResponseMsg& resp);
    static bool deserializePairingResponse(const std::string& data, PairingResponseMsg& outResp);

    // File Transfer Manifest Serialization
    static std::string serializeTransferManifest(const TransferManifest& manifest);
    static bool deserializeTransferManifest(const std::string& data, TransferManifest& outManifest);

    // Chunk Framing Envelope
    static std::vector<uint8_t> serializeChunkEnvelope(const ChunkHeader& header, const uint8_t* chunkData, size_t chunkSize);
    static bool deserializeChunkHeader(const uint8_t* headerData, size_t headerLen, ChunkHeader& outHeader);
    static bool deserializeChunkEnvelope(const uint8_t* envelopeData, size_t envelopeLen, ChunkHeader& outHeader, const uint8_t*& outDataPtr);

    // Fast CRC32C Checksum Calculation (Castagnoli 0x1EDC6F41)
    static uint32_t computeCRC32C(const uint8_t* data, size_t length);
};

} // namespace aerosync

#endif // AEROSYNC_PROTOCOL_SERIALIZER_HPP
