cmake_minimum_required(VERSION 3.14)

project(TerariaClone)

# Detect OS and set preprocessor definitions
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(OS_NAME=WINDOWS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(OS_NAME=LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(OS_NAME=MACOS)
else()
    add_compile_definitions(OS_NAME=UNKNOWN)
endif()

message(STATUS "Detected OS: ${CMAKE_SYSTEM_NAME}")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 1. Load all external dependencies first
add_subdirectory(lib)

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(game)
