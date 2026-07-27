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

# --- Font baker (freetype + msdf-atlas-gen) ---
# Only needed to BAKE a font atlas from a .ttf. The Font resource reads the
# baked .png/.json cache without either library, so a shipping build that ships
# its .cache/fonts/ can turn this off and drop both dependencies entirely.
option(ENGINE_BUILD_FONT_BAKER "Build the msdf-atlas-gen backed font baker" ON)

if(ENGINE_BUILD_FONT_BAKER)
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/freetype")
        message(FATAL_ERROR "freetype submodule is missing! Run: git submodule update --init --recursive")
    endif()
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen")
        message(FATAL_ERROR "msdf-atlas-gen submodule is missing! Run: git submodule update --init --recursive")
    endif()

    # Strip every optional freetype dependency so it builds with no system packages.
    set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
    add_subdirectory(freetype)

    # freetype only sets EXPORT_NAME Freetype::Freetype for its install export, so
    # that target does not exist in a build tree. msdfgen guards its
    # find_package(Freetype) on `if(NOT TARGET Freetype::Freetype)` -- without this
    # alias it would go looking for a system freetype that doesn't exist here.
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_VCPKG        OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_SKIA         OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_NO_ARTERY_FONT   ON  CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_INSTALL          OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_MSDFGEN_EXTERNAL OFF CACHE BOOL "" FORCE)
    # The rest of the project links the dynamic MSVC runtime; msdf-atlas-gen
    # defaults to the static one and mismatched runtimes fail at link.
    set(MSDF_ATLAS_DYNAMIC_RUNTIME  ON  CACHE BOOL "" FORCE)
    # Drops the libpng dependency from both msdfgen and msdf-atlas-gen. We hand
    # the baked bitmap to stb through FileManager::createImageFile instead.
    set(MSDFGEN_DISABLE_PNG         ON  CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_SVG         ON  CACHE BOOL "" FORCE)
    add_subdirectory(msdf-atlas-gen)
endif()