//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <thread>
#include "net/include/Socket.hpp"
#include "utils/include/ThreadPool.hpp"

namespace ref_storage::core {
    class Server {
    public:
        // ==========================================
        // 全局环境生命周期管理
        // ==========================================
        static void init_env();
        static void cleanup_env();

        static Server& get_instance() {
            static Server instance;
            return instance;
        }

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        void init(int port = 12344, const std::string& config_path = "");
        void start(size_t thread_const = std::thread::hardware_concurrency());
        void stop();

        // ==========================================
        // 主线程挂起接口
        // ==========================================
        void waitForShutdown();

        net::Socket* get_socket();
        void add_client_socket(net::Socket client_socket);
        void add_chat_worker(net::Socket& socket);

    private:
        Server();
        ~Server();

        void doInit(int port, const std::string& config_path);
        void serverChatWorker(net::Socket& Socket);

        std::unique_ptr<utils::ThreadPool> thread_pool_;
        std::vector<net::Socket> client_sockets_;
        net::Socket listen_socket_;
        std::mutex mutex_;
        static std::once_flag init_flag;

        int port_;
        std::string address_;
        size_t num_threads_;

        // ==========================================
        // 业务层控制 (数据面)
        // ==========================================
        std::atomic<bool> is_running_{false};
        void startBusiness();
        void stopBusiness();
        void acceptBusinessConnections();

        // ==========================================
        // 运维层控制 (控制面)
        // ==========================================
        std::atomic<bool> admin_running_{false};
        using CommandHandler = std::function<std::string(const std::string& args)>;
        std::unordered_map<std::string, CommandHandler> command_handlers_;

        net::Socket admin_listen_socket_;

        void registerCommands();
        void acceptAdminConnections();
        void adminWorker(std::shared_ptr<net::Socket> admin_sock);
    };
}

#endif // SERVER_HPP