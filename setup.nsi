; Audio Format Converter - NSIS Installer Script
Unicode true
!include "MUI2.nsh"

Name "音频格式转换器"
OutFile "AudioConverter_Setup.exe"
InstallDir "$PROGRAMFILES64\AudioConverter"
RequestExecutionLevel admin

!define MUI_ICON "app.ico"
!define MUI_UNICON "app.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Install"
    SetOutPath "$INSTDIR"
    File "AudioConverter.exe"
    File "ffmpeg.exe"
    File "avcodec-55.dll"
    File "avformat-55.dll"
    File "avutil-52.dll"
    File "swresample-0.dll"
    File "swscale-2.dll"
    File "avdevice-55.dll"
    File "avfilter-3.dll"
    File "postproc-52.dll"

    CreateShortCut "$DESKTOP\音频格式转换器.lnk" "$INSTDIR\AudioConverter.exe"
    CreateDirectory "$SMPROGRAMS\音频格式转换器"
    CreateShortCut "$SMPROGRAMS\音频格式转换器\音频格式转换器.lnk" "$INSTDIR\AudioConverter.exe"
    CreateShortCut "$SMPROGRAMS\音频格式转换器\卸载.lnk" "$INSTDIR\Uninstall.exe"

    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "DisplayName" "音频格式转换器"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "DisplayIcon" "$INSTDIR\AudioConverter.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "Publisher" "AudioFormatConverter"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "DisplayVersion" "1.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\AudioConverter.exe"
    Delete "$INSTDIR\ffmpeg.exe"
    Delete "$INSTDIR\avcodec-55.dll"
    Delete "$INSTDIR\avformat-55.dll"
    Delete "$INSTDIR\avutil-52.dll"
    Delete "$INSTDIR\swresample-0.dll"
    Delete "$INSTDIR\swscale-2.dll"
    Delete "$INSTDIR\avdevice-55.dll"
    Delete "$INSTDIR\avfilter-3.dll"
    Delete "$INSTDIR\postproc-52.dll"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\音频格式转换器.lnk"
    Delete "$SMPROGRAMS\音频格式转换器\音频格式转换器.lnk"
    Delete "$SMPROGRAMS\音频格式转换器\卸载.lnk"
    RMDir "$SMPROGRAMS\音频格式转换器"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioConverter"
SectionEnd
