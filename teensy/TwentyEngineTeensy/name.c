// USB MIDI name: the Teensy shows up on the computer as "Twenty Engine"
// (the plugin's controller search looks for this name)
#include "usb_names.h"

#define MIDI_NAME     {'T','w','e','n','t','y',' ','E','n','g','i','n','e'}
#define MIDI_NAME_LEN 13

struct usb_string_descriptor_struct usb_string_product_name = {
  2 + MIDI_NAME_LEN * 2,
  3,
  MIDI_NAME
};
