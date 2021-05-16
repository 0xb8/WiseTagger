
[Setup]
AppName=WiseTagger
AppCopyright=Copyright (C) 2021 catgirl
AppVersion=0.5.9
VersionInfoVersion=0.5.9

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

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "WiseTagger.exe"; DestDir: "{app}"; DestName: "WiseTagger.exe"
Source: *; Excludes: "*.iss"; DestDir: "{app}" ; Flags: recursesubdirs

[Icons]
Name: "{group}\WiseTagger"; Filename: "{app}\WiseTagger.exe"
Name: "{group}\Uninstall WiseTagger"; Filename: "{uninstallexe}"

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
Type: files; Name: "{app}\imageformats\qmng.dll"
Type: files; Name: "{app}\platforms\qminimal.dll"
Type: files; Name: "{app}\platforms\qoffscreen.dll"
Type: files; Name: "{app}\platforms\qwebgl.dll"

 [Registry]
Root: HKCU; Subkey: "Software\Classes\.wt-session"; ValueType: string; ValueName: ""; ValueData: "WiseTaggerSessionFile"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile"; ValueType: string; ValueName: ""; ValueData: "WiseTagger Session File"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WiseTagger.exe,0"
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WiseTagger.exe"" ""%1"""
