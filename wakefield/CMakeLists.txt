cmake_minimum_required(VERSION 3.13)
project(PolySynth VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED rtaudio)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

option(WAKEFIELD_BUILD_PROFILER "Build the standalone profiling runner" ON)
option(WAKEFIELD_ENABLE_GPROF "Enable gprof instrumentation for profiling builds" OFF)

set(WAKEFIELD_CORE_SOURCES
    src/synth.cpp
    src/midi.cpp
    src/envelope.cpp
    src/oscillator.cpp
    src/brainwave_osc.cpp
    src/lfo.cpp
    src/voice.cpp
    src/reverb.cpp
    src/compressor.cpp
    src/preset.cpp
    src/clock.cpp
    src/constraint.cpp
    src/markov.cpp
    src/euclidean.cpp
    src/pattern.cpp
    src/track.cpp
    src/sequencer.cpp
    src/cpu_monitor.cpp
    src/theme.cpp
    src/sampler.cpp
    src/sample_bank.cpp
    src/ui.cpp
    src/ui/ui_utils.cpp
    src/ui/ui_core.cpp
    src/ui/ui_parameters.cpp
    src/ui/ui_presets.cpp
    src/ui/ui_help.cpp
    src/ui/ui_input.cpp
    src/ui/ui_drawing.cpp
    src/ui/ui_mod.cpp
    src/ui/ui_mod_data.cpp
    src/ui/ui_sample_browser.cpp
    src/ui/ui_preset_browser.cpp
    src/ui/pages/ui_page_parameters.cpp
    src/ui/pages/ui_page_oscillator.cpp
    src/ui/pages/ui_page_sampler.cpp
    src/ui/pages/ui_page_mixer.cpp
    src/ui/pages/ui_page_lfo.cpp
    src/ui/pages/ui_page_envelope.cpp
    src/ui/pages/ui_page_fm.cpp
    src/ui/pages/ui_page_mod.cpp
    src/ui/pages/ui_page_reverb.cpp
    src/ui/pages/ui_page_filter.cpp
    src/ui/pages/ui_page_compressor.cpp
    src/ui/pages/ui_page_chaos.cpp
    src/ui/pages/ui_page_config.cpp
    src/ui/pages/ui_page_debug.cpp
    src/ui/sequencer/ui_sequencer_state.cpp
    src/ui/sequencer/ui_sequencer_drawing.cpp
    src/ui/sequencer/ui_sequencer_fields.cpp
    src/ui/sequencer/ui_sequencer_actions.cpp
    src/ui/sequencer/ui_sequencer_input.cpp
)

add_library(wakefield_core STATIC ${WAKEFIELD_CORE_SOURCES})
target_include_directories(wakefield_core PUBLIC
    ${RTAUDIO_INCLUDE_DIRS}
    ${RTMIDI_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/reverb
)

set(WAKEFIELD_LINK_LIBS
    ${RTAUDIO_LIBRARIES}
    ${RTMIDI_LIBRARIES}
    ${CURSES_LIBRARIES}
    ncursesw
    Threads::Threads
)

target_link_libraries(wakefield_core PUBLIC ${WAKEFIELD_LINK_LIBS})

if (WAKEFIELD_ENABLE_GPROF)
    target_compile_options(wakefield_core PRIVATE -pg)
    target_link_options(wakefield_core PRIVATE -pg)
endif()
add_executable(synth src/main.cpp)
target_link_libraries(synth PRIVATE wakefield_core)

if (WAKEFIELD_ENABLE_GPROF)
    target_compile_options(synth PRIVATE -pg)
    target_link_options(synth PRIVATE -pg)
endif()

if (WAKEFIELD_BUILD_PROFILER)
    add_executable(profile_runner src/profile_runner.cpp)
    target_link_libraries(profile_runner PRIVATE wakefield_core)
    if (WAKEFIELD_ENABLE_GPROF)
        target_compile_options(profile_runner PRIVATE -pg)
        target_link_options(profile_runner PRIVATE -pg)
    endif()
endif()

# Unit tests: compressor
add_executable(compressor_tests
    src/tests/compressor_tests.cpp
    src/compressor.cpp
)
target_include_directories(compressor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
