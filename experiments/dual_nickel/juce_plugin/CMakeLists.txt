cmake_minimum_required(VERSION 3.22)
project(NickelOpto VERSION 0.1.0 LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_Declare(chowdsp_wdf GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_wdf.git
                         GIT_TAG main GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE chowdsp_wdf)
juce_add_plugin(NickelOpto
    COMPANY_NAME Kilgore
    PLUGIN_MANUFACTURER_CODE Jkil
    PLUGIN_CODE NOpt
    PRODUCT_NAME "Nickel Opto"
    FORMATS VST3
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics)
target_sources(NickelOpto PRIVATE Plugin.cpp)
target_compile_features(NickelOpto PRIVATE cxx_std_20)
target_compile_definitions(NickelOpto PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                                               JUCE_VST3_CAN_REPLACE_VST2=0 NOMINMAX)
target_link_libraries(NickelOpto PRIVATE juce::juce_audio_processors
                                            juce::juce_gui_basics
                                            juce::juce_recommended_config_flags
                                            chowdsp_wdf)

# Offscreen capture utility renders the compiled editor's own paint method.
juce_add_console_app(NickelOptoCapture PRODUCT_NAME "Nickel Opto GUI Capture")
target_sources(NickelOptoCapture PRIVATE CaptureActualGUI.cpp)
target_compile_features(NickelOptoCapture PRIVATE cxx_std_20)
target_compile_definitions(NickelOptoCapture PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 NOMINMAX)
target_link_libraries(NickelOptoCapture PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags chowdsp_wdf)
