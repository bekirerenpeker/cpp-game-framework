cmake_minimum_required(VERSION 3.15)
project(TerariaClone VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Define OS Constants globally ---
add_compile_definitions(OS_WINDOWS=1 OS_MACOS=2 OS_LINUX=3)

# --- Inject the current OS identifier ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(OS_NAME=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(OS_NAME=2)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(OS_NAME=3)
endif()

# --- Include Modules & Dependencies ---
add_subdirectory(lib)
add_subdirectory(engine)
add_subdirectory(game)
