# chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(chat_server)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(chat_server
        src/main.cpp

)

target_include_directories(chat_server PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
)

target_link_libraries(chat_server PRIVATE
        common
        ${COMMON_LINK_LIBRARIES}
)

target_compile_options(chat_server PRIVATE ${COMMON_COMPILE_OPTIONS})

set_target_properties(chat_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)



