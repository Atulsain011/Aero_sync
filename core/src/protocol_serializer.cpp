#include "aerosync/protocol_serializer.hpp"
#include <sstream>
#include <cstring>
#include <iostream>
#include <iomanip>

#ifdef _WIN32
    #include <winsock2.h>
    #include <stdlib.h>
#else
    #include <arpa/inet.h>
    #if defined(__linux__) || defined(__ANDROID__)
        #include <endian.h>
    #endif
#endif

namespace aerosync {

// Helper: 64-bit endian swap
static inline uint64_t hton64(uint64_t val) {
#if defined(__linux__) || defined(__ANDROID__)
    return htobe64(val);
#elif defined(__APPLE__)
    return OSSwapHostToBigInt64(val);
#elif defined(__GNUC__) || defined(__clang__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return __builtin_bswap64(val);
    #else
        return val;
    #endif
#elif defined(_MSC_VER)
    return _byteswap_uint64(val);
#else
    uint32_t high = htonl(static_cast<uint32_t>(val >> 32));
    uint32_t low = htonl(static_cast<uint32_t>(val & 0xFFFFFFFFULL));
    return (static_cast<uint64_t>(high) << 32) | low;
#endif
}

static inline uint64_t ntoh64(uint64_t val) {
#if defined(__linux__) || defined(__ANDROID__)
    return be64toh(val);
#elif defined(__APPLE__)
    return OSSwapBigToHostInt64(val);
#else
    return hton64(val);
#endif
}

// Minimal JSON escape / unescape helpers
static std::string escapeJson(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else ss << c;
    }
    return ss.str();
}

static std::string unescapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            if (str[i+1] == '"') { result += '"'; i++; }
            else if (str[i+1] == '\\') { result += '\\'; i++; }
            else if (str[i+1] == '/') { result += '/'; i++; }
            else if (str[i+1] == 'b') { result += '\b'; i++; }
            else if (str[i+1] == 'f') { result += '\f'; i++; }
            else if (str[i+1] == 'n') { result += '\n'; i++; }
            else if (str[i+1] == 'r') { result += '\r'; i++; }
            else if (str[i+1] == 't') { result += '\t'; i++; }
            else { result += str[i]; }
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::vector<std::string> keysToTry;
    keysToTry.push_back(key);
    if (key == "device_name") {
        keysToTry.push_back("deviceName");
        keysToTry.push_back("name");
    } else if (key == "device_id") {
        keysToTry.push_back("deviceId");
        keysToTry.push_back("id");
    } else if (key == "app_version") {
        keysToTry.push_back("appVersion");
    } else if (key == "listening_port") {
        keysToTry.push_back("listeningPort");
        keysToTry.push_back("port");
    }

    for (const auto& k : keysToTry) {
        std::string searchKey = "\"" + k + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) continue;

        size_t colonPos = json.find(':', keyPos + searchKey.length());
        if (colonPos == std::string::npos) continue;

        size_t quoteStart = json.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) continue;

        size_t valStart = quoteStart + 1;
        size_t valEnd = valStart;
        while (valEnd < json.length()) {
            if (json[valEnd] == '"' && json[valEnd - 1] != '\\') {
                break;
            }
            valEnd++;
        }
        if (valEnd >= json.length()) continue;

        std::string rawVal = json.substr(valStart, valEnd - valStart);
        std::string unescaped = unescapeJson(rawVal);
        if (!unescaped.empty()) return unescaped;
    }
    return "";
}

static int extractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::vector<std::string> keysToTry = { key };
    if (key == "listening_port") {
        keysToTry.push_back("listeningPort");
        keysToTry.push_back("port");
    }
    for (const auto& k : keysToTry) {
        std::string pattern = "\"" + k + "\":";
        size_t start = json.find(pattern);
        if (start == std::string::npos) continue;
        start += pattern.length();
        while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
        size_t end = start;
        while (end < json.length() && (isdigit(json[end]) || json[end] == '-')) end++;
        if (end == start) continue;
        try {
            return std::stoi(json.substr(start, end - start));
        } catch (...) {}
    }
    return defaultVal;
}

static uint64_t extractJsonUint64(const std::string& json, const std::string& key, uint64_t defaultVal = 0) {
    std::vector<std::string> keysToTry = { key };
    if (key == "timestamp_ms") {
        keysToTry.push_back("timestampMs");
    }
    for (const auto& k : keysToTry) {
        std::string pattern = "\"" + k + "\":";
        size_t start = json.find(pattern);
        if (start == std::string::npos) continue;
        start += pattern.length();
        while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
        size_t end = start;
        while (end < json.length() && isdigit(json[end])) end++;
        if (end == start) continue;
        try {
            return std::stoull(json.substr(start, end - start));
        } catch (...) {}
    }
    return defaultVal;
}

// 1. Wire Control Frame Encoding / Decoding
// Header Layout (13 bytes total):
// [4 bytes Magic (0x4145524F)] [1 byte MsgType] [4 bytes SeqNum] [4 bytes PayloadLen] [Payload Bytes]
std::string ProtocolSerializer::encodeControlFrame(ControlMessageType msgType, uint32_t sequenceNum, const std::string& payload) {
    std::string frame;
    uint32_t magicNet = htonl(WIRE_MAGIC);
    uint8_t typeByte = static_cast<uint8_t>(msgType);
    uint32_t seqNet = htonl(sequenceNum);
    uint32_t lenNet = htonl(static_cast<uint32_t>(payload.size()));

    frame.reserve(13 + payload.size());
    frame.append(reinterpret_cast<const char*>(&magicNet), sizeof(magicNet));
    frame.append(reinterpret_cast<const char*>(&typeByte), sizeof(typeByte));
    frame.append(reinterpret_cast<const char*>(&seqNet), sizeof(seqNet));
    frame.append(reinterpret_cast<const char*>(&lenNet), sizeof(lenNet));
    frame.append(payload);
    return frame;
}

bool ProtocolSerializer::decodeControlFrame(const std::string& rawFrame, ControlMessageType& outType, uint32_t& outSeq, std::string& outPayload) {
    if (rawFrame.size() < 13) return false;

    uint32_t magicNet = 0;
    std::memcpy(&magicNet, rawFrame.data(), 4);
    if (ntohl(magicNet) != WIRE_MAGIC) return false;

    outType = static_cast<ControlMessageType>(static_cast<uint8_t>(rawFrame[4]));

    uint32_t seqNet = 0;
    std::memcpy(&seqNet, rawFrame.data() + 5, 4);
    outSeq = ntohl(seqNet);

    uint32_t lenNet = 0;
    std::memcpy(&lenNet, rawFrame.data() + 9, 4);
    uint32_t payloadLen = ntohl(lenNet);

    if (rawFrame.size() < 13 + payloadLen) return false;
    outPayload = rawFrame.substr(13, payloadLen);
    return true;
}

// 2. Discovery Beacon Serialization
std::string ProtocolSerializer::serializeDiscoveryBeacon(const PeerInfo& peer) {
    std::ostringstream ss;
    std::string safeName = peer.deviceName;
    if (safeName.empty()) {
        safeName = deviceTypeToString(peer.deviceType) + " Device";
    }
    ss << "{"
       << "\"device_name\":\"" << escapeJson(safeName) << "\","
       << "\"deviceName\":\"" << escapeJson(safeName) << "\","
       << "\"device_id\":\"" << escapeJson(peer.deviceId) << "\","
       << "\"deviceId\":\"" << escapeJson(peer.deviceId) << "\","
       << "\"platform\":\"" << deviceTypeToString(peer.deviceType) << "\","
       << "\"app_version\":\"" << escapeJson(peer.appVersion) << "\","
       << "\"listening_port\":" << peer.port << ","
       << "\"timestamp_ms\":" << peer.lastSeenMs
       << "}";
    return ss.str();
}

bool ProtocolSerializer::deserializeDiscoveryBeacon(const std::string& data, const std::string& senderIp, PeerInfo& outPeer) {
    std::string deviceId = extractJsonString(data, "device_id");
    std::string deviceName = extractJsonString(data, "device_name");
    std::string platformStr = extractJsonString(data, "platform");
    std::string appVerStr = extractJsonString(data, "app_version");
    int port = extractJsonInt(data, "listening_port", CONTROL_TCP_PORT);
    uint64_t timestamp = extractJsonUint64(data, "timestamp_ms", 0);

    if (deviceId.empty()) return false;

    outPeer.deviceId = deviceId;
    outPeer.platform = platformStr;
    outPeer.deviceType = stringToDeviceType(platformStr);
    
    if (deviceName.empty() || deviceName == "Unknown Device") {
        std::string pName = platformStr.empty() ? "Remote Device" : platformStr;
        if (!pName.empty()) pName[0] = static_cast<char>(std::toupper(pName[0]));
        outPeer.deviceName = pName + " (" + senderIp + ")";
    } else {
        outPeer.deviceName = deviceName;
    }

    outPeer.appVersion = appVerStr.empty() ? "1.0.0" : appVerStr;
    outPeer.ipAddress = senderIp;
    outPeer.port = static_cast<uint16_t>(port);
    outPeer.lastSeenMs = timestamp;
    return true;
}

// 3. Pairing Protocol Serialization
std::string ProtocolSerializer::serializePairingRequest(const ConnectRequest& req) {
    std::ostringstream ss;
    ss << req.senderId << "|"
       << req.senderName << "|"
       << req.platform << "|"
       << req.appVersion << "|"
       << req.pairingPin << "|"
       << req.sessionNonce;
    return ss.str();
}

bool ProtocolSerializer::deserializePairingRequest(const std::string& data, ConnectRequest& outReq) {
    std::stringstream ss(data);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, '|')) parts.push_back(token);

    if (parts.size() < 2) return false;

    outReq.senderId = parts[0];
    outReq.senderName = parts[1];
    if (parts.size() >= 3) outReq.platform = parts[2];
    if (parts.size() >= 4) outReq.appVersion = parts[3];
    if (parts.size() >= 5) outReq.pairingPin = parts[4];
    if (parts.size() >= 6 && !parts[5].empty()) {
        try { outReq.sessionNonce = std::stoull(parts[5]); } catch (...) {}
    }
    return true;
}

std::string ProtocolSerializer::serializePairingResponse(const PairingResponseMsg& resp) {
    std::ostringstream ss;
    ss << static_cast<int>(resp.status) << "|"
       << resp.sessionToken << "|"
       << resp.responderId << "|"
       << resp.responderName << "|"
       << resp.maxStreams << "|"
       << resp.chunkSize << "|"
       << resp.reason;
    return ss.str();
}

bool ProtocolSerializer::deserializePairingResponse(const std::string& data, PairingResponseMsg& outResp) {
    std::stringstream ss(data);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, '|')) parts.push_back(token);

    if (parts.empty()) return false;

    try {
        outResp.status = static_cast<PairingStatus>(std::stoi(parts[0]));
    } catch (...) {
        return false;
    }

    if (parts.size() >= 2) outResp.sessionToken = parts[1];
    if (parts.size() >= 3) outResp.responderId = parts[2];
    if (parts.size() >= 4) outResp.responderName = parts[3];
    if (parts.size() >= 5 && !parts[4].empty()) {
        try { outResp.maxStreams = std::stoul(parts[4]); } catch (...) {}
    }
    if (parts.size() >= 6 && !parts[5].empty()) {
        try { outResp.chunkSize = std::stoul(parts[5]); } catch (...) {}
    }
    if (parts.size() >= 7) outResp.reason = parts[6];
    return true;
}

// 4. File Transfer Manifest Serialization
std::string ProtocolSerializer::serializeTransferManifest(const TransferManifest& manifest) {
    std::ostringstream ss;
    ss << manifest.batchId << "|"
       << manifest.senderId << "|"
       << manifest.senderName << "|"
       << manifest.sessionToken << "|"
       << manifest.totalFiles << "|"
       << manifest.totalBytes << "|"
       << manifest.chunkSize << "|"
       << manifest.streamCount;

    for (const auto& f : manifest.files) {
        ss << ";" << f.fileIndex << ":" << f.relativePath << ":" << f.fileSize << ":" << f.sha256;
    }
    return ss.str();
}

bool ProtocolSerializer::deserializeTransferManifest(const std::string& data, TransferManifest& outManifest) {
    size_t filesPos = data.find(';');
    std::string header = (filesPos == std::string::npos) ? data : data.substr(0, filesPos);

    std::stringstream ss(header);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, '|')) parts.push_back(token);

    if (parts.size() < 6) return false;

    outManifest.batchId = parts[0];
    outManifest.senderId = parts[1];
    outManifest.senderName = parts[2];
    outManifest.sessionToken = parts[3];
    try {
        outManifest.totalFiles = std::stoul(parts[4]);
        outManifest.totalBytes = std::stoull(parts[5]);
        if (parts.size() >= 7) outManifest.chunkSize = std::stoul(parts[6]);
        if (parts.size() >= 8) outManifest.streamCount = std::stoul(parts[7]);
    } catch (...) {
        return false;
    }

    outManifest.files.clear();
    if (filesPos != std::string::npos) {
        std::stringstream ssFiles(data.substr(filesPos + 1));
        std::string fileToken;
        while (std::getline(ssFiles, fileToken, ';')) {
            if (fileToken.empty()) continue;
            size_t firstColon = fileToken.find(':');
            if (firstColon == std::string::npos) continue;
            size_t lastColon = fileToken.rfind(':');
            if (lastColon == std::string::npos || lastColon == firstColon) continue;
            size_t secondLastColon = fileToken.rfind(':', lastColon - 1);
            if (secondLastColon == std::string::npos || secondLastColon < firstColon) continue;

            std::string idxStr = fileToken.substr(0, firstColon);
            std::string relPath = fileToken.substr(firstColon + 1, secondLastColon - firstColon - 1);
            std::string sizeStr = fileToken.substr(secondLastColon + 1, lastColon - secondLastColon - 1);
            std::string shaStr = fileToken.substr(lastColon + 1);

            FileMetadata fm;
            try {
                fm.fileIndex = std::stoul(idxStr);
                fm.relativePath = relPath;
                fm.fileSize = std::stoull(sizeStr);
                fm.sha256 = shaStr;
                outManifest.files.push_back(fm);
            } catch (...) {}
        }
    }
    return true;
}

// 5. Chunk Framing Envelope
// Chunk Envelope Binary Layout (24 bytes header + data):
// [4B FileIndex] [4B ChunkIndex] [8B Offset] [4B Length] [4B CRC32C] [Data...]
std::vector<uint8_t> ProtocolSerializer::serializeChunkEnvelope(const ChunkHeader& header, const uint8_t* chunkData, size_t chunkSize) {
    std::vector<uint8_t> envelope(24 + chunkSize);
    uint32_t fileIdxNet = htonl(header.fileIndex);
    uint32_t chunkIdxNet = htonl(header.chunkIndex);
    uint64_t offsetNet = hton64(header.offset);
    uint32_t lenNet = htonl(static_cast<uint32_t>(chunkSize));
    uint32_t crcNet = htonl(header.crc32c);

    std::memcpy(envelope.data() + 0, &fileIdxNet, 4);
    std::memcpy(envelope.data() + 4, &chunkIdxNet, 4);
    std::memcpy(envelope.data() + 8, &offsetNet, 8);
    std::memcpy(envelope.data() + 16, &lenNet, 4);
    std::memcpy(envelope.data() + 20, &crcNet, 4);

    if (chunkData && chunkSize > 0) {
        std::memcpy(envelope.data() + 24, chunkData, chunkSize);
    }
    return envelope;
}

bool ProtocolSerializer::deserializeChunkHeader(const uint8_t* headerData, size_t headerLen, ChunkHeader& outHeader) {
    if (headerLen < 24) return false;

    uint32_t fileIdxNet = 0, chunkIdxNet = 0, lenNet = 0, crcNet = 0;
    uint64_t offsetNet = 0;

    std::memcpy(&fileIdxNet, headerData + 0, 4);
    std::memcpy(&chunkIdxNet, headerData + 4, 4);
    std::memcpy(&offsetNet, headerData + 8, 8);
    std::memcpy(&lenNet, headerData + 16, 4);
    std::memcpy(&crcNet, headerData + 20, 4);

    outHeader.fileIndex = ntohl(fileIdxNet);
    outHeader.chunkIndex = ntohl(chunkIdxNet);
    outHeader.offset = ntoh64(offsetNet);
    outHeader.length = ntohl(lenNet);
    outHeader.crc32c = ntohl(crcNet);

    return true;
}

bool ProtocolSerializer::deserializeChunkEnvelope(const uint8_t* envelopeData, size_t envelopeLen, ChunkHeader& outHeader, const uint8_t*& outDataPtr) {
    if (!deserializeChunkHeader(envelopeData, envelopeLen, outHeader)) return false;
    if (envelopeLen < 24 + outHeader.length) return false;

    outDataPtr = envelopeData + 24;
    return true;
}

// 6. Ultra-Fast Slice-by-8 Castagnoli CRC32C Checksum Calculation (Poly: 0x82F63B78)
static const uint32_t (*getCRC32CSliceTable())[256] {
    static uint32_t table[8][256];
    static bool initialized = false;
    if (!initialized) {
        // Table 0
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int k = 0; k < 8; ++k) {
                crc = (crc >> 1) ^ (0x82F63B78 & -(crc & 1));
            }
            table[0][i] = crc;
        }
        // Tables 1 to 7 (Slice-by-8)
        for (int slice = 1; slice < 8; ++slice) {
            for (uint32_t i = 0; i < 256; ++i) {
                table[slice][i] = (table[slice - 1][i] >> 8) ^ table[0][table[slice - 1][i] & 0xFF];
            }
        }
        initialized = true;
    }
    return table;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
__attribute__((target("sse4.2")))
static inline uint32_t computeCRC32C_Hardware(const uint8_t* data, size_t length) {
    uint64_t crc = 0xFFFFFFFF;
    const uint8_t* p = data;
    while (length >= 8) {
        crc = __builtin_ia32_crc32di(crc, *reinterpret_cast<const uint64_t*>(p));
        p += 8;
        length -= 8;
    }
    while (length > 0) {
        crc = __builtin_ia32_crc32qi(static_cast<uint32_t>(crc), *p);
        p++;
        length--;
    }
    return static_cast<uint32_t>(crc ^ 0xFFFFFFFF);
}
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#if defined(__ARM_FEATURE_CRC32)
static inline uint32_t computeCRC32C_ARM(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = data;
    while (length >= 8) {
        crc = __builtin_arm_crc32cd(crc, *reinterpret_cast<const uint64_t*>(p));
        p += 8;
        length -= 8;
    }
    while (length > 0) {
        crc = __builtin_arm_crc32cb(crc, *p);
        p++;
        length--;
    }
    return crc ^ 0xFFFFFFFF;
}
#endif
#endif

uint32_t ProtocolSerializer::computeCRC32C(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
    #if defined(__SSE4_2__)
        return computeCRC32C_Hardware(data, length);
    #endif
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    #if defined(__ARM_FEATURE_CRC32)
        return computeCRC32C_ARM(data, length);
    #endif
#endif

    const auto table = getCRC32CSliceTable();
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = data;

    // Fast 8-byte word processing
    while (length >= 8) {
        uint32_t one = static_cast<uint32_t>(p[0]) |
                      (static_cast<uint32_t>(p[1]) << 8) |
                      (static_cast<uint32_t>(p[2]) << 16) |
                      (static_cast<uint32_t>(p[3]) << 24);
        one ^= crc;

        uint32_t two = static_cast<uint32_t>(p[4]) |
                      (static_cast<uint32_t>(p[5]) << 8) |
                      (static_cast<uint32_t>(p[6]) << 16) |
                      (static_cast<uint32_t>(p[7]) << 24);

        crc = table[7][one & 0xFF] ^
              table[6][(one >> 8) & 0xFF] ^
              table[5][(one >> 16) & 0xFF] ^
              table[4][(one >> 24) & 0xFF] ^
              table[3][two & 0xFF] ^
              table[2][(two >> 8) & 0xFF] ^
              table[1][(two >> 16) & 0xFF] ^
              table[0][(two >> 24) & 0xFF];

        p += 8;
        length -= 8;
    }

    // Remaining trailing bytes
    while (length--) {
        crc = (crc >> 8) ^ table[0][(crc ^ (*p++)) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

} // namespace aerosync
