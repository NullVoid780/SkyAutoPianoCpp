#ifndef AppVersion
  #error AppVersion must be defined
#endif
#ifndef StagingDir
  #error StagingDir must be defined
#endif
#ifndef IconFile
  #error IconFile must be defined
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{6C2E4F7A-3B1D-4E8F-9A5C-2D7B8E1F4A60}
AppName=Sky Auto Piano
AppVersion={#AppVersion}
AppVerName=Sky Auto Piano {#AppVersion}
AppPublisher=Sky Auto Piano
DefaultDirName={localappdata}\Programs\Sky Auto Piano
DefaultGroupName=Sky Auto Piano
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=SkyAutoPiano-{#AppVersion}-setup
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\SkyAutoPiano.exe
InfoBeforeFile={#StagingDir}\licenses\THIRD-PARTY-NOTICES.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
Type: files; Name: "{app}\SkyCrashReporter.exe"

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Sky Auto Piano"; Filename: "{app}\SkyAutoPiano.exe"
Name: "{group}\Third-party licences"; Filename: "{app}\licenses"
Name: "{autodesktop}\Sky Auto Piano"; Filename: "{app}\SkyAutoPiano.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\SkyAutoPiano.exe"; Description: "{cm:LaunchProgram,Sky Auto Piano}"; Flags: nowait postinstall
