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

# Add the stb directory. 
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/stb")
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(stb_impl "${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_impl.cpp")
    target_link_libraries(stb_impl PUBLIC stb)
else()
    message(FATAL_ERROR "stb directory is missing! get the files [stb_image_write.h, stb_image.h, stb_perlin.h] from the stb repo and adad a impl cpp file")
endif()

# Add the miniaudio submodule directory. 
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(miniaudio_impl "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/miniaudio.c")
    target_link_libraries(miniaudio_impl PUBLIC miniaudio)
else()
    message(FATAL_ERROR "miniaudio submodule is missing! Run: git submodule update --init --recursive")
endif()

# add the nlohmann_json directory
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json")
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    add_subdirectory(nlohmann_json)
else()
    message(FATAL_ERROR "nlohmann/json submodule is missing! Run: git submodule update --init --recursive")
endif()

# --- GLAD Configuration ---
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/glad")
    add_library(glad "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c")
    target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")
else()
    message(FATAL_ERROR "GLAD directory is missing! Generate it from the web service and place the include/ and src/ folders in lib/glad.")
endif()