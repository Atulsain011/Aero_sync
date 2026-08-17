#include "aerosync/socket_transport.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <io.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "mswsock.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <sys/sendfile.h>
    #include <sys/mman.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace aerosync {

void SocketTransport::configureHighThroughputSocket(int sockFd, size_t bufferSize) {
    socket_t s = static_cast<socket_t>(sockFd);

    // Socket buffer scaling: 16 MB SO_SNDBUF and SO_RCVBUF for ultra-high throughput saturation
    int bufSize = static_cast<int>(bufferSize > 0 ? bufferSize : (16 * 1024 * 1024));
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));

    // Disable Nagle's Algorithm (TCP_NODELAY) for ultra-low latency chunk dispatch
    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

#if defined(__linux__) || defined(__ANDROID__)
#ifdef TCP_QUICKACK
    int quickack = 1;
    setsockopt(s, IPPROTO_TCP, TCP_QUICKACK, (const char*)&quickack, sizeof(quickack));
#endif
#endif

    // Enable TCP KeepAlive
    int keepalive = 1;
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive));

#ifdef _WIN32
    DWORD tv = 60000; // 60s timeout for stream resilience
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif
}

int SocketTransport::createTcpServer(uint16_t port) {
    socket_t listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) return -1;

    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(listenSock);
        return -1;
    }

    if (listen(listenSock, 64) == SOCKET_ERROR) {
        CLOSE_SOCKET(listenSock);
        return -1;
    }

    configureHighThroughputSocket(static_cast<int>(listenSock));
    return static_cast<int>(listenSock);
}

int SocketTransport::connectTcpClient(const std::string& ip, uint16_t port, int timeoutMs) {
    socket_t clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock == INVALID_SOCKET) return -1;

    configureHighThroughputSocket(static_cast<int>(clientSock));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        CLOSE_SOCKET(clientSock);
        return -1;
    }

    int tMs = (timeoutMs > 0) ? timeoutMs : 5000;

    // Set non-blocking mode for connection timeout
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(clientSock, FIONBIO, &mode);
#else
    int flags = fcntl(clientSock, F_GETFL, 0);
    fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);
#endif

    int res = connect(clientSock, (sockaddr*)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            CLOSE_SOCKET(clientSock);
            return -1;
        }
#else
        if (errno != EINPROGRESS) {
            CLOSE_SOCKET(clientSock);
            return -1;
        }
#endif

        fd_set writeSet, errSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errSet);
        FD_SET(clientSock, &writeSet);
        FD_SET(clientSock, &errSet);

        timeval tv;
        tv.tv_sec = tMs / 1000;
        tv.tv_usec = (tMs % 1000) * 1000;

        int selRes = select(static_cast<int>(clientSock + 1), NULL, &writeSet, &errSet, &tv);
        if (selRes <= 0 || FD_ISSET(clientSock, &errSet) || !FD_ISSET(clientSock, &writeSet)) {
            CLOSE_SOCKET(clientSock);
            return -1;
        }

        // Verify socket error code
        int soError = 0;
#ifdef _WIN32
        int len = sizeof(soError);
        getsockopt(clientSock, SOL_SOCKET, SO_ERROR, (char*)&soError, &len);
#else
        socklen_t len = sizeof(soError);
        getsockopt(clientSock, SOL_SOCKET, SO_ERROR, &soError, &len);
#endif
        if (soError != 0) {
            CLOSE_SOCKET(clientSock);
            return -1;
        }
    }

    // Restore blocking mode
#ifdef _WIN32
    mode = 0;
    ioctlsocket(clientSock, FIONBIO, &mode);
#else
    fcntl(clientSock, F_SETFL, flags);
#endif

    return static_cast<int>(clientSock);
}

bool SocketTransport::sendRaw(int sockFd, const uint8_t* buffer, size_t length) {
    socket_t s = static_cast<socket_t>(sockFd);
    size_t totalSent = 0;
    while (totalSent < length) {
        int sent = send(s, (const char*)(buffer + totalSent), static_cast<int>(length - totalSent), 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool SocketTransport::recvRaw(int sockFd, uint8_t* buffer, size_t length) {
    socket_t s = static_cast<socket_t>(sockFd);
    size_t totalRecv = 0;
    while (totalRecv < length) {
        int recvBytes = recv(s, (char*)(buffer + totalRecv), static_cast<int>(length - totalRecv), 0);
        if (recvBytes <= 0) return false;
        totalRecv += recvBytes;
    }
    return true;
}

bool SocketTransport::sendZeroCopy(int sockFd, int fileFd, uint64_t offset, size_t length) {
#ifdef _WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(fileFd);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    OVERLAPPED overlapped = {0};
    overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

    BOOL ok = TransmitFile(static_cast<SOCKET>(sockFd), hFile, static_cast<DWORD>(length), 0, &overlapped, NULL, 0);
    return (ok == TRUE);
#elif defined(__linux__) || defined(__ANDROID__)
    off_t off = static_cast<off_t>(offset);
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent = sendfile(sockFd, fileFd, &off, length - totalSent);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            return false;
        }
        totalSent += sent;
    }
    return true;
#else
    void* map = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fileFd, offset);
    if (map == MAP_FAILED) return false;
    bool ok = sendRaw(sockFd, static_cast<const uint8_t*>(map), length);
    munmap(map, length);
    return ok;
#endif
}

bool SocketTransport::sendControlFrame(int sockFd, ControlMessageType msgType, uint32_t seq, const std::string& payload) {
    std::string frame = ProtocolSerializer::encodeControlFrame(msgType, seq, payload);
    return sendRaw(sockFd, reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
}

bool SocketTransport::recvControlFrame(int sockFd, ControlMessageType& outType, uint32_t& outSeq, std::string& outPayload) {
    // Read 13-byte header first: [4B Magic] [1B MsgType] [4B SeqNum] [4B PayloadLen]
    uint8_t headerBuf[13];
    if (!recvRaw(sockFd, headerBuf, sizeof(headerBuf))) return false;

    uint32_t magicNet = 0;
    std::memcpy(&magicNet, headerBuf, 4);
    if (ntohl(magicNet) != WIRE_MAGIC) return false;

    outType = static_cast<ControlMessageType>(headerBuf[4]);

    uint32_t seqNet = 0;
    std::memcpy(&seqNet, headerBuf + 5, 4);
    outSeq = ntohl(seqNet);

    uint32_t lenNet = 0;
    std::memcpy(&lenNet, headerBuf + 9, 4);
    uint32_t payloadLen = ntohl(lenNet);

    if (payloadLen > 32 * 1024 * 1024) return false; // Safety limit: 32MB

    outPayload.resize(payloadLen);
    if (payloadLen > 0) {
        if (!recvRaw(sockFd, reinterpret_cast<uint8_t*>(outPayload.data()), payloadLen)) {
            return false;
        }
    }
    return true;
}

// Framing compatibility (wire magic + 4-byte length + payload)
bool SocketTransport::sendFrame(int sockFd, const std::string& payload) {
    uint32_t magic = htonl(WIRE_MAGIC);
    uint32_t len = htonl(static_cast<uint32_t>(payload.length()));

    if (!sendRaw(sockFd, (const uint8_t*)&magic, sizeof(magic))) return false;
    if (!sendRaw(sockFd, (const uint8_t*)&len, sizeof(len))) return false;
    if (!payload.empty()) {
        if (!sendRaw(sockFd, (const uint8_t*)payload.data(), payload.length())) return false;
    }
    return true;
}

bool SocketTransport::recvFrame(int sockFd, std::string& payload) {
    uint32_t magicNet = 0;
    uint32_t lenNet = 0;

    if (!recvRaw(sockFd, (uint8_t*)&magicNet, sizeof(magicNet))) return false;
    if (!recvRaw(sockFd, (uint8_t*)&lenNet, sizeof(lenNet))) return false;

    uint32_t magic = ntohl(magicNet);
    uint32_t len = ntohl(lenNet);

    if (magic != WIRE_MAGIC) return false;
    if (len > 32 * 1024 * 1024) return false; // 32MB payload limit

    payload.resize(len);
    if (len > 0) {
        if (!recvRaw(sockFd, (uint8_t*)payload.data(), len)) return false;
    }
    return true;
}

uint32_t SocketTransport::computeCRC32C(const uint8_t* data, size_t length) {
    return ProtocolSerializer::computeCRC32C(data, length);
}

} // namespace aerosync
