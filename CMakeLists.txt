cmake_minimum_required(VERSION 3.15)
project(ServerKit)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set global output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

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

# Common compile options and link libraries
set(COMMON_COMPILE_OPTIONS
        -Wall
)

set(COMMON_LINK_LIBRARIES
        Threads::Threads
        nlohmann_json::nlohmann_json
)

# Function to add common settings to targets
function(add_common_settings target)
    target_compile_options(${target} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_link_libraries(${target} PRIVATE ${COMMON_LINK_LIBRARIES})
    target_include_directories(${target} PRIVATE ${ASIO_INCLUDE_DIR})
endfunction()

# Add subdirectories
add_subdirectory(common)
add_subdirectory(example_api_server)
add_subdirectory(example_chat_server)

# Print include directories for debugging
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "Include dir: '${dir}'")
endforeach()