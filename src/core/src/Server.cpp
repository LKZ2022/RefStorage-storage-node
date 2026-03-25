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
        if (is_running) {
            return;
        }

        if (!thread_pool_) {
            thread_pool_ = std::make_unique<utils::ThreadPool>(thread_const);
            num_threads = thread_const;
            std::cout << "ThreadPool successfully created with " << num_threads << " threads.\n";
        }

        is_running = true;
        std::cout << "Server started on port " << port << "...\n";
    }

    void Server::stop() {
        is_running = false;
        std::cout << "Server stopped.\n";
    }

    Server::Server() : thread_pool_(nullptr), is_running(false), port(8080), num_threads(std::thread::hardware_concurrency()){ }

    void Server::doInit(int port, const std::string &config_path) {
        // 处理配置文件
    }

    std::once_flag Server::init_flag;
}