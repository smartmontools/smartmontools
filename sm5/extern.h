/*
 * extern.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_


#ifndef CVSID3
#define CVSID3 "$Id: extern.h,v 1.7 2002/10/22 09:44:55 ballen4705 Exp $\n"
#endif

extern unsigned char driveinfo;
extern unsigned char checksmart;
extern unsigned char smartvendorattrib;
extern unsigned char generalsmartvalues;
extern unsigned char smartselftestlog;
extern unsigned char smarterrorlog;
extern unsigned char smartdisable;
extern unsigned char smartenable; 
extern unsigned char smartstatus;
extern unsigned char smartexeoffimmediate;
extern unsigned char smartshortselftest;
extern unsigned char smartextendselftest;
extern unsigned char smartshortcapselftest;
extern unsigned char smartextendcapselftest;
extern unsigned char smartselftestabort;
extern unsigned char smartautoofflineenable;
extern unsigned char smartautoofflinedisable;
extern unsigned char smartautosaveenable;
extern unsigned char smartautosavedisable;
extern unsigned char smart009minutes;
extern int           testcase;
#endif
