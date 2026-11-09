cmake_minimum_required(VERSION 3.22)
project(Saturno8 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
# Binario universal (Intel + Apple Silicon) en macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(S8_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND S8_FORMATS AU)
endif()

juce_add_plugin(Saturno8
    COMPANY_NAME "Victor"
    BUNDLE_ID "com.victor.saturno8"
    PLUGIN_MANUFACTURER_CODE Vict
    PLUGIN_CODE Sat8
    FORMATS ${S8_FORMATS}
    PRODUCT_NAME "Saturno 8"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Saturno8)

target_sources(Saturno8 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Params.h
    src/dsp/DspUtils.h
    src/dsp/Voice.h
    src/dsp/Layer.h
    src/dsp/Synth.h)

# Los textos de la interfaz llevan acentos: compilar como UTF-8 también en Windows.
if(MSVC)
    target_compile_options(Saturno8 PRIVATE /utf-8)
endif()

target_compile_definitions(Saturno8 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Saturno8
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Herramienta de línea de comandos que renderiza sonidos de ejemplo a WAV (sin JUCE).
add_executable(render_demo tools/render_demo.cpp)
target_compile_features(render_demo PRIVATE cxx_std_20)

# Pruebas del motor (sin JUCE): ctest --test-dir build -C Release
enable_testing()
add_executable(test_voice tests/test_voice.cpp)
target_compile_features(test_voice PRIVATE cxx_std_20)
add_test(NAME voice COMMAND test_voice)
add_executable(test_synth tests/test_synth.cpp)
target_compile_features(test_synth PRIVATE cxx_std_20)
add_test(NAME synth COMMAND test_synth)

# Captura de la interfaz sin ventana (opcional): -DS8_UI_SNAPSHOT=ON
option(S8_UI_SNAPSHOT "Construir la herramienta de captura de la interfaz" OFF)
if(S8_UI_SNAPSHOT)
    juce_add_console_app(ui_snapshot PRODUCT_NAME "ui_snapshot")
    target_sources(ui_snapshot PRIVATE tools/ui_snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(ui_snapshot PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/Saturno8_artefacts/JuceLibraryCode)
    target_compile_definitions(ui_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Saturno 8" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0 JUCE_STANDALONE_APPLICATION=1)
    juce_generate_juce_header(ui_snapshot)
    target_link_libraries(ui_snapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
