# Twenty Engine for Linux: the standalone app with the panel window (DPF, JACK or plain ALSA/PulseAudio).
#   sudo apt install build-essential cmake pkg-config libasound2-dev libjack-jackd2-dev libgl-dev libx11-dev \
#                    libxext-dev libxrandr-dev libxcursor-dev libdbus-1-dev
#   cmake -S linux -B build-linux -DCMAKE_BUILD_TYPE=Release && cmake --build build-linux -j
#   ./build-linux/bin/TwentyEngine
# DPF is fetched from GitHub (a fixed commit), or taken from -DTE_DPF_DIR=/path/to/DPF.
cmake_minimum_required(VERSION 3.15)
project(TwentyEngineLinux CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TE_DPF_DIR "" CACHE PATH "a local DPF checkout (empty: fetch it)")
if(TE_DPF_DIR)
  add_subdirectory(${TE_DPF_DIR} dpf)
else()
  include(FetchContent)
  FetchContent_Declare(dpf GIT_REPOSITORY https://github.com/DISTRHO/DPF.git GIT_TAG 4238e1c7f0351bbe488d79f0899c540543ac7583 GIT_SUBMODULES_RECURSE ON)
  FetchContent_MakeAvailable(dpf)
endif()

set(TE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

dpf_add_plugin(TwentyEngine
  TARGETS jack
  UI_TYPE opengl
  FILES_DSP
    TwentyEnginePlugin.cpp
    ${TE_ROOT}/engine/engine_globals.cpp
  FILES_UI
    TwentyEngineUI.cpp)

target_include_directories(TwentyEngine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/shim
  ${TE_ROOT})
target_compile_definitions(TwentyEngine PUBLIC TWENTY_ALSA=1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
target_link_libraries(TwentyEngine PUBLIC PkgConfig::ALSA)
