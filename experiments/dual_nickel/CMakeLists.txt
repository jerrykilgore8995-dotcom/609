cmake_minimum_required(VERSION 3.22)
project(DualNickelMagnetics LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(chowdsp_wdf
  GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_wdf.git
  GIT_TAG main
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(chowdsp_wdf)
add_executable(nickel_smoke test_nickel.cpp)
target_compile_features(nickel_smoke PRIVATE cxx_std_20)
target_link_libraries(nickel_smoke PRIVATE chowdsp_wdf)
add_executable(nickel_input_sweep sweep_input_drive.cpp)
target_compile_features(nickel_input_sweep PRIVATE cxx_std_20)
target_link_libraries(nickel_input_sweep PRIVATE chowdsp_wdf)
add_executable(nickel_dual_sweep sweep_both_transformers.cpp)
target_compile_features(nickel_dual_sweep PRIVATE cxx_std_20)
target_link_libraries(nickel_dual_sweep PRIVATE chowdsp_wdf)
enable_testing()
add_test(NAME nickel_smoke COMMAND nickel_smoke)

add_executable(nickel_option_c test_option_c.cpp)
target_compile_features(nickel_option_c PRIVATE cxx_std_20)
target_link_libraries(nickel_option_c PRIVATE chowdsp_wdf)
add_test(NAME option_c_driver COMMAND nickel_option_c)

add_executable(nickel_analog_opto test_analog_opto.cpp)
target_compile_features(nickel_analog_opto PRIVATE cxx_std_20)
target_link_libraries(nickel_analog_opto PRIVATE chowdsp_wdf)
add_test(NAME analog_opto_pipeline COMMAND nickel_analog_opto)
