#define MyAppName "Tunerite"
#define MyAppVersion "0.1.1"
#define MyAppPublisher "MurderMittenMedia"
#define MyAppExeName "Tunerite-Setup-0.1.1"

[Setup]
AppId={{A7B7D1D5-8D53-4F6F-9B1E-4B5249443031}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Common Files\VST3\Tunerite.vst3
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename={#MyAppExeName}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern

[Files]
Source: "stage\Tunerite.vst3\*"; DestDir: "{autopf}\Common Files\VST3\Tunerite.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\README.txt"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{autopf}\Common Files\VST3\Tunerite.vst3"

[Messages]
FinishedLabel=Tunerite by MurderMittenMedia has been installed. Restart FL Studio and rescan plug-ins if it does not appear immediately.
