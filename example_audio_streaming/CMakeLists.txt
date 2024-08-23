# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(audio_server)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Chat Server
add_executable(audio_server
        src/main_server.cpp
)

target_include_directories(audio_server PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(audio_server PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(audio_server PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(audio_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


# Chat Client
add_executable(audio_client
        src/main_client.cpp
)

target_include_directories(audio_client PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(audio_client PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(audio_client PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(audio_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

