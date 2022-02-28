;
; os_win32/installer.nsi - smartmontools install NSIS script
;
; Home page of code is: https://www.smartmontools.org
;
; Copyright (C) 2006-22 Christian Franke
;
; SPDX-License-Identifier: GPL-2.0-or-later
;
; $Id$
;


;--------------------------------------------------------------------
; Command line arguments:
; makensis -DINPDIR=<input-dir> -DINPDIR64=<input-dir-64-bit> \
;   -DOUTFILE=<output-file> -DVERSTR=<version-string> installer.nsi

!ifndef INPDIR
  !define INPDIR "."
!endif

!ifndef OUTFILE
  !define OUTFILE "smartmontools.win32-setup.exe"
!endif

;--------------------------------------------------------------------
; General

Name "smartmontools"
OutFile "${OUTFILE}"

RequestExecutionLevel admin

SetCompressor /solid lzma

XPStyle on
InstallColors /windows

; Set in .onInit
;InstallDir "$PROGRAMFILES\smartmontools"
;InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "InstallLocation"

!ifdef VERSION
  VIProductVersion "${VERSION}"
  VIAddVersionKey /LANG=1033-English "CompanyName" "www.smartmontools.org"
  VIAddVersionKey /LANG=1033-English "FileDescription" "SMART Monitoring Tools"
  VIAddVersionKey /LANG=1033-English "FileVersion" "${VERSION}"
 !ifdef YY
  VIAddVersionKey /LANG=1033-English "LegalCopyright" "(C) 2002-20${YY}, Bruce Allen, Christian Franke, www.smartmontools.org"
 !endif
  VIAddVersionKey /LANG=1033-English "OriginalFilename" "${OUTFILE}"
  VIAddVersionKey /LANG=1033-English "ProductName" "smartmontools"
  VIAddVersionKey /LANG=1033-English "ProductVersion" "${VERSION}"
!endif

Var EDITOR

!ifdef INPDIR64
  Var X64
  Var INSTDIR32
  Var INSTDIR64
!endif

LicenseData "${INPDIR}\doc\COPYING.txt"

!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"


;--------------------------------------------------------------------
; Pages

Page license
Page components
!ifdef INPDIR64
  Page directory CheckX64
!else
  Page directory
!endif
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

InstType "Full"
InstType "Extract files only"
InstType "Drive menu"
!ifdef INPDIR64
InstType "Full (x64)"
InstType "Extract files only (x64)"
InstType "Drive menu (x64)"
!endif


;--------------------------------------------------------------------
; Sections

!ifdef INPDIR64
  Section "64-bit version" X64_SECTION
    SectionIn 4 5 6
    ; Handled in Function CheckX64
  SectionEnd

  !define FULL_TYPES "1 4"
  !define EXTRACT_TYPES  "2 5"
  !define DRIVEMENU_TYPE     "3 6"
!else
  !define FULL_TYPES "1"
  !define EXTRACT_TYPES  "2"
  !define DRIVEMENU_TYPE     "3"
!endif

SectionGroup "!Program files"

  !macro FileExe path option
    !ifdef INPDIR64
      ; Use dummy SetOutPath to control archive location of executables
      ${If} $X64 != ""
        Goto +2
          SetOutPath "$INSTDIR\bin64"
        File ${option} '${INPDIR64}\${path}'
      ${Else}
        Goto +2
          SetOutPath "$INSTDIR\bin"
        File ${option} '${INPDIR}\${path}'
      ${EndIf}
    !else
      File ${option} '${INPDIR}\${path}'
    !endif
  !macroend

  Section "smartctl" SMARTCTL_SECTION

    SectionIn ${FULL_TYPES} ${EXTRACT_TYPES}

    SetOutPath "$INSTDIR\bin"
    !insertmacro FileExe "bin\smartctl.exe" ""

  SectionEnd

  Section "smartd" SMARTD_SECTION

    SectionIn ${FULL_TYPES} ${EXTRACT_TYPES}

    SetOutPath "$INSTDIR\bin"

    ; Stop service ?
    StrCpy $1 ""
    ${If} ${FileExists} "$INSTDIR\bin\smartd.exe"
      ReadRegStr $0 HKLM "System\CurrentControlSet\Services\smartd" "ImagePath"
      ${If} $0 != ""
        ExecWait "net stop smartd" $1
      ${EndIf}
    ${EndIf}
    !insertmacro FileExe "bin\smartd.exe" ""

    IfFileExists "$INSTDIR\bin\smartd.conf" 0 +2
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Replace existing configuration file$\n$INSTDIR\bin\smartd.conf ?" /SD IDNO IDYES 0 IDNO +2
        File "${INPDIR}\doc\smartd.conf"

    File "${INPDIR}\bin\smartd_mailer.ps1"
    File "${INPDIR}\bin\smartd_mailer.conf.sample.ps1"
    File "${INPDIR}\bin\smartd_warning.cmd"
    !insertmacro FileExe "bin\wtssendmsg.exe" ""

    ; Restart service ?
    ${If} $1 == "0"
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Restart smartd service ?" /SD IDNO IDYES 0 IDNO +2
        ExecWait "net start smartd"
    ${EndIf}

  SectionEnd

  Section "smartctl-nc (GSmartControl)" SMARTCTL_NC_SECTION

    SectionIn ${FULL_TYPES} ${EXTRACT_TYPES}

    SetOutPath "$INSTDIR\bin"
    !insertmacro FileExe "bin\smartctl-nc.exe" ""

  SectionEnd

  Section "drivedb.h (Drive Database)" DRIVEDB_SECTION

    SectionIn ${FULL_TYPES} ${EXTRACT_TYPES}

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\drivedb.h"
    Delete "$INSTDIR\bin\update-smart-drivedb.exe" ; TODO: Remove after smartmontools 7.3
    File "${INPDIR}\bin\update-smart-drivedb.ps1"

  SectionEnd

SectionGroupEnd

Section "!Documentation" DOC_SECTION

  SectionIn ${FULL_TYPES} ${EXTRACT_TYPES}

  SetOutPath "$INSTDIR\doc"
  File "${INPDIR}\doc\AUTHORS.txt"
  File "${INPDIR}\doc\ChangeLog.txt"
  File "${INPDIR}\doc\ChangeLog-6.0-7.0.txt"
  File "${INPDIR}\doc\COPYING.txt"
  File "${INPDIR}\doc\INSTALL.txt"
  File "${INPDIR}\doc\NEWS.txt"
  File "${INPDIR}\doc\README.txt"
  File "${INPDIR}\doc\TODO.txt"
!ifdef INPDIR64
  ${If} $X64 != ""
    File "${INPDIR64}\doc\checksums64.txt"
  ${Else}
    File "${INPDIR}\doc\checksums32.txt"
  ${EndIf}
!else
  File "${INPDIR}\doc\checksums??.txt"
!endif
  File "${INPDIR}\doc\smartctl.8.html"
  File "${INPDIR}\doc\smartctl.8.pdf"
  File "${INPDIR}\doc\smartd.8.html"
  File "${INPDIR}\doc\smartd.8.pdf"
  File "${INPDIR}\doc\smartd.conf"
  File "${INPDIR}\doc\smartd.conf.5.html"
  File "${INPDIR}\doc\smartd.conf.5.pdf"

SectionEnd

Section "Uninstaller" UNINST_SECTION

  SectionIn ${FULL_TYPES}
  AddSize 40

  CreateDirectory "$INSTDIR"

  ; Write uninstall keys and program
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "DisplayName" "smartmontools"
!ifdef VERSTR
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "DisplayVersion" "${VERSTR}"
!endif
  ; Important: GSmartControl (>= 1.0.0) reads "InstallLocation" to detect location of bin\smartctl-nc.exe
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "UninstallString" '"$INSTDIR\uninst-smartmontools.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "Publisher"     "smartmontools.org"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "URLInfoAbout"  "https://www.smartmontools.org/"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "HelpLink"      "https://www.smartmontools.org/wiki/Help"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "URLUpdateInfo" "https://builds.smartmontools.org/"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoRepair" 1
  WriteUninstaller "uninst-smartmontools.exe"

SectionEnd

Section "Start Menu Shortcuts" MENU_SECTION

  SectionIn ${FULL_TYPES}

  SetShellVarContext all

  CreateDirectory "$SMPROGRAMS\smartmontools"

  !macro CreateAdminShortCut link target args
    CreateShortCut '${link}' '${target}' '${args}'
    push '${link}'
    Call ShellLinkSetRunAs
  !macroend

  ; runcmdu
  ${If}   ${FileExists} "$INSTDIR\bin\smartctl.exe"
  ${OrIf} ${FileExists} "$INSTDIR\bin\smartd.exe"
    SetOutPath "$INSTDIR\bin"
    !insertmacro FileExe "bin\runcmdu.exe" ""
  ${EndIf}

  ; smartctl
  ${If} ${FileExists} "$INSTDIR\bin\smartctl.exe"
    SetOutPath "$INSTDIR\bin"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl (Admin CMD).lnk" "$WINDIR\system32\cmd.exe" '/k PATH=$INSTDIR\bin;%PATH%&cd /d "$INSTDIR\bin"'
    CreateDirectory "$SMPROGRAMS\smartmontools\smartctl Examples"
    FileOpen $0 "$SMPROGRAMS\smartmontools\smartctl Examples\!Read this first!.txt" "w"
    FileWrite $0 "All the example commands in this directory$\r$\napply to the first drive (sda).$\r$\n"
    FileClose $0
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\All info (-x).lnk"                    "$INSTDIR\bin\runcmdu.exe" "smartctl -x sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Identify drive (-i).lnk"              "$INSTDIR\bin\runcmdu.exe" "smartctl -i sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART attributes (-A -f brief).lnk"   "$INSTDIR\bin\runcmdu.exe" "smartctl -A -f brief sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART capabilities (-c).lnk"          "$INSTDIR\bin\runcmdu.exe" "smartctl -c sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART health status (-H).lnk"         "$INSTDIR\bin\runcmdu.exe" "smartctl -H sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART error log (-l error).lnk"       "$INSTDIR\bin\runcmdu.exe" "smartctl -l error sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART selftest log (-l selftest).lnk" "$INSTDIR\bin\runcmdu.exe" "smartctl -l selftest sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start long selftest (-t long).lnk"    "$INSTDIR\bin\runcmdu.exe" "smartctl -t long sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start offline test (-t offline).lnk"  "$INSTDIR\bin\runcmdu.exe" "smartctl -t offline sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start short selftest (-t short).lnk"  "$INSTDIR\bin\runcmdu.exe" "smartctl -t short sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Stop(Abort) selftest (-X).lnk"        "$INSTDIR\bin\runcmdu.exe" "smartctl -X sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART off (-s off).lnk"          "$INSTDIR\bin\runcmdu.exe" "smartctl -s off sda"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART on (-s on).lnk"            "$INSTDIR\bin\runcmdu.exe" "smartctl -s on sda"
  ${EndIf}

  ; smartd
  ${If} ${FileExists} "$INSTDIR\bin\smartd.exe"
    SetOutPath "$INSTDIR\bin"
    CreateDirectory "$SMPROGRAMS\smartmontools\smartd Examples"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, smartd.log.lnk"           "$INSTDIR\bin\runcmdu.exe" "smartd -l local0"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, eventlog.lnk"             "$INSTDIR\bin\runcmdu.exe" "smartd"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon stop.lnk"                        "$INSTDIR\bin\runcmdu.exe" "smartd stop"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Do all tests once (-q onecheck).lnk"    "$INSTDIR\bin\runcmdu.exe" "smartd -q onecheck"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Debug mode (-d).lnk"                    "$INSTDIR\bin\runcmdu.exe" "smartd -d"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\smartd.conf (edit).lnk" "$EDITOR" '"$INSTDIR\bin\smartd.conf"'
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\smartd.conf (view).lnk"                   "$EDITOR" '"$INSTDIR\bin\smartd.conf"'
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\smartd.log (view).lnk"                    "$EDITOR" '"$INSTDIR\bin\smartd.log"'
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\smartd_mailer.conf.sample.ps1 (view).lnk" "$EDITOR" '"$INSTDIR\bin\smartd_mailer.conf.sample.ps1"'
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\smartd_mailer.conf.ps1 (create, edit).lnk" "$EDITOR" '"$INSTDIR\bin\smartd_mailer.conf.ps1"'

    ; smartd service
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, eventlog, 30min.lnk"   "$INSTDIR\bin\runcmdu.exe" "smartd install"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, smartd.log, 10min.lnk" "$INSTDIR\bin\runcmdu.exe" "smartd install -l local0 -i 600"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, smartd.log, 30min.lnk" "$INSTDIR\bin\runcmdu.exe" "smartd install -l local0"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service remove.lnk"                     "$INSTDIR\bin\runcmdu.exe" "smartd remove"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service start.lnk"                      "$INSTDIR\bin\runcmdu.exe" "net start smartd"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service stop.lnk"                       "$INSTDIR\bin\runcmdu.exe" "net stop smartd"
  ${EndIf}

  ; Documentation
  ${If} ${FileExists} "$INSTDIR\doc\README.TXT"
    SetOutPath "$INSTDIR\doc"
    CreateDirectory "$SMPROGRAMS\smartmontools\Documentation"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartctl manual page (html).lnk"    "$INSTDIR\doc\smartctl.8.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd manual page (html).lnk"      "$INSTDIR\doc\smartd.8.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf manual page (html).lnk" "$INSTDIR\doc\smartd.conf.5.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartctl manual page (pdf).lnk"     "$INSTDIR\doc\smartctl.8.pdf"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd manual page (pdf).lnk"       "$INSTDIR\doc\smartd.8.pdf"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf manual page (pdf).lnk"  "$INSTDIR\doc\smartd.conf.5.pdf"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf sample.lnk" "$EDITOR" '"$INSTDIR\doc\smartd.conf"'
    ${If} ${FileExists} "$INSTDIR\bin\drivedb.h"
      CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\drivedb.h (view).lnk" "$EDITOR" '"$INSTDIR\bin\drivedb.h"'
      !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\Documentation\drivedb-add.h (create, edit).lnk" "$EDITOR" '"$INSTDIR\bin\drivedb-add.h"'
    ${EndIf}
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\ChangeLog.lnk" "$INSTDIR\doc\ChangeLog.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\COPYING.lnk"   "$INSTDIR\doc\COPYING.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\NEWS.lnk"      "$INSTDIR\doc\NEWS.txt"
  ${EndIf}

  ; Homepage
  CreateShortCut "$SMPROGRAMS\smartmontools\smartmontools Home Page.lnk" "https://www.smartmontools.org/"
  CreateShortCut "$SMPROGRAMS\smartmontools\smartmontools Daily Builds.lnk" "https://builds.smartmontools.org/"

  ; drivedb.h update
  Delete "$SMPROGRAMS\smartmontools\drivedb.h update.lnk" ; TODO: Remove after smartmontools 7.3
  ${If} ${FileExists} "$INSTDIR\bin\update-smart-drivedb.ps1"
    SetOutPath "$INSTDIR\bin"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\drivedb.h update (ps1).lnk" "$INSTDIR\bin\runcmdu.exe" "powershell -NoProfile -ExecutionPolicy Bypass .\update-smart-drivedb.ps1"
    ${If} ${FileExists} "$INSTDIR\doc\README.TXT"
      CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\drivedb.h update help (ps1).lnk" "$INSTDIR\bin\runcmdu.exe" 'powershell -NoProfile -ExecutionPolicy Bypass "Get-Help .\update-smart-drivedb.ps1 -Detail | more"'
    ${EndIf}
  ${EndIf}

  ; Uninstall
  ${If} ${FileExists} "$INSTDIR\uninst-smartmontools.exe"
    SetOutPath "$INSTDIR"
    !insertmacro CreateAdminShortCut "$SMPROGRAMS\smartmontools\Uninstall smartmontools.lnk" "$INSTDIR\uninst-smartmontools.exe" ""
  ${EndIf}

SectionEnd

Section "Add install dir to PATH" PATH_SECTION

  SectionIn ${FULL_TYPES}

  Push "$INSTDIR\bin"
  Call AddToPath
 
SectionEnd

SectionGroup "Add smartctl to drive menu"

!macro DriveMenuRemove
  DetailPrint "Remove drive menu entries"
  DeleteRegKey HKCR "Drive\shell\smartctl0"
  DeleteRegKey HKCR "Drive\shell\smartctl1"
  DeleteRegKey HKCR "Drive\shell\smartctl2"
  DeleteRegKey HKCR "Drive\shell\smartctl3"
  DeleteRegKey HKCR "Drive\shell\smartctl4"
  DeleteRegKey HKCR "Drive\shell\smartctl5"
!macroend

  Section "Remove existing entries first" DRIVE_REMOVE_SECTION
    SectionIn ${DRIVEMENU_TYPE}
    !insertmacro DriveMenuRemove
  SectionEnd

!macro DriveSection id name args
  Section 'smartctl ${args} ...' DRIVE_${id}_SECTION
    SectionIn ${DRIVEMENU_TYPE}
    Call CheckRunCmdA
    DetailPrint 'Add drive menu entry "${name}": smartctl ${args} ...'
    WriteRegStr HKCR "Drive\shell\smartctl${id}" "" "${name}"
    WriteRegStr HKCR "Drive\shell\smartctl${id}\command" "" '"$INSTDIR\bin\runcmda.exe" "$INSTDIR\bin\smartctl.exe" ${args} %L'
  SectionEnd
!macroend

  !insertmacro DriveSection 0 "SMART all info"       "-x"
  !insertmacro DriveSection 1 "SMART status"         "-Hc"
  !insertmacro DriveSection 2 "SMART attributes"     "-A -f brief"
  !insertmacro DriveSection 3 "SMART short selftest" "-t short"
  !insertmacro DriveSection 4 "SMART long selftest"  "-t long"
  !insertmacro DriveSection 5 "SMART continue selective selftest"  '-t "selective,cont"'

SectionGroupEnd

;--------------------------------------------------------------------

Section "Uninstall"
  
  ; Stop & remove service
  ${If} ${FileExists} "$INSTDIR\bin\smartd.exe"
    ReadRegStr $0 HKLM "System\CurrentControlSet\Services\smartd" "ImagePath"
    ${If} $0 != ""
      ExecWait "net stop smartd"
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Remove smartd service ?" /SD IDNO IDYES 0 IDNO +2
        ExecWait "$INSTDIR\bin\smartd.exe remove"
    ${EndIf}
  ${EndIf}

  ; Remove installer registry key
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools"

  ; Remove conf file ?
  ${If} ${FileExists} "$INSTDIR\bin\smartd.conf"
    ; Assume unchanged if timestamp is equal to sample file
    GetFileTime "$INSTDIR\bin\smartd.conf" $0 $1
    GetFileTime "$INSTDIR\doc\smartd.conf" $2 $3
    StrCmp "$0:$1" "$2:$3" +2 0
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete configuration file$\n$INSTDIR\bin\smartd.conf ?" /SD IDNO IDYES 0 IDNO +2
        Delete "$INSTDIR\bin\smartd.conf"
  ${EndIf}

  ; Remove log file ?
  ${If} ${FileExists} "$INSTDIR\bin\smartd.log"
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete log file$\n$INSTDIR\bin\smartd.log ?" /SD IDNO IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\smartd.log"
  ${EndIf}

  ; Remove drivedb-add file ?
  ${If} ${FileExists} "$INSTDIR\bin\drivedb-add.h"
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete local drive database file$\n$INSTDIR\bin\drivedb-add.h ?" /SD IDNO IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\drivedb-add.h"
  ${EndIf}

  ; Remove smartd_mailer.conf.ps1 file ?
  ${If} ${FileExists} "$INSTDIR\bin\smartd_mailer.conf.ps1"
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Delete mailer configuration file$\n$INSTDIR\bin\smartd_mailer.conf.ps1 ?" /SD IDNO IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\smartd_mailer.conf.ps1"
  ${EndIf}

  ; Remove files
  Delete "$INSTDIR\bin\smartctl.exe"
  Delete "$INSTDIR\bin\smartctl-nc.exe"
  Delete "$INSTDIR\bin\smartd.exe"
  Delete "$INSTDIR\bin\smartd_mailer.ps1"
  Delete "$INSTDIR\bin\smartd_mailer.conf.sample.ps1"
  Delete "$INSTDIR\bin\smartd_warning.cmd" ; TODO: Check for modifications?
  Delete "$INSTDIR\bin\drivedb.h"
  Delete "$INSTDIR\bin\drivedb.h.error"
  Delete "$INSTDIR\bin\drivedb.h.lastcheck"
  Delete "$INSTDIR\bin\drivedb.h.old"
  Delete "$INSTDIR\bin\update-smart-drivedb.exe" ; TODO: Remove after smartmontools 7.3
  Delete "$INSTDIR\bin\update-smart-drivedb.ps1"
  Delete "$INSTDIR\bin\runcmda.exe"
  Delete "$INSTDIR\bin\runcmdu.exe"
  Delete "$INSTDIR\bin\wtssendmsg.exe"
  Delete "$INSTDIR\doc\AUTHORS.txt"
  Delete "$INSTDIR\doc\ChangeLog.txt"
  Delete "$INSTDIR\doc\ChangeLog-6.0-7.0.txt"
  Delete "$INSTDIR\doc\COPYING.txt"
  Delete "$INSTDIR\doc\INSTALL.txt"
  Delete "$INSTDIR\doc\NEWS.txt"
  Delete "$INSTDIR\doc\README.txt"
  Delete "$INSTDIR\doc\TODO.txt"
  Delete "$INSTDIR\doc\checksums*.txt"
  Delete "$INSTDIR\doc\smartctl.8.html"
  Delete "$INSTDIR\doc\smartctl.8.pdf"
  Delete "$INSTDIR\doc\smartd.8.html"
  Delete "$INSTDIR\doc\smartd.8.pdf"
  Delete "$INSTDIR\doc\smartd.conf"
  Delete "$INSTDIR\doc\smartd.conf.5.html"
  Delete "$INSTDIR\doc\smartd.conf.5.pdf"
  Delete "$INSTDIR\uninst-smartmontools.exe"

  ; Remove shortcuts
  SetShellVarContext all
  Delete "$SMPROGRAMS\smartmontools\*.*"
  Delete "$SMPROGRAMS\smartmontools\Documentation\*.*"
  Delete "$SMPROGRAMS\smartmontools\smartctl Examples\*.*"
  Delete "$SMPROGRAMS\smartmontools\smartd Examples\*.*"

  ; Remove folders
  RMDir  "$SMPROGRAMS\smartmontools\Documentation"
  RMDir  "$SMPROGRAMS\smartmontools\smartctl Examples"
  RMDir  "$SMPROGRAMS\smartmontools\smartd Examples"
  RMDir  "$SMPROGRAMS\smartmontools"
  RMDir  "$INSTDIR\bin"
  RMDir  "$INSTDIR\doc"
  RMDir  "$INSTDIR"

  ; Remove install dir from PATH
  Push "$INSTDIR\bin"
  Call un.RemoveFromPath

  ; Remove drive menu registry entries
  !insertmacro DriveMenuRemove

  ; Check for still existing entries
  ${If} ${FileExists} "$INSTDIR\bin\smartd.exe"
    MessageBox MB_OK|MB_ICONEXCLAMATION "$INSTDIR\bin\smartd.exe could not be removed.$\nsmartd is possibly still running." /SD IDOK
  ${ElseIf} ${FileExists} "$INSTDIR"
    MessageBox MB_OK "Note: $INSTDIR could not be removed." /SD IDOK
  ${EndIf}

  ${If} ${FileExists} "$SMPROGRAMS\smartmontools"
    MessageBox MB_OK "Note: $SMPROGRAMS\smartmontools could not be removed." /SD IDOK
  ${EndIf}

SectionEnd

;--------------------------------------------------------------------
; Functions

!macro AdjustSectionSize section
  SectionGetSize ${section} $0
  IntOp $0 $0 / 2
  SectionSetSize ${section} $0
!macroend

Function .onInit

  ; Set default install directories
  ${If} $INSTDIR == "" ; /D=PATH option not specified ?
    ReadRegStr $INSTDIR HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "InstallLocation"
    ${If} $INSTDIR == "" ; Not already installed ?
      StrCpy $INSTDIR "$PROGRAMFILES\smartmontools"
!ifdef INPDIR64
      StrCpy $INSTDIR32 $INSTDIR
      StrCpy $INSTDIR64 "$PROGRAMFILES64\smartmontools"
!endif
    ${EndIf}
  ${EndIf}

!ifdef INPDIR64
  ; Check for 64-bit unless already installed in 32-bit location
  ${If} $INSTDIR64 != ""
  ${OrIf} $INSTDIR != "$PROGRAMFILES\smartmontools"
    ; $1 = IsWow64Process(GetCurrentProcess(), ($0=FALSE, &$0))
    System::Call "kernel32::GetCurrentProcess() i.s"
    System::Call "kernel32::IsWow64Process(i s, *i 0 r0) i.r1"
    ${If} "$0 $1" == "1 1" ; 64-bit Windows ?
      !insertmacro SelectSection ${X64_SECTION}
    ${EndIf}
  ${EndIf}

  ; Sizes of binary sections include 32-bit and 64-bit executables
  !insertmacro AdjustSectionSize ${SMARTCTL_SECTION}
  !insertmacro AdjustSectionSize ${SMARTD_SECTION}
  !insertmacro AdjustSectionSize ${SMARTCTL_NC_SECTION}
!endif

  ; Use 32-bit or 64-bit Notepad++ if installed
  StrCpy $EDITOR "$PROGRAMFILES\Notepad++\notepad++.exe"
  ${IfNot} ${FileExists} "$EDITOR"
    StrCpy $EDITOR "$PROGRAMFILES64\Notepad++\notepad++.exe"
    ${IfNot} ${FileExists} "$EDITOR"
      StrCpy $EDITOR "notepad.exe"
    ${EndIf}
  ${EndIf}

  Call ParseCmdLine

!ifdef INPDIR64
  Call CheckX64
!endif
FunctionEnd

; Check x64 section and update INSTDIR accordingly

!ifdef INPDIR64
Function CheckX64
  ${IfNot} ${SectionIsSelected} ${X64_SECTION}
    StrCpy $X64 ""
    ${If} $INSTDIR32 != ""
    ${AndIf} $INSTDIR == $INSTDIR64
      StrCpy $INSTDIR $INSTDIR32
    ${EndIf}
  ${Else}
    StrCpy $X64 "t"
    ${If} $INSTDIR64 != ""
    ${AndIf} $INSTDIR == $INSTDIR32
      StrCpy $INSTDIR $INSTDIR64
    ${EndIf}
  ${EndIf}
FunctionEnd
!endif

; Command line parsing

!macro GetCmdLineOption var name
  Push ",$opts,"
  Push ",${name},"
  Call StrStr
  Pop ${var}
  ${If} ${var} != ""
    StrCpy $nomatch ""
  ${EndIf}
!macroend

!macro CheckCmdLineOption name section
  StrCpy $allopts "$allopts,${name}"
  !insertmacro GetCmdLineOption $0 ${name}
  ${If} $0 == ""
    !insertmacro UnselectSection ${section}
  ${Else}
    !insertmacro SelectSection ${section}
  ${EndIf}
!macroend

Function ParseCmdLine
  ; get /SO option
  Var /global opts
  ${GetParameters} $R0
  ${GetOptions} $R0 "/SO" $opts
  ${If} ${Errors}
    Return
  ${EndIf}
  Var /global allopts
  Var /global nomatch
  StrCpy $nomatch "t"
!ifdef INPDIR64
  ; Change previous 64-bit setting
  StrCpy $allopts ",x32|x64"
  !insertmacro GetCmdLineOption $0 "x32"
  ${If} $0 != ""
    !insertmacro UnselectSection ${X64_SECTION}
  ${EndIf}
  !insertmacro GetCmdLineOption $0 "x64"
  ${If} $0 != ""
    !insertmacro SelectSection ${X64_SECTION}
  ${EndIf}
  ; Leave other sections unchanged if only "x32" or "x64" is specified
  ${If}   $opts == "x32"
  ${OrIf} $opts == "x64"
    Return
  ${EndIf}
!endif
  ; Turn sections on or off
  !insertmacro CheckCmdLineOption "smartctl" ${SMARTCTL_SECTION}
  !insertmacro CheckCmdLineOption "smartd" ${SMARTD_SECTION}
  !insertmacro CheckCmdLineOption "smartctlnc" ${SMARTCTL_NC_SECTION}
  !insertmacro CheckCmdLineOption "drivedb" ${DRIVEDB_SECTION}
  !insertmacro CheckCmdLineOption "doc" ${DOC_SECTION}
  !insertmacro CheckCmdLineOption "uninst" ${UNINST_SECTION}
  !insertmacro CheckCmdLineOption "menu" ${MENU_SECTION}
  !insertmacro CheckCmdLineOption "path" ${PATH_SECTION}
  !insertmacro CheckCmdLineOption "driveremove" ${DRIVE_REMOVE_SECTION}
  !insertmacro CheckCmdLineOption "drive0" ${DRIVE_0_SECTION}
  !insertmacro CheckCmdLineOption "drive1" ${DRIVE_1_SECTION}
  !insertmacro CheckCmdLineOption "drive2" ${DRIVE_2_SECTION}
  !insertmacro CheckCmdLineOption "drive3" ${DRIVE_3_SECTION}
  !insertmacro CheckCmdLineOption "drive4" ${DRIVE_4_SECTION}
  !insertmacro CheckCmdLineOption "drive5" ${DRIVE_5_SECTION}
  ${If} $opts != "-"
    ${If} $nomatch != ""
      StrCpy $0 "$allopts,-" "" 1
      MessageBox MB_OK "Usage: smartmontools-VERSION.win32-setup [/S] [/SO component,...] [/D=INSTDIR]$\n$\ncomponents:$\n  $0"
      Abort
    ${EndIf}
  ${EndIf}
FunctionEnd

; Install runcmda.exe only once

Function CheckRunCmdA
  Var /global runcmda
  ${If} $runcmda == ""
    StrCpy $runcmda "t"
    SetOutPath "$INSTDIR\bin"
    !insertmacro FileExe "bin\runcmda.exe" ""
  ${EndIf}
FunctionEnd


;--------------------------------------------------------------------
; Path functions
;
; Based on example from:
; http://nsis.sourceforge.net/Path_Manipulation
;


!include "WinMessages.nsh"

; Registry Entry for environment (NT4,2000,XP)
; All users:
;!define Environ 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
; Current user only:
!define Environ 'HKCU "Environment"'


; AddToPath - Appends dir to PATH
;   (does not work on Win9x/ME)
;
; Usage:
;   Push "dir"
;   Call AddToPath

Function AddToPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4

  ; NSIS ReadRegStr returns empty string on string overflow
  ; Native calls are used here to check actual length of PATH

  ; $4 = RegOpenKey(HKEY_CURRENT_USER, "Environment", &$3)
  System::Call "advapi32::RegOpenKey(i 0x80000001, t'Environment', *i.r3) i.r4"
  IntCmp $4 0 0 done done
  ; $4 = RegQueryValueEx($3, "PATH", (DWORD*)0, (DWORD*)0, &$1, ($2=NSIS_MAX_STRLEN, &$2))
  ; RegCloseKey($3)
  System::Call "advapi32::RegQueryValueEx(i $3, t'PATH', i 0, i 0, t.r1, *i ${NSIS_MAX_STRLEN} r2) i.r4"
  System::Call "advapi32::RegCloseKey(i $3)"

  ${If} $4 = 234 ; ERROR_MORE_DATA
    DetailPrint "AddToPath: original length $2 > ${NSIS_MAX_STRLEN}"
    MessageBox MB_OK "PATH not updated, original length $2 > ${NSIS_MAX_STRLEN}" /SD IDOK
    Goto done
  ${EndIf}

  ${If} $4 <> 0 ; NO_ERROR
    ${If} $4 <> 2 ; ERROR_FILE_NOT_FOUND
      DetailPrint "AddToPath: unexpected error code $4"
      Goto done
    ${EndIf}
    StrCpy $1 ""
  ${EndIf}

  ; Check if already in PATH
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" 0 done

  ; Prevent NSIS string overflow
  StrLen $2 $0
  StrLen $3 $1
  IntOp $2 $2 + $3
  IntOp $2 $2 + 2 ; $2 = strlen(dir) + strlen(PATH) + sizeof(";")
  ${If} $2 > ${NSIS_MAX_STRLEN}
    DetailPrint "AddToPath: new length $2 > ${NSIS_MAX_STRLEN}"
    MessageBox MB_OK "PATH not updated, new length $2 > ${NSIS_MAX_STRLEN}." /SD IDOK
    Goto done
  ${EndIf}

  ; Append dir to PATH
  DetailPrint "Add to PATH: $0"
  StrCpy $2 $1 1 -1
  ${If} $2 == ";"
    StrCpy $1 $1 -1 ; remove trailing ';'
  ${EndIf}
  ${If} $1 != "" ; no leading ';'
    StrCpy $0 "$1;$0"
  ${EndIf}
  WriteRegExpandStr ${Environ} "PATH" $0
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

done:
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd


; RemoveFromPath - Removes dir from PATH
;
; Usage:
;   Push "dir"
;   Call RemoveFromPath

Function un.RemoveFromPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  ReadRegStr $1 ${Environ} "PATH"
  StrCpy $5 $1 1 -1
  ${If} $5 != ";"
    StrCpy $1 "$1;" ; ensure trailing ';'
  ${EndIf}
  Push $1
  Push "$0;"
  Call un.StrStr
  Pop $2 ; pos of our dir
  StrCmp $2 "" done

  DetailPrint "Remove from PATH: $0"
  StrLen $3 "$0;"
  StrLen $4 $2
  StrCpy $5 $1 -$4 ; $5 is now the part before the path to remove
  StrCpy $6 $2 "" $3 ; $6 is now the part after the path to remove
  StrCpy $3 "$5$6"
  StrCpy $5 $3 1 -1
  ${If} $5 == ";"
    StrCpy $3 $3 -1 ; remove trailing ';'
  ${EndIf}
  WriteRegExpandStr ${Environ} "PATH" $3
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

done:
  Pop $6
  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd
 

; StrStr - find substring in a string
;
; Usage:
;   Push "this is some string"
;   Push "some"
;   Call StrStr
;   Pop $0 ; "some string"

!macro StrStr un
Function ${un}StrStr
  Exch $R1 ; $R1=substring, stack=[old$R1,string,...]
  Exch     ;                stack=[string,old$R1,...]
  Exch $R2 ; $R2=string,    stack=[old$R2,old$R1,...]
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=substring, $R2=string, $R3=strlen(substring)
  ; $R4=count, $R5=tmp
  ${Do}
    StrCpy $R5 $R2 $R3 $R4
    ${IfThen} $R5 == $R1 ${|} ${ExitDo} ${|}
    ${IfThen} $R5 == ""  ${|} ${ExitDo} ${|}
    IntOp $R4 $R4 + 1
  ${Loop}
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1 ; $R1=old$R1, stack=[result,...]
FunctionEnd
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."


;--------------------------------------------------------------------
; Set Run As Administrator flag in shortcut
;
; Slightly modified version from:
; http://nsis.sourceforge.net/IShellLink_Set_RunAs_flag
;

!define IPersistFile {0000010b-0000-0000-c000-000000000046}
!define CLSID_ShellLink {00021401-0000-0000-C000-000000000046}
!define IID_IShellLinkA {000214EE-0000-0000-C000-000000000046}
!define IID_IShellLinkW {000214F9-0000-0000-C000-000000000046}
!define IShellLinkDataList {45e2b4ae-b1c3-11d0-b92f-00a0c90312e1}
!ifdef NSIS_UNICODE
  !define IID_IShellLink ${IID_IShellLinkW}
!else
  !define IID_IShellLink ${IID_IShellLinkA}
!endif

Function ShellLinkSetRunAs
  ; Set archive location of $PLUGINSDIR
  Goto +2
    SetOutPath "$INSTDIR"

  System::Store S ; push $0-$9, $R0-$R9
  pop $9
  ; $0 = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, IID_IShellLink, &$1)
  System::Call "ole32::CoCreateInstance(g'${CLSID_ShellLink}',i0,i1,g'${IID_IShellLink}',*i.r1)i.r0"
  ${If} $0 = 0
    System::Call "$1->0(g'${IPersistFile}',*i.r2)i.r0" ; $0 = $1->QueryInterface(IPersistFile, &$2)
    ${If} $0 = 0
      System::Call "$2->5(w '$9',i 0)i.r0" ; $0 = $2->Load($9, STGM_READ)
      ${If} $0 = 0
        System::Call "$1->0(g'${IShellLinkDataList}',*i.r3)i.r0" ; $0 = $1->QueryInterface(IShellLinkDataList, &$3)
        ${If} $0 = 0
          System::Call "$3->6(*i.r4)i.r0"; $0 = $3->GetFlags(&$4)
          ${If} $0 = 0
            System::Call "$3->7(i $4|0x2000)i.r0" ; $0 = $3->SetFlags($4|SLDF_RUNAS_USER)
            ${If} $0 = 0
              System::Call "$2->6(w '$9',i1)i.r0" ; $2->Save($9, TRUE)
            ${EndIf}
          ${EndIf}
          System::Call "$3->2()" ; $3->Release()
        ${EndIf}
        System::Call "$2->2()" ; $2->Release()
      ${EndIf}
    ${EndIf}
    System::Call "$1->2()" ; $1->Release()
  ${EndIf}
  ${If} $0 <> 0
    DetailPrint "Set RunAsAdmin: $9 failed ($0)"
  ${Else}
    DetailPrint "Set RunAsAdmin: $9"
  ${EndIf}
  System::Store L ; pop $R9-$R0, $9-$0
FunctionEnd
