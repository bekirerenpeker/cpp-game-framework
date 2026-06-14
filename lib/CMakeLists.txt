cmake_minimum_required(VERSION 3.15)

# --- GLFW Configuration ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Add the GLFW submodule directory. 
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/glfw")
    add_subdirectory(glfw)
else()
    message(FATAL_ERROR "GLFW submodule is missing! Run: git submodule update --init --recursive")
endif()

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

# Add the implementation file as a library so it compiles once
add_library(stb_impl "${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_impl.cpp")
target_link_libraries(stb_impl PUBLIC stb)
