cmake_minimum_required(VERSION 3.22)
project(Compressor609 VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Compressor609
    COMPANY_NAME Kilgore
    PLUGIN_MANUFACTURER_CODE Jkil
    PLUGIN_CODE K609
    PRODUCT_NAME "609"
    FORMATS VST3
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics)
target_sources(Compressor609 PRIVATE Plugin.cpp)
target_compile_features(Compressor609 PRIVATE cxx_std_20)
target_compile_definitions(Compressor609 PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                                                 JUCE_VST3_CAN_REPLACE_VST2=0 NOMINMAX)
target_link_libraries(Compressor609 PRIVATE juce::juce_audio_processors juce::juce_gui_basics
                                           juce::juce_recommended_config_flags)
juce_add_console_app(Vst3Smoke)
target_sources(Vst3Smoke PRIVATE Vst3Smoke.cpp Plugin.cpp)
target_compile_features(Vst3Smoke PRIVATE cxx_std_20)
target_compile_definitions(Vst3Smoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 NOMINMAX)
target_link_libraries(Vst3Smoke PRIVATE juce::juce_audio_processors juce::juce_gui_basics
                                        juce::juce_recommended_config_flags)
