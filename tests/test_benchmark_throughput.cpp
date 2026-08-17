#include "aerosync/socket_transport.hpp"
#include "aerosync/protocol_serializer.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
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

namespace {

struct BenchmarkConfig {
    double sizeGB = 1.0;
    size_t chunkSizeMB = 4;
    size_t streams = 4;
    size_t bufferMB = 8;
    std::string mode = "loopback"; // "server", "client", "loopback"
    std::string host = "127.0.0.1";
    uint16_t basePort = 45450;
    bool withDisk = false;
};

void printUsage() {
    std::cout << "AeroSync High-Throughput Synthetic N-GB Benchmark Harness" << std::endl;
    std::cout << "Usage: aerosync_benchmark [options]" << std::endl;
    std::cout << "  --size-gb <N>         Total transfer size in Gigabytes (default: 1.0)" << std::endl;
    std::cout << "  --chunk-size-mb <M>   Chunk size in Megabytes (default: 4)" << std::endl;
    std::cout << "  --streams <S>         Number of parallel TCP streams (default: 4)" << std::endl;
    std::cout << "  --buffer-mb <B>       Socket buffer size SO_SNDBUF/SO_RCVBUF in MB (default: 8)" << std::endl;
    std::cout << "  --mode <mode>         Mode: 'loopback', 'server', 'client' (default: loopback)" << std::endl;
    std::cout << "  --host <IP>           Remote host IP for client mode (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port <P>            Base TCP port (default: 45450)" << std::endl;
    std::cout << "  --with-disk           Persist to disk (tests NVMe/UFS flash vs pure network link)" << std::endl;
    std::cout << "  --no-disk             In-memory synthetic zero-disk mode (default)" << std::endl;
}

BenchmarkConfig parseArgs(int argc, char* argv[]) {
    BenchmarkConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            exit(0);
        } else if (arg == "--size-gb" && i + 1 < argc) {
            config.sizeGB = std::stod(argv[++i]);
        } else if (arg == "--chunk-size-mb" && i + 1 < argc) {
            config.chunkSizeMB = std::stoul(argv[++i]);
        } else if (arg == "--streams" && i + 1 < argc) {
            config.streams = std::stoul(argv[++i]);
        } else if (arg == "--buffer-mb" && i + 1 < argc) {
            config.bufferMB = std::stoul(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            config.mode = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.basePort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--with-disk") {
            config.withDisk = true;
        } else if (arg == "--no-disk") {
            config.withDisk = false;
        }
    }
    return config;
}

void runServer(const BenchmarkConfig& config) {
    uint64_t totalTargetBytes = static_cast<uint64_t>(config.sizeGB * 1024.0 * 1024.0 * 1024.0);
    uint64_t bytesPerStream = totalTargetBytes / config.streams;
    size_t chunkBytes = config.chunkSizeMB * 1024 * 1024;
    size_t socketBufBytes = config.bufferMB * 1024 * 1024;

    std::cout << "[SERVER] Initializing " << config.streams << " parallel receiver listener ports..." << std::endl;

    std::vector<int> serverSocks(config.streams);
    for (size_t s = 0; s < config.streams; ++s) {
        uint16_t port = config.basePort + static_cast<uint16_t>(s);
        serverSocks[s] = aerosync::SocketTransport::createTcpServer(port);
        if (serverSocks[s] < 0) {
            std::cerr << "[SERVER ERROR] Failed to bind server port " << port << std::endl;
            return;
        }
    }

    std::cout << "[SERVER] Listening on ports " << config.basePort << " - " << (config.basePort + config.streams - 1) << std::endl;

    std::vector<std::thread> receiverThreads;
    std::atomic<uint64_t> totalReceivedBytes{0};
    auto startTime = std::chrono::steady_clock::now();

    for (size_t s = 0; s < config.streams; ++s) {
        receiverThreads.emplace_back([&, s]() {
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int len = sizeof(clientAddr);
#else
            socklen_t len = sizeof(clientAddr);
#endif
            socket_t clientSock = accept(serverSocks[s], (sockaddr*)&clientAddr, &len);
            if (clientSock == INVALID_SOCKET) return;

            aerosync::SocketTransport::configureHighThroughputSocket(static_cast<int>(clientSock), socketBufBytes);

            std::vector<uint8_t> buffer(chunkBytes);
            uint64_t streamReceived = 0;

            std::ofstream diskFile;
            if (config.withDisk) {
                std::string fname = "bench_recv_stream_" + std::to_string(s) + ".part";
                diskFile.open(fname, std::ios::binary | std::ios::trunc);
            }

            while (streamReceived < bytesPerStream) {
                size_t toRecv = std::min(static_cast<uint64_t>(chunkBytes), bytesPerStream - streamReceived);
                if (!aerosync::SocketTransport::recvRaw(static_cast<int>(clientSock), buffer.data(), toRecv)) {
                    break;
                }

                // Checksum computation validation
                volatile uint32_t crc = aerosync::ProtocolSerializer::computeCRC32C(buffer.data(), toRecv);
                (void)crc;

                if (config.withDisk && diskFile.is_open()) {
                    diskFile.write(reinterpret_cast<const char*>(buffer.data()), toRecv);
                }

                streamReceived += toRecv;
                totalReceivedBytes += toRecv;
            }

            if (config.withDisk && diskFile.is_open()) {
                diskFile.close();
            }

            CLOSE_SOCKET(clientSock);
            CLOSE_SOCKET(serverSocks[s]);
        });
    }

    // Monitor Thread
    std::thread reporterThread([&]() {
        while (totalReceivedBytes < totalTargetBytes) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            auto now = std::chrono::steady_clock::now();
            double elapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
            uint64_t currentBytes = totalReceivedBytes.load();
            if (elapsedSec > 0) {
                double mbps = (currentBytes * 8.0) / (elapsedSec * 1000000.0);
                double mbSec = currentBytes / (elapsedSec * 1024.0 * 1024.0);
                double pct = (static_cast<double>(currentBytes) / totalTargetBytes) * 100.0;
                std::cout << "\r[BENCHMARK RECV] Progress: " << std::fixed << std::setprecision(1) << pct
                          << "% | Speed: " << std::setprecision(2) << mbps << " Mbps (" << mbSec << " MB/s)" << std::flush;
            }
        }
    });

    for (auto& t : receiverThreads) {
        if (t.joinable()) t.join();
    }
    if (reporterThread.joinable()) reporterThread.join();

    auto endTime = std::chrono::steady_clock::now();
    double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000000.0;
    double finalMbps = (totalReceivedBytes * 8.0) / (totalElapsedSec * 1000000.0);
    double finalMBSec = totalReceivedBytes / (totalElapsedSec * 1024.0 * 1024.0);

    std::cout << "\n\n========================================================" << std::endl;
    std::cout << ">>> BENCHMARK SERVER RESULTS <<<" << std::endl;
    std::cout << "Total Transferred: " << (totalReceivedBytes / (1024 * 1024)) << " MB in " << totalElapsedSec << " s" << std::endl;
    std::cout << "Throughput:        " << finalMbps << " Mbps (" << finalMBSec << " MB/s)" << std::endl;
    std::cout << "========================================================" << std::endl;
}

double runClient(const BenchmarkConfig& config) {
    uint64_t totalTargetBytes = static_cast<uint64_t>(config.sizeGB * 1024.0 * 1024.0 * 1024.0);
    uint64_t bytesPerStream = totalTargetBytes / config.streams;
    size_t chunkBytes = config.chunkSizeMB * 1024 * 1024;
    size_t socketBufBytes = config.bufferMB * 1024 * 1024;

    std::cout << "[CLIENT] Connecting " << config.streams << " parallel TCP streams to " << config.host << "..." << std::endl;

    std::vector<int> clientSocks(config.streams);
    for (size_t s = 0; s < config.streams; ++s) {
        uint16_t port = config.basePort + static_cast<uint16_t>(s);
        int sock = -1;
        for (int retry = 0; retry < 20; ++retry) {
            sock = aerosync::SocketTransport::connectTcpClient(config.host, port, 2000);
            if (sock >= 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (sock < 0) {
            std::cerr << "[CLIENT ERROR] Failed to connect stream " << s << " on port " << port << std::endl;
            return 0.0;
        }
        aerosync::SocketTransport::configureHighThroughputSocket(sock, socketBufBytes);
        clientSocks[s] = sock;
    }

    std::cout << "[CLIENT] All " << config.streams << " streams connected. Commencing high-speed pipelined transmission..." << std::endl;

    std::atomic<uint64_t> totalSentBytes{0};
    auto startTime = std::chrono::steady_clock::now();

    std::vector<std::thread> senderThreads;
    for (size_t s = 0; s < config.streams; ++s) {
        senderThreads.emplace_back([&, s]() {
            int sock = clientSocks[s];
            std::vector<uint8_t> buffer(chunkBytes, static_cast<uint8_t>(0xAA ^ s));
            uint64_t streamSent = 0;

            while (streamSent < bytesPerStream) {
                size_t toSend = std::min(static_cast<uint64_t>(chunkBytes), bytesPerStream - streamSent);
                
                // Hardware CRC32C computation
                volatile uint32_t crc = aerosync::ProtocolSerializer::computeCRC32C(buffer.data(), toSend);
                (void)crc;

                if (!aerosync::SocketTransport::sendRaw(sock, buffer.data(), toSend)) {
                    break;
                }

                streamSent += toSend;
                totalSentBytes += toSend;
            }

            CLOSE_SOCKET(sock);
        });
    }

    // Monitor Thread
    std::thread reporterThread([&]() {
        while (totalSentBytes < totalTargetBytes) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            auto now = std::chrono::steady_clock::now();
            double elapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0;
            uint64_t currentBytes = totalSentBytes.load();
            if (elapsedSec > 0) {
                double mbps = (currentBytes * 8.0) / (elapsedSec * 1000000.0);
                double mbSec = currentBytes / (elapsedSec * 1024.0 * 1024.0);
                double pct = (static_cast<double>(currentBytes) / totalTargetBytes) * 100.0;
                std::cout << "\r[BENCHMARK SEND] Progress: " << std::fixed << std::setprecision(1) << pct
                          << "% | Speed: " << std::setprecision(2) << mbps << " Mbps (" << mbSec << " MB/s)" << std::flush;
            }
        }
    });

    for (auto& t : senderThreads) {
        if (t.joinable()) t.join();
    }
    if (reporterThread.joinable()) reporterThread.join();

    auto endTime = std::chrono::steady_clock::now();
    double totalElapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000000.0;
    double finalMbps = (totalSentBytes * 8.0) / (totalElapsedSec * 1000000.0);
    double finalMBSec = totalSentBytes / (totalElapsedSec * 1024.0 * 1024.0);

    std::cout << "\n\n========================================================" << std::endl;
    std::cout << ">>> BENCHMARK CLIENT RESULTS <<<" << std::endl;
    std::cout << "Total Transferred: " << (totalSentBytes / (1024 * 1024)) << " MB in " << totalElapsedSec << " s" << std::endl;
    std::cout << "Throughput:        " << finalMbps << " Mbps (" << finalMBSec << " MB/s)" << std::endl;
    std::cout << "========================================================" << std::endl;

    return finalMbps;
}

void runLoopback(const BenchmarkConfig& config) {
    std::cout << "========================================================" << std::endl;
    std::cout << "AEROSYNC HIGH-THROUGHPUT SYNTHETIC N-GB BENCHMARK" << std::endl;
    std::cout << "Transfer Size:     " << config.sizeGB << " GB (" << (config.sizeGB * 1024) << " MB)" << std::endl;
    std::cout << "Chunk Size:        " << config.chunkSizeMB << " MB" << std::endl;
    std::cout << "Parallel Streams:  " << config.streams << " TCP Streams" << std::endl;
    std::cout << "Socket Buffer:     " << config.bufferMB << " MB (SO_SNDBUF / SO_RCVBUF)" << std::endl;
    std::cout << "Target Throughput: > 400.0 Mbps" << std::endl;
    std::cout << "Persistence:       " << (config.withDisk ? "With Disk I/O" : "In-Memory (Zero-Disk)") << std::endl;
    std::cout << "========================================================\n" << std::endl;

    std::thread serverThread([&]() {
        runServer(config);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    double measuredMbps = runClient(config);

    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::cout << "\n========================================================" << std::endl;
    if (measuredMbps >= 400.0) {
        std::cout << ">>> [SUCCESS] 400+ Mbps TARGET EXCEEDED: " << measuredMbps << " Mbps! <<<" << std::endl;
    } else {
        std::cout << ">>> [INFO] Measured Throughput: " << measuredMbps << " Mbps <<<" << std::endl;
    }
    std::cout << "========================================================" << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    BenchmarkConfig config = parseArgs(argc, argv);

    if (config.mode == "server") {
        runServer(config);
    } else if (config.mode == "client") {
        runClient(config);
    } else {
        runLoopback(config);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
