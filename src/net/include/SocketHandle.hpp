//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#pragma once
#include <utility>
#include <system_error>
#include <iostream>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#elif __linux__

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cerrno>

#endif

namespace ref_storage::net {

    /* Here, we define a C++ class and use conditional compilation to encapsulate SOCKET (uint_ptr) on Windows systems
     * and int (file descriptor) on Linux systems, along with their corresponding invalid values.
     * At the same time, we encapsulate functions that are frequently used and differ between Windows and Linux systems.
     * This approach can reduce the use of conditional compilation and improve portability.
     * All errors thrown in this class are of type std::system_error.
     */

    class SocketHandle {
    private:
#ifdef _WIN32
        using NativeSocketType = SOCKET;
        static constexpr NativeSocketType kInvalid = INVALID_SOCKET;
#elif __linux__
        using NativeSocketType = int;
        static constexpr NativeSocketType kInvalid = -1;
#endif

        NativeSocketType _handle = kInvalid;

    public:
        // Do not make the default constructor publicly accessible.
        explicit SocketHandle() noexcept;

        // Constructed from a native handle, requires taking ownership.
        explicit SocketHandle(NativeSocketType handle) noexcept;

        // Move Constructor and Move Assignment Operator.
        SocketHandle(SocketHandle&& other) noexcept : _handle(std::exchange(other._handle, kInvalid)) {}
        SocketHandle& operator=(SocketHandle&& other) noexcept;

        // Copying is prohibited to prevent security issues.
        SocketHandle(const SocketHandle&) = delete;
        SocketHandle& operator=(const SocketHandle&) = delete;

        // Destructor.
        ~SocketHandle() noexcept;

        /* Because implicit conversion can lead to some uncontrollable risks,
         * we completely prohibit implicit conversion here.
         * Implicit conversions can lead to certain uncontrollable risks, so we completely prohibit implicit conversions here.
         * For instance, in all the constructors mentioned above, we have used 'explicit'.
         */

        /* Obtain the native value, similar to pointer dereferencing.
         * This is the only permitted way to access a native handle.
         */
        [[nodiscard]] NativeSocketType native_handle() const noexcept;

        // We demonstrate the removal of all types of implicit conversion operations.
        template<typename T>
        operator T() const = delete;

        // Allow displaying bool conversion for conditional evaluation.
        explicit operator bool() const noexcept;

        // Validity Check.
        [[nodiscard]] bool is_valid_handle() const noexcept;

        // Allow comparisons within the same type.
        bool operator==(const SocketHandle& other) const noexcept;
        bool operator!=(const SocketHandle& other) const noexcept;
        bool operator<(const SocketHandle& other) const noexcept;
        bool operator<=(const SocketHandle& other) const noexcept;
        bool operator>(const SocketHandle& other) const noexcept;
        bool operator>=(const SocketHandle& other) const noexcept;

        // Remove comparisons with primitive types.
        bool operator==(NativeSocketType) const = delete;
        bool operator!=(NativeSocketType) const = delete;
        bool operator<(NativeSocketType) const = delete;
        bool operator<=(NativeSocketType) const = delete;
        bool operator>(NativeSocketType) const = delete;
        bool operator>=(NativeSocketType) const = delete;

        // Resource Management.
        NativeSocketType release_handle() noexcept;
        void reset_handle(NativeSocketType new_handle = kInvalid) noexcept;
        void close_handle() noexcept;

        // The following encapsulates all native system functions (Windows and Linux).
        int bind_handle(const struct sockaddr* addr, socklen_t addrlen);
        // int backlog: Waiting queue length
        [[nodiscard]] int listen_handle(int backlog = 10);
        SocketHandle accept_handle(struct sockaddr* addr = nullptr, socklen_t* addrlen = nullptr) const;

        // The following is the factory method.
        static SocketHandle create_socket_handle();

    private:
        static void throw_last_error(const char* operation);
    };

}