# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(api_server)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Server
add_executable(api_server
        src/server_main.cpp
)

target_include_directories(api_server PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(api_server PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(api_server PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(api_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


# Client
add_executable(api_client
        src/client_main.cpp
)

target_include_directories(api_client PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(api_client PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(api_client PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(api_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)