cmake_minimum_required(VERSION 3.10)
project(PolySynth VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED rtaudio)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
find_package(Curses REQUIRED)

# Create executable with multiple source files
add_executable(synth 
    src/main.cpp
    src/synth.cpp
    src/midi.cpp
    src/envelope.cpp
    src/ui.cpp
    src/oscillator.cpp
    src/reverb.cpp
    src/preset.cpp
)

# Include directories
target_include_directories(synth PRIVATE 
    ${RTAUDIO_INCLUDE_DIRS} 
    ${RTMIDI_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/reverb
)

# Link libraries
target_link_libraries(synth PRIVATE 
    ${RTAUDIO_LIBRARIES} 
    ${RTMIDI_LIBRARIES}
    ${CURSES_LIBRARIES}
)