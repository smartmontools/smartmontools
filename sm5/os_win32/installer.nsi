;
; installer.nsi - NSIS install script for smartmontools
;
; Copyright (C) 2006 Christian Franke <smartmontools-support@lists.sourceforge.net>
;
; Project home page is: http://smartmontools.sourceforge.net
;
; Download and install NSIS from: http://nsis.sourceforge.net/Download
; Process with makensis to create installer (tested with NSIS 2.17).
;
; $Id: installer.nsi,v 1.1 2006/06/05 17:35:34 chrfranke Exp $
;


;--------------------------------------------------------------------
; Command line arguments:
; makensis /DINPDIR=<input-dir> /DOUTFILE=<output-file> installer.nsi

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

LicenseData "${INPDIR}\doc\COPYING.txt"

;--------------------------------------------------------------------
; Pages

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

InstType "Full"
InstType "Extract files only"


;--------------------------------------------------------------------
; Sections

SectionGroup "Program files"

  Section "smartctl"

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\smartctl.exe"

  SectionEnd

  Section "smartd"

    SectionIn 1 2

    SetOutPath "$INSTDIR\bin"
    File "${INPDIR}\bin\smartd.exe"

    IfFileExists "$INSTDIR\bin\smartd.conf" 0 +2
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Replace existing configuration file$\n$INSTDIR\bin\smartd.conf ?" IDYES 0 IDNO +2
        File "${INPDIR}\doc\smartd.conf"

    IfFileExists "$WINDIR\system32\cmd.exe" 0 +2
      File /nonfatal "${INPDIR}\bin\syslogevt.exe"

  SectionEnd

SectionGroupEnd

Section "Documentation"

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
  File "${INPDIR}\doc\smartctl.8.html"
  File "${INPDIR}\doc\smartctl.8.txt"
  File "${INPDIR}\doc\smartd.8.html"
  File "${INPDIR}\doc\smartd.8.txt"
  File "${INPDIR}\doc\smartd.conf"
  File "${INPDIR}\doc\smartd.conf.5.html"
  File "${INPDIR}\doc\smartd.conf.5.txt"

SectionEnd

Section "Uninstaller"

  SectionIn 1
  AddSize 35

  CreateDirectory "$INSTDIR"

  ; Save installation location
  WriteRegStr HKLM "Software\smartmontools" "Install_Dir" "$INSTDIR"

  ; Write uninstall keys and program
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "DisplayName" "smartmontools"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "UninstallString" '"$INSTDIR\uninst-smartmontools.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools" "NoRepair" 1
  WriteUninstaller "uninst-smartmontools.exe"

SectionEnd

Section "Start Menu Shortcuts"

  SectionIn 1

  CreateDirectory "$SMPROGRAMS\smartmontools"

  ; smartctl
  IfFileExists "$INSTDIR\bin\smartctl.exe" 0 noctl
    SetOutPath "$INSTDIR\bin"
    DetailPrint "Create file: $INSTDIR\bin\smartctl-run.bat"
    FileOpen $0 "$INSTDIR\bin\smartctl-run.bat" "w"
    FileWrite $0 "@echo off$\r$\necho smartctl %1 %2 %3 %4 %5$\r$\nsmartctl %1 %2 %3 %4 %5$\r$\npause$\r$\n"
    FileClose $0
    CreateDirectory "$SMPROGRAMS\smartmontools\smartctl Examples"
    FileOpen $0 "$SMPROGRAMS\smartmontools\smartctl Examples\!Read this first!.txt" "w"
    FileWrite $0 "All the example commands in this directory$\r$\napply to the first IDE/ATA/SATA drive (hda).$\r$\n"
    FileClose $0
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\All info (-a).lnk"                    "$INSTDIR\bin\smartctl-run.bat" "-a hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Identify drive (-i).lnk"              "$INSTDIR\bin\smartctl-run.bat" "-i hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART attributes (-A).lnk"            "$INSTDIR\bin\smartctl-run.bat" "-A hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART capabilities (-c).lnk"          "$INSTDIR\bin\smartctl-run.bat" "-c hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART health status (-H).lnk"         "$INSTDIR\bin\smartctl-run.bat" "-H hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART error log (-l error).lnk"       "$INSTDIR\bin\smartctl-run.bat" "-l error hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\SMART selftest log (-l selftest).lnk" "$INSTDIR\bin\smartctl-run.bat" "-l selftest hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start long selfttest (-t long).lnk"   "$INSTDIR\bin\smartctl-run.bat" "-t long hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start offline test (-t offline).lnk"  "$INSTDIR\bin\smartctl-run.bat" "-t offline hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Start short selfttest (-t short).lnk" "$INSTDIR\bin\smartctl-run.bat" "-t short hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Stop(Abort) selfttest (-X).lnk"       "$INSTDIR\bin\smartctl-run.bat" "-X hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART off (-s off).lnk"          "$INSTDIR\bin\smartctl-run.bat" "-s off hda"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartctl Examples\Turn SMART on (-s on).lnk"            "$INSTDIR\bin\smartctl-run.bat" "-s on hda"  
  noctl:

  ; smartd
  IfFileExists "$INSTDIR\bin\smartd.exe" 0 nod
    SetOutPath "$INSTDIR\bin"
    DetailPrint "Create file: $INSTDIR\bin\smartd-run.bat"
    FileOpen $0 "$INSTDIR\bin\smartd-run.bat" "w"
    FileWrite $0 "@echo off$\r$\necho smartd %1 %2 %3 %4 %5$\r$\nsmartd %1 %2 %3 %4 %5$\r$\npause$\r$\n"
    FileClose $0
    CreateDirectory "$SMPROGRAMS\smartmontools\smartd Examples"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, log to smartd.log.lnk" "$INSTDIR\bin\smartd-run.bat" "-l local0"
    IfFileExists "$WINDIR\system32\cmd.exe" 0 +2
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon start, log to eventlog.lnk" "$INSTDIR\bin\smartd-run.bat" ""
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Daemon stop.lnk" "$INSTDIR\bin\smartd-run.bat" "stop"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Do all tests once (-q onecheck).lnk" "$INSTDIR\bin\smartd-run.bat" "-q onecheck"
    CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Debug mode (-d).lnk" "$INSTDIR\bin\smartd-run.bat" "-d"
    IfFileExists "$WINDIR\notepad.exe" 0 nopad
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Edit smartd.conf.lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\smartd.conf"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\View smartd.log.lnk" "$WINDIR\notepad.exe" "$INSTDIR\bin\smartd.log"
    nopad:

    ; smartd service (not on 9x/ME)
    IfFileExists "$WINDIR\system32\cmd.exe" 0 nosvc
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, log to eventlog.lnk" "$INSTDIR\bin\smartd-run.bat" "install"
      CreateShortCut "$SMPROGRAMS\smartmontools\smartd Examples\Service install, log to smartd.log.lnk" "$INSTDIR\bin\smartd-run.bat" "install -l local0"
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
    IfFileExists "$WINDIR\notepad.exe" 0 +2
      CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\smartd.conf sample.lnk" "$WINDIR\notepad.exe" "$INSTDIR\doc\smartd.conf"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\AUTHORS.lnk"   "$INSTDIR\doc\AUTHORS.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\CHANGELOG.lnk" "$INSTDIR\doc\CHANGELOG.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\COPYING.lnk"   "$INSTDIR\doc\COPYING.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\INSTALL.lnk"   "$INSTDIR\doc\INSTALL.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\NEWS.lnk"      "$INSTDIR\doc\NEWS.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\README.lnk"    "$INSTDIR\doc\README.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\TODO.lnk"      "$INSTDIR\doc\TODO.txt"
    CreateShortCut "$SMPROGRAMS\smartmontools\Documentation\WARNINGS.lnk"  "$INSTDIR\doc\WARNINGS.txt"
  nodoc:

  ; Homepage
  CreateShortCut "$SMPROGRAMS\smartmontools\smartmontools Home Page.lnk" "http://smartmontools.sourceforge.net/"

  ; Uninstall
  IfFileExists "$INSTDIR\uninst-smartmontools.exe" 0 +2
    CreateShortCut "$SMPROGRAMS\smartmontools\Uninstall smartmontools.lnk" "$INSTDIR\uninst-smartmontools.exe"

SectionEnd

Section "Add install dir to PATH" PATH_IDX

  SectionIn 1

  IfFileExists "$WINDIR\system32\cmd.exe" 0 +3
    Push "$INSTDIR\bin"
    Call AddToPath
 
SectionEnd

;--------------------------------------------------------------------

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools"
  DeleteRegKey HKLM "Software\smartmontools"

  IfFileExists "$INSTDIR\bin\smartd.conf" 0 noconf
    ; Assume unchanged if timestamp is equal to sample file
    GetFileTime "$INSTDIR\bin\smartd.conf" $0 $1
    GetFileTime "$INSTDIR\doc\smartd.conf" $2 $3
    StrCmp "$0:$1" "$2:$3" +2 0
      MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete configuration file$\n$INSTDIR\bin\smartd.conf ?" IDYES 0 IDNO noconf
        Delete "$INSTDIR\bin\smartd.conf"
  noconf:

  IfFileExists "$INSTDIR\bin\smartd.log" 0 +3
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2  "Delete log file$\n$INSTDIR\bin\smartd.log ?" IDYES 0 IDNO +2
      Delete "$INSTDIR\bin\smartd.log"

  ; Remove files
  Delete "$INSTDIR\bin\smartctl.exe"
  Delete "$INSTDIR\bin\smartd.exe"
  Delete "$INSTDIR\bin\syslogevt.exe"
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
  RMDir  "$SMPROGRAMS\smartmontools\smartctl Examples\"
  RMDir  "$SMPROGRAMS\smartmontools\smartd Examples\"
  RMDir  "$SMPROGRAMS\smartmontools"
  RMDir  "$INSTDIR\bin"
  RMDir  "$INSTDIR\doc"
  RMDir  "$INSTDIR"

  ; Remove install dir from PATH
  IfFileExists "$WINDIR\system32\cmd.exe" 0 +3
    Push "$INSTDIR\bin"
    Call un.RemoveFromPath

  ; Check for still existing files
  IfFileExists "$INSTDIR\bin\smartd.exe" 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION "$INSTDIR\bin\smartd.exe could not be removed.$\nsmartd is possibly still running."
    Goto +3
  IfFileExists "$INSTDIR" 0 +2
    MessageBox MB_OK "Note: $INSTDIR could not be removed."

SectionEnd

;--------------------------------------------------------------------

Function .onInit

  ; Hide "Add install dir to PATH" on 9x/ME
  IfFileExists "$WINDIR\system32\cmd.exe" +2 0
    SectionSetText ${PATH_IDX} ""

FunctionEnd


;--------------------------------------------------------------------
; Utility functions
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
  StrCmp $2 "" "" done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" done
 
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
