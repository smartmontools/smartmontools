;
; smartmontools install NSIS script
;
; Home page of code is: http://smartmontools.sourceforge.net
;
; Copyright (C) 2006-11 Christian Franke <smartmontools-support@lists.sourceforge.net>
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2, or (at your option)
; any later version.
;
; You should have received a copy of the GNU General Public License
; (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
;
; $Id$
;


;--------------------------------------------------------------------
; Command line arguments:
; makensis -DINPDIR=<input-dir> -DOUTFILE=<output-file> -DVERSTR=<version-string> installer.nsi

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

SetCompressor /solid lzma

XPStyle on
InstallColors /windows

InstallDir "$PROGRAMFILES\smartmontools"
InstallDirRegKey HKLM "Software\smartmontools" "Install_Dir"
Var UBCDDIR

LicenseData "${INPDIR}\doc\COPYING.txt"

;--------------------------------------------------------------------
; Pages

Page license
Page components
Page directory SkipProgPath "" ""
PageEx directory
  PageCallbacks SkipUBCDPath "" ""
  DirText "Setup will install the UBCD4Win plugin in the following folder."
  DirVar $UBCDDIR
PageExEnd
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

InstType "Full"
InstType "Extract files only"
InstType "Drive menu"
InstType "UBCD4Win plugin"


;--------------------------------------------------------------------
; Sections

SectionGroup "!Program files"

  Section "smartctl" SMARTCTL_SECTION

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\smartctl.exe"

  SectionEnd

  Section "smartd" SMARTD_SECTION

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"

    ; Stop service ?
    StrCpy $1 ""
    IfFileExists "$INSTDIR\bin\smartd.exe" 0 nosrv
      ReadRegStr $0 HKLM "System\CurrentControlSet\Services\smartd" "ImagePath"
      StrCmp $0 "" nosrv
        ExecWait "net stop smartd" $1
  nosrv:
    File "${INPDIR}\bin\smartd.exe"

    IfFileExists "$INSTDIR\bin\smartd.conf" 0 +2
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Replace existing configuration file$\n$INSTDIR\bin\smartd.conf ?" IDYES 0 IDNO +2
        File "${INPDIR}\doc\smartd.conf"

    IfFileExists "$WINDIR\system32\cmd.exe" 0 +2
      File /nonfatal "${INPDIR}\bin\syslogevt.exe"

    ; Restart service ?
    StrCmp $1 "0" 0 +3
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Restart smartd service ?" IDYES 0 IDNO +2
        ExecWait "net start smartd"

  SectionEnd

  Section "smartctl-nc (GSmartControl)" SMARTCTL_NC_SECTION

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\smartctl-nc.exe"

  SectionEnd

  Section "drivedb.h (Drive Database)" DRIVEDB_SECTION

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\drivedb.h"
    File "${INPDIR}\bin\update-smart-drivedb.exe"

  SectionEnd

SectionGroupEnd

Section "!Documentation" DOC_SECTION

  SectionIn 1 2

  SetOutPath "$INSTDIR\doc"
  File "${INPDIR}\doc\AUTHORS.txt"
  File "${INPDIR}\doc\CHANGELOG.txt"
  File "${INPDIR}\doc\COPYING.txt"
  File "${INPDIR}\doc\INSTALL.txt"
  File "${INPDIR}\doc\NEWS.txt"
  File "${INPDIR}\doc\README.txt"
  File "${INPDIR}\doc\TODO.txt"
  File "${INPDIR}\doc\WARNINGS.txt"
  File "${INPDIR}\doc\checksums.txt"
  File "${INPDIR}\doc\smartctl.8.html"
  File "${INPDIR}\doc\smartctl.8.txt"
  File "${INPDIR}\doc\smartd.8.html"
  File "${INPDIR}\doc\smartd.8.txt"
  File "${INPDIR}\doc\smartd.conf"
  File "${INPDIR}\doc\smartd.conf.5.html"
  File "${INPDIR}\doc\smartd.conf.5.txt"

SectionEnd

Section "Uninstaller" UNINST_SECTION

  SectionIn 1
  AddSize 35

  CreateDirectory "$INSTDIR"

  ; Save installation location
  WriteRegStr HKLM "Software\smartmontools" "Install_Dir" "$INSTDIR"

  ; Write uninstall keys and program
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "DisplayName" "smartmontools"
!ifdef VERSTR
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "DisplayVersion" "${VERSTR}"
!endif
  ;WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "Publisher" "smartmontools"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "UninstallString" '"$INSTDIR\uninst-smartmontools.exe"'
  ;WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "URLInfoAbout" "http://smartmontools.sourceforge.net/"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "HelpLink"     "http://smartmontools.sourceforge.net/"
  ;WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "URLUpdateInfo" "http://sourceforge.net/project/showfiles.php?group_id=64297"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "URLUpdateInfo" "http://smartmontools-win32.dyndns.org/"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoRepair" 1
  WriteUninstaller "uninst-smartmontools.exe"

SectionEnd

Section "Start Menu Shortcuts" MENU_SECTION

  SectionIn 1

  CreateDirectory "$SMPROGRAMS\smartmontools"

  ; smartctl
  IfFileExists "$INSTDIR\bin\smartctl.exe" 0 noctl
    SetOutPath "$INSTDIR\bin"
    DetailPrint "Create file: $INSTDIR\bin\smartctl-run.bat"
    Push "$INSTDIR\bin\smartctl-run.bat"
    Call CreateSmartctlBat
    IfFileExists "$WINDIR\system32\cmd.exe" 0 +2
      CreateShortCut "$SMPROGRAMS\smartmontools\smartctl (CMD).lnk" "cmd.exe" "/k smartctl-run.bat"
    CreateDirectory "$SMPROGRAMS\smartmontools\smartctl Examples"
    FileOpen $0 "$SMPROGRAMS\smartmontools\smartctl Examples\!Read this first!.txt" "w"
    FileWrite $0 "All the example commands in this directory$\r$\napply to the first drive (sda).$\r$\n"
    FileClose $0
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\All info (-a).lnk"                    "$INSTDIR\bin\smartctl-run.bat" "-a sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Identify drive (-i).lnk"              "$INSTDIR\bin\smartctl-run.bat" "-i sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART attributes (-A).lnk"            "$INSTDIR\bin\smartctl-run.bat" "-A sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART capabilities (-c).lnk"          "$INSTDIR\bin\smartctl-run.bat" "-c sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART health status (-H).lnk"         "$INSTDIR\bin\smartctl-run.bat" "-H sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART error log (-l error).lnk"       "$INSTDIR\bin\smartctl-run.bat" "-l error sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART selftest log (-l selftest).lnk" "$INSTDIR\bin\smartctl-run.bat" "-l selftest sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start long selftest (-t long).lnk"    "$INSTDIR\bin\smartctl-run.bat" "-t long sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start offline test (-t offline).lnk"  "$INSTDIR\bin\smartctl-run.bat" "-t offline sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start short selftest (-t short).lnk"  "$INSTDIR\bin\smartctl-run.bat" "-t short sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Stop(Abort) selftest (-X).lnk"        "$INSTDIR\bin\smartctl-run.bat" "-X sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART off (-s off).lnk"          "$INSTDIR\bin\smartctl-run.bat" "-s off sda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART on (-s on).lnk"            "$INSTDIR\bin\smartctl-run.bat" "-s on sda"  
  noctl:

  ; smartd
  IfFileExists "$INSTDIR\bin\smartd.exe" 0 nod
    SetOutPath "$INSTDIR\bin"
    DetailPrint "Create file: $INSTDIR\bin\smartd-run.bat"
    FileOpen $0 "$INSTDIR\bin\smartd-run.bat" "w"
    FileWrite $0 '@echo off$\r$\necho smartd %1 %2 %3 %4 %5$\r$\n"$INSTDIR\bin\smartd" %1 %2 %3 %4 %5$\r$\npause$\r$\n'
    FileClose $0
    CreateDirectory "$SMPROGRAMS\smartmontools\smartd Examples"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, smartd.log.lnk" "$INSTDIR\bin\smartd-run.bat" "-l local0"
    IfFileExists "$WINDIR\system32\cmd.exe" 0 +2
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, eventlog.lnk" "$INSTDIR\bin\smartd-run.bat" ""
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon stop.lnk" "$INSTDIR\bin\smartd-run.bat" "stop"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Do all tests once (-q onecheck).lnk" "$INSTDIR\bin\smartd-run.bat" "-q onecheck"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Debug mode (-d).lnk" "$INSTDIR\bin\smartd-run.bat" "-d"
    IfFileExists "$WINDIR\notepad.exe" 0 nopad
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Edit smartd.conf.lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\smartd.conf"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\View smartd.log.lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\smartd.log"
    nopad:

    ; smartd service (not on 9x/ME)
    IfFileExists "$WINDIR\system32\cmd.exe" 0 nosvc
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, eventlog, 30min.lnk" "$INSTDIR\bin\smartd-run.bat" "install"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, smartd.log, 10min.lnk" "$INSTDIR\bin\smartd-run.bat" "install -l local0 -i 600"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, smartd.log, 30min.lnk" "$INSTDIR\bin\smartd-run.bat" "install -l local0"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service remove.lnk" "$INSTDIR\bin\smartd-run.bat" "remove"
      DetailPrint "Create file: $INSTDIR\bin\net-run.bat"
      FileOpen $0 "$INSTDIR\bin\net-run.bat" "w"
      FileWrite $0 "@echo off$\r$\necho net %1 %2 %3 %4 %5$\r$\nnet %1 %2 %3 %4 %5$\r$\npause$\r$\n"
      FileClose $0
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service start.lnk" "$INSTDIR\bin\net-run.bat" "start smartd"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service stop.lnk" "$INSTDIR\bin\net-run.bat" "stop smartd"
    nosvc:
  nod:

  ; Documentation
  IfFileExists "$INSTDIR\doc\README.TXT" 0 nodoc
    SetOutPath "$INSTDIR\doc"
    CreateDirectory "$SMPROGRAMS\smartmontools\Documentation"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartctl manual page (html).lnk"    "$INSTDIR\doc\smartctl.8.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd manual page (html).lnk"      "$INSTDIR\doc\smartd.8.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf manual page (html).lnk" "$INSTDIR\doc\smartd.conf.5.html"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartctl manual page (txt).lnk"     "$INSTDIR\doc\smartctl.8.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd manual page (txt).lnk"       "$INSTDIR\doc\smartd.8.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf manual page (txt).lnk"  "$INSTDIR\doc\smartd.conf.5.txt"
    IfFileExists "$WINDIR\notepad.exe" 0 +5
      CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf sample.lnk" "$WINDIR\notepad.exe" "$INSTDIR\doc\smartd.conf"
      IfFileExists "$INSTDIR\bin\drivedb.h" 0 +3
        CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\drivedb.h (view).lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\drivedb.h"
        CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\drivedb-add.h (create, edit).lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\drivedb-add.h"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\AUTHORS.lnk"   "$INSTDIR\doc\AUTHORS.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\CHANGELOG.lnk" "$INSTDIR\doc\CHANGELOG.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\COPYING.lnk"   "$INSTDIR\doc\COPYING.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\INSTALL.lnk"   "$INSTDIR\doc\INSTALL.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\NEWS.lnk"      "$INSTDIR\doc\NEWS.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\README.lnk"    "$INSTDIR\doc\README.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\TODO.lnk"      "$INSTDIR\doc\TODO.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\WARNINGS.lnk"  "$INSTDIR\doc\WARNINGS.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\Windows version download page.lnk" "http://smartmontools-win32.dyndns.org/smartmontools/"
  nodoc:

  ; Homepage
  CreateShortCut "$SMPROGRAMS\smartmontools\smartmontools Home Page.lnk" "http://smartmontools.sourceforge.net/"

  ; drivedb.h update
  IfFileExists "$INSTDIR\bin\update-smart-drivedb.exe" 0 +2
    CreateShortCut "$SMPROGRAMS\smartmontools\drivedb.h update.lnk" "$INSTDIR\bin\update-smart-drivedb.exe"

  ; Uninstall
  IfFileExists "$INSTDIR\uninst-smartmontools.exe" 0 +2
    CreateShortCut "$SMPROGRAMS\smartmontools\Uninstall smartmontools.lnk" "$INSTDIR\uninst-smartmontools.exe"

SectionEnd

Section "Add install dir to PATH" PATH_SECTION

  SectionIn 1

  IfFileExists "$WINDIR\system32\cmd.exe" 0 +3
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

  Section "Remove existing entries first"
    SectionIn 3
    !insertmacro DriveMenuRemove
  SectionEnd

!macro DriveSection id name args
  Section 'smartctl ${args} ...' DRIVE_${id}_SECTION
    SectionIn 3
    Call CheckSmartctlBat
    DetailPrint 'Add drive menu entry "${name}": smartctl ${args} ...'
    WriteRegStr HKCR "Drive\shell\smartctl${id}" "" "${name}"
    WriteRegStr HKCR "Drive\shell\smartctl${id}\command" "" '"$INSTDIR\bin\smartctl-run.bat" ${args} %L'
  SectionEnd
!macroend

  !insertmacro DriveSection 0 "SMART all info"       "-a"
  !insertmacro DriveSection 1 "SMART status"         "-Hc"
  !insertmacro DriveSection 2 "SMART attributes"     "-A"
  !insertmacro DriveSection 3 "SMART short selftest" "-t short"
  !insertmacro DriveSection 4 "SMART long selftest"  "-t long"
  !insertmacro DriveSection 5 "SMART continue selective selftest"  '-t "selective,cont"'

SectionGroupEnd

Section "UBCD4Win Plugin" UBCD_SECTION

  SectionIn 4

  SetOutPath "$UBCDDIR"
  DetailPrint "Create file: smartmontools.inf"
  FileOpen $0 "$UBCDDIR\smartmontools.inf" "w"
  FileWrite $0 '; smartmontools.inf$\r$\n; PE Builder v3 plug-in INF file$\r$\n'
  FileWrite $0 '; Created by smartmontools installer$\r$\n'
  FileWrite $0 '; http://smartmontools.sourceforge.net/$\r$\n$\r$\n'
  FileWrite $0 '[Version]$\r$\nSignature= "$$Windows NT$$"$\r$\n$\r$\n'
  FileWrite $0 '[PEBuilder]$\r$\nName="Disk -Diagnostic: smartmontools"$\r$\n'
  FileWrite $0 'Enable=1$\r$\nHelp="files\smartctl.8.html"$\r$\n$\r$\n'
  FileWrite $0 '[WinntDirectories]$\r$\na=Programs\smartmontools,2$\r$\n$\r$\n'
  FileWrite $0 '[SourceDisksFolders]$\r$\nfiles=a,,1$\r$\n$\r$\n'
  FileWrite $0 '[Append]$\r$\nnu2menu.xml, smartmontools_nu2menu.xml$\r$\n'
  FileClose $0

  DetailPrint "Create file: smartmontools_nu2menu.xml"
  FileOpen $0 "$UBCDDIR\smartmontools_nu2menu.xml" "w"
  FileWrite $0 '<!-- Nu2Menu entry for smartmontools -->$\r$\n<NU2MENU>$\r$\n'
  FileWrite $0 '$\t<MENU ID="Programs">$\r$\n$\t$\t<MITEM TYPE="POPUP" MENUID="Disk Tools">'
  FileWrite $0 'Disk Tools</MITEM>$\r$\n$\t</MENU>$\r$\n$\t<MENU ID="Disk Tools">$\r$\n'
  FileWrite $0 '$\t$\t<MITEM TYPE="POPUP" MENUID="Diagnostic">Diagnostic</MITEM>$\r$\n$\t</MENU>'
  FileWrite $0 '$\r$\n$\t<MENU ID="Diagnostic">$\r$\n$\t$\t<MITEM TYPE="ITEM" DISABLED="'
  FileWrite $0 '@Not(@FileExists(@GetProgramDrive()\Programs\smartmontools\smartctl.exe))" '
  FileWrite $0 'CMD="RUN" FUNC="cmd.exe /k cd /d @GetProgramDrive()\Programs\smartmontools&'
  FileWrite $0 'set PATH=@GetProgramDrive()\Programs\smartmontools;%PATH%&smartctl-run.bat  ">'
  FileWrite $0 'smartctl</MITEM>$\r$\n$\t</MENU>$\r$\n</NU2MENU>$\r$\n'
  FileClose $0
  
  SetOutPath "$UBCDDIR\files"
  DetailPrint "Create file: smartctl-run.bat"
  Push "$UBCDDIR\files\smartctl-run.bat"
  Call CreateSmartctlBat
  File "${INPDIR}\bin\smartctl.exe"
  File "${INPDIR}\bin\smartd.exe"
  File "${INPDIR}\doc\smartctl.8.html"
  File "${INPDIR}\doc\smartctl.8.txt"
  File "${INPDIR}\doc\smartd.8.html"
  File "${INPDIR}\doc\smartd.8.txt"
  File "${INPDIR}\doc\smartd.conf"

SectionEnd


;--------------------------------------------------------------------

Section "Uninstall"
  
  ; Stop & remove service
  IfFileExists "$INSTDIR\bin\smartd.exe" 0 nosrv
    ReadRegStr $0 HKLM "System\CurrentControlSet\Services\smartd" "ImagePath"
    StrCmp $0 "" nosrv
      ExecWait "net stop smartd"
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Remove smartd service ?" IDYES 0 IDNO nosrv
        ExecWait "$INSTDIR\bin\smartd.exe remove"
  nosrv:

  ; Remove installer registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools"
  DeleteRegKey HKLM "Software\smartmontools"

  ; Remove conf file ?
  IfFileExists "$INSTDIR\bin\smartd.conf" 0 noconf
    ; Assume unchanged if timestamp is equal to sample file
    GetFileTime "$INSTDIR\bin\smartd.conf" $0 $1
    GetFileTime "$INSTDIR\doc\smartd.conf" $2 $3
    StrCmp "$0:$1" "$2:$3" +2 0
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete configuration file$\n$INSTDIR\bin\smartd.conf ?" IDYES 0 IDNO noconf
        Delete "$INSTDIR\bin\smartd.conf"
  noconf:

  ; Remove log file ?
  IfFileExists "$INSTDIR\bin\smartd.log" 0 +3
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete log file$\n$INSTDIR\bin\smartd.log ?" IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\smartd.log"

  ; Remove drivedb-add file ?
  IfFileExists "$INSTDIR\bin\drivedb-add.h" 0 +3
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete local drive database file$\n$INSTDIR\bin\drivedb-add.h ?" IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\drivedb-add.h"

  ; Remove files
  Delete "$INSTDIR\bin\smartctl.exe"
  Delete "$INSTDIR\bin\smartctl-nc.exe"
  Delete "$INSTDIR\bin\smartd.exe"
  Delete "$INSTDIR\bin\syslogevt.exe"
  Delete "$INSTDIR\bin\drivedb.h"
  Delete "$INSTDIR\bin\drivedb.h.error"
  Delete "$INSTDIR\bin\drivedb.h.lastcheck"
  Delete "$INSTDIR\bin\drivedb.h.old"
  Delete "$INSTDIR\bin\update-smart-drivedb.exe"
  Delete "$INSTDIR\bin\smartctl-run.bat"
  Delete "$INSTDIR\bin\smartd-run.bat"
  Delete "$INSTDIR\bin\net-run.bat"
  Delete "$INSTDIR\doc\AUTHORS.txt"
  Delete "$INSTDIR\doc\CHANGELOG.txt"
  Delete "$INSTDIR\doc\COPYING.txt"
  Delete "$INSTDIR\doc\INSTALL.txt"
  Delete "$INSTDIR\doc\NEWS.txt"
  Delete "$INSTDIR\doc\README.txt"
  Delete "$INSTDIR\doc\TODO.txt"
  Delete "$INSTDIR\doc\WARNINGS.txt"
  Delete "$INSTDIR\doc\checksums.txt"
  Delete "$INSTDIR\doc\smartctl.8.html"
  Delete "$INSTDIR\doc\smartctl.8.txt"
  Delete "$INSTDIR\doc\smartd.8.html"
  Delete "$INSTDIR\doc\smartd.8.txt"
  Delete "$INSTDIR\doc\smartd.conf"
  Delete "$INSTDIR\doc\smartd.conf.5.html"
  Delete "$INSTDIR\doc\smartd.conf.5.txt"
  Delete "$INSTDIR\uninst-smartmontools.exe"

  ; Remove shortcuts
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
  IfFileExists "$WINDIR\system32\cmd.exe" 0 +3
    Push "$INSTDIR\bin"
    Call un.RemoveFromPath

  ; Remove drive menu registry entries
  !insertmacro DriveMenuRemove

  ; Check for still existing entries
  IfFileExists "$INSTDIR\bin\smartd.exe" 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION "$INSTDIR\bin\smartd.exe could not be removed.$\nsmartd is possibly still running."
    Goto +3
  IfFileExists "$INSTDIR" 0 +2
    MessageBox MB_OK "Note: $INSTDIR could not be removed."

  IfFileExists "$SMPROGRAMS\smartmontools" 0 +2
    MessageBox MB_OK "Note: $SMPROGRAMS\smartmontools could not be removed."

SectionEnd

;--------------------------------------------------------------------
; Functions

Function .onInit

  ; Get UBCD4Win install location
  ReadRegStr $0 HKLM "Software\UBCD4Win" "InstallPath"
  StrCmp $0 "" 0 +2
    StrCpy $0 "C:\UBCD4Win"
  StrCpy $UBCDDIR "$0\plugin\Disk\Diagnostic\smartmontools"

  ; Hide "Add install dir to PATH" on 9x/ME
  IfFileExists "$WINDIR\system32\cmd.exe" +2 0
    SectionSetText ${PATH_SECTION} ""

FunctionEnd

; Directory page callbacks

!macro CheckSection section
  SectionGetFlags ${section} $0
  IntOp $0 $0 & 1
  IntCmp $0 1 done
!macroend

Function SkipProgPath
  !insertmacro CheckSection ${SMARTCTL_SECTION}
  !insertmacro CheckSection ${SMARTCTL_NC_SECTION}
  !insertmacro CheckSection ${SMARTD_SECTION}
  !insertmacro CheckSection ${DRIVEDB_SECTION}
  !insertmacro CheckSection ${DOC_SECTION}
  !insertmacro CheckSection ${MENU_SECTION}
  !insertmacro CheckSection ${PATH_SECTION}
  !insertmacro CheckSection ${DRIVE_0_SECTION}
  !insertmacro CheckSection ${DRIVE_1_SECTION}
  !insertmacro CheckSection ${DRIVE_2_SECTION}
  !insertmacro CheckSection ${DRIVE_3_SECTION}
  !insertmacro CheckSection ${DRIVE_4_SECTION}
  !insertmacro CheckSection ${DRIVE_5_SECTION}
  Abort
done:
FunctionEnd

Function SkipUBCDPath
  !insertmacro CheckSection ${UBCD_SECTION}
  Abort
done:
FunctionEnd


; Create smartctl-run.bat

Function CreateSmartctlBat
  Exch $0
  FileOpen $0 $0 "w"
  FileWrite $0 '@echo off$\r$\nif not "%1" == "" goto run$\r$\n'
  FileWrite $0 'echo Examples (for first drive):$\r$\n'
  FileWrite $0 'echo smartctl -i sda            Show identify information$\r$\n'
  FileWrite $0 'echo smartctl -H sda            Show SMART health status$\r$\n'
  FileWrite $0 'echo smartctl -c sda            Show SMART capabilities$\r$\n'
  FileWrite $0 'echo smartctl -A sda            Show SMART attributes$\r$\n'
  FileWrite $0 'echo smartctl -l error sda      Show error log$\r$\n'
  FileWrite $0 'echo smartctl -l selftest sda   Show self-test log$\r$\n'
  FileWrite $0 'echo smartctl -a sda            Show all of the above$\r$\n'
  FileWrite $0 'echo smartctl -t short sda      Start short self test$\r$\n'
  FileWrite $0 'echo smartctl -t long sda       Start long self test$\r$\n'
  FileWrite $0 'echo Use "sdb", "sdc", ... for second, third, ... drive.$\r$\n'
  FileWrite $0 'echo See man page (smartctl.8.*) for further info.$\r$\n'
  FileWrite $0 'goto end$\r$\n:run$\r$\n'
  FileWrite $0 'echo smartctl %1 %2 %3 %4 %5$\r$\n'
  FileWrite $0 '"$INSTDIR\bin\smartctl" %1 %2 %3 %4 %5$\r$\n'
  FileWrite $0 'pause$\r$\n:end$\r$\n'
  FileClose $0
  Pop $0
FunctionEnd

; Create smartctl-run.bat if missing

Function CheckSmartctlBat
  IfFileExists "$INSTDIR\bin\smartctl-run.bat" done 0
    SetOutPath "$INSTDIR\bin"
    DetailPrint "Create file: $INSTDIR\bin\smartctl-run.bat"
    Push "$INSTDIR\bin\smartctl-run.bat"
    Call CreateSmartctlBat
  done:
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

  ReadRegStr $1 ${Environ} "PATH"
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

  DetailPrint "Add to PATH: $0"
  StrCpy $2 $1 1 -1
  StrCmp $2 ";" 0 +2
    StrCpy $1 $1 -1 ; remove trailing ';'
  StrCmp $1 "" +2   ; no leading ';'
    StrCpy $0 "$1;$0"
  WriteRegExpandStr ${Environ} "PATH" $0
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

done:
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
  StrCmp $5 ";" +2
    StrCpy $1 "$1;" ; ensure trailing ';'
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
  StrCmp $5 ";" 0 +2
    StrCpy $3 $3 -1 ; remove trailing ';'
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
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
done:
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
