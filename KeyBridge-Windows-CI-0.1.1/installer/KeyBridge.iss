#define MyAppName "KeyBridge"
#define MyAppVersion "0.1.1"
#define MyAppPublisher "KeyBridge"
#define MyAppExeName "KeyBridge-Setup-0.1.1.exe"

[Setup]
AppId={{A7B7D1D5-8D53-4F6F-9B1E-KEYBRIDGE0011}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Common Files\VST3\KeyBridge.vst3
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename={#MyAppExeName}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern

[Files]
Source: "stage\KeyBridge.vst3\*"; DestDir: "{autopf}\Common Files\VST3\KeyBridge.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\README.txt"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{autopf}\Common Files\VST3\KeyBridge.vst3"

[Messages]
FinishedLabel=KeyBridge has been installed. Restart FL Studio and rescan plug-ins if it does not appear immediately.
