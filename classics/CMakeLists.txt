cmake_minimum_required(VERSION 3.22)
project(ClassicDiodeBridge VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DECK_DIR "" CACHE PATH "Directory containing <model>/Deck.hpp")
set(native_dir "${CMAKE_CURRENT_SOURCE_DIR}/../native")
foreach(model IN ITEMS 2254 2264)
    set(deck "${DECK_DIR}/${model}")
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/${model}")
    file(MAKE_DIRECTORY "${generated}")
    if(NOT EXISTS "${deck}/Deck.hpp")
        message(FATAL_ERROR "Missing generated SPICE deck for ${model} at ${deck}")
    endif()
    add_executable(Generate${model} "${native_dir}/GenerateSolve.cpp")
    target_include_directories(Generate${model} PRIVATE "${deck}")
    target_compile_features(Generate${model} PRIVATE cxx_std_20)
    add_custom_command(OUTPUT "${generated}/GeneratedSolve.hpp"
        COMMAND Generate${model} "${generated}/GeneratedSolve.hpp"
        DEPENDS Generate${model} "${deck}/Deck.hpp"
        VERBATIM)
    add_custom_target(Deck${model} DEPENDS "${generated}/GeneratedSolve.hpp")

    string(SUBSTRING "${model}" 1 3 code)
    juce_add_plugin(Classic${model}
        COMPANY_NAME Kilgore
        PLUGIN_MANUFACTURER_CODE Jkil
        PLUGIN_CODE N${code}
        BUNDLE_ID com.kilgore.nodal${model}
        PRODUCT_NAME "${model} Nodal"
        FORMATS VST3
        IS_SYNTH FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics)
    target_sources(Classic${model} PRIVATE Plugin.cpp)
    target_link_libraries(Classic${model} PRIVATE juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    target_compile_definitions(Classic${model} PRIVATE CLASSIC_MODEL=${model}
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 NOMINMAX)
    add_executable(Bench${model} Bench.cpp)
    target_compile_definitions(Bench${model} PRIVATE CLASSIC_MODEL=${model} NOMINMAX)
    foreach(target IN ITEMS Classic${model} Bench${model})
        add_dependencies(${target} Deck${model})
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_include_directories(${target} PRIVATE "${deck}" "${generated}")
        if(MSVC)
            target_compile_definitions(${target} PRIVATE MKII_CODE_SPEED)
            target_compile_options(${target} PRIVATE /arch:AVX2 /fp:precise)
        endif()
    endforeach()
    if(MSVC)
        target_compile_options(Generate${model} PRIVATE /arch:AVX2 /fp:precise)
    endif()
endforeach()
