cmake_minimum_required(VERSION 3.10)
project(PolySynth VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED rtaudio)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

# Create executable with multiple source files
add_executable(synth
    src/main.cpp
    src/synth.cpp
    src/midi.cpp
    src/envelope.cpp
    src/oscillator.cpp
    src/brainwave_osc.cpp
    src/lfo.cpp
    src/voice.cpp
    src/reverb.cpp
    src/preset.cpp
    src/loop_manager.cpp
    src/clock.cpp
    src/constraint.cpp
    src/markov.cpp
    src/euclidean.cpp
    src/pattern.cpp
    src/track.cpp
    src/sequencer.cpp
    src/cpu_monitor.cpp
    # UI core files
    src/ui.cpp
    src/ui/ui_utils.cpp
    src/ui/ui_core.cpp
    src/ui/ui_parameters.cpp
    src/ui/ui_presets.cpp
    src/ui/ui_help.cpp
    src/ui/ui_input.cpp
    src/ui/ui_drawing.cpp
    # UI page files
    src/ui/pages/ui_page_parameters.cpp
    src/ui/pages/ui_page_oscillator.cpp
    src/ui/pages/ui_page_lfo.cpp
    src/ui/pages/ui_page_envelope.cpp
    src/ui/pages/ui_page_fm.cpp
    src/ui/pages/ui_page_mod.cpp
    src/ui/pages/ui_page_reverb.cpp
    src/ui/pages/ui_page_filter.cpp
    src/ui/pages/ui_page_looper.cpp
    src/ui/pages/ui_page_config.cpp
    # UI sequencer files
    src/ui/sequencer/ui_sequencer_state.cpp
    src/ui/sequencer/ui_sequencer_drawing.cpp
    src/ui/sequencer/ui_sequencer_fields.cpp
    src/ui/sequencer/ui_sequencer_actions.cpp
    src/ui/sequencer/ui_sequencer_input.cpp
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
    ncursesw
)
