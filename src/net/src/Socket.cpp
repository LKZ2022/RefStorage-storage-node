//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.


#include "../include/Socket.hpp"



namespace ref_storage::net {

    Socket::Socket() {
#ifdef _WIN32
        WSADATA wsaData;
        int result;
        //Initialize Winsock
        result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw_last_error(("WSAStartup() failed: " + std::to_string(result)).c_str());
        }

        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
            WSACleanup();
            throw_last_error(("WSAStartup() failed: Winsock 2.2 is not supported, actual version: " +
                              std::to_string(LOBYTE(wsaData.wVersion)) + "." +
                              std::to_string(HIBYTE(wsaData.wVersion))).c_str());
        }

        _fd = SocketHandle::create_socket_handle();

        if (_fd.native_handle() == INVALID_SOCKET) {
            WSACleanup();
            throw_last_error("create_socket_handle() failed: ");
        }


#elif __linux__
        _fd = SocketHandle::create_socket_handle();

        if (_fd.native_handle() == -1) {
            throw_last_error("create_socket_handle() failed: ");
        }
#endif


    }

    Socket::Socket(SocketHandle&& fd) : _fd(std::move(fd)) {}

    Socket::Socket(Socket &&other) noexcept {
        _fd = std::exchange(other._fd,SocketHandle());
    }

    Socket & Socket::operator=(Socket &&other) noexcept {
        if (this != &other) {
            _fd.close_handle();
            _fd = std::move(other._fd);
            other._fd = SocketHandle();
        }
        return *this;
    }

    Socket::~Socket() {
        _fd.close_handle();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Socket::setReuseAddress(bool enable) {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }
        // Set address reuse.
        int opt = enable ? 1 : 0;
#ifdef _WIN32
        const char* opt_ptr = reinterpret_cast<const char*>(&opt);
#else
        const void* opt_ptr = reinterpret_cast<const void*>(&opt);
#endif
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_REUSEADDR,
                                    opt_ptr , sizeof(opt));

#ifdef _WIN32
        if (result == SOCKET_ERROR) {
#elif __linux__
        if (result < 0) {}
#endif
            throw_last_error("setsockopt() failed: ");
        }
        _reuseAddress = enable;
    }

    void Socket::setKeepAlive(bool enable) {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }
        // Set address reuse.
        int opt = enable ? 1 : 0;
#ifdef _WIN32
        const char* opt_ptr = reinterpret_cast<const char*>(&opt);
#else
        const void* opt_ptr = reinterpret_cast<const void*>(&opt);
#endif
        int result = setsockopt(_fd.native_handle(), SOL_SOCKET, SO_KEEPALIVE,
                                opt_ptr , sizeof(opt));

#ifdef _WIN32
        if (result == SOCKET_ERROR) {
#elif __linux__
        if (result < 0) {
#endif
            throw_last_error("setsockopt() failed: ");
        }
        _keepAlive = enable;
    }

    void Socket::setNonBlocking(bool enable) {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }
#ifdef _WIN32
        u_long mode = enable ? 1 : 0;
        if (ioctlsocket(_fd.native_handle(), FIONBIO, &mode) == SOCKET_ERROR) {
            throw_last_error("Failed to set non-blocking mode");
        }
#elif __linux__
        int flags = fcntl(_fd.native_handle(), F_GETFL, 0);
        if (flags == -1) {
            throw_last_error("Failed to get socket flags");
        }

        if (enable) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        if (fcntl(socket_fd_, F_SETFL, flags) == -1) {
            throw_last_error("Failed to set non-blocking mode");
        }
#endif
        _nonBlocking = enable;
    }

    void Socket::bindAndListen(int port, const char *address) {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        if (address == nullptr) {
            addr.sin6_addr = in6addr_any;
        }else {
            if (inet_pton(AF_INET6, address, &addr.sin6_addr) != 1) {
                std::string error_message = std::string("Invalid IPv6 address.") + address;
                throw_last_error(error_message.c_str());
            }
        }

        if (_fd.bind_handle(reinterpret_cast<struct sockaddr *>(&addr),sizeof(addr)) < 0) {
            throw_last_error("bind() failed: ");
        }

        if (_fd.listen_handle() == -1) {
            throw_last_error("listen() failed: ");
        }
    }

    Socket Socket::acceptClient() const {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }
        Socket CommunicationSocket = Socket(_fd.accept_handle());
        if (!CommunicationSocket._fd.is_valid_handle()) {
            throw_last_error("accept() failed: ");
        }
        return CommunicationSocket;
    }

    void Socket::sendData(const void *buf, size_t len, int timeout_ms) const {
        // Consider only blocking mode.
        if (!buf || len == 0) {
            return;
        }

        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }

        size_t remaining = len;
        const char* sent_ptr = static_cast<const char*>(buf);

        while (remaining > 0) {
#ifdef _WIN32
            int loc_sent = send(_fd.native_handle(),sent_ptr, static_cast<int>(remaining),0);
#elif __linux__
            ssize_t loc_sent = send(_fd.native_handle(), sent_ptr, remaining, 0);
#endif

            if (loc_sent > 0) {
                remaining -= loc_sent;
                sent_ptr += loc_sent;
            }else if (loc_sent == 0) {
                throw std::runtime_error("Connection closed by peer. ");
            }else {
                throw_last_error("send() failed: ");
            }

        }
    }

    std::vector<char> Socket::recvData(size_t expectedSize) const {
        if (!_fd.is_valid_handle()) {
            throw_last_error("Invalid socket. ");
        }

        std::vector<char> buffet;

        // Directly receive data of the specified size.
        if (expectedSize > 0) {
            buffet.resize(expectedSize);
            size_t total_received = 0;

            while (total_received < expectedSize) {
#ifdef _WIN32
                int result = recv(_fd.native_handle(), buffet.data(),
                            static_cast<int>(expectedSize - total_received), 0);
                if (result == SOCKET_ERROR) {
                    throw std::runtime_error("recv() failed: " + std::to_string(WSAGetLastError()));
                }else if (result == 0) {
                    throw std::runtime_error("Connection closed by peer. ");
                }
#elif __linux__
                ssize_t result = recv(_fd.native_handle(), buffet.data(), expectedSize - total_received, 0);
                if (result < 0 ) {
                    throw std::runtime_error("recv() failed: " + std::to_string(errno);
                }else if (result == 0) {
                    throw std::runtime_error("Connection closed by peer. ");
                }
#endif
                total_received += static_cast<size_t>(result);
            }
        }else {
            /* Using the 'length-prefix' protocol to address the problem of packet sticking.
             * Read the 4-byte message header
             */
            uint32_t datasize = 0;
            size_t head_received = 0;
            while (head_received < sizeof(datasize)) {
#ifdef _WIN32
            int result = recv(_fd.native_handle(), reinterpret_cast<char *>(&datasize) +
                head_received, static_cast<int>(sizeof(datasize) - head_received), 0);
            if (result == SOCKET_ERROR) {
                throw std::runtime_error("recv() failed: " + std::to_string(WSAGetLastError()));
            }else if (result == 0) {
                throw std::runtime_error("Connection closed by peer. ");
            }
#elif __linux__
                ssize_t result = recv(_fd.native_handle(),
                                         reinterpret_cast<char*>(&dataSize) + headerReceived,
                                         sizeof(dataSize) - headerReceived, 0);
                if (result < 0) {
                    throw std::runtime_error("Linux recv header failed, error code: " +
                                             std::to_string(errno));
                } else if (result == 0) {
                    throw std::runtime_error("Connection closed by peer while reading header.");
                }
#endif
                head_received += static_cast<size_t>(result);
            }

            datasize = ntohl(datasize);

            if (datasize == 0) {
                return buffet;
            }

            buffet.resize(datasize);
            size_t total_received = 0;

            while (total_received < datasize) {
#ifdef _WIN32
                size_t result = recv(_fd.native_handle(), buffet.data() + total_received,
                        static_cast<int>(datasize - total_received), 0);
                if (result == SOCKET_ERROR) {
                    throw std::runtime_error("recv() failed: " + std::to_string(WSAGetLastError()));
                }else if (result == 0) {
                    throw std::runtime_error("Connection closed by peer. ");
                }
#elif __linux__
                ssize_t result = recv(_fd.native_handle(), buffet.data() + total_received, datasize - total_received, 0);
                if (result < 0) {
                    throw std::runtime_error("recv() failed: " + std::to_string(errno));
                }else if (result == 0) {
                    throw std::runtime_error("Connection closed by peer. ");
                }
#endif
                total_received += static_cast<size_t>(result);
            }
        }

        return buffet;
    }

    void Socket::sendFile(const std::string& filepath) {
#ifdef _WIN32
        /* CreateFileA: Windows API used to open or create a file
         * GENERIC_READ: Read-only access permission
         * FILE_SHARE_READ: Allows other processes to read the file
         * nullptr: Default security attributes
         * OPEN_EXISTING: The file must already exist
         * FILE_ATTRIBUTE_NORMAL: Standard file attributes
         * FILE_FLAG_SEQUENTIAL_SCAN: Optimization flag indicating the system will read the file sequentially
         * nullptr: No template file
         * A single call can transmit up to approximately 2 GB of data (2,147,483,646 bytes).
         */
        HANDLE hFile = CreateFileA(filepath.c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                nullptr);

        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file.");
        }

        TRANSMIT_FILE_BUFFERS transmit_buffers = {};
        /* hFile: File handle
         * First 0: Number of bytes to transfer (0 indicates the entire file)
         * Second 0: Size of each data block to send (0 indicates the system default)
         * nullptr: No additional data to send
         * &transmit_buffers: Pointer to the transmission buffer
         * TF_USE_DEFAULT_WORKER: Use the system's default worker thread
         */
        BOOL result = TransmitFile(_fd.native_handle(), hFile,
                                    0,
                                    0,
                                    nullptr,
                                    &transmit_buffers,
                                    TF_USE_DEFAULT_WORKER);

        CloseHandle(hFile);
        if (result == FALSE) {
            throw std::runtime_error("TransmitFile() failed. ");
        }
#elif __linux__


        int file_fd = open(filepath.c_str(), O_RDONLY);
        if (file_fd < 0) return false;

        struct stat stat_buf;
        fstat(file_fd, &stat_buf);
        off_t offset = 0;
        ssize_t sent_bytes;

        // Zero-copy transmission.
        while (offset < stat_buf.st_size) {
            sent_bytes = sendfile(_fd.native_handle(), file_fd, &offset, stat_buf.st_size - offset);
            if (sent_bytes <= 0) {
                if (errno == EAGAIN) continue;
                close(file_fd);
                return false;
            }
            // The offset will be automatically updated by sendfile.
        }

        close(file_fd);
        return true;
    }


#endif
    }

    void Socket::throw_last_error(const char *operation) {
#ifdef _WIN32
        throw std::system_error(WSAGetLastError(), std::system_category(), operation);
#elif __linux__
        throw std::system_error(errno, std::system_category(), operation);
#endif
    }
}