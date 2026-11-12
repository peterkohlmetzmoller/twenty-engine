cmake_minimum_required(VERSION 3.14)
project(TwentyEngine VERSION 1.0.1)

# iPlug2 lives next to the sources (cloned by the build workflow at a fixed version)
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac: AU + VST3. Windows: VST3.
# AAX (Pro Tools) is built only when the AAX SDK is present in iPlug2/Dependencies/IPlug/AAX_SDK
# (free from Avid for registered developers); without it iPlug2 skips the AAX target.
if(APPLE)
  set(TWENTY_FORMATS AU VST3 AAX)
else()
  set(TWENTY_FORMATS VST3 AAX)
endif()

# Minimal window: name + connection status (Logic shows the parameters in its own controls view).
# Windows: no IGraphics (its OpenGL window crashed with some drivers, e.g. in virtual machines); the plugin
# draws the same status window itself with GDI (TwentyEngine.cpp).
set(TWENTY_DEFS "")
if(WIN32)
  set(TWENTY_UI UI NONE)
  list(APPEND TWENTY_DEFS NO_IGRAPHICS)
else()
  set(TWENTY_UI "")
endif()
# VST3: MIDI CCs reach the plugin on all 16 channels (the sound in the region uses channels 15 and 16)
list(APPEND TWENTY_DEFS VST3_NUM_MIDI_IN_CHANS=16 VST3_NUM_CC_CHANS=16)
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
  ${TWENTY_UI}
  DEFINES ${TWENTY_DEFS}
)

# The LED port and controller detection use Core MIDI on the Mac (every plugin format), WinMM on Windows
foreach(_t ${PROJECT_NAME}-au ${PROJECT_NAME}-vst3 ${PROJECT_NAME}-aax)
  if(TARGET ${_t})
    if(APPLE)
      target_link_libraries(${_t} PRIVATE "-framework CoreMIDI" "-framework CoreFoundation")
    elseif(WIN32)
      target_link_libraries(${_t} PRIVATE winmm)
    endif()
  endif()
endforeach()

# iPlug2 compiles its Metal NanoVG file once, without the per-plugin Objective-C prefix, so every iPlug2 plugin
# exports the same class names (MNVGbuffers, MNVGcontext, MNVGtexture). Give ours a name of their own, so they
# cannot clash with another plugin's copy in the same host. Only nanovg_mtl.m uses these classes.
if(APPLE AND TARGET iPlug2_IGraphics_NanoVG_Metal_obj)
  target_compile_options(iPlug2_IGraphics_NanoVG_Metal_obj PRIVATE "SHELL:-include ${IPLUG2_DIR}/IPlug/IPlugOBJCPrefix.pch")
  target_compile_definitions(iPlug2_IGraphics_NanoVG_Metal_obj PRIVATE OBJC_PREFIX=v${PROJECT_NAME} API=_nvg)
endif()
