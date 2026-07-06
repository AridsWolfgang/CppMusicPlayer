cmake_minimum_required(VERSION 3.15)
project(VibeStream VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig QUIET)

find_package(Curses REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)

FetchContent_Declare(
  cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG v1.7.18
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cjson)

set(SOURCES
  src/main.c
  src/config.c
  src/player.c
  src/library.c
  src/downloader.c
  src/ui.c
)

set(HEADERS
  include/vibestream/types.h
  include/vibestream/config.h
  include/vibestream/player.h
  include/vibestream/library.h
  include/vibestream/downloader.h
  include/vibestream/ui.h
)

add_executable(vibestream ${SOURCES} ${HEADERS})

target_include_directories(vibestream PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CURSES_INCLUDE_DIRS}
  ${SQLite3_INCLUDE_DIRS}
  ${miniaudio_SOURCE_DIR}
  ${cjson_SOURCE_DIR}
)

target_compile_definitions(vibestream PRIVATE
  MA_NO_EXCEPTIONS
  MA_NO_OPENSL
  MA_NO_AAUDIO
  MA_NO_JACK
  MA_NO_SDL2
)

target_link_libraries(vibestream PRIVATE
  ${CURSES_LIBRARIES}
  ${SQLite3_LIBRARIES}
  cjson
  m
  pthread
)

if(UNIX AND NOT APPLE)
  target_link_libraries(vibestream PRIVATE rt)
endif()

install(TARGETS vibestream RUNTIME DESTINATION bin)
