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

# Conditionally exclude Windows-specific files on non-Windows platforms
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(FILTER ENGINE_HEADER_FILES EXCLUDE REGEX "WindowsWindow\\.hpp$")
    list(FILTER ENGINE_SOURCE_FILES EXCLUDE REGEX "WindowsWindow\\.cpp$")
    message(STATUS "Excluded Windows-specific files on ${CMAKE_SYSTEM_NAME}")
endif()

# Create engine library
add_library(engine ${ENGINE_HEADER_FILES} ${ENGINE_SOURCE_FILES})

# Include directories
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Apply OS preprocessor definition to the target
target_compile_definitions(engine PRIVATE OS_NAME=${OS_NAME})

# Print out collected files (for debugging)
message(STATUS "Engine Headers: ${ENGINE_HEADER_FILES}")
message(STATUS "Engine Sources: ${ENGINE_SOURCE_FILES}")
