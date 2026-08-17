#include "aerosync/transfer_engine.hpp"
#include "aerosync/socket_transport.hpp"
#include "aerosync/connection_manager.hpp"
#include "aerosync/protocol_serializer.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <set>
#include <fcntl.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <io.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

namespace aerosync {

// Journal Helper: reads/writes completed chunk indices for crash recovery
static std::set<uint32_t> loadResumeJournal(const std::filesystem::path& journalPath) {
    std::set<uint32_t> completedChunks;
    if (std::filesystem::exists(journalPath)) {
        std::ifstream jFile(journalPath, std::ios::binary);
        uint32_t idx;
        while (jFile.read(reinterpret_cast<char*>(&idx), sizeof(idx))) {
            completedChunks.insert(idx);
        }
    }
    return completedChunks;
}

static void appendResumeJournal(const std::filesystem::path& journalPath, uint32_t chunkIdx) {
    std::ofstream jFile(journalPath, std::ios::binary | std::ios::app);
    if (jFile.is_open()) {
        jFile.write(reinterpret_cast<const char*>(&chunkIdx), sizeof(chunkIdx));
        jFile.flush();
    }
}

static std::string sanitizeFilename(const std::string& rawPath) {
    std::string s = rawPath;
    // Strip everything before the last slash or backslash
    size_t lastSlash = s.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        s = s.substr(lastSlash + 1);
    }
    // Also remove any leading drive prefix like "C:" if present
    size_t colon = s.find_last_of(':');
    if (colon != std::string::npos) {
        s = s.substr(colon + 1);
    }
    if (s.empty() || s == "." || s == "..") {
        return "unnamed_file";
    }
    std::string clean;
    for (char c : s) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            clean += '_';
        } else {
            clean += c;
        }
    }
    return clean;
}

static inline uint64_t hton64_val(uint64_t val) {
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

bool TransferEngine::sendStreamChunk(int streamSockFd, const ChunkHeader& header, const uint8_t* buffer, size_t length) {
    std::vector<uint8_t> packet(24 + length);
    uint32_t fileIdxNet = htonl(header.fileIndex);
    uint32_t chunkIdxNet = htonl(header.chunkIndex);
    uint64_t offsetNet = hton64_val(header.offset);
    uint32_t lenNet = htonl(static_cast<uint32_t>(length));
    uint32_t crcNet = htonl(header.crc32c);

    std::memcpy(packet.data() + 0, &fileIdxNet, 4);
    std::memcpy(packet.data() + 4, &chunkIdxNet, 4);
    std::memcpy(packet.data() + 8, &offsetNet, 8);
    std::memcpy(packet.data() + 16, &lenNet, 4);
    std::memcpy(packet.data() + 20, &crcNet, 4);

    if (buffer && length > 0) {
        std::memcpy(packet.data() + 24, buffer, length);
    }
    return SocketTransport::sendRaw(streamSockFd, packet.data(), packet.size());
}

bool TransferEngine::receiveStreamChunk(int streamSockFd, ChunkHeader& outHeader, std::vector<uint8_t>& outBuffer) {
    uint8_t headerBuf[24];
    if (!SocketTransport::recvRaw(streamSockFd, headerBuf, sizeof(headerBuf))) return false;

    if (!ProtocolSerializer::deserializeChunkHeader(headerBuf, sizeof(headerBuf), outHeader)) {
        return false;
    }

    if (outHeader.length > LARGE_CHUNK_SIZE * 2) return false;

    outBuffer.resize(outHeader.length);
    if (outHeader.length > 0) {
        if (!SocketTransport::recvRaw(streamSockFd, outBuffer.data(), outHeader.length)) {
            return false;
        }
    }

    uint32_t calcCrc = ProtocolSerializer::computeCRC32C(outBuffer.data(), outBuffer.size());
    return (calcCrc == outHeader.crc32c);
}

bool TransferEngine::sendFileBatch(int sockFd,
                                   const TransferManifest& manifest,
                                   const std::vector<std::filesystem::path>& localFilePaths,
                                   TransferProgressCallback progressCb,
                                   std::atomic<bool>& cancelSignal,
                                   uint64_t resumeByteOffset) {
    uint64_t batchBytesTransferred = resumeByteOffset;
    auto startTime = std::chrono::steady_clock::now();
    auto lastReportTime = startTime;

    size_t chunkSize = manifest.chunkSize > 0 ? manifest.chunkSize : LARGE_CHUNK_SIZE;
    std::vector<uint8_t> txBuffer(24 + chunkSize);

    for (size_t i = 0; i < manifest.files.size(); ++i) {
        if (cancelSignal) return false;

        const auto& fileMeta = manifest.files[i];
        const auto& localPath = localFilePaths[i];

        std::filesystem::path journalPath = localPath.string() + ".aerosync.journal";
        std::set<uint32_t> completedChunks = loadResumeJournal(journalPath);

        std::vector<char> fileStreamBuf(4 * 1024 * 1024);
        std::ifstream file;
        file.rdbuf()->pubsetbuf(fileStreamBuf.data(), fileStreamBuf.size());
        file.open(localPath, std::ios::binary);
        if (!file.is_open()) return false;

        uint64_t fileBytesTransferred = 0;
        uint32_t chunkIndex = 0;

        // Apply resumeByteOffset on current file if resuming
        if (i == 0 && resumeByteOffset > 0 && resumeByteOffset < fileMeta.fileSize) {
            fileBytesTransferred = resumeByteOffset;
            chunkIndex = static_cast<uint32_t>(resumeByteOffset / chunkSize);
            file.seekg(static_cast<std::streamoff>(fileBytesTransferred));
        }

        while (fileBytesTransferred < fileMeta.fileSize) {
            if (cancelSignal) return false;

            size_t bytesToRead = std::min(static_cast<uint64_t>(chunkSize), fileMeta.fileSize - fileBytesTransferred);

            // Skip completed chunks if resuming
            if (completedChunks.count(chunkIndex) > 0 && fileBytesTransferred + bytesToRead <= resumeByteOffset) {
                fileBytesTransferred += bytesToRead;
                chunkIndex++;
                file.seekg(static_cast<std::streamoff>(fileBytesTransferred));
                continue;
            }

            // Read directly into txBuffer at offset 24 for single-pass zero copy send
            file.read(reinterpret_cast<char*>(txBuffer.data() + 24), bytesToRead);
            size_t bytesRead = file.gcount();
            if (bytesRead == 0) break;

            uint32_t crc = ProtocolSerializer::computeCRC32C(txBuffer.data() + 24, bytesRead);

            uint32_t fileIdxNet = htonl(static_cast<uint32_t>(i));
            uint32_t chunkIdxNet = htonl(chunkIndex);
            uint64_t offsetNet = hton64_val(fileBytesTransferred);
            uint32_t lenNet = htonl(static_cast<uint32_t>(bytesRead));
            uint32_t crcNet = htonl(crc);

            std::memcpy(txBuffer.data() + 0, &fileIdxNet, 4);
            std::memcpy(txBuffer.data() + 4, &chunkIdxNet, 4);
            std::memcpy(txBuffer.data() + 8, &offsetNet, 8);
            std::memcpy(txBuffer.data() + 16, &lenNet, 4);
            std::memcpy(txBuffer.data() + 20, &crcNet, 4);

            // Single unified socket write: header + payload together in one packet
            if (!SocketTransport::sendRaw(sockFd, txBuffer.data(), 24 + bytesRead)) {
                appendResumeJournal(journalPath, chunkIndex);
                return false;
            }

            fileBytesTransferred += bytesRead;
            batchBytesTransferred += bytesRead;
            chunkIndex++;

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime).count();
            if (elapsedMs >= 100 && progressCb) {
                double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
                double speedBps = totalElapsedSec > 0 ? (batchBytesTransferred / totalElapsedSec) : 0.0;
                double remainingBytes = (manifest.totalBytes > batchBytesTransferred) ? (manifest.totalBytes - batchBytesTransferred) : 0;
                double etaSec = speedBps > 0 ? (remainingBytes / speedBps) : 0.0;

                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.currentFileIndex = static_cast<uint32_t>(i);
                prog.currentFileName = fileMeta.relativePath;
                prog.fileBytesTransferred = fileBytesTransferred;
                prog.fileSize = fileMeta.fileSize;
                prog.batchBytesTransferred = batchBytesTransferred;
                prog.batchTotalBytes = manifest.totalBytes;
                prog.speedBytesPerSec = speedBps;
                prog.speedMbps = (speedBps * 8.0) / 1000000.0;
                prog.state = TransferState::TRANSFERRING;
                prog.activeStreams = manifest.streamCount;
                prog.etaSeconds = etaSec;

                progressCb(prog);
                lastReportTime = now;
            }
        }

        if (std::filesystem::exists(journalPath)) {
            std::filesystem::remove(journalPath);
        }
    }

    if (progressCb) {
        auto now = std::chrono::steady_clock::now();
        double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
        double speedBps = totalElapsedSec > 0 ? (batchBytesTransferred / totalElapsedSec) : 0.0;

        TransferProgress prog;
        prog.batchId = manifest.batchId;
        prog.currentFileIndex = static_cast<uint32_t>(manifest.files.empty() ? 0 : manifest.files.size() - 1);
        prog.currentFileName = manifest.files.empty() ? "" : manifest.files.back().relativePath;
        prog.fileBytesTransferred = manifest.files.empty() ? 0 : manifest.files.back().fileSize;
        prog.fileSize = manifest.files.empty() ? 0 : manifest.files.back().fileSize;
        prog.batchBytesTransferred = manifest.totalBytes;
        prog.batchTotalBytes = manifest.totalBytes;
        prog.speedBytesPerSec = speedBps;
        prog.speedMbps = (speedBps * 8.0) / 1000000.0;
        prog.state = TransferState::COMPLETED;
        prog.activeStreams = manifest.streamCount;
        prog.etaSeconds = 0.0;
        progressCb(prog);
    }

    return true;
}

bool TransferEngine::receiveFileBatch(int sockFd,
                                      const TransferManifest& manifest,
                                      const std::filesystem::path& downloadDirectory,
                                      TransferProgressCallback progressCb,
                                      std::atomic<bool>& cancelSignal,
                                      uint64_t resumeByteOffset) {
    uint64_t batchBytesTransferred = resumeByteOffset;
    auto startTime = std::chrono::steady_clock::now();
    auto lastReportTime = startTime;

    std::filesystem::create_directories(downloadDirectory);

    for (size_t i = 0; i < manifest.files.size(); ++i) {
        if (cancelSignal) return false;

        const auto& fileMeta = manifest.files[i];
        std::string safeName = sanitizeFilename(fileMeta.relativePath);
        std::filesystem::path finalPath = downloadDirectory / safeName;
        std::filesystem::path partPath = downloadDirectory / (safeName + ".aerosync.part");
        std::filesystem::path journalPath = downloadDirectory / (safeName + ".aerosync.journal");

        if (finalPath.has_parent_path()) {
            std::error_code ecDir;
            std::filesystem::create_directories(finalPath.parent_path(), ecDir);
        }

        uint64_t fileBytesTransferred = 0;
        if (i == 0 && resumeByteOffset > 0) {
            fileBytesTransferred = resumeByteOffset;
        }

        std::vector<char> fileStreamBuf(4 * 1024 * 1024);
        std::fstream file;
        file.rdbuf()->pubsetbuf(fileStreamBuf.data(), fileStreamBuf.size());

        // Check if resuming from part file
        if (fileBytesTransferred > 0 && std::filesystem::exists(partPath)) {
            file.open(partPath, std::ios::binary | std::ios::in | std::ios::out);
        }
        if (!file.is_open()) {
            // Open new part file
            file.open(partPath, std::ios::binary | std::ios::out | std::ios::trunc);
            file.close();
            file.open(partPath, std::ios::binary | std::ios::in | std::ios::out);
        }
        if (!file.is_open()) return false;

        file.seekp(static_cast<std::streamoff>(fileBytesTransferred));

        std::vector<uint8_t> chunkData;
        chunkData.reserve(LARGE_CHUNK_SIZE);

        while (fileBytesTransferred < fileMeta.fileSize) {
            if (cancelSignal) {
                file.close();
                return false;
            }

            ChunkHeader header;
            if (!receiveStreamChunk(sockFd, header, chunkData)) {
                std::cerr << "CRC32C mismatch or socket error on received chunk!" << std::endl;
                file.close();
                return false;
            }

            file.seekp(static_cast<std::streamoff>(header.offset));
            file.write(reinterpret_cast<const char*>(chunkData.data()), header.length);

            fileBytesTransferred += header.length;
            batchBytesTransferred += header.length;

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime).count();
            if (elapsedMs >= 100 && progressCb) {
                double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
                double speedBps = totalElapsedSec > 0 ? (batchBytesTransferred / totalElapsedSec) : 0.0;
                double remainingBytes = (manifest.totalBytes > batchBytesTransferred) ? (manifest.totalBytes - batchBytesTransferred) : 0;
                double etaSec = speedBps > 0 ? (remainingBytes / speedBps) : 0.0;

                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.currentFileIndex = static_cast<uint32_t>(i);
                prog.currentFileName = fileMeta.relativePath;
                prog.fileBytesTransferred = fileBytesTransferred;
                prog.fileSize = fileMeta.fileSize;
                prog.batchBytesTransferred = batchBytesTransferred;
                prog.batchTotalBytes = manifest.totalBytes;
                prog.speedBytesPerSec = speedBps;
                prog.speedMbps = (speedBps * 8.0) / 1000000.0;
                prog.state = TransferState::TRANSFERRING;
                prog.activeStreams = manifest.streamCount;
                prog.etaSeconds = etaSec;

                progressCb(prog);
                lastReportTime = now;
            }
        }

        file.close();

        if (std::filesystem::exists(journalPath)) {
            std::filesystem::remove(journalPath);
        }

        std::error_code ec;
        if (std::filesystem::exists(finalPath, ec)) {
            std::filesystem::remove(finalPath, ec);
        }
        std::filesystem::rename(partPath, finalPath, ec);
        if (ec) {
            std::error_code ecCopy;
            std::filesystem::copy_file(partPath, finalPath, std::filesystem::copy_options::overwrite_existing, ecCopy);
            if (!ecCopy) {
                std::filesystem::remove(partPath, ecCopy);
            } else {
                std::cerr << "Failed to rename or copy received file: " << ec.message() << std::endl;
                return false;
            }
        }
    }

    if (progressCb) {
        auto now = std::chrono::steady_clock::now();
        double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
        double speedBps = totalElapsedSec > 0 ? (batchBytesTransferred / totalElapsedSec) : 0.0;

        TransferProgress prog;
        prog.batchId = manifest.batchId;
        prog.currentFileIndex = static_cast<uint32_t>(manifest.files.empty() ? 0 : manifest.files.size() - 1);
        prog.currentFileName = manifest.files.empty() ? "" : manifest.files.back().relativePath;
        prog.fileBytesTransferred = manifest.files.empty() ? 0 : manifest.files.back().fileSize;
        prog.fileSize = manifest.files.empty() ? 0 : manifest.files.back().fileSize;
        prog.batchBytesTransferred = manifest.totalBytes;
        prog.batchTotalBytes = manifest.totalBytes;
        prog.speedBytesPerSec = speedBps;
        prog.speedMbps = (speedBps * 8.0) / 1000000.0;
        prog.state = TransferState::COMPLETED;
        prog.activeStreams = manifest.streamCount;
        prog.etaSeconds = 0.0;
        progressCb(prog);
    }

    return true;
}

} // namespace aerosync
