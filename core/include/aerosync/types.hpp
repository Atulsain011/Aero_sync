#ifndef AEROSYNC_TYPES_HPP
#define AEROSYNC_TYPES_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <filesystem>

namespace aerosync {

constexpr uint16_t DISCOVERY_UDP_PORT  = 48123;
constexpr uint16_t CONTROL_TCP_PORT    = 48124;
constexpr uint16_t DATA_TCP_PORT       = 48125;
constexpr uint32_t WIRE_MAGIC          = 0x4145524F; // "AERO"
constexpr size_t   PARALLEL_STREAMS    = 4;           // 4 parallel TCP streams
constexpr size_t   LARGE_CHUNK_SIZE    = 2 * 1024 * 1024; // 2 MB high-throughput streaming chunk
constexpr size_t   MAX_IN_FLIGHT_BYTES = 128 * 1024 * 1024; // 128 MB ring buffer backpressure
constexpr int      PAIRING_TIMEOUT_SEC = 5;           // 5 seconds fast pairing timeout

enum class DeviceType : uint8_t {
    DEVICE_UNKNOWN = 0,
    DEVICE_ANDROID = 1,
    DEVICE_WINDOWS = 2,
    DEVICE_LINUX   = 3,
    DEVICE_MACOS   = 4,
    DEVICE_IOS     = 5
};

inline std::string deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::DEVICE_ANDROID: return "android";
        case DeviceType::DEVICE_WINDOWS: return "windows";
        case DeviceType::DEVICE_LINUX:   return "linux";
        case DeviceType::DEVICE_MACOS:   return "macos";
        case DeviceType::DEVICE_IOS:     return "ios";
        default: return "unknown";
    }
}

inline DeviceType stringToDeviceType(const std::string& str) {
    if (str == "android") return DeviceType::DEVICE_ANDROID;
    if (str == "windows") return DeviceType::DEVICE_WINDOWS;
    if (str == "linux")   return DeviceType::DEVICE_LINUX;
    if (str == "macos")   return DeviceType::DEVICE_MACOS;
    if (str == "ios")     return DeviceType::DEVICE_IOS;
    return DeviceType::DEVICE_UNKNOWN;
}

// Pairing State Machine States
enum class PairingState {
    UNPAIRED,
    PAIRING_REQUESTED,
    AWAITING_PIN_CONFIRMATION,
    AUTHENTICATED_SESSION,
    TRANSFERRING,
    DISCONNECTED
};

inline std::string pairingStateToString(PairingState state) {
    switch (state) {
        case PairingState::UNPAIRED: return "UNPAIRED";
        case PairingState::PAIRING_REQUESTED: return "PAIRING_REQUESTED";
        case PairingState::AWAITING_PIN_CONFIRMATION: return "AWAITING_PIN_CONFIRMATION";
        case PairingState::AUTHENTICATED_SESSION: return "AUTHENTICATED_SESSION";
        case PairingState::TRANSFERRING: return "TRANSFERRING";
        case PairingState::DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

enum class PairingStatus : uint8_t {
    PAIRING_PENDING      = 0,
    PAIRING_ACCEPTED     = 1,
    PAIRING_DECLINED     = 2,
    PAIRING_TIMEOUT      = 3,
    PAIRING_BUSY         = 4,
    PAIRING_PIN_MISMATCH = 5
};

struct PeerInfo {
    std::string deviceId;
    std::string deviceName;
    DeviceType deviceType{DeviceType::DEVICE_UNKNOWN};
    std::string platform;
    std::string appVersion{"1.0.0"};
    std::string ipAddress;
    uint16_t port{CONTROL_TCP_PORT};
    uint64_t lastSeenMs{0};
};

struct FileMetadata {
    uint32_t fileIndex{0};
    std::string relativePath;
    uint64_t fileSize{0};
    std::string sha256;
    uint64_t lastModifiedMs{0};
};

struct TransferManifest {
    std::string batchId;
    std::string senderId;
    std::string senderName;
    std::string sessionToken;
    uint32_t totalFiles{0};
    uint64_t totalBytes{0};
    std::vector<FileMetadata> files;
    uint32_t chunkSize{LARGE_CHUNK_SIZE};
    uint32_t streamCount{PARALLEL_STREAMS};
};

struct ChunkHeader {
    std::string batchId;
    uint32_t fileIndex{0};
    uint32_t chunkIndex{0};
    uint64_t offset{0};
    uint32_t length{0};
    uint32_t crc32c{0};
    bool isLastChunk{false};
};

enum class TransferState {
    IDLE,
    WAITING_ACCEPTANCE,
    CONNECTING,
    TRANSFERRING,
    COMPLETED,
    FAILED,
    DECLINED,
    CANCELLED
};

struct TransferProgress {
    std::string batchId;
    uint32_t currentFileIndex{0};
    std::string currentFileName;
    uint64_t fileBytesTransferred{0};
    uint64_t fileSize{0};
    uint64_t batchBytesTransferred{0};
    uint64_t batchTotalBytes{0};
    double speedBytesPerSec{0.0};
    double speedMbps{0.0};
    TransferState state{TransferState::IDLE};
    uint32_t activeStreams{PARALLEL_STREAMS};
    double etaSeconds{0.0};
};

using TransferProgressCallback = std::function<void(const TransferProgress& progress)>;
using PairingStateChangedCallback = std::function<void(PairingState newState, const std::string& reason)>;

} // namespace aerosync

#endif // AEROSYNC_TYPES_HPP
