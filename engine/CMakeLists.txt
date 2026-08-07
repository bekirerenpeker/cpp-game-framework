# Collect all engine source code
file(GLOB_RECURSE ENGINE_SOURCES "src/**.cpp")

# Create the engine static library
add_library(engine STATIC ${ENGINE_SOURCES})

# Setup Include Directories
target_include_directories(engine PUBLIC include)

# Link the external libraries to the engine
target_link_libraries(engine PUBLIC
    glfw
    stb
    stb_impl
    miniaudio
    miniaudio_impl
    nlohmann_json::nlohmann_json
    glad
)

target_compile_definitions(engine PUBLIC GLFW_INCLUDE_NONE)

# The engine's own assets have to resolve from the engine folder rather than the
# process working directory, or nothing the engine ships can be found once the
# executable is launched from somewhere else. PRIVATE because only engine code
# resolves against it; a shipping game repoints it with setEngineAssetRoot.
target_compile_definitions(engine PRIVATE
    ENGINE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# The font cache evicts stale atlases through IFileEntry::deleteFromDisk, which is
# compiled out unless this is defined. Engine-scoped, so it un-gates deletion for
# engine code only -- game code still gets the safe no-op.
target_compile_definitions(engine PRIVATE ALLOW_DELETE_FROM_DISK)

# PRIVATE: only FontBaker.cpp touches msdf-atlas-gen, and the game must never see
# a third-party header. Font.hpp is identical with or without the baker.
if(ENGINE_BUILD_FONT_BAKER)
    target_link_libraries(engine PRIVATE msdf-atlas-gen::msdf-atlas-gen)
    target_compile_definitions(engine PRIVATE ENGINE_HAS_FONT_BAKER)
endif()