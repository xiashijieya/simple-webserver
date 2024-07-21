cmake_minimum_required(VERSION 3.10)
project(SimpleWebServer CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(webserver
    src/main.cpp
    src/logger.cpp
    src/socket_ops.cpp
    src/buffer.cpp
    src/server.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/http_connection.cpp
    src/router.cpp
    src/static_file_handler.cpp
    src/thread_pool.cpp
)

if(WIN32)
    target_link_libraries(webserver PRIVATE ws2_32)
endif()
