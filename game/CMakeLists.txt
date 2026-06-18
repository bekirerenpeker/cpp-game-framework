# Collect game sandbox source code
file(GLOB_RECURSE GAME_SOURCES "src/**.cpp")

# Create the final game executable
add_executable(game ${GAME_SOURCES})

# add the games include dir
target_include_directories(engine PUBLIC include)

# Link against the engine library. 
target_link_libraries(game PRIVATE engine)
