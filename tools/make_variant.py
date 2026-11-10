#!/usr/bin/env python3
# Builds the source tree of the second plugin, "Twenty Engine Studio", from this one (same code; the
# behaviour differs only where TwentyEngine.cpp checks TWENTY_STUDIO). Everything that identifies a plugin
# to a host gets its own value: name, AU/VST3/AAX IDs, bundle IDs, Objective-C class prefix (from the
# CMake project name), so both plugins can be installed and loaded side by side.
#   python3 tools/make_variant.py studio        -> ./studio (build it with -DIPLUG2_DIR=<repo>/iPlug2)
import os, sys, shutil
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'studio'))
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
    ddir = os.path.join(res_dst, rel.replace('TwentyEngine', 'TwentyEngineStudio'))
    os.makedirs(ddir, exist_ok=True)
    for f in files:
        src = os.path.join(dirpath, f)
        dst = os.path.join(ddir, f.replace('TwentyEngine', 'TwentyEngineStudio'))
        if f.endswith(TEXT):
            s = open(src, encoding='utf-8', errors='surrogateescape').read()
            s = s.replace('TwentyEngine', 'TwentyEngineStudio').replace('Twenty Engine', 'Twenty Engine Studio').replace('Tw20', 'Tw2S')
            open(dst, 'w', encoding='utf-8', errors='surrogateescape').write(s)
        else:
            shutil.copy2(src, dst)

def edit(rel, pairs):
    p = os.path.join(out, rel); s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        if s.count(a) != 1: raise SystemExit('make_variant: no unique match for %r in %s' % (a, rel))
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)

edit('CMakeLists.txt', [('project(TwentyEngine VERSION', 'project(TwentyEngineStudio VERSION')])
edit('config.h', [
    ('#define PLUG_NAME "Twenty Engine"', '#define TWENTY_STUDIO 1\n#define PLUG_NAME "Twenty Engine Studio"'),
    ('#define PLUG_WIDTH 360', '#define PLUG_WIDTH 1224'),     # Studio has the full editor window (ui/StudioUI.h)
    ('#define PLUG_HEIGHT 120', '#define PLUG_HEIGHT 580'),
    ('#define PLUG_HOST_RESIZE 0', '#define PLUG_HOST_RESIZE 1'),
    ("#define PLUG_UNIQUE_ID 'Tw20'", "#define PLUG_UNIQUE_ID 'Tw2S'"),
    ('#define BUNDLE_NAME "TwentyEngine"', '#define BUNDLE_NAME "TwentyEngineStudio"'),
    ('#define SHARED_RESOURCES_SUBPATH "TwentyEngine"', '#define SHARED_RESOURCES_SUBPATH "TwentyEngineStudio"'),
    ('#define AUV2_ENTRY TwentyEngine_Entry', '#define AUV2_ENTRY TwentyEngineStudio_Entry'),
    ('#define AUV2_ENTRY_STR "TwentyEngine_Entry"', '#define AUV2_ENTRY_STR "TwentyEngineStudio_Entry"'),
    ('#define AUV2_FACTORY TwentyEngine_Factory', '#define AUV2_FACTORY TwentyEngineStudio_Factory'),
    ('#define AUV2_VIEW_CLASS TwentyEngine_View', '#define AUV2_VIEW_CLASS TwentyEngineStudio_View'),
    ('#define AUV2_VIEW_CLASS_STR "TwentyEngine_View"', '#define AUV2_VIEW_CLASS_STR "TwentyEngineStudio_View"'),
    ("#define AAX_TYPE_IDS 'TW21', 'TW22'", "#define AAX_TYPE_IDS 'TW2T', 'TW2U'"),
    ('#define AAX_PLUG_NAME_STR "TwentyEngine\\nTW20"', '#define AAX_PLUG_NAME_STR "TwentyEngineStudio\\nTW2S"'),
])
print('Twenty Engine Studio source in', out)
