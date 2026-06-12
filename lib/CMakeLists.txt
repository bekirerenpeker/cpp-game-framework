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
