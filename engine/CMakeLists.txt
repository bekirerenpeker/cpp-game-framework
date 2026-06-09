# Engine CMakeLists.txt

# Header files (.h, .hpp)
set(ENGINE_HEADERS include/)

# Source files (.c, .cpp)
set(ENGINE_SOURCES src/)

# Get all header files
file(GLOB_RECURSE ENGINE_HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Get all source files
file(GLOB_RECURSE ENGINE_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Create engine library
add_library(engine ${ENGINE_HEADER_FILES} ${ENGINE_SOURCE_FILES})

# Include directories
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Print out collected files (for debugging)
message(STATUS "Engine Headers: ${ENGINE_HEADER_FILES}")
message(STATUS "Engine Sources: ${ENGINE_SOURCE_FILES}")
