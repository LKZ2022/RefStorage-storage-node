//Copyright (c) 2026 Liu Kaizhi
//Licensed under the Apache License, Version 2.0.


#include "../include/Socket.hpp"

#include <cstdint>

namespace ref_storage::net {

    Socket::Socket() {

        try {
            /* In this constructor, a socket will be cretaed.
             * Here, we use the TCP protocol within the IPv6 address family. */
#ifdef _WIN32

            //This is the processing under the Windows system.

            //We create the WSAData structure and initialize Winsock 2.2.
            WSADATA wsaData;
            int result;
            result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                std::cerr << "WSAStartup failed with error: " << result << std::endl;
                throw std::runtime_error("WSAStartup failed with error");
            }

            // Check version.
            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
                std::cerr << "WSAStartup failed with error：Could not find a usable version of Winsock.dll." <<
                    std::endl;
                WSACleanup();
                throw std::runtime_error("WSAStartup failed with error：Could not find a usable version of Winsock.dll.");
            }

            /* Create socket.
             * Use Tcp protocol */
            _fd = socket(AF_INET, SOCK_STREAM, 0);

            if (_fd == INVALID_SOCKET) {
                std::cerr << "Socket failed with error: " << WSAGetLastError() << std::endl;
                WSACleanup();
                throw std::runtime_error("Socket failed with error");
            }

            // Set Address Reuse.
            int opt = 1;

            result = setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR,
                                reinterpret_cast<const char *>(&opt), sizeof(opt));
            if (result != 0) {
                std::cerr << "setsockopt failed with error: " << result << std::endl;
                WSACleanup();
                throw std::runtime_error("setsockopt failed with error");
            }

            // Socket initialization completed.

#elif __linux__

            //This is the processing under the linux system.

            /* Create socket.
             * Use Tcp protocol */
            _fd = socket(AF_INET, SOCK_STREAM, 0);

            if (_fd == INVALID_SOCKET) {
                std::cerr << "Socket failed with error: " << WSAGetLastError() << std::endl;
                WSACleanup();
                throw std::runtime_error("Socket failed with error");
            }

            // Set Address Reuse.
            int opt = 1;

            result = setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR,
                                reinterpret_cast<const char *>(&opt), sizeof(opt));
            if (result != 0) {
                std::cerr << "setsockopt failed with error: " << result << std::endl;
                WSACleanup();
                throw std::runtime_error("setsockopt failed with error");
            }

#endif

            //Here,we will address errors that occur during the construction process.
        }catch(std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }

    }
#ifdef _WIN32
    Socket::Socket(SOCKET fd) {
        _fd = fd;
    }
#elif __linux__

#endif

}
