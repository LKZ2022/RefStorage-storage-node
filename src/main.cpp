#include <iostream>
#include <../src/utils/include/ThreadPool.hpp>

#include "core/include/Server.hpp"

int main() {

    ref_storage::core::Server::init(12344, "test_server_config.config");
    ref_storage::core::Server::get_instance().start(100);
    ref_storage::core::Server::get_instance().add_client_socket(ref_storage::core::Server::get_instance().get_socket()->acceptClient());

    return 0;
}
