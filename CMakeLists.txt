cmake_minimum_required(VERSION 3.22)

project(V0ACycleResampler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()

add_library(v0a_dsp STATIC
    src/dsp/CycleResampler.cpp
    src/dsp/HumanizeController.cpp
    src/dsp/NoteMatrix.cpp
    src/dsp/NoteQuantizer.cpp
    src/dsp/PitchDetector.cpp
    src/dsp/PitchCorrectionEngine.cpp
    src/dsp/RetuneController.cpp
    src/dsp/TargetController.cpp)
target_include_directories(v0a_dsp PUBLIC src)
set_target_properties(v0a_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(v0a_io STATIC src/io/WavFile.cpp)
target_include_directories(v0a_io PUBLIC src)

add_library(v0a_app STATIC
    src/app/AutoTuneRenderer.cpp
    src/app/OfflineRenderer.cpp
    src/app/SignalGenerator.cpp
    src/app/V0BAcceptance.cpp)
target_include_directories(v0a_app PUBLIC src)
target_link_libraries(v0a_app PUBLIC v0a_dsp v0a_io)

add_executable(v0a_render tools/offline_renderer/main.cpp)
target_link_libraries(v0a_render PRIVATE v0a_app)

add_executable(v0a_generate_signals tools/signal_generator/main.cpp)
target_link_libraries(v0a_generate_signals PRIVATE v0a_app)

add_executable(v0b_render tools/v0b_renderer/main.cpp)
target_link_libraries(v0b_render PRIVATE v0a_app)

add_executable(v0b_acceptance tools/v0b_acceptance/main.cpp)
target_link_libraries(v0b_acceptance PRIVATE v0a_app)
add_test(NAME v0b_acceptance
         COMMAND v0b_acceptance --output-dir ${CMAKE_BINARY_DIR}/v0b_acceptance_artifacts)

add_executable(test_signal_generator tests/test_signal_generator.cpp)
target_include_directories(test_signal_generator PRIVATE src tests)
target_link_libraries(test_signal_generator PRIVATE v0a_app)
add_test(NAME signal_generator COMMAND test_signal_generator)

add_executable(test_interpolators tests/test_interpolators.cpp)
target_include_directories(test_interpolators PRIVATE src tests)
add_test(NAME interpolators COMMAND test_interpolators)

add_executable(test_cycle_resampler tests/test_cycle_resampler.cpp)
target_include_directories(test_cycle_resampler PRIVATE src tests)
target_link_libraries(test_cycle_resampler PRIVATE v0a_dsp)
add_test(NAME cycle_resampler COMMAND test_cycle_resampler)

add_executable(test_note_quantizer tests/test_note_quantizer.cpp)
target_include_directories(test_note_quantizer PRIVATE src tests)
target_link_libraries(test_note_quantizer PRIVATE v0a_dsp)
add_test(NAME note_quantizer COMMAND test_note_quantizer)

add_executable(test_retune_controller tests/test_retune_controller.cpp)
target_include_directories(test_retune_controller PRIVATE src tests)
target_link_libraries(test_retune_controller PRIVATE v0a_dsp)
add_test(NAME retune_controller COMMAND test_retune_controller)

add_executable(test_pitch_detector tests/test_pitch_detector.cpp)
target_include_directories(test_pitch_detector PRIVATE src tests)
target_link_libraries(test_pitch_detector PRIVATE v0a_dsp)
add_test(NAME pitch_detector COMMAND test_pitch_detector)

add_executable(test_pitch_correction_engine tests/test_pitch_correction_engine.cpp)
target_include_directories(test_pitch_correction_engine PRIVATE src tests)
target_link_libraries(test_pitch_correction_engine PRIVATE v0a_dsp)
add_test(NAME pitch_correction_engine COMMAND test_pitch_correction_engine)

add_executable(test_humanize_controller tests/test_humanize_controller.cpp)
target_include_directories(test_humanize_controller PRIVATE src tests)
target_link_libraries(test_humanize_controller PRIVATE v0a_dsp)
add_test(NAME humanize_controller COMMAND test_humanize_controller)

add_executable(test_target_controller tests/test_target_controller.cpp)
target_include_directories(test_target_controller PRIVATE src tests)
target_link_libraries(test_target_controller PRIVATE v0a_dsp)
add_test(NAME target_controller COMMAND test_target_controller)

add_executable(test_auto_tune_renderer tests/test_auto_tune_renderer.cpp)
target_include_directories(test_auto_tune_renderer PRIVATE src tests)
target_link_libraries(test_auto_tune_renderer PRIVATE v0a_app)
add_test(NAME auto_tune_renderer COMMAND test_auto_tune_renderer)

add_executable(test_wav_file tests/test_wav_file.cpp)
target_include_directories(test_wav_file PRIVATE src tests)
target_link_libraries(test_wav_file PRIVATE v0a_io)
add_test(NAME wav_file COMMAND test_wav_file)

add_executable(test_offline_renderer tests/test_offline_renderer.cpp)
target_include_directories(test_offline_renderer PRIVATE src tests)
target_link_libraries(test_offline_renderer PRIVATE v0a_app)
add_test(NAME offline_renderer COMMAND test_offline_renderer)

option(V0A_BUILD_V1_LAB "Build the V1-Lab JUCE AU/VST3" OFF)
set(JUCE_SOURCE_DIR "/Users/hnsk/Projects/Tools-and-Triage/motes/JUCE"
    CACHE PATH "Path to a JUCE source checkout")

if(V0A_BUILD_V1_LAB)
    if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_SOURCE_DIR does not point to a JUCE source checkout")
    endif()
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

    juce_add_plugin(V1Lab
        COMPANY_NAME "HNSK Labs"
        BUNDLE_ID "com.hnsk.v1lab"
        PLUGIN_MANUFACTURER_CODE Hnsk
        PLUGIN_CODE Vlab
        FORMATS AU VST3
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Pitch Shift
        PRODUCT_NAME "V1-Lab")

    target_sources(V1Lab PRIVATE
        plugin/PluginEditor.cpp
        plugin/PluginEditor.h
        plugin/PluginProcessor.cpp
        plugin/PluginProcessor.h
        plugin/Parameters.h)
    target_include_directories(V1Lab PRIVATE src plugin)
    target_compile_definitions(V1Lab PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(V1Lab
        PRIVATE v0a_dsp juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)

    add_executable(test_v1lab_processor tests/test_v1lab_processor.cpp)
    target_include_directories(test_v1lab_processor PRIVATE src plugin tests)
    target_link_libraries(test_v1lab_processor PRIVATE V1Lab juce::juce_audio_utils)
    add_test(NAME v1lab_processor COMMAND test_v1lab_processor)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(V1Lab PRIVATE
            -Wno-error=deprecated-declarations
            -Wno-error=unused-but-set-variable
            -Wno-error=unused-parameter)
        target_compile_options(V1Lab_AU PRIVATE -Wno-error=deprecated-declarations)
        target_compile_options(V1Lab_VST3 PRIVATE -Wno-error=deprecated-declarations)
        target_compile_options(test_v1lab_processor PRIVATE
            -Wno-error=deprecated-declarations
            -Wno-error=unused-but-set-variable
            -Wno-error=unused-parameter)
    endif()

    if(APPLE)
        add_custom_command(TARGET V1Lab_AU POST_BUILD
            COMMAND /usr/bin/codesign --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:V1Lab_AU>"
            COMMENT "Ad-hoc signing V1-Lab AU for local validation"
            VERBATIM)
    endif()
endif()
