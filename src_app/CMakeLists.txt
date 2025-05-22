cmake_minimum_required(VERSION 3.5)

project(whisper_realtime_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Include directories from whisper.cpp and ggml
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/ggml/include)

# Add our application sources
file(GLOB APP_SOURCES
    "*.cpp"
    "*.h"
)

# Add Qt sources
qt_add_executable(whisper_realtime_app ${APP_SOURCES})

# Link necessary libraries
target_link_libraries(whisper_realtime_app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    whisper
    ggml
)

# Add SDL2 and SDL2_mixer libraries (adjust paths/names if necessary)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

target_link_libraries(whisper_realtime_app PRIVATE
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
)

# Add xdotool (just a note, xdotool is an external command, not a library)
# We will call it using QProcess 