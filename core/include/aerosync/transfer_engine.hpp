#ifndef AEROSYNC_TRANSFER_ENGINE_HPP
#define AEROSYNC_TRANSFER_ENGINE_HPP

#include "types.hpp"
#include "protocol_serializer.hpp"
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <filesystem>

namespace aerosync {

class TransferEngine {
public:
    static bool sendFileBatch(int sockFd,
                             const TransferManifest& manifest,
                             const std::vector<std::filesystem::path>& localFilePaths,
                             TransferProgressCallback progressCb,
                             std::atomic<bool>& cancelSignal,
                             uint64_t resumeByteOffset = 0);

    static bool receiveFileBatch(int sockFd,
                                const TransferManifest& manifest,
                                const std::filesystem::path& downloadDirectory,
                                TransferProgressCallback progressCb,
                                std::atomic<bool>& cancelSignal,
                                uint64_t resumeByteOffset = 0);

    // Relative path sanitization & duplicate resolution
    static std::string sanitizeRelativePath(const std::string& rawPath);
    static std::filesystem::path resolveNonCollidingPath(const std::filesystem::path& targetPath);

    // Parallel multi-stream worker routines
    static bool sendStreamChunk(int streamSockFd, const ChunkHeader& header, const uint8_t* buffer, size_t length);
    static bool receiveStreamChunk(int streamSockFd, ChunkHeader& outHeader, std::vector<uint8_t>& outBuffer);
};

} // namespace aerosync

#endif // AEROSYNC_TRANSFER_ENGINE_HPP
