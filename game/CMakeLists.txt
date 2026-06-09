# Game CMakeLists.txt

# Header files (.h, .hpp)
set(GAME_HEADERS include/)

# Source files (.c, .cpp)
set(GAME_SOURCES src/)

# Get all header files
file(GLOB_RECURSE GAME_HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Get all source files
file(GLOB_RECURSE GAME_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Create game executable
add_executable(terraria_clone ${GAME_HEADER_FILES} ${GAME_SOURCE_FILES})

# Link engine library
target_link_libraries(terraria_clone PRIVATE engine)

# Include directories
target_include_directories(terraria_clone PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Print out collected files (for debugging)
message(STATUS "Game Headers: ${GAME_HEADER_FILES}")
message(STATUS "Game Sources: ${GAME_SOURCE_FILES}")
