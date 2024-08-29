cmake_minimum_required(VERSION 3.15)
project(common)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Glob all header files in the include directory
file(GLOB HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

file(GLOB SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add library target
add_library(common INTERFACE ${SOURCE_FILES})

# Set include directories for the target
target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add header files to the target
target_sources(common INTERFACE ${HEADER_FILES})

# Set compile options
target_compile_options(common INTERFACE ${COMMON_COMPILE_OPTIONS})

# Link libraries
target_link_libraries(common INTERFACE ${COMMON_LINK_LIBRARIES})