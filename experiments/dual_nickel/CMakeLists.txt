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
enable_testing()
add_test(NAME nickel_smoke COMMAND nickel_smoke)
