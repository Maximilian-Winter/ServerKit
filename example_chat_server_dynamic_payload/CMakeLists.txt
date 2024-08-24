# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(chat_server_dynamic)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Chat Server
add_executable(chat_server_dynamic
        src/main_server.cpp
)

target_include_directories(chat_server_dynamic PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(chat_server_dynamic PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(chat_server_dynamic PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(chat_server_dynamic PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


# Chat Client
add_executable(chat_client_dynamic
        src/main_client.cpp
)

target_include_directories(chat_client_dynamic PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(chat_client_dynamic PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(chat_client_dynamic PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(chat_client_dynamic PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


# Performance Test
add_executable(dynamic_performance
        src/main_test.cpp
)

target_include_directories(dynamic_performance PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(dynamic_performance PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(dynamic_performance PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(dynamic_performance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
