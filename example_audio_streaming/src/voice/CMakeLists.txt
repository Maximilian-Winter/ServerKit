# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(audio_server)
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

# Find PortAudio package
find_package(portaudio CONFIG REQUIRED)

# Chat Server
add_executable(audio_server
        main_server.cpp
)

target_include_directories(audio_server PRIVATE ${ASIO_INCLUDE_DIR})

set_target_properties(audio_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Chat Client
add_executable(audio_client
        main_client.cpp
)

target_link_libraries(audio_client PRIVATE
        portaudio
)

set_target_properties(audio_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(audio_client PRIVATE ${ASIO_INCLUDE_DIR})