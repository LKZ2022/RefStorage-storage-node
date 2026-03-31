cmake_minimum_required(VERSION 3.15)
project(storage_node CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")



include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(storage_node
        src/net/src/Socket.cpp
        src/net/include/Socket.hpp
        src/net/src/TcpServer.cpp
        src/net/include/TcpServer.hpp
        src/net/src/HttpContext.cpp
        src/net/include/HttpContext.hpp
        src/net/src/HttpResponse.cpp
        src/net/include/HttpResponse.hpp
        src/core/src/StorageEngine.cpp
        src/core/include/StorageEngine.hpp
        src/core/src/RequestHandler.cpp
        src/core/include/RequestHandler.hpp
        src/utils/src/ThreadPool.cpp
        src/utils/include/ThreadPool.hpp
        src/utils/src/AsyncLogger.cpp
        src/utils/include/AsyncLogger.hpp
        src/net/src/SocketHandle.cpp
        src/net/include/SocketHandle.hpp
        src/main.cpp
        src/core/src/Server.cpp
        src/core/include/Server.hpp
        src/core/include/PluginAPI.hpp
)

if(WIN32)
    target_link_libraries(storage_node
            ws2_32
            mswsock
    )
    target_compile_options(storage_node PRIVATE
            /Zc:preprocessor
            /utf-8
    )
endif()
