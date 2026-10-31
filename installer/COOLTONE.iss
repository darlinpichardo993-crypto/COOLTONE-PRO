; COOLTONE by ORYMY - YMY RECORDS - Windows installer (Inno Setup 6.3+)
#define MyAppName "COOLTONE"
#define MyAppVersion "1.0.0"

[Setup]
AppId={{B3B7E5A2-5E4C-4B7B-9C55-4C0D4F2A7C11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=YMY RECORDS
DefaultDirName={autopf}\YMY RECORDS\COOLTONE
DefaultGroupName=YMY RECORDS
OutputDir=..\dist
OutputBaseFilename=COOLTONE_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=COOLTONE by ORYMY

[Files]
; VST3 goes to the standard folder that FL Studio scans: C:\Program Files\Common Files\VST3
Source: "..\build\COOLTONE_artefacts\Release\VST3\COOLTONE.vst3\*"; DestDir: "{commoncf64}\VST3\COOLTONE.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\COOLTONE_artefacts\Release\Standalone\COOLTONE.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\COOLTONE"; Filename: "{app}\COOLTONE.exe"
Name: "{group}\Uninstall COOLTONE"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\COOLTONE.exe"; Description: "Launch COOLTONE Standalone"; Flags: nowait postinstall skipifsilent unchecked
