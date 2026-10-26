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

# AU only, no graphics: Logic shows its own parameter controls
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
  FORMATS AU
  UI NONE
  DEFINES NO_IGRAPHICS
)
