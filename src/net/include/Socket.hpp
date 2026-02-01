//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#pragma once
#include <string>
#include <iostream>
#include <stdexcept>
#include "SocketHandle.hpp"
#include <cstdint>
#include <vector>

namespace ref_storage::net {

    /* The following C++ class encapsulates native sockets to provide multiple APIs for invocation,
     * allowing the server to properly receive data transmitted by clients.
     * It also uses conditional compilation, enabling normal operation in both Windows and Linux environments.
     * All error handling is done using throw statements in C++ and caught by catch blocks.
     * For higher performance, consider converting all throws to error code handling and removing all try and catch blocks.
     * Note: This class is intended for server-side use only.
     */

    class Socket {
    private:
        SocketHandle _fd = SocketHandle();

        bool _reuseAddress = false;
        bool _keepAlive = false;
        bool _nonBlocking = false;

    public:

        /* Constructor: Creates a new TCP Socket
         * Internal Handling of WSAStartup (Windows)
         */
        Socket();

        // Construct from an existing fd (used for a socket returned by accept)
        explicit Socket(SocketHandle&& fd);


        // Copying is disabled (Socket is an exclusive resource), moving is allowed (Move semantics)
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        // Destructor: automatically close(fd)
        ~Socket();

        // =========== Core API ===========

        // Server settings. enable: toggle switch
        void setReuseAddress(bool enable);
        void setKeepAlive(bool enable);
        void setNonBlocking(bool enable);

        /* Bind the port and listen;
         * Not specifying a port means that a port will be automatically selected for listening,
         * while not specifying an IPv6 address means that the specified port will be listened to on all IPv6 addresses.
         * port: port, address: IPv6 address
         */
        void bindAndListen(int port = 0, const char* address = nullptr);


        /* Accept connection (blocking)
         * Returns a new Socket object representing the client connection.
         */
        [[nodiscard]] Socket acceptClient() const;

        /* Send data.
         * Return the actual number of bytes sent. */
        void sendData(const void *buf, size_t len, int timeout_ms = 1000) const;

        /* Receiving data.
         * expectedSize: The expected number of bytes to be received.
         * If no parameter is provided, it indicates the use of a length-prefixed protocol,
         * where the first 4 bytes in the transmitted file header represent the length.
         */
        [[nodiscard]] std::vector<char> recvData(size_t expectedSize = 0) const;

        /* [Core] Cross-Platform Zero-Copy File Transfer
         * offset: file offset, count: number of bytes to send.
         * Here, we will first assume that what is being transmitted is the entire file. */
        void sendFile(const std::string& filepath);

    private:
        static void throw_last_error(const char* operation);
    };

}