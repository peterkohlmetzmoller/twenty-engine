; Twenty Engine - Windows installer (Inno Setup 6)
#define AppVer GetEnv("TE_VERSION")
[Setup]
AppId={{6E2B8C14-7A3F-4D1E-9B20-20E5A1C0FFEE}
AppName=Twenty Engine
AppVersion={#AppVer}
AppPublisher=PeteLabs
DefaultDirName={commoncf64}\VST3\TwentyEngine.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallDisplayName=Twenty Engine (VST3)
LicenseFile=resources\license.txt
InfoBeforeFile=resources\welcome_win.txt
OutputDir=..
OutputBaseFilename=TwentyEngine-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\out\TwentyEngine.vst3\*"; DestDir: "{commoncf64}\VST3\TwentyEngine.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TwentyEngine.vst3"

[Messages]
FinishedLabel=Twenty Engine is installed. Rescan plugins in your music software (or restart it), then add Twenty Engine to an instrument track with the MS-20 iC as its MIDI input.
