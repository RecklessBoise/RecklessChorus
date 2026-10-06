cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(RecklessChorus VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RECKLESS_BUILD_TESTS "Build the DSP unit tests" ON)
option(RECKLESS_COPY_PLUGIN "Copy the built plug-ins to the system plug-in folders" OFF)

include(FetchContent)

# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to reuse a local checkout.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Pure C++ DSP core (no JUCE dependency, unit-testable)
# ---------------------------------------------------------------------------
add_library(reckless_dsp STATIC
    src/dsp/ChorusCore.cpp)
target_include_directories(reckless_dsp PUBLIC src)
target_compile_features(reckless_dsp PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# Plug-in
# ---------------------------------------------------------------------------
juce_add_plugin(RecklessChorus
    PRODUCT_NAME             "RecklessChorus"
    COMPANY_NAME             "RecklessBoise"
    BUNDLE_ID                com.recklessboise.recklesschorus
    PLUGIN_MANUFACTURER_CODE Rkbs
    PLUGIN_CODE              Rkch
    FORMATS                  VST3 AU Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          Fx Modulation
    AU_MAIN_TYPE             kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD  ${RECKLESS_COPY_PLUGIN})

set(RECKLESS_PLUGIN_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Parameters.cpp
    src/Presets.cpp
    src/PresetManager.cpp
    src/ui/RecklessLookAndFeel.cpp
    src/ui/Widgets.cpp
    src/ui/VoiceVisualizer.cpp
    src/ui/GraphPanels.cpp
    src/ui/PresetDialog.cpp)
target_sources(RecklessChorus PRIVATE ${RECKLESS_PLUGIN_SOURCES})

target_compile_definitions(RecklessChorus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RecklessChorus
    PRIVATE
        reckless_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(RECKLESS_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    add_executable(RecklessTests tests/ChorusCoreTests.cpp)
    target_link_libraries(RecklessTests PRIVATE reckless_dsp Catch2::Catch2WithMain)

    # Plug-in level tests (presets, state) — compiles the plug-in sources against JUCE.
    juce_add_console_app(RecklessPluginTests PRODUCT_NAME "RecklessPluginTests")
    target_sources(RecklessPluginTests PRIVATE tests/PresetManagerTests.cpp ${RECKLESS_PLUGIN_SOURCES})
    target_include_directories(RecklessPluginTests PRIVATE src)
    target_compile_definitions(RecklessPluginTests PRIVATE
        JucePlugin_Name="RecklessChorus"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RecklessPluginTests PRIVATE
        reckless_dsp Catch2::Catch2WithMain juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(RecklessTests)
    catch_discover_tests(RecklessPluginTests)
endif()

# ---------------------------------------------------------------------------
# Offscreen editor renderer (README screenshots)
# ---------------------------------------------------------------------------
option(RECKLESS_BUILD_SCREENSHOT "Build the offscreen editor screenshot tool" OFF)
if(RECKLESS_BUILD_SCREENSHOT)
    juce_add_console_app(RecklessScreenshot PRODUCT_NAME "RecklessScreenshot")
    target_sources(RecklessScreenshot PRIVATE tools/Screenshot.cpp ${RECKLESS_PLUGIN_SOURCES})
    target_include_directories(RecklessScreenshot PRIVATE src)
    target_compile_definitions(RecklessScreenshot PRIVATE
        JucePlugin_Name="RecklessChorus"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RecklessScreenshot PRIVATE
        reckless_dsp juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
