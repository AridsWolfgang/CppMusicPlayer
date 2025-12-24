cmake_minimum_required(VERSION 3.10)
project(MusicPlayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For Windows, might need different settings
if(WIN32)
    add_definitions(-DWIN32)
endif()

# Add executable
add_executable(music_player
    src/main.cpp
    src/tui.cpp
    src/downloader.cpp
    src/player.cpp
)

# Include directories
target_include_directories(music_player PRIVATE include)

# Link libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(music_player pthread)
endif()
