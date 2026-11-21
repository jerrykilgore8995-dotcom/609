cmake_minimum_required(VERSION 3.22)
project(Compressor609 VERSION 0.1.0 LANGUAGES C CXX)

set(native_dir "${CMAKE_CURRENT_SOURCE_DIR}/../native")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/nodal")
set(NODAL_DECK_HEADER "" CACHE FILEPATH "Deck.hpp generated from the current SPICE circuit")
if(NODAL_DECK_HEADER)
    add_custom_command(OUTPUT "${generated_dir}/Deck.hpp"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${generated_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy "${NODAL_DECK_HEADER}" "${generated_dir}/Deck.hpp"
        DEPENDS "${NODAL_DECK_HEADER}"
        VERBATIM)
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(NGSPICE_EXECUTABLE ngspice REQUIRED)
    set(circuit_dir "${CMAKE_CURRENT_SOURCE_DIR}/../circuit")
    file(GLOB circuit_sources CONFIGURE_DEPENDS "${circuit_dir}/*.cir" "${circuit_dir}/*.inc")
    add_custom_command(OUTPUT "${generated_dir}/Deck.hpp"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${generated_dir}"
        COMMAND ${CMAKE_COMMAND} -E env "NGSPICE=${NGSPICE_EXECUTABLE}"
                "${Python3_EXECUTABLE}" "${native_dir}/../tools/native_seed.py" "${generated_dir}/seed.raw"
        COMMAND "${Python3_EXECUTABLE}" "${native_dir}/compile_deck.py"
                "${circuit_dir}" "${generated_dir}/seed.raw" "${generated_dir}/Deck.hpp"
        DEPENDS ${circuit_sources} "${native_dir}/compile_deck.py"
                "${native_dir}/../tools/native_seed.py" "${native_dir}/../tools/measure.py"
        VERBATIM)
endif()
add_executable(Generate609Solve "${native_dir}/GenerateSolve.cpp" "${generated_dir}/Deck.hpp")
target_include_directories(Generate609Solve PRIVATE "${generated_dir}")
target_compile_features(Generate609Solve PRIVATE cxx_std_20)
add_custom_command(OUTPUT "${generated_dir}/GeneratedSolve.hpp"
    COMMAND Generate609Solve "${generated_dir}/GeneratedSolve.hpp"
    DEPENDS Generate609Solve "${generated_dir}/Deck.hpp"
    VERBATIM)
add_custom_target(Generate609Deck DEPENDS "${generated_dir}/GeneratedSolve.hpp")

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Compressor609
    COMPANY_NAME Kilgore
    PLUGIN_MANUFACTURER_CODE Jkil
    PLUGIN_CODE K69E
    BUNDLE_ID com.kilgore.33609jd2x
    PRODUCT_NAME "33609JD 2x"
    FORMATS VST3
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics)
target_sources(Compressor609 PRIVATE Plugin.cpp)
target_compile_features(Compressor609 PRIVATE cxx_std_20)
target_compile_definitions(Compressor609 PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                                                 JUCE_VST3_CAN_REPLACE_VST2=0 NOMINMAX MKII_CODE_SPEED)
target_link_libraries(Compressor609 PRIVATE juce::juce_audio_processors juce::juce_gui_basics
                                           juce::juce_recommended_config_flags)
juce_add_console_app(Vst3Smoke)
target_sources(Vst3Smoke PRIVATE Vst3Smoke.cpp Plugin.cpp)
target_compile_features(Vst3Smoke PRIVATE cxx_std_20)
target_compile_definitions(Vst3Smoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 NOMINMAX)
target_link_libraries(Vst3Smoke PRIVATE juce::juce_audio_processors juce::juce_gui_basics
                                        juce::juce_recommended_config_flags)
foreach(target Compressor609 Vst3Smoke)
    add_dependencies(${target} Generate609Deck)
    target_include_directories(${target} PRIVATE "${generated_dir}")
endforeach()
if(MSVC)
    foreach(target Generate609Solve Compressor609 Vst3Smoke)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/arch:AVX2> $<$<CONFIG:Release>:/fp:precise>)
    endforeach()
endif()
