#!/usr/bin/env python3
# Builds the source tree of the test plugin "Twenty Engine UI" (the MS-20 panel window, ui/StudioUI.h) from this
# one (same code; it differs only where the code checks TWENTY_UI). Everything that identifies a plugin to a host
# gets its own value: name, AU/VST3/AAX IDs, bundle IDs, Objective-C class prefix (from the CMake project name),
# so it installs and loads next to Twenty Engine.
#   python3 tools/make_ui_variant.py build-ui     -> ./build-ui (build it with -DIPLUG2_DIR=<repo>/iPlug2)
import os, sys, shutil
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'build-ui'))
if os.path.exists(out): shutil.rmtree(out)
os.makedirs(out)

def copy(rel):
    src = os.path.join(root, rel); dst = os.path.join(out, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.isdir(src): shutil.copytree(src, dst)
    else: shutil.copy2(src, dst)

for f in ['TwentyEngine.cpp', 'TwentyEngine.h', 'led_port.h', 'config.h', 'CMakeLists.txt', 'engine', 'ui']:
    copy(f)

# resources: file names and identities
res_src, res_dst = os.path.join(root, 'resources'), os.path.join(out, 'resources')
TEXT = ('.plist', '.xib', '.xml', '.rc', '.storyboard', '_mac_dlg', '_mac_menu')
for dirpath, dirs, files in os.walk(res_src):
    rel = os.path.relpath(dirpath, res_src)
    ddir = os.path.join(res_dst, rel.replace('TwentyEngine', 'TwentyEngineUI'))
    os.makedirs(ddir, exist_ok=True)
    for f in files:
        src = os.path.join(dirpath, f)
        dst = os.path.join(ddir, f.replace('TwentyEngine', 'TwentyEngineUI'))
        if f.endswith(TEXT):
            s = open(src, encoding='utf-8', errors='surrogateescape').read()
            s = s.replace('Twenty Engine MS-20ic', 'Twenty Engine')   # the classic plugin's name -> the UI plugin's
            s = s.replace('TwentyEngine', 'TwentyEngineUI').replace('Twenty Engine', 'Twenty Engine UI').replace('Tw20', 'Tw2U')
            open(dst, 'w', encoding='utf-8', errors='surrogateescape').write(s)
        else:
            shutil.copy2(src, dst)

def edit(rel, pairs):
    p = os.path.join(out, rel); s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        if s.count(a) != 1: raise SystemExit('make_ui_variant: no unique match for %r in %s' % (a, rel))
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)

edit('CMakeLists.txt', [('project(TwentyEngine VERSION', 'project(TwentyEngineUI VERSION')])
edit('config.h', [
    ('#define PLUG_NAME "Twenty Engine MS-20ic"', '#define TWENTY_UI 1\n#define PLUG_NAME "Twenty Engine UI"'),
    ('#define PLUG_WIDTH 360', '#define PLUG_WIDTH 1224'),     # the full MS-20 panel (ui/StudioUI.h)
    ('#define PLUG_HEIGHT 120', '#define PLUG_HEIGHT 580'),
    ('#define PLUG_HOST_RESIZE 0', '#ifdef _WIN32\n#define PLUG_HOST_RESIZE 0   /* Windows: fixed sizes from the window\'s size menu */\n#else\n#define PLUG_HOST_RESIZE 1\n#endif'),
    ("#define PLUG_UNIQUE_ID 'Tw20'", "#define PLUG_UNIQUE_ID 'Tw2U'"),
    ('#define BUNDLE_NAME "TwentyEngine"', '#define BUNDLE_NAME "TwentyEngineUI"'),
    ('#define SHARED_RESOURCES_SUBPATH "TwentyEngine"', '#define SHARED_RESOURCES_SUBPATH "TwentyEngineUI"'),
    ('#define AUV2_ENTRY TwentyEngine_Entry', '#define AUV2_ENTRY TwentyEngineUI_Entry'),
    ('#define AUV2_ENTRY_STR "TwentyEngine_Entry"', '#define AUV2_ENTRY_STR "TwentyEngineUI_Entry"'),
    ('#define AUV2_FACTORY TwentyEngine_Factory', '#define AUV2_FACTORY TwentyEngineUI_Factory'),
    ('#define AUV2_VIEW_CLASS TwentyEngine_View', '#define AUV2_VIEW_CLASS TwentyEngineUI_View'),
    ('#define AUV2_VIEW_CLASS_STR "TwentyEngine_View"', '#define AUV2_VIEW_CLASS_STR "TwentyEngineUI_View"'),
    ("#define AAX_TYPE_IDS 'TW21', 'TW22'", "#define AAX_TYPE_IDS 'TW2V', 'TW2W'"),
    ('#define AAX_PLUG_NAME_STR "TwentyEngine\\nTW20"', '#define AAX_PLUG_NAME_STR "TwentyEngineUI\\nTW2U"'),
])
# the standalone app (APP target) is called "Twenty Engine": its bundle name, menus and window title
def edit_all(rel, a, b):
    p = os.path.join(out, rel); s = open(p, encoding='utf-8').read()
    if a not in s: raise SystemExit('make_ui_variant: %r not in %s' % (a, rel))
    open(p, 'w', encoding='utf-8').write(s.replace(a, b))
edit_all('resources/TwentyEngineUI-macOS-Info.plist', '<string>TwentyEngineUI</string>\n\t<key>CFBundleDisplayName</key>\n\t<string>TwentyEngineUI</string>',
         '<string>Twenty Engine</string>\n\t<key>CFBundleDisplayName</key>\n\t<string>Twenty Engine</string>')
edit_all('resources/TwentyEngineUI-macOS-MainMenu.xib', 'title="About TwentyEngineUI"', 'title="About Twenty Engine"')
edit_all('resources/TwentyEngineUI-macOS-MainMenu.xib', 'title="Hide TwentyEngineUI"', 'title="Hide Twenty Engine"')
edit_all('resources/TwentyEngineUI-macOS-MainMenu.xib', 'title="Quit TwentyEngineUI"', 'title="Quit Twenty Engine"')
edit_all('resources/TwentyEngineUI-macOS-MainMenu.xib', 'title="TwentyEngineUI"', 'title="Twenty Engine"')
edit_all('resources/main.rc', 'CAPTION "TwentyEngineUI"', 'CAPTION "Twenty Engine"')
edit_all('resources/main.rc_mac_dlg', '"TwentyEngineUI",300,300', '"Twenty Engine",300,300')   # the Mac window's title
# the app's menus (Windows: main.rc, Mac: main.rc_mac_menu): no developer menu (live edit, control bounds, FPS,
# screenshot), no "Read Manual" (it only said "See the manual")
for rc in ('resources/main.rc', 'resources/main.rc_mac_menu'):
  edit_all(rc, '''    POPUP "&Debug"
    BEGIN
        MENUITEM "&Live Edit Mode\\tCtrl+E",     ID_LIVE_EDIT
        MENUITEM "&Show Control Bounds\\tCtrl+B", ID_SHOW_BOUNDS
        MENUITEM "&Show Drawn Area\\tCtrl+D",    ID_SHOW_DRAWN
        MENUITEM "&Show FPS\\tCtrl+F",           ID_SHOW_FPS
        MENUITEM SEPARATOR
        MENUITEM "&Save Screenshot\\tCtrl+Shift+S", ID_SCREENSHOT
    END
''', '')
  edit_all(rc, '''        MENUITEM "&Read Manual",                ID_HELP
''', '')
edit_all('resources/main.rc', '''    "B",            ID_SHOW_BOUNDS,         VIRTKEY, CONTROL, NOINVERT
    "D",            ID_SHOW_DRAWN,          VIRTKEY, CONTROL, NOINVERT
    "F",            ID_SHOW_FPS,            VIRTKEY, CONTROL, NOINVERT
    "E",            ID_LIVE_EDIT,           VIRTKEY, CONTROL, NOINVERT
    "S",            ID_SCREENSHOT,          VIRTKEY, CONTROL, SHIFT, NOINVERT
''', '')   # and their keyboard shortcuts (Windows)
print('Twenty Engine UI source in', out)
