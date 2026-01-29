//Copyright (c) 2026 Liu Kaizhi
//Licensed under the Apache License, Version 2.0.

#pragma once
#include <string>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>
#include <windows.h>
#pragma comment(lib, "Mswsock.lib")

#elif __linux__

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <cerrno>

#endif

namespace ref_storage::net {

    /* The following C++ class encapsulates native sockets to provide multiple APIs for invocation,
     * allowing the server to properly receive data transmitted by clients.
     * It also uses conditional compilation, enabling normal operation in both Windows and Linux environments.
     * All error handling is done using throw statements in C++ and caught by catch blocks.
     * For higher performance, consider converting all throws to error code handling and removing all try and catch blocks.
     * Note: This class is intended for server-side use only. */

    class Socket {

    public:

        /* Constructor: Creates a new TCP Socket
         * Internal Handling of WSAStartup (Windows) */
        Socket();

        // Construct from an existing fd (used for a socket returned by accept)
#ifdef _WIN32
        explicit Socket(SOCKET fd);
#elif __linux__
        explicit Socket(int fd);
#endif


        // Copying is disabled (Socket is an exclusive resource), moving is allowed (Move semantics)
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        // Destructor: automatically close(fd)
        ~Socket();

        // =========== Core API ===========

        /* Bind the port and listen;
         * Not specifying a port means that a port will be automatically selected for listening,
         * while not specifying an IPv6 address means that the specified port will be listened to on all IPv6 addresses. */
        void bindAndListen(int port = 0, const char* address = nullptr);


        /* Accept connection (blocking)
         * Returns a new Socket object representing the client connection. */
        Socket acceptClient() const;

        /* Send data.
         * Return the actual number of bytes sent. */
        size_t sendData(const void* buf, size_t len) const;
        size_t sendData(const std::string& data) const;

        /* Receiving data.
         * Note: If the variable 'flag' is not 0, it means the client IP will not be retrieved
         * when the client disconnects, which can improve performance to a certain extent. */
        void recvData(void* buf, size_t len, int flag = 0) const;

        /* [Core] Cross-Platform Zero-Copy File Transfer
         * offset: file offset, count: number of bytes to send.
         * Here, we will first assume that what is being transmitted is the entire file. */
        void sendFile(int fileFd, off_t offset, size_t count);

        // 获取原生句柄 (仅供内部使用)
        int native_handle() const;

        // 设置超时时间
        void setRecvTimeout(int seconds);

    private:
        /* Under Linux, it is an int, while under Windows, it is a SOCKET (uint_ptr).
         * Therefore,we encapsulate it into a structure to reduce the use of conditional compilation. */
        struct FD {
#ifdef _WIN32
            SOCKET _fd;
            operator int() const {
                return static_cast<int>(_fd);
            }

            FD& operator=(SOCKET other) {
                this->_fd = other;
            }

#elif __linux__
            int _fd;
            operator int() const {
                return _fd;
            }

            FD& operator=(int other) {
                this->_fd = other;
            }
#endif
        };

        FD _fd;

        //Helper Function:


    };

}