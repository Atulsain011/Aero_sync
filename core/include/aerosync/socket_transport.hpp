#ifndef AEROSYNC_SOCKET_TRANSPORT_HPP
#define AEROSYNC_SOCKET_TRANSPORT_HPP

#include "types.hpp"
#include "protocol_serializer.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace aerosync {

class SocketTransport {
public:
    static int createTcpServer(uint16_t port);
    static int connectTcpClient(const std::string& ip, uint16_t port, int timeoutMs = 3000);
    static void configureHighThroughputSocket(int sockFd, size_t bufferSize = 8 * 1024 * 1024);

    static bool sendControlFrame(int sockFd, ControlMessageType msgType, uint32_t seq, const std::string& payload);
    static bool recvControlFrame(int sockFd, ControlMessageType& outType, uint32_t& outSeq, std::string& outPayload);

    // Raw framing compatibility
    static bool sendFrame(int sockFd, const std::string& payload);
    static bool recvFrame(int sockFd, std::string& payload);

    static bool sendRaw(int sockFd, const uint8_t* buffer, size_t length);
    static bool recvRaw(int sockFd, uint8_t* buffer, size_t length);
    static bool sendZeroCopy(int sockFd, int fileFd, uint64_t offset, size_t length);

    static uint32_t computeCRC32C(const uint8_t* data, size_t length);
};

} // namespace aerosync

#endif // AEROSYNC_SOCKET_TRANSPORT_HPP
