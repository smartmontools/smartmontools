;
; smartmontools drive database update NSIS script
;
; Home page of code is: http://www.smartmontools.org
;
; Copyright (C) 2011-13 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
; makensis -DBRANCH=<svn-branch-name> update-smart-drivedb.nsi

!include "FileFunc.nsh"

Name "update-smart-drivedb"
Caption "Update smartmontools drivedb.h"
OutFile "update-smart-drivedb.exe"

SetCompressor /solid lzma

XPStyle on
InstallColors /windows

Page instfiles

Section ""

  SetOutPath $INSTDIR

!ifdef BRANCH
  StrCpy $0 "branches/${BRANCH}"
  Push $0
  Call Download
  IfErrors 0 endload
!endif

  StrCpy $0 "trunk"
  Push $0
  Call Download
  IfErrors 0 endload
    MessageBox MB_OK "Download failed" /SD IDOK
    Abort "Download failed"
  endload:

  ; Check syntax
  Delete "drivedb.h.error"
  IfFileExists "smartctl-nc.exe" 0 endsyntax
    ExecWait '.\smartctl-nc.exe -B drivedb.h.new -P showall' $1
    StrCmp $1 "0" endsyntax
      Rename "drivedb.h.new" "drivedb.h.error"
      MessageBox MB_OK "drivedb.h.error: rejected by smartctl, probably no longer compatible" /SD IDOK
      Abort "drivedb.h.error: rejected by smartctl, probably no longer compatible"
  endsyntax:

  ; Keep old file if identical
  Delete "drivedb.h.lastcheck"
  IfFileExists "drivedb.h" 0 endcomp
    Call Cmp
    IfErrors changed 0
      DetailPrint "drivedb.h is already up to date"
      MessageBox MB_OK "$INSTDIR\drivedb.h is already up to date" /SD IDOK
      Delete "drivedb.h.new"
      DetailPrint "Create file: drivedb.h.lastcheck"
      FileOpen $1 "drivedb.h.lastcheck" w
      FileClose $1
      Return
    changed:
    Delete "drivedb.h.old"
    Rename "drivedb.h" "drivedb.h.old"

  endcomp:
  Rename "drivedb.h.new" "drivedb.h"
  MessageBox MB_OK "$INSTDIR\drivedb.h updated from $0" /SD IDOK

SectionEnd

Function .onInit
  ; Install in same directory
  ${GetExePath} $INSTDIR
FunctionEnd

; Download from branch or trunk on stack, SetErrors on error
Function Download
  Pop $R0
  DetailPrint "Download from $R0"

  ; SVN repository read-only URL
  ; (SF code browser does not return ContentLength required for NSISdl::download)
  StrCpy $R1 "http://svn.code.sf.net/p/smartmontools/code/$R0/smartmontools/drivedb.h"

  DetailPrint "($R1)"

  NSISdl::download $R1 "drivedb.h.new"
  Pop $R0
  DetailPrint "Download: $R0"
  ClearErrors
  StrCmp $R0 "success" 0 err

  ; File must start with comment
  FileOpen $R0 "drivedb.h.new" r
  FileReadByte $R0 $R1
  FileClose $R0
  ClearErrors
  StrCmp $R1 "47" 0 +2
    Return
  DetailPrint "drivedb.h.new: syntax error ($R1)"

err:
  Delete "drivedb.h.new"
  SetErrors
FunctionEnd

; Compare drivedb.h drivedb.h.new, SetErrors if different
; TODO: ignore differences in Id string
Function Cmp
  ClearErrors
  FileOpen $R0 "drivedb.h" r
  FileOpen $R1 "drivedb.h.new" r
  readloop:
    FileRead $R0 $R2
    FileRead $R1 $R3
    StrCmp $R2 $R3 0 +2
  IfErrors 0 readloop
  FileClose $R0
  FileClose $R1
  ClearErrors
  StrCmp $R2 $R3 0 +2
    Return
  SetErrors
FunctionEnd
