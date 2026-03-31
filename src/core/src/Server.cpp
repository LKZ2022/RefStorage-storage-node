//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#include "../include/Server.hpp"
#include "utils/include/AsyncLogger.hpp"
#include <chrono>
#include <iostream>

// 跨平台动态库加载头文件
#ifdef _WIN32
    #include <windows.h>
    using LibHandle = HMODULE;
#else
    #include <dlfcn.h>
    using LibHandle = void*;
#endif

namespace ref_storage::core {

    std::once_flag Server::init_flag;

    // ==========================================
    // 静态方法：全局环境初始化
    // ==========================================
    void Server::init_env() {
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "[Fatal Error] WSAStartup() failed: " << result << std::endl;
            exit(1);
        }
#endif
    }

    // ==========================================
    // 静态方法：全局环境清理
    // ==========================================
    void Server::cleanup_env() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    Server::Server() : thread_pool_(nullptr), is_running_(false), admin_running_(false),
                       port_(12344), address_("::1"),
                       num_threads_(std::thread::hardware_concurrency()){ }

    Server::~Server() { stop(); }

    void Server::init(int port, const std::string &config_path) {
        std::call_once(init_flag, [&]() {
            LOG_SYNC_INFO("Initializing Server...");
            get_instance().doInit(port, config_path);
        });
    }

    void Server::doInit(int port, const std::string &config_path) {
        port_ = port;
    }

    void Server::start(size_t thread_const) {
        if (admin_running_) return;

        if (!thread_pool_) {
            thread_pool_ = std::make_unique<utils::ThreadPool>(thread_const);
            num_threads_ = thread_const;
        }

        admin_running_ = true;

        utils::AsyncLogger::getInstance().init("server.log", utils::LogLevel::Debug);
        thread_pool_->enqueue([]() { utils::AsyncLogger::getInstance().consumeLogs(); });

        registerCommands();

        // 1. 启动运维监听
        std::thread([this]() {
            try {
                admin_listen_socket_ = net::Socket();
                admin_listen_socket_.setReuseAddress(true);
                admin_listen_socket_.bindAndListen(12345, "::1");
                LOG_INFO("[Admin] Admin interface listening on [::1]:12345");
                acceptAdminConnections();
            } catch (const std::exception& e) {
                LOG_ERROR("[Admin] Failed to start admin interface: {}", e.what());
            }
        }).detach();

        // 2. 随主进程启动业务层
        startBusiness();
    }

    // ==========================================
    // 业务层热启停逻辑
    // ==========================================
    void Server::startBusiness() {
        if (is_running_) return;
        try {
            listen_socket_ = net::Socket();
            listen_socket_.setReuseAddress(true);

            listen_socket_.bindAndListen(port_, address_.c_str());
            is_running_ = true;

            std::thread([this]() {
                this->acceptBusinessConnections();
            }).detach();

            LOG_SYNC_INFO("Business Server STARTED on port {}...", port_);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to start business server: {}", e.what());
        }
    }

    void Server::stopBusiness() {
        if (!is_running_) return;
        LOG_INFO("Pausing business server. Disconnecting all clients...");

        is_running_ = false;

        net::Socket empty_socket;
        listen_socket_ = std::move(empty_socket);

        std::lock_guard<std::mutex> lock(mutex_);
        client_sockets_.clear();

        LOG_INFO("Business server PAUSED. Waiting for 'start' command...");
    }

    void Server::acceptBusinessConnections() {
        while (is_running_) {
            try {
                net::Socket client = listen_socket_.acceptClient();
                this->add_client_socket(std::move(client));
            } catch (...) {
                if (!is_running_) break;
                LOG_ERROR("Business accept error.");
            }
        }
    }

    // ==========================================
    // 彻底关闭程序的总闸
    // ==========================================
    void Server::stop() {
        if (!admin_running_) return;

        stopBusiness();

        admin_running_ = false;
        net::Socket empty_admin;
        admin_listen_socket_ = std::move(empty_admin);

        LOG_INFO("Node completely shut down.");
    }

    // ==========================================
    // 主线程挂起接口
    // ==========================================
    void Server::waitForShutdown() {
        while (is_running_ || admin_running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // ==========================================
    // 业务通信逻辑
    // ==========================================
    net::Socket* Server::get_socket() { return &(listen_socket_); }

    void Server::add_client_socket(net::Socket client_socket) {
        mutex_.lock();
        client_sockets_.push_back(std::move(client_socket));
        size_t index = client_sockets_.size() - 1;
        mutex_.unlock();
        thread_pool_->enqueue([this, index]() {
            std::lock_guard<std::mutex> lock(mutex_);
            this->serverChatWorker(client_sockets_.at(index));
        });
    }

    void Server::add_chat_worker(net::Socket &socket) { }

    void Server::serverChatWorker(net::Socket& Socket) {
        auto shared_sock = std::make_shared<net::Socket>(std::move(Socket));
        thread_pool_->enqueue([shared_sock]() {
            LOG_INFO("New business client connected.");
            try {
                while (true) {
                    std::vector<char> buffer = shared_sock->recvData(0);
                    if (buffer.empty()) {
                        LOG_INFO("Business client disconnected normally.");
                        break;
                    }
                    std::string receivedMsg(buffer.data(), buffer.size());
                    LOG_INFO("[收到消息]: {}", receivedMsg);

                    std::string replyMsg = "服务端已收到: [" + receivedMsg + "]";
                    shared_sock->sendData(replyMsg.c_str(), replyMsg.size());
                }
            } catch (const std::exception& e) {
                LOG_ERROR("[异常退出]: {}", e.what());
            }
        });
    }

    // ==========================================
    // 运维指令系统
    // ==========================================
    void Server::registerCommands() {
        command_handlers_["stop"] = [this](const std::string& args) {
            if (!this->is_running_) return std::string("Business server is already stopped.");
            this->stopBusiness();
            return std::string("Business server STOPPED. Admin console remains active. Use 'start' to resume.");
        };

        command_handlers_["start"] = [this](const std::string& args) {
            if (this->is_running_) return std::string("Business server is already running.");
            this->startBusiness();
            return std::string("Business server STARTED successfully.");
        };

        command_handlers_["shutdown"] = [this](const std::string& args) {
            LOG_FATAL("[Admin] Shutdown command received. Terminating process...");
            this->stop();
            return std::string("Node shutting down... Goodbye.");
        };

        command_handlers_["status"] = [this](const std::string& args) {
            std::string state = this->is_running_ ? "RUNNING" : "PAUSED";
            return std::format("Business State: [{}]. Threads: {}, Clients: {}",
                               state, num_threads_, client_sockets_.size());
        };

        command_handlers_["load"] = [this](const std::string& args) {
            if (args.empty()) return std::string("Usage: load <plugin_name>");

            LibHandle hPlugin = nullptr;
            std::string error_msg;

#ifdef _WIN32
            hPlugin = LoadLibraryA(args.c_str());
            if (!hPlugin) error_msg = "Win32 Error: " + std::to_string(GetLastError());
#else
            hPlugin = dlopen(args.c_str(), RTLD_NOW);
            if (!hPlugin) {
                const char* err = dlerror();
                error_msg = err ? std::string(err) : "Unknown dlopen error";
            }
#endif

            if (!hPlugin) {
                LOG_ERROR("[Admin] Failed to load plugin: {}. Error: {}", args, error_msg);
                return "Failed to load plugin: " + error_msg;
            }

            typedef const char* (*GetNameFunc)();
            typedef void (*ExecuteFunc)(const char*, char*, int);
            GetNameFunc get_name = nullptr;
            ExecuteFunc execute_cmd = nullptr;

#ifdef _WIN32
            get_name = (GetNameFunc)GetProcAddress(hPlugin, "GetCommandName");
            execute_cmd = (ExecuteFunc)GetProcAddress(hPlugin, "ExecuteCommand");
#else
            get_name = (GetNameFunc)dlsym(hPlugin, "GetCommandName");
            execute_cmd = (ExecuteFunc)dlsym(hPlugin, "ExecuteCommand");
#endif

            if (!get_name || !execute_cmd) {
#ifdef _WIN32
                FreeLibrary(hPlugin);
#else
                dlclose(hPlugin);
#endif
                return std::string("Invalid plugin format.");
            }

            std::string newCmdName = get_name();
            this->command_handlers_[newCmdName] = [execute_cmd](const std::string& input_args) {
                char buffer[4096] = {0};
                execute_cmd(input_args.c_str(), buffer, sizeof(buffer));
                return std::string(buffer);
            };

            LOG_INFO("[Admin] Loaded dynamic command: {}", newCmdName);
            return "Successfully loaded command: '" + newCmdName + "'";
        };
    }

    void Server::acceptAdminConnections() {
        while (admin_running_) {
            try {
                net::Socket admin_client = admin_listen_socket_.acceptClient();
                auto shared_admin_sock = std::make_shared<net::Socket>(std::move(admin_client));
                thread_pool_->enqueue([this, shared_admin_sock]() {
                    this->adminWorker(shared_admin_sock);
                });
            } catch (...) {
                if (!admin_running_) break;
            }
        }
    }

    void Server::adminWorker(std::shared_ptr<net::Socket> admin_sock) {
        LOG_INFO("[Admin] New console connected.");
        try {
            while (admin_running_) {
                std::vector<char> buffer = admin_sock->recvData(0);
                if (buffer.empty()) break;

                std::string input(buffer.data(), buffer.size());
                std::string cmd, args;
                size_t space_pos = input.find(' ');
                if (space_pos != std::string::npos) {
                    cmd = input.substr(0, space_pos);
                    args = input.substr(space_pos + 1);
                } else {
                    cmd = input;
                }

                std::string reply;
                auto it = command_handlers_.find(cmd);
                if (it != command_handlers_.end()) reply = it->second(args);
                else reply = "Unknown command: " + cmd;

                admin_sock->sendData(reply.c_str(), reply.size());

                if (cmd == "shutdown") {
                    break;
                }
            }
        } catch (...) { }
    }

} // namespace ref_storage::core