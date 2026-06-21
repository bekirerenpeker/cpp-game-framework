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
)
