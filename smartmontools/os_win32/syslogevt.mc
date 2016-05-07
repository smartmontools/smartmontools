;/*
; * os_win32/syslogevt.mc
; *
; * Home page of code is: http://www.smartmontools.org
; *
; * Copyright (C) 2004-10 Christian Franke <smartmontools-support@lists.sourceforge.net>
; *
; * This program is free software; you can redistribute it and/or modify
; * it under the terms of the GNU General Public License as published by
; * the Free Software Foundation; either version 2, or (at your option)
; * any later version.
; *
; * You should have received a copy of the GNU General Public License
; * (for example COPYING); if not, write to the Free Software Foundation,
; * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
; *
; */
;
;// $Id$
;
;// Use message compiler "mc" or "windmc" to generate
;//   syslogevt.rc, syslogevt.h, msg00001.bin
;// from this file.
;// MSG_SYSLOG in syslogmsg.h must be zero
;// MSG_SYSLOG_nn must be == nn
;
;// MS and binutils message compiler defaults for FacilityNames differ:
;// mc:     Application = 0x000
;// windmc: Application = 0xfff
FacilityNames = (Application = 0x000)

MessageId=0x0
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG
Language=English
%1
.
;// 1-10 Line SYSLOG Messages
;// %1=Ident, %2=PID, %3=Severity, %[4-13]=Line 1-10
MessageId=0x1
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_01
Language=English
%1[%2]:%3: %4
.
MessageId=0x2
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_02
Language=English
%1[%2]:%3%n
%4%n
%5
.
MessageId=0x3
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_03
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6
.
MessageId=0x4
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_04
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7
.
MessageId=0x5
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_05
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8
.
MessageId=0x6
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_06
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8%n
%9
.
MessageId=0x7
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_07
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8%n
%9%n
%10
.
MessageId=0x8
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_08
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8%n
%9%n
%10%n
%11
.
MessageId=0x9
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_09
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8%n
%9%n
%10%n
%11%n
%12
.
MessageId=0xa
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG_10
Language=English
%1[%2]:%3%n
%4%n
%5%n
%6%n
%7%n
%8%n
%9%n
%10%n
%11%n
%12%n
%13
.
