//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.


#include "../include/Server.hpp"

namespace ref_storage::core {

    void Server::init(int port, const std::string &config_path) {
        static auto initialed = [port, config_path]() {
                std::call_once(init_flag, [&]() {
                get_instance().doInit(port, config_path);
            });
        };
    }

    void Server::start(size_t thread_const) {
        if (is_running_) {
            return;
        }

        if (!thread_pool_) {
            thread_pool_ = std::make_unique<utils::ThreadPool>(thread_const);
            num_threads_ = thread_const;
            std::cout << "ThreadPool successfully created with " << num_threads_ << " threads.\n";
        }

        is_running_ = true;
        std::cout << "Server started on port " << port_ << "...\n";

        net::Socket socket;
        listen_socket_ = std::move(socket);
        listen_socket_.bindAndListen(port_);
    }

    void Server::stop() {
        is_running_ = false;
        std::cout << "Server stopped.\n";
    }

    Server::Server() : thread_pool_(nullptr), is_running_(false), port_(8080), num_threads_(std::thread::hardware_concurrency()){ }

    void Server::doInit(int port, const std::string &config_path) {
        // 处理配置文件
    }

    std::once_flag Server::init_flag;
}