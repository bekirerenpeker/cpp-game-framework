# Collect all engine source code
file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

# Create the engine static library
add_library(engine STATIC ${ENGINE_SOURCES})

# Setup Include Directories
target_include_directories(engine PUBLIC include)

# Link GLFW to the engine
target_link_libraries(engine PUBLIC glfw)
