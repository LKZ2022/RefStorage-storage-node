//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.


#include "../include/SocketHandle.hpp"




namespace ref_storage::net {

    SocketHandle::SocketHandle() noexcept = default;

    SocketHandle::SocketHandle(NativeSocketType handle) noexcept : _handle(handle) {}

    SocketHandle & SocketHandle::operator=(SocketHandle &&other) noexcept {
        if (this != &other) {
            close_handle();
            _handle = std::exchange(other._handle, kInvalid);
        }
        return *this;
    }

    SocketHandle::~SocketHandle() noexcept {
        close_handle();
    }

    SocketHandle::NativeSocketType SocketHandle::native_handle() const noexcept {
        return _handle;
    }

    SocketHandle::operator bool() const noexcept {
        return _handle != kInvalid;
    }

    bool SocketHandle::is_valid_handle() const noexcept {
        return static_cast<bool>(*this);
    }

    bool SocketHandle::operator==(const SocketHandle &other) const noexcept {
        return _handle == other._handle;
    }

    bool SocketHandle::operator!=(const SocketHandle &other) const noexcept {
        return _handle != other._handle;
    }

    bool SocketHandle::operator<(const SocketHandle &other) const noexcept {
        return _handle < other._handle;
    }

    bool SocketHandle::operator<=(const SocketHandle &other) const noexcept {
        return _handle <= other._handle;
    }

    bool SocketHandle::operator>(const SocketHandle &other) const noexcept {
        return _handle > other._handle;
    }

    bool SocketHandle::operator>=(const SocketHandle &other) const noexcept {
        return _handle >= other._handle;
    }

    SocketHandle::NativeSocketType SocketHandle::release_handle() noexcept {
        return std::exchange(_handle, kInvalid);
    }

    void SocketHandle::reset_handle(NativeSocketType new_handle) noexcept {
        close_handle();
        _handle = new_handle;
    }

    void SocketHandle::close_handle() noexcept {
        if (is_valid_handle()) {
            /* Graceful shutdown: no longer sending or receiving new data,
             * but the operating system will flush the underlying buffers. */
#ifdef _WIN32
            shutdown(_handle, SD_BOTH);
            int result = closesocket(_handle);
#elif __linux__
            shutdown(_handle, SHUT_RDWR);
            int result = close(_handle);
#endif

            if (result == -1) {
#ifdef _WIN32
                std::cerr << "Warning: close_handle() failed on handle " << _handle
                          << ", Error Code: " << WSAGetLastError() << std::endl;
#elif __linux__
                std::cerr << "Warning: close_handle() failed on handle " << _handle
                          << ", Error Code: " << errno << std::endl;
#endif
            }

            _handle = kInvalid;
        }
    }

    int SocketHandle::bind_handle(const struct sockaddr *addr, socklen_t addrlen)  {
        if (!is_valid_handle()) {
            throw_last_error("bind_handle() failed: Invalid socket ");
        }
        return bind(_handle, addr, addrlen);
    }

    int SocketHandle::listen_handle(int backlog) {
        if (!is_valid_handle()) {
            throw_last_error("listen_handle() failed: Invalid socket ");
        }
        return listen(_handle, backlog);
    }

    SocketHandle SocketHandle::accept_handle(struct sockaddr *addr, socklen_t *addrlen) const {
        if (!is_valid_handle()) {
            return SocketHandle(kInvalid);
        }
        NativeSocketType new_handle = accept(_handle, addr, addrlen);
        return SocketHandle(new_handle);
    }

    SocketHandle SocketHandle::create_socket_handle() {
        NativeSocketType sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock == kInvalid) {
            throw_last_error("socket() failed");
        }
        return SocketHandle(sock);
    }

    void SocketHandle::throw_last_error(const char *operation) {
#ifdef _WIN32
        throw std::system_error(WSAGetLastError(), std::system_category(), operation);
#elif __linux__
        throw std::system_error(errno, std::system_category(), operation);
#endif
    }
}
