[Setup]
AppName=WiseTagger
AppCopyright=Copyright (C) 2024 catgirl
AppVersion=0.6.11
VersionInfoVersion=0.6.11

AppPublisher=catgirl
AppPublisherURL=https://github.com/0xb8/WiseTagger
AppReadmeFile=https://github.com/0xb8/WiseTagger
AppSupportURL=https://github.com/0xb8/WiseTagger

DefaultDirName={userpf}\WiseTagger
DefaultGroupName=WiseTagger
UninstallDisplayIcon={app}\WiseTagger.exe,0
ChangesAssociations=yes
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
OutputDir=..\
LicenseFile=LICENSE.txt
SetupIconFile=..\icon.ico
WizardStyle=modern
AllowNoIcons=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
Source: "WiseTagger.exe"; DestDir: "{app}"; DestName: "WiseTagger.exe"
Source: *; Excludes: "*.iss"; DestDir: "{app}" ; Flags: recursesubdirs replacesameversion

[Icons]
Name: "{group}\WiseTagger"; Filename: "{app}\WiseTagger.exe"

[InstallDelete]
Type: files; Name: "{app}\libicudt62.dll"
Type: files; Name: "{app}\libicuin62.dll"
Type: files; Name: "{app}\libicuuc62.dll"
Type: files; Name: "{app}\libicudt64.dll"
Type: files; Name: "{app}\libicuin64.dll"
Type: files; Name: "{app}\libicuuc64.dll"
Type: files; Name: "{app}\libicudt65.dll"
Type: files; Name: "{app}\libicuin65.dll"
Type: files; Name: "{app}\libicuuc65.dll"
Type: files; Name: "{app}\libicudt67.dll"
Type: files; Name: "{app}\libicuin67.dll"
Type: files; Name: "{app}\libicuuc67.dll"
Type: files; Name: "{app}\libicudt68.dll"
Type: files; Name: "{app}\libicuin68.dll"
Type: files; Name: "{app}\libicuuc68.dll"
Type: files; Name: "{app}\libicudt69.dll"
Type: files; Name: "{app}\libicuin69.dll"
Type: files; Name: "{app}\libicuuc69.dll"
Type: files; Name: "{app}\libicudt72.dll"
Type: files; Name: "{app}\libicuin72.dll"
Type: files; Name: "{app}\libicuuc72.dll"
Type: files; Name: "{app}\libicudt73.dll"
Type: files; Name: "{app}\libicuin73.dll"
Type: files; Name: "{app}\libicuuc73.dll"
Type: files; Name: "{app}\libtiff-5.dll"
Type: files; Name: "{app}\libavif.dll"
Type: files; Name: "{app}\libavif.dll"
Type: files; Name: "{app}\libdav1d.dll"
Type: files; Name: "{app}\libSvtAv1Enc.dll"
Type: files; Name: "{app}\libx265.dll"
Type: files; Name: "{app}\imageformats\qmng.dll"
Type: files; Name: "{app}\platforms\qminimal.dll"
Type: files; Name: "{app}\platforms\qoffscreen.dll"
Type: files; Name: "{app}\platforms\qwebgl.dll"

[Registry]
Root: HKCU; Subkey: "Software\Classes\.wt-session"; ValueType: string; ValueName: ""; ValueData: "WiseTaggerSessionFile"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile"; ValueType: string; ValueName: ""; ValueData: "WiseTagger Session File"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WiseTagger.exe,0"
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WiseTagger.exe"" ""%1"""
