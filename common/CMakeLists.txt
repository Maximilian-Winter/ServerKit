cmake_minimum_required(VERSION 3.15)
project(common)

add_library(common STATIC
        src/Logger.cpp
)

target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(common PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(common PUBLIC ${COMMON_LINK_LIBRARIES})

