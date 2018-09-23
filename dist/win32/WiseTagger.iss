
[Setup]
AppName=WiseTagger
AppCopyright=Copyright (C) 2018 catgirl
AppVersion=0.5.4
VersionInfoVersion=0.5.4

AppPublisher=catgirl
AppPublisherURL=https://wolfgirl.org/
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


ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

MinVersion=6.0

[Files]
Source: "WiseTagger.exe"; DestDir: "{app}"; DestName: "WiseTagger.exe"
Source: *; Excludes: "*.iss"; DestDir: "{app}" ; Flags: recursesubdirs


[Icons]
Name: "{group}\WiseTagger"; Filename: "{app}\WiseTagger.exe"
Name: "{group}\Uninstall WiseTagger"; Filename: "{uninstallexe}"

 [Registry]
Root: HKCU; Subkey: "Software\Classes\.wt-session"; ValueType: string; ValueName: ""; ValueData: "WiseTaggerSessionFile"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile"; ValueType: string; ValueName: ""; ValueData: "WiseTagger Session File"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WiseTagger.exe,0"
Root: HKCU; Subkey: "Software\Classes\WiseTaggerSessionFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WiseTagger.exe"" ""%1""" 
