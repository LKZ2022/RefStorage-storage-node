#include <iostream>
#include <../src/utils/include/ThreadPool.hpp>

#include "core/include/Server.hpp"

int main() {

    ref_storage::core::Server::init(8080, "test_server_config.config");
    ref_storage::core::Server::get_instance().start(100);

    return 0;
}
