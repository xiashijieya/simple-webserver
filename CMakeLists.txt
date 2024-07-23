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

include_directories(${CMAKE_SOURCE_DIR}/src)

set(SWS_CORE_SRCS
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
    src/iocp_server.cpp
)

add_library(sws_core STATIC ${SWS_CORE_SRCS})
if(WIN32)
    target_link_libraries(sws_core PUBLIC ws2_32 mswsock)
endif()

add_executable(webserver src/main.cpp)
target_link_libraries(webserver PRIVATE sws_core)

add_executable(bench bench/bench.cpp)
if(WIN32)
    target_link_libraries(bench PRIVATE ws2_32)
endif()

enable_testing()
configure_file(tests/test_config.h.in
               ${CMAKE_BINARY_DIR}/tests/test_config.h)

set(SWS_TESTS
    test_buffer
    test_parser
    test_router
)

foreach(test_name ${SWS_TESTS})
    add_executable(${test_name}
        tests/${test_name}.cpp
        tests/mini_test.cpp
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_BINARY_DIR}/tests
    )
    target_link_libraries(${test_name} PRIVATE sws_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
