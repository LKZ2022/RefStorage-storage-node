//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.


#include "../include/Server.hpp"

namespace ref_storage::core {

    void Server::init(int port, const std::string &config_path) {
        std::call_once(init_flag, [&]() {
            // 启动socket
    #ifdef _WIN32
            WSADATA wsaData;
            int result;
            //Initialize Winsock
            result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error(("WSAStartup() failed: " + std::to_string(result)).c_str());
            }

            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
                WSACleanup();
                throw std::runtime_error(("WSAStartup() failed: Winsock 2.2 is not supported, actual version: " +
                              std::to_string(LOBYTE(wsaData.wVersion)) + "." +
                              std::to_string(HIBYTE(wsaData.wVersion))).c_str());
            }
    #elif __linux__

    #endif
            std::cout << "Initial Socket Server..." << std::endl;
            get_instance().doInit(port, config_path);
        });
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

        listen_socket_.bindAndListen(port_, address_);

        std::cout << "Server started on port " << port_ << "..." << std::endl;
    }

    void Server::stop() {
        if (!is_running_) return;

        std::cout << "Shutting down the server and all connections..." << std::endl;

        // 1. 清空客户端连接（假设你是用 std::vector 或类似容器存的）
        // 这会触发所有客户端 Socket 的析构和 close_handle，此时 Winsock 还没下班，完美关闭！
        client_sockets_.clear();

        // 2. 关闭监听 Socket
        // 使用你封装的 release 机制或新建一个空对象来掏空它
        {
            net::Socket empty_socket;
            listen_socket_ = std::move(empty_socket);
        }

        is_running_ = false;

#ifdef _WIN32
        WSACleanup();
#endif
    }

    net::Socket* Server::get_socket() {
        return &(listen_socket_);
    }

    void Server::add_client_socket(net::Socket &&client_socket) {
        client_sockets_.push_back(std::move(client_socket));
    }

    Server::Server() : thread_pool_(nullptr), is_running_(false), port_(8080), address_("::1"),
                       num_threads_(std::thread::hardware_concurrency()){ }

    Server::~Server() {
        stop();
    }

    void Server::doInit(int port, const std::string &config_path) {
        // 处理配置文件

        port_ = port;
    }

    std::once_flag Server::init_flag;
}