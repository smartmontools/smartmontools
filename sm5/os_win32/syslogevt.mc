;/*
; * os_win32/syslogevt.mc
; *
; * Home page of code is: http://smartmontools.sourceforge.net
; *
; * Copyright (C) 2004 Christian Franke <smartmontools-support@lists.sourceforge.net>
; *
; * This program is free software; you can redistribute it and/or modify
; * it under the terms of the GNU General Public License as published by
; * the Free Software Foundation; either version 2, or (at your option)
; * any later version.
; *
; * You should have received a copy of the GNU General Public License
; * (for example COPYING); if not, write to the Free
; * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
; *
; */
;
;// $Id: syslogevt.mc,v 1.1 2004/03/15 10:48:28 chrfranke Exp $
;
;// Use message compiler "mc" to generate
;//   syslogevt.rc, syslogevt.h, msg00001.bin
;// from this file.
;// MSG_SYSLOG in syslogmsg.h must be zero
;
;

MessageId=0x0
Severity=Success
Facility=Application
SymbolicName=MSG_SYSLOG
Language=English
%1
.
