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

        client_sockets_.clear();

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
        // temp
        std::vector<char> tmp;
        try {
            tmp = client_sockets_.back().recvData(0);
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
        std::cout << std::string(tmp.data(), tmp.size()) << std::endl;
    }

    Server::Server() : thread_pool_(nullptr), is_running_(false), port_(8080), address_("::1"),
                       num_threads_(std::thread::hardware_concurrency()){ }

    Server::~Server() {
        stop();
    }

    void Server::doInit(int port, const std::string &config_path) {


        port_ = port;
    }

    void Server::serverChatWorker(net::Socket& socket) {
        std::cout << "[线程 " << std::this_thread::get_id() << "] 新客户端已接入，开始聊天服务。" << std::endl;

        try {
            while (true) {
                std::vector<char> buffer = socket.recvData(0);

                if (buffer.empty()) {
                    continue;
                }

                std::string receivedMsg(buffer.data(), buffer.size());
                std::cout << "[收到客户端消息]: " << receivedMsg << std::endl;

                std::string replyMsg = "服务端已收到你的消息: [" + receivedMsg + "]";

                socket.sendData(replyMsg.c_str(), replyMsg.size());
            }
        }
        catch (const std::exception& e) {
            // 5. 异常捕获：客户端断开连接、网络波动等都会跳到这里，不会导致整个服务端崩溃
            std::cerr << "[客户端断开或异常] 线程 " << std::this_thread::get_id()
                      << " 退出: " << e.what() << std::endl;
        }
    }

    std::once_flag Server::init_flag;
}