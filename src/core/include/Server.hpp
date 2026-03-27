//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.
#pragma once

#include <mutex>
#include <vector>
#include "./net/include/Socket.hpp"
#include "utils/include/ThreadPool.hpp"

namespace ref_storage::core {

    // Use a singleton to ensure that a class is created only once.
    class Server {
    private:

        std::mutex mutex_;
        net::Socket listen_socket_;
        std::vector<net::Socket> client_sockets_;
        std::unique_ptr<utils::ThreadPool> thread_pool_;
        bool is_running_;
        int port_;
        const char *address_;
        size_t num_threads_;

    private:
        Server();
        ~Server();

        void doInit(int port, const std::string& config_path);

        void serverChatWorker(net::Socket &socket);

        static std::once_flag init_flag;
    public:

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        static Server& get_instance() {
            static Server instance;
            return instance;
        }

        static void init(int port, const std::string &config_path);

        void start(size_t thread_const);
        void stop();

        net::Socket* get_socket();

        void add_client_socket(net::Socket&& client_socket);

    };
}