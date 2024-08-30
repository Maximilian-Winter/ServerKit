# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(text_test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define ASIO_STANDALONE to avoid needing Boost
add_definitions(-DASIO_STANDALONE)

# Set Asio include directory
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/asio-1-28-0/asio/include/)

# Check if Asio directory exists
if(NOT EXISTS ${ASIO_INCLUDE_DIR})
    message(FATAL_ERROR "Asio include directory not found: ${ASIO_INCLUDE_DIR}")
endif()

# Add the include directory for Asio
include_directories(${ASIO_INCLUDE_DIR})

# Print Asio include path for debugging
message(STATUS "Asio include path: ${ASIO_INCLUDE_DIR}")



# Chat Server
add_executable(text_test_server
        main_server.cpp
)

set_target_properties(text_test_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(text_test_server PRIVATE ${ASIO_INCLUDE_DIR})

# Chat Client
add_executable(text_test_client
        main_client.cpp
)

set_target_properties(text_test_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(text_test_client PRIVATE ${ASIO_INCLUDE_DIR})