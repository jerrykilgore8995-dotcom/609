cmake_minimum_required(VERSION 3.22)
project(Nodal550A VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DECK_DIR "" CACHE PATH "Directory containing Deck.hpp")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/nodal")
file(MAKE_DIRECTORY "${generated}")
function(nodal_options target)
    target_compile_definitions(${target} PRIVATE CLASSIC_MODEL=550 NATIVE550A NOMINMAX)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE "${DECK_DIR}" "${generated}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /arch:AVX2 /fp:precise /utf-8)
        target_compile_definitions(${target} PRIVATE MKII_CODE_SPEED)
    else()
        target_compile_options(${target} PRIVATE -ffp-contract=off)
    endif()
endfunction()

add_executable(Generate550A ../native/GenerateSolve.cpp)
nodal_options(Generate550A)
add_custom_command(OUTPUT "${generated}/GeneratedSolve.hpp"
    COMMAND Generate550A "${generated}/GeneratedSolve.hpp"
    DEPENDS Generate550A "${DECK_DIR}/Deck.hpp"
    VERBATIM)
add_custom_target(GenerateCircuit DEPENDS "${generated}/GeneratedSolve.hpp")

juce_add_plugin(Nodal550A
    COMPANY_NAME Kilgore PLUGIN_MANUFACTURER_CODE Jkil PLUGIN_CODE N55A
    BUNDLE_ID com.kilgore.nodal550a PRODUCT_NAME "550A Nodal"
    FORMATS VST3 IS_SYNTH FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ)
target_sources(Nodal550A PRIVATE Plugin.cpp)
target_link_libraries(Nodal550A PRIVATE juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)
target_compile_definitions(Nodal550A PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
nodal_options(Nodal550A)
add_dependencies(Nodal550A GenerateCircuit)

add_executable(Bench550A Bench.cpp)
nodal_options(Bench550A)
add_dependencies(Bench550A GenerateCircuit)

add_executable(Host550A Host.cpp)
target_compile_features(Host550A PRIVATE cxx_std_20)
target_compile_definitions(Host550A PRIVATE NOMINMAX JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(Host550A PRIVATE juce::juce_audio_processors juce::juce_gui_basics)
