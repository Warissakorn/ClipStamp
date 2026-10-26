Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "config.nsh"

Name "ClipStamp"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\ClipStamp"
RequestExecutionLevel user
ManifestDPIAware true
SetCompressor /SOLID zlib
BrandingText "ClipStamp"
ShowInstDetails hide
ShowUninstDetails hide
VIProductVersion "${PRODUCT_VERSION_QUAD}"
VIAddVersionKey /LANG=1033 "ProductName" "ClipStamp"
VIAddVersionKey /LANG=1033 "FileDescription" "ClipStamp Windows Installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "ClipStamp"

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ClipStamp"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\ClipStamp.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "English"

Var NoIntegration

Function .onInit
    SetShellVarContext current
    SetRegView 64
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "ClipStamp requires 64-bit Windows."
        SetErrorLevel 2
        Quit
    ${EndIf}
    ${IfNot} ${AtLeastWin10}
        MessageBox MB_OK|MB_ICONSTOP "ClipStamp requires Windows 10 or Windows 11."
        SetErrorLevel 2
        Quit
    ${EndIf}
    ${IfNot} ${AtLeastBuild} 17763
        MessageBox MB_OK|MB_ICONSTOP "ClipStamp requires Windows 10 version 1809 or later, or Windows 11."
        SetErrorLevel 2
        Quit
    ${EndIf}
    StrCpy $NoIntegration 0
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} $0 "/NOINTEGRATION" $1
    ${IfNot} ${Errors}
        StrCpy $NoIntegration 1
    ${EndIf}
    ; Isolated verification can omit registry and shortcut changes.
    ${If} $NoIntegration == 0
        ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "InstallLocation"
        ${If} $0 != ""
            StrCpy $INSTDIR $0
        ${EndIf}
    ${EndIf}
FunctionEnd

Function .onVerifyInstDir
    ${GetRoot} "$INSTDIR" $0
    StrCmp $INSTDIR $0 invalid
    StrCmp $INSTDIR "$WINDIR" invalid
    StrCmp $INSTDIR "$SYSDIR" invalid
    StrCmp $INSTDIR "$PROFILE" invalid
    StrCmp $INSTDIR "$DESKTOP" invalid
    StrCmp $INSTDIR "$LOCALAPPDATA" invalid
    Return
invalid:
    Abort
FunctionEnd

Section "ClipStamp" MainSection
    SetShellVarContext current
    SetRegView 64
    SetOutPath "$INSTDIR"
    SetOverwrite on
    ClearErrors
    File /r "${PAYLOAD_DIR}\*.*"
    ; Do not register or report a successful install if any file was skipped.
    IfErrors install_failed
    WriteINIStr "$INSTDIR\installation.ini" "Install" "Product" "ClipStamp"
    WriteINIStr "$INSTDIR\installation.ini" "Install" "Version" "${PRODUCT_VERSION}"
    WriteINIStr "$INSTDIR\installation.ini" "Install" "NoIntegration" "$NoIntegration"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    IfErrors install_failed
    ${If} $NoIntegration == 0
        CreateDirectory "$SMPROGRAMS\ClipStamp"
        CreateShortcut "$SMPROGRAMS\ClipStamp\ClipStamp.lnk" "$INSTDIR\ClipStamp.exe"
        CreateShortcut "$SMPROGRAMS\ClipStamp\Uninstall ClipStamp.lnk" "$INSTDIR\Uninstall.exe"
        CreateShortcut "$DESKTOP\ClipStamp.lnk" "$INSTDIR\ClipStamp.exe"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "ClipStamp"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "ClipStamp"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\ClipStamp.exe"
        WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
        WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
        WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" ${INSTALLED_SIZE_KB}
        WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
        WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    ${EndIf}
    SetErrorLevel 0
    Goto install_done
install_failed:
    MessageBox MB_OK|MB_ICONSTOP "ClipStamp could not be installed completely. Close ClipStamp and run Setup again." /SD IDOK
    SetErrorLevel 3
    Abort
install_done:
SectionEnd

Function un.onInit
    SetShellVarContext current
    SetRegView 64
    ReadINIStr $0 "$INSTDIR\installation.ini" "Install" "Product"
    StrCmp $0 "ClipStamp" valid_install
    MessageBox MB_OK|MB_ICONSTOP "Installation record not found. Uninstall has been cancelled." /SD IDOK
    SetErrorLevel 2
    Abort
valid_install:
    ReadINIStr $NoIntegration "$INSTDIR\installation.ini" "Install" "NoIntegration"
FunctionEnd

Section "Uninstall"
    SetShellVarContext current
    SetRegView 64
    ; A manifest removes only shipped files. User videos/projects are never recursively deleted.
    ClearErrors
    !include "uninstall-files.nsh"
    IfErrors uninstall_failed
    ${If} $NoIntegration == 0
        ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "InstallLocation"
        ${If} $0 == $INSTDIR
            Delete "$DESKTOP\ClipStamp.lnk"
            Delete "$SMPROGRAMS\ClipStamp\ClipStamp.lnk"
            Delete "$SMPROGRAMS\ClipStamp\Uninstall ClipStamp.lnk"
            RMDir "$SMPROGRAMS\ClipStamp"
            DeleteRegKey HKCU "${UNINSTALL_KEY}"
        ${EndIf}
    ${EndIf}
    !include "uninstall-directories.nsh"
    ClearErrors
    Delete "$INSTDIR\installation.ini"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    SetErrorLevel 0
    Goto uninstall_done
uninstall_failed:
    MessageBox MB_OK|MB_ICONSTOP "Some program files are in use. Close ClipStamp and run Uninstall again." /SD IDOK
    SetErrorLevel 3
    Abort
uninstall_done:
SectionEnd
