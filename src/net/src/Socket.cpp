//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#include "../include/Socket.hpp"
#include "utils/include/AsyncLogger.hpp"

namespace ref_storage::net {

    Socket::Socket() {
#ifdef _WIN32
        _fd = SocketHandle::create_socket_handle();
        if (_fd.native_handle() == INVALID_SOCKET) throw_last_error("create_socket_handle() failed: ");
#elif __linux__
        _fd = SocketHandle::create_socket_handle();
        if (_fd.native_handle() == -1) throw_last_error("create_socket_handle() failed: ");
#endif
        LOG_INFO("Socket created successfully. FD: {}", _fd.native_handle());
    }

    Socket::Socket(SocketHandle&& fd) : _fd(std::move(fd)) {
        if (_fd.is_valid_handle()) LOG_INFO("Socket handle moved/wrapped. FD: {}", _fd.native_handle());
    }

    Socket::Socket(Socket &&other) noexcept {
        _fd = std::exchange(other._fd,SocketHandle());
    }

    Socket & Socket::operator=(Socket &&other) noexcept {
        if (this != &other) {
            if (_fd.is_valid_handle()) {
                LOG_INFO("Closing old socket FD: {} due to move assignment.", _fd.native_handle());
                _fd.close_handle();
            }
            _fd = std::move(other._fd);
            other._fd = SocketHandle();
        }
        return *this;
    }

    Socket::~Socket() {
        if (_fd.is_valid_handle()) {
            LOG_INFO("Socket closing. FD: {}", _fd.native_handle());
            _fd.close_handle();
        }
    }

    void Socket::setReuseAddress(bool enable) {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
        int opt = enable ? 1 : 0;
#ifdef _WIN32
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        if (result < 0) throw_last_error("setsockopt() failed: ");
        _reuseAddress = enable;
        LOG_DEBUG("Socket SO_REUSEADDR set to {}", enable);
    }

    void Socket::setKeepAlive(bool enable) {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
        int opt = enable ? 1 : 0;
#ifdef _WIN32
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_KEEPALIVE, (const char*)&opt, sizeof(opt));
#else
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
#endif
        if (result < 0) throw_last_error("setsockopt() failed: ");
        _keepAlive = enable;
        LOG_DEBUG("Socket SO_KEEPALIVE set to {}", enable);
    }

    void Socket::setNonBlocking(bool enable) {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
#ifdef _WIN32
        u_long mode = enable ? 1 : 0;
        if (ioctlsocket(_fd.native_handle(), FIONBIO, &mode) == SOCKET_ERROR) throw_last_error("Failed to set non-blocking mode");
#elif __linux__
        int flags = fcntl(_fd.native_handle(), F_GETFL, 0);
        if (flags == -1) throw_last_error("Failed to get socket flags");
        if (enable) flags |= O_NONBLOCK; else flags &= ~O_NONBLOCK;
        if (fcntl(_fd.native_handle(), F_SETFL, flags) == -1) throw_last_error("Failed to set non-blocking mode");
#endif
        _nonBlocking = enable;
        LOG_DEBUG("Socket NonBlocking mode set to {}", enable);
    }

    void Socket::bindAndListen(int port, const char *address) {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        if (address == nullptr) addr.sin6_addr = in6addr_any;
        else if (inet_pton(AF_INET6, address, &addr.sin6_addr) != 1) throw_last_error("Invalid IPv6 address");

        int no = 0;
        setsockopt(_fd.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no));
        if (_fd.bind_handle(reinterpret_cast<struct sockaddr *>(&addr),sizeof(addr)) < 0) throw_last_error("bind() failed: ");
        if (_fd.listen_handle() == -1) throw_last_error("listen() failed: ");
        LOG_INFO("Socket successfully bound and listening on port {}", port);
    }

    Socket Socket::acceptClient() const {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
        Socket CommunicationSocket = Socket(_fd.accept_handle());
        if (!CommunicationSocket._fd.is_valid_handle()) throw_last_error("accept() failed");
        return CommunicationSocket;
    }

    void Socket::sendData(const void *buf, size_t len, int timeout_ms) const {
        if (!buf || len == 0 || !_fd.is_valid_handle()) return;

        uint32_t net_len = htonl(static_cast<uint32_t>(len));
        const char* head_ptr = reinterpret_cast<const char*>(&net_len);
        size_t head_remaining = sizeof(net_len);

        while (head_remaining > 0) {
#ifdef _WIN32
            int loc_sent = send(_fd.native_handle(), head_ptr, static_cast<int>(head_remaining), 0);
#else
            ssize_t loc_sent = send(_fd.native_handle(), head_ptr, head_remaining, 0);
#endif
            if (loc_sent > 0) { head_remaining -= loc_sent; head_ptr += loc_sent; }
            else if (loc_sent == 0) { LOG_INFO("Connection closed by peer during header send. FD: {}", _fd.native_handle()); return; }
            else throw_last_error("send() header failed: ");
        }

        size_t remaining = len;
        const char* sent_ptr = static_cast<const char*>(buf);

        while (remaining > 0) {
#ifdef _WIN32
            int loc_sent = send(_fd.native_handle(), sent_ptr, static_cast<int>(remaining), 0);
#else
            ssize_t loc_sent = send(_fd.native_handle(), sent_ptr, remaining, 0);
#endif
            if (loc_sent > 0) { remaining -= loc_sent; sent_ptr += loc_sent; }
            else if (loc_sent == 0) { LOG_INFO("Connection closed by peer during data send. FD: {}", _fd.native_handle()); return; }
            else throw_last_error("send() data failed: ");
        }
    }

    std::vector<char> Socket::recvData(size_t expectedSize) const {
        if (!_fd.is_valid_handle()) throw_last_error("Invalid socket. ");
        std::vector<char> buffet;

        if (expectedSize > 0) {
            buffet.resize(expectedSize);
            size_t total_received = 0;
            while (total_received < expectedSize) {
#ifdef _WIN32
                int result = recv(_fd.native_handle(), buffet.data() + total_received, static_cast<int>(expectedSize - total_received), 0);
#else
                ssize_t result = recv(_fd.native_handle(), buffet.data() + total_received, expectedSize - total_received, 0);
#endif
                if (result > 0) total_received += static_cast<size_t>(result);
                else if (result == 0) { LOG_INFO("Connection closed by peer. FD: {}", _fd.native_handle()); buffet.resize(total_received); return buffet; }
                else throw_last_error("recv() failed");
            }
        } else {
            uint32_t datasize = 0;
            size_t head_received = 0;
            while (head_received < sizeof(datasize)) {
#ifdef _WIN32
                int result = recv(_fd.native_handle(), reinterpret_cast<char*>(&datasize) + head_received, static_cast<int>(sizeof(datasize) - head_received), 0);
#else
                ssize_t result = recv(_fd.native_handle(), reinterpret_cast<char*>(&datasize) + head_received, sizeof(datasize) - head_received, 0);
#endif
                if (result > 0) head_received += static_cast<size_t>(result);
                else if (result == 0) { LOG_INFO("Connection closed by peer. FD: {}", _fd.native_handle()); return buffet; }
                else throw_last_error("recv() header failed");
            }

            datasize = ntohl(datasize);
            if (datasize == 0) return buffet;
            buffet.resize(datasize);
            size_t total_received = 0;

            while (total_received < datasize) {
#ifdef _WIN32
                int result = recv(_fd.native_handle(), buffet.data() + total_received, static_cast<int>(datasize - total_received), 0);
#else
                ssize_t result = recv(_fd.native_handle(), buffet.data() + total_received, datasize - total_received, 0);
#endif
                if (result > 0) total_received += static_cast<size_t>(result);
                else if (result == 0) { LOG_INFO("Connection closed by peer. FD: {}", _fd.native_handle()); buffet.resize(total_received); return buffet; }
                else throw_last_error("recv() payload failed");
            }
        }
        return buffet;
    }

    void Socket::sendFile(const std::string& filepath) {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open file.");
        TRANSMIT_FILE_BUFFERS transmit_buffers = {};
        BOOL result = TransmitFile(_fd.native_handle(), hFile, 0, 0, nullptr, &transmit_buffers, TF_USE_DEFAULT_WORKER);
        CloseHandle(hFile);
        if (result == FALSE) throw std::runtime_error("TransmitFile() failed. ");
#elif __linux__
        int file_fd = open(filepath.c_str(), O_RDONLY);
        if (file_fd < 0) throw std::runtime_error("Failed to open file.");
        struct stat stat_buf{}; fstat(file_fd, &stat_buf);
        off_t offset = 0; ssize_t sent_bytes;
        while (offset < stat_buf.st_size) {
            sent_bytes = sendfile(_fd.native_handle(), file_fd, &offset, stat_buf.st_size - offset);
            if (sent_bytes <= 0) { if (errno == EAGAIN) continue; close(file_fd); throw std::runtime_error("Failed to send file."); }
        }
        close(file_fd);
#endif
    }

    void Socket::throw_last_error(const char *operation) const {
#ifdef _WIN32
        int err = WSAGetLastError();
        LOG_ERROR("Socket operation '{}' failed on FD: {}. WSA code: {}", operation, _fd.native_handle(), err);
        throw std::system_error(err, std::system_category(), operation);
#elif __linux__
        int err = errno;
        LOG_ERROR("Socket operation '{}' failed on FD: {}. errno: {}", operation, _fd.native_handle(), err);
        throw std::system_error(err, std::system_category(), operation);
#endif
    }
}