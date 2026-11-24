// Twenty Engine for Linux (DPF): the standalone app with the panel window.
#pragma once

#define DISTRHO_PLUGIN_BRAND   "Peter Kohlmetz Moller"
#define DISTRHO_PLUGIN_NAME    "Twenty Engine"
#define DISTRHO_PLUGIN_URI     "https://gumroad.com/twenty-engine"
#define DISTRHO_PLUGIN_CLAP_ID "dk.kohlmetz.twenty-engine"

#define DISTRHO_PLUGIN_NUM_INPUTS   0
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2
#define DISTRHO_PLUGIN_IS_SYNTH     1
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
// the controller and keyboards come in through the app's own ALSA client (led_port.h); the host's MIDI input
// (DPF requires one for a synth) is not used by the standalone app
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1

#define DISTRHO_PLUGIN_HAS_UI 1
#define DISTRHO_UI_USE_NANOVG 1
#define DISTRHO_UI_USER_RESIZABLE 1
#define DISTRHO_UI_DEFAULT_WIDTH  1224
#define DISTRHO_UI_DEFAULT_HEIGHT 580
