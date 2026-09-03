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
#include <memory>
#include <algorithm>
#include <cstring>
#include <fcntl.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <io.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace aerosync {

// Buffer Block for zero-allocation recycling
struct ChunkBufferBlock {
    std::vector<uint8_t> data;
    size_t capacity{0};

    explicit ChunkBufferBlock(size_t cap = LARGE_CHUNK_SIZE) : capacity(cap) {
        data.resize(cap);
    }
};

// Thread-safe Fixed Pre-allocated Buffer Pool
class BufferPool {
private:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::vector<std::unique_ptr<ChunkBufferBlock>> m_pool;
    size_t m_blockSize;
    bool m_stopping{false};

public:
    BufferPool(size_t poolSize, size_t blockSize) : m_blockSize(blockSize) {
        m_pool.reserve(poolSize);
        for (size_t i = 0; i < poolSize; ++i) {
            m_pool.push_back(std::make_unique<ChunkBufferBlock>(blockSize));
        }
    }

    std::unique_ptr<ChunkBufferBlock> acquire(const std::atomic<bool>& cancelSignal) {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [&]() {
            return !m_pool.empty() || m_stopping || cancelSignal.load();
        });
        if (m_stopping || cancelSignal.load() || m_pool.empty()) {
            return nullptr;
        }
        auto buf = std::move(m_pool.back());
        m_pool.pop_back();
        return buf;
    }

    void release(std::unique_ptr<ChunkBufferBlock> buf) {
        if (!buf) return;
        std::lock_guard<std::mutex> lock(m_mtx);
        m_pool.push_back(std::move(buf));
        m_cv.notify_one();
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopping = true;
        m_cv.notify_all();
    }
};

// Queue item passed from Network Producer thread to Disk Consumer thread
struct AsyncChunkPacket {
    ChunkHeader header;
    std::unique_ptr<ChunkBufferBlock> buffer;
    bool isEof{false};
    bool isError{false};
};

// Thread-safe bounded channel with backpressure
class AsyncChunkQueue {
private:
    std::mutex m_mtx;
    std::condition_variable m_cvNotFull;
    std::condition_variable m_cvNotEmpty;
    std::queue<AsyncChunkPacket> m_queue;
    size_t m_maxCapacity;
    bool m_stopped{false};

public:
    explicit AsyncChunkQueue(size_t maxCap = 8) : m_maxCapacity(maxCap) {}

    bool push(AsyncChunkPacket&& packet, const std::atomic<bool>& cancelSignal) {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cvNotFull.wait(lock, [&]() {
            return m_queue.size() < m_maxCapacity || m_stopped || cancelSignal.load();
        });
        if (m_stopped || cancelSignal.load()) return false;
        m_queue.push(std::move(packet));
        m_cvNotEmpty.notify_one();
        return true;
    }

    bool pop(AsyncChunkPacket& outPacket) {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cvNotEmpty.wait(lock, [&]() {
            return !m_queue.empty() || m_stopped;
        });
        if (m_queue.empty()) return false;
        outPacket = std::move(m_queue.front());
        m_queue.pop();
        m_cvNotFull.notify_one();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_stopped = true;
        m_cvNotEmpty.notify_all();
        m_cvNotFull.notify_all();
    }
};

// Journal Helper: reads/writes completed chunk indices for crash recovery
static std::set<uint32_t> loadResumeJournal(const std::filesystem::path& journalPath) {
    std::set<uint32_t> completedChunks;
    std::error_code ec;
    if (std::filesystem::exists(journalPath, ec)) {
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
    size_t lastSlash = s.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        s = s.substr(lastSlash + 1);
    }
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

// -----------------------------------------------------------------------------
// High-Speed Double-Buffered Sender Engine
// -----------------------------------------------------------------------------
bool TransferEngine::sendFileBatch(int sockFd,
                                   const TransferManifest& manifest,
                                   const std::vector<std::filesystem::path>& localFilePaths,
                                   TransferProgressCallback progressCb,
                                   std::atomic<bool>& cancelSignal,
                                   uint64_t resumeByteOffset) {
    uint64_t batchBytesTransferred = resumeByteOffset;
    uint64_t lastReportBytes = resumeByteOffset;
    double currentSmoothedSpeedBps = 0.0;
    auto startTime = std::chrono::steady_clock::now();
    auto lastReportTime = startTime;

    size_t chunkSize = manifest.chunkSize > 0 ? manifest.chunkSize : LARGE_CHUNK_SIZE;

    // Pre-allocated double buffers for sending
    std::vector<uint8_t> txBuffer(24 + chunkSize);

    for (size_t i = 0; i < manifest.files.size(); ++i) {
        if (cancelSignal) {
            if (progressCb) {
                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.state = TransferState::CANCELLED;
                progressCb(prog);
            }
            return false;
        }

        const auto& fileMeta = manifest.files[i];
        const auto& localPath = localFilePaths[i];

        // Emit instant progress notification at start of file transfer
        if (progressCb) {
            TransferProgress initProg;
            initProg.batchId = manifest.batchId;
            initProg.currentFileIndex = static_cast<uint32_t>(i);
            initProg.currentFileName = fileMeta.relativePath;
            initProg.fileSize = fileMeta.fileSize;
            initProg.fileBytesTransferred = (i == 0) ? resumeByteOffset : 0;
            initProg.batchTotalBytes = manifest.totalBytes;
            initProg.batchBytesTransferred = batchBytesTransferred;
            initProg.state = TransferState::TRANSFERRING;
            initProg.speedBytesPerSec = 0.0;
            initProg.speedMbps = 0.0;
            progressCb(initProg);
        }

        std::filesystem::path journalPath = localPath.string() + ".aerosync.journal";
        std::set<uint32_t> completedChunks = loadResumeJournal(journalPath);

        // High-Speed Double-Buffered Async Sender Engine
        const size_t POOL_SIZE = 16;
        BufferPool sendPool(POOL_SIZE, chunkSize);
        AsyncChunkQueue sendQueue(POOL_SIZE);
        std::atomic<bool> readerSuccess{true};

        // Launch background disk reader thread to pre-read next chunk while current chunk is sending
        std::thread diskReaderThread([&]() {
            std::vector<char> fileStreamBuf(4 * 1024 * 1024);
            std::ifstream file;
            file.rdbuf()->pubsetbuf(fileStreamBuf.data(), fileStreamBuf.size());
            file.open(localPath, std::ios::binary);
            if (!file.is_open()) {
                readerSuccess = false;
                sendQueue.stop();
                return;
            }

            uint64_t fBytesRead = 0;
            uint32_t cIndex = 0;

            if (i == 0 && resumeByteOffset > 0 && resumeByteOffset < fileMeta.fileSize) {
                fBytesRead = resumeByteOffset;
                cIndex = static_cast<uint32_t>(resumeByteOffset / chunkSize);
                file.seekg(static_cast<std::streamoff>(fBytesRead));
            }

            while (fBytesRead < fileMeta.fileSize) {
                if (cancelSignal.load()) {
                    readerSuccess = false;
                    break;
                }

                size_t bytesToRead = std::min(static_cast<uint64_t>(chunkSize), fileMeta.fileSize - fBytesRead);

                if (completedChunks.count(cIndex) > 0 && fBytesRead + bytesToRead <= resumeByteOffset) {
                    fBytesRead += bytesToRead;
                    cIndex++;
                    file.seekg(static_cast<std::streamoff>(fBytesRead));
                    continue;
                }

                auto block = sendPool.acquire(cancelSignal);
                if (!block) {
                    readerSuccess = false;
                    break;
                }

                file.read(reinterpret_cast<char*>(block->data.data()), bytesToRead);
                size_t actualRead = file.gcount();
                if (actualRead == 0) {
                    sendPool.release(std::move(block));
                    break;
                }

                uint32_t crc = ProtocolSerializer::computeCRC32C(block->data.data(), actualRead);

                ChunkHeader hdr;
                hdr.fileIndex = static_cast<uint32_t>(i);
                hdr.chunkIndex = cIndex;
                hdr.offset = fBytesRead;
                hdr.length = actualRead;
                hdr.crc32c = crc;

                AsyncChunkPacket packet;
                packet.header = hdr;
                packet.buffer = std::move(block);
                packet.isEof = false;
                packet.isError = false;

                if (!sendQueue.push(std::move(packet), cancelSignal)) {
                    readerSuccess = false;
                    break;
                }

                fBytesRead += actualRead;
                cIndex++;
            }

            AsyncChunkPacket eofPacket;
            eofPacket.isEof = true;
            sendQueue.push(std::move(eofPacket), cancelSignal);
            sendQueue.stop();
        });

        // Network Sender Thread (Dedicated to 100% Wire Saturation)
        uint64_t fileBytesTransferred = (i == 0) ? resumeByteOffset : 0;
        bool networkSuccess = true;

        while (true) {
            AsyncChunkPacket packet;
            if (!sendQueue.pop(packet)) {
                break;
            }

            if (packet.isEof) {
                sendPool.release(std::move(packet.buffer));
                break;
            }

            if (packet.isError || cancelSignal.load() || !readerSuccess) {
                networkSuccess = false;
                sendPool.release(std::move(packet.buffer));
                break;
            }

            uint32_t fileIdxNet = htonl(packet.header.fileIndex);
            uint32_t chunkIdxNet = htonl(packet.header.chunkIndex);
            uint64_t offsetNet = hton64_val(packet.header.offset);
            uint32_t lenNet = htonl(static_cast<uint32_t>(packet.header.length));
            uint32_t crcNet = htonl(packet.header.crc32c);

            std::memcpy(txBuffer.data() + 0, &fileIdxNet, 4);
            std::memcpy(txBuffer.data() + 4, &chunkIdxNet, 4);
            std::memcpy(txBuffer.data() + 8, &offsetNet, 8);
            std::memcpy(txBuffer.data() + 16, &lenNet, 4);
            std::memcpy(txBuffer.data() + 20, &crcNet, 4);
            std::memcpy(txBuffer.data() + 24, packet.buffer->data.data(), packet.header.length);

            if (!SocketTransport::sendRaw(sockFd, txBuffer.data(), 24 + packet.header.length)) {
                networkSuccess = false;
                appendResumeJournal(journalPath, packet.header.chunkIndex);
                sendPool.release(std::move(packet.buffer));
                break;
            }

            fileBytesTransferred += packet.header.length;
            batchBytesTransferred += packet.header.length;
            sendPool.release(std::move(packet.buffer));

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime).count();
            if (elapsedMs >= 100 && progressCb) {
                uint64_t bytesDelta = (batchBytesTransferred >= lastReportBytes) ? (batchBytesTransferred - lastReportBytes) : 0;
                double instantSpeedBps = (bytesDelta * 1000.0) / static_cast<double>(elapsedMs);
                if (currentSmoothedSpeedBps <= 0.0) {
                    currentSmoothedSpeedBps = instantSpeedBps;
                } else {
                    currentSmoothedSpeedBps = 0.65 * currentSmoothedSpeedBps + 0.35 * instantSpeedBps;
                }
                double remainingBytes = (manifest.totalBytes > batchBytesTransferred) ? (manifest.totalBytes - batchBytesTransferred) : 0;
                double etaSec = currentSmoothedSpeedBps > 0 ? (remainingBytes / currentSmoothedSpeedBps) : 0.0;

                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.currentFileIndex = static_cast<uint32_t>(i);
                prog.currentFileName = fileMeta.relativePath;
                prog.fileBytesTransferred = fileBytesTransferred;
                prog.fileSize = fileMeta.fileSize;
                prog.batchBytesTransferred = batchBytesTransferred;
                prog.batchTotalBytes = manifest.totalBytes;
                prog.speedBytesPerSec = currentSmoothedSpeedBps;
                prog.speedMbps = (currentSmoothedSpeedBps * 8.0) / 1000000.0;
                prog.state = TransferState::TRANSFERRING;
                prog.activeStreams = manifest.streamCount;
                prog.etaSeconds = etaSec;

                progressCb(prog);
                lastReportTime = now;
                lastReportBytes = batchBytesTransferred;
            }
        }

        sendQueue.stop();
        if (diskReaderThread.joinable()) {
            diskReaderThread.join();
        }

        if (!networkSuccess || !readerSuccess || cancelSignal.load()) {
            if (progressCb) {
                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.state = TransferState::CANCELLED;
                prog.currentFileName = fileMeta.relativePath;
                progressCb(prog);
            }
            return false;
        }

        std::error_code ecJ;
        if (std::filesystem::exists(journalPath, ecJ)) {
            std::filesystem::remove(journalPath, ecJ);
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

// -----------------------------------------------------------------------------
// High-Speed Asynchronous Decoupled Receiver Pipeline (Producer-Consumer)
// -----------------------------------------------------------------------------
bool TransferEngine::receiveFileBatch(int sockFd,
                                      const TransferManifest& manifest,
                                      const std::filesystem::path& downloadDirectory,
                                      TransferProgressCallback progressCb,
                                      std::atomic<bool>& cancelSignal,
                                      uint64_t resumeByteOffset) {
    uint64_t batchBytesTransferred = resumeByteOffset;
    uint64_t lastReportBytes = resumeByteOffset;
    double currentSmoothedSpeedBps = 0.0;
    auto startTime = std::chrono::steady_clock::now();
    auto lastReportTime = startTime;

    std::error_code ecDl;
    std::filesystem::create_directories(downloadDirectory, ecDl);

    size_t chunkSize = manifest.chunkSize > 0 ? manifest.chunkSize : LARGE_CHUNK_SIZE;
    
    // Fixed Pool of 16 Pre-allocated 1MB chunk buffers (16MB RAM max)
    const size_t POOL_SIZE = 16;
    BufferPool bufferPool(POOL_SIZE, chunkSize);

    for (size_t i = 0; i < manifest.files.size(); ++i) {
        if (cancelSignal) {
            if (progressCb) {
                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.state = TransferState::CANCELLED;
                progressCb(prog);
            }
            return false;
        }

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

        // Emit instant progress notification at start of receiving file
        if (progressCb) {
            TransferProgress initProg;
            initProg.batchId = manifest.batchId;
            initProg.currentFileIndex = static_cast<uint32_t>(i);
            initProg.currentFileName = safeName;
            initProg.fileSize = fileMeta.fileSize;
            initProg.fileBytesTransferred = fileBytesTransferred;
            initProg.batchTotalBytes = manifest.totalBytes;
            initProg.batchBytesTransferred = batchBytesTransferred;
            initProg.state = TransferState::TRANSFERRING;
            initProg.speedBytesPerSec = 0.0;
            initProg.speedMbps = 0.0;
            progressCb(initProg);
        }

        // 1. Open destination file using high-performance Direct POSIX / Native file handles
#if defined(__linux__) || defined(__ANDROID__)
        int fd = open(partPath.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
        if (fd < 0) {
            return false;
        }
#if defined(POSIX_FADV_SEQUENTIAL)
        posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#else
        std::vector<char> fileStreamBuf(4 * 1024 * 1024);
        std::fstream file;
        file.rdbuf()->pubsetbuf(fileStreamBuf.data(), fileStreamBuf.size());

        if (fileBytesTransferred > 0 && std::filesystem::exists(partPath)) {
            file.open(partPath, std::ios::binary | std::ios::in | std::ios::out);
        } else {
            file.open(partPath, std::ios::binary | std::ios::out | std::ios::trunc);
        }
        if (!file.is_open()) return false;
        file.seekp(static_cast<std::streamoff>(fileBytesTransferred));
#endif

        // 2. Launch Background Asynchronous Disk Writer Worker Thread
        AsyncChunkQueue chunkQueue(POOL_SIZE);
        std::atomic<bool> writerSuccess{true};
        std::atomic<uint64_t> currentFileWritten{fileBytesTransferred};

        std::thread diskWriterThread([&]() {
            while (true) {
                AsyncChunkPacket packet;
                if (!chunkQueue.pop(packet)) {
                    break;
                }

                if (packet.isEof) {
                    bufferPool.release(std::move(packet.buffer));
                    break;
                }

                if (packet.isError || cancelSignal.load()) {
                    writerSuccess = false;
                    bufferPool.release(std::move(packet.buffer));
                    break;
                }

                // Verify hardware-accelerated CRC32C checksum
                uint32_t calcCrc = ProtocolSerializer::computeCRC32C(packet.buffer->data.data(), packet.header.length);
                if (calcCrc != packet.header.crc32c) {
                    std::cerr << "CRC32C mismatch on chunk offset: " << packet.header.offset << std::endl;
                    writerSuccess = false;
                    bufferPool.release(std::move(packet.buffer));
                    break;
                }

                // Direct asynchronous disk write
#if defined(__linux__) || defined(__ANDROID__)
                ssize_t written = pwrite(fd, packet.buffer->data.data(), packet.header.length, static_cast<off_t>(packet.header.offset));
                if (written < 0 || static_cast<size_t>(written) != packet.header.length) {
                    writerSuccess = false;
                    bufferPool.release(std::move(packet.buffer));
                    break;
                }
#else
                file.seekp(static_cast<std::streamoff>(packet.header.offset));
                file.write(reinterpret_cast<const char*>(packet.buffer->data.data()), packet.header.length);
                if (!file.good()) {
                    writerSuccess = false;
                    bufferPool.release(std::move(packet.buffer));
                    break;
                }
#endif

                currentFileWritten += packet.header.length;
                bufferPool.release(std::move(packet.buffer));
            }
        });

        // 3. Main Network Receiver Loop (Dedicated to 100% Wire Saturation)
        uint64_t fileBytesReceived = fileBytesTransferred;
        bool networkSuccess = true;

        while (fileBytesReceived < fileMeta.fileSize) {
            if (cancelSignal || !writerSuccess) {
                networkSuccess = false;
                break;
            }

            // Acquire pre-allocated buffer from pool (Zero heap allocations)
            auto block = bufferPool.acquire(cancelSignal);
            if (!block) {
                networkSuccess = false;
                break;
            }

            // Read 24-byte Chunk Header
            uint8_t headerBuf[24];
            if (!SocketTransport::recvRaw(sockFd, headerBuf, sizeof(headerBuf))) {
                bufferPool.release(std::move(block));
                networkSuccess = false;
                break;
            }

            ChunkHeader header;
            if (!ProtocolSerializer::deserializeChunkHeader(headerBuf, sizeof(headerBuf), header)) {
                bufferPool.release(std::move(block));
                networkSuccess = false;
                break;
            }

            if (header.length > block->capacity) {
                bufferPool.release(std::move(block));
                networkSuccess = false;
                break;
            }

            // Read payload directly into pre-allocated memory
            if (header.length > 0) {
                if (!SocketTransport::recvRaw(sockFd, block->data.data(), header.length)) {
                    bufferPool.release(std::move(block));
                    networkSuccess = false;
                    break;
                }
            }

#if defined(__linux__) || defined(__ANDROID__)
#ifdef TCP_QUICKACK
            int quickack = 1;
            setsockopt(sockFd, IPPROTO_TCP, TCP_QUICKACK, (const char*)&quickack, sizeof(quickack));
#endif
#endif

            // Enqueue chunk packet for asynchronous disk write
            AsyncChunkPacket packet;
            packet.header = header;
            packet.buffer = std::move(block);
            packet.isEof = false;
            packet.isError = false;

            if (!chunkQueue.push(std::move(packet), cancelSignal)) {
                networkSuccess = false;
                break;
            }

            fileBytesReceived += header.length;
            batchBytesTransferred += header.length;

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime).count();
            if (elapsedMs >= 100 && progressCb) {
                uint64_t bytesDelta = (batchBytesTransferred >= lastReportBytes) ? (batchBytesTransferred - lastReportBytes) : 0;
                double instantSpeedBps = (bytesDelta * 1000.0) / static_cast<double>(elapsedMs);
                if (currentSmoothedSpeedBps <= 0.0) {
                    currentSmoothedSpeedBps = instantSpeedBps;
                } else {
                    currentSmoothedSpeedBps = 0.65 * currentSmoothedSpeedBps + 0.35 * instantSpeedBps;
                }
                double remainingBytes = (manifest.totalBytes > batchBytesTransferred) ? (manifest.totalBytes - batchBytesTransferred) : 0;
                double etaSec = currentSmoothedSpeedBps > 0 ? (remainingBytes / currentSmoothedSpeedBps) : 0.0;

                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.currentFileIndex = static_cast<uint32_t>(i);
                prog.currentFileName = fileMeta.relativePath;
                prog.fileBytesTransferred = currentFileWritten.load();
                prog.fileSize = fileMeta.fileSize;
                prog.batchBytesTransferred = batchBytesTransferred;
                prog.batchTotalBytes = manifest.totalBytes;
                prog.speedBytesPerSec = currentSmoothedSpeedBps;
                prog.speedMbps = (currentSmoothedSpeedBps * 8.0) / 1000000.0;
                prog.state = TransferState::TRANSFERRING;
                prog.activeStreams = manifest.streamCount;
                prog.etaSeconds = etaSec;

                progressCb(prog);
                lastReportTime = now;
                lastReportBytes = batchBytesTransferred;
            }
        }

        // 4. Signal EOF to writer and join thread
        AsyncChunkPacket eofPacket;
        eofPacket.isEof = true;
        eofPacket.isError = !networkSuccess;
        chunkQueue.push(std::move(eofPacket), cancelSignal);
        chunkQueue.stop();

        if (diskWriterThread.joinable()) {
            diskWriterThread.join();
        }

        // 5. Close file descriptors
#if defined(__linux__) || defined(__ANDROID__)
        close(fd);
#else
        file.close();
#endif

        if (!networkSuccess || !writerSuccess || cancelSignal) {
            if (progressCb) {
                TransferProgress prog;
                prog.batchId = manifest.batchId;
                prog.state = TransferState::CANCELLED;
                prog.currentFileName = fileMeta.relativePath;
                progressCb(prog);
            }
            return false;
        }

        if (std::filesystem::exists(journalPath)) {
            std::filesystem::remove(journalPath);
        }

        // 6. Atomic Rename Part -> Final
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
