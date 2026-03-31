//Copyright (c) 2026 Kaizhi Liu
//Licensed under the Apache License, Version 2.0.

#include "core/include/Server.hpp"

int main() {
    // 1. 全局网络环境初始化 (静态成员函数)
    ref_storage::core::Server::init_env();

    // 2. 获取服务器的全局唯一实例
    auto& server = ref_storage::core::Server::get_instance();

    // 3. 初始化并启动服务器 (内部会分离出业务监听和运维监听)
    server.init(12344);
    server.start();

    // 4. 挂起主线程，直到系统收到彻底关机的指令
    server.waitForShutdown();

    // 5. 全局网络环境清理 (静态成员函数)
    ref_storage::core::Server::cleanup_env();

    return 0;
}