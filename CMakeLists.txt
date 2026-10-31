cmake_minimum_required(VERSION 3.14)
project(TwentyEngine VERSION 1.0.0)

# iPlug2 lives next to the sources (cloned by the build workflow at a fixed version)
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac: AU + VST3. Windows: VST3.
if(APPLE)
  set(TWENTY_FORMATS AU VST3)
else()
  set(TWENTY_FORMATS VST3)
endif()

# Minimal window: name + connection status (Logic shows the parameters in its own controls view)
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    TwentyEngine.cpp
    TwentyEngine.h
    led_port.h
    engine/engine_gen.h
    engine/ksoloti_host.h
    engine/ms20_host.h
    engine/engine_globals.cpp
    resources/resource.h
    resources/font_data.h
  FORMATS ${TWENTY_FORMATS}
)

# The LED port and controller detection use Core MIDI on the Mac (every plugin format), WinMM on Windows
foreach(_t ${PROJECT_NAME}-au ${PROJECT_NAME}-vst3)
  if(TARGET ${_t})
    if(APPLE)
      target_link_libraries(${_t} PRIVATE "-framework CoreMIDI" "-framework CoreFoundation")
    elseif(WIN32)
      target_link_libraries(${_t} PRIVATE winmm)
    endif()
  endif()
endforeach()
