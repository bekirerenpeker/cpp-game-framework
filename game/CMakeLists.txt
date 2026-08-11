# Collect game sandbox source code
file(GLOB_RECURSE GAME_SOURCES "src/**.cpp")

# Create the final game executable
add_executable(game ${GAME_SOURCES})

# add the games include dir
target_include_directories(engine PUBLIC include)

# Link against the engine library.
target_link_libraries(game PRIVATE engine)

# Baked in as an absolute path so assets resolve wherever the executable is launched from
target_compile_definitions(game PRIVATE GAME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
