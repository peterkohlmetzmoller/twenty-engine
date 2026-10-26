// Product identity - change names here before release (keep "Korg"/"MS-20" out of the product name).
// The AU manufacturer and unique IDs must also match resources/TwentyEngine-AU-Info.plist.
#define PLUG_NAME "TwentyEngine"
#define PLUG_MFR "PeteLabs"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'Tw20'
#define PLUG_MFR_ID 'PtLb'
#define PLUG_URL_STR "https://example.com"
#define PLUG_EMAIL_STR "support@example.com"
#define PLUG_COPYRIGHT_STR "Copyright 2026 PeteLabs"
#define PLUG_CLASS_NAME TwentyEngine

#define BUNDLE_NAME "TwentyEngine"
#define BUNDLE_MFR "PeteLabs"
#define BUNDLE_DOMAIN "com"

#define PLUG_CHANNEL_IO "0-2"
#define SHARED_RESOURCES_SUBPATH "TwentyEngine"

#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 0
#define PLUG_WIDTH 400
#define PLUG_HEIGHT 300
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY TwentyEngine_Entry
#define AUV2_ENTRY_STR "TwentyEngine_Entry"
#define AUV2_FACTORY TwentyEngine_Factory
#define AUV2_VIEW_CLASS TwentyEngine_View
#define AUV2_VIEW_CLASS_STR "TwentyEngine_View"

#define AAX_TYPE_IDS 'TW21', 'TW22'
#define AAX_PLUG_MFR_STR "PeteLabs"
#define AAX_PLUG_NAME_STR "TwentyEngine\nTW20"
#define AAX_DOES_AUDIOSUITE 0
#define AAX_PLUG_CATEGORY_STR "Synth"

#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL "https://example.com"
#define CLAP_SUPPORT_URL "https://example.com"
#define CLAP_DESCRIPTION "MS-20 iC driven poly synth"
#define CLAP_FEATURES "instrument"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64
