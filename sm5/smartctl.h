/*
 * smartctl.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
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

#ifndef __SMARTCTL_H_
#define __SMARTCTL_H_

#ifndef CVSID6
#define CVSID6 "$Id: smartctl.h,v 1.7 2002/10/22 09:44:55 ballen4705 Exp $\n"
#endif

/* Defines for command line options */ 
#define DRIVEINFO		'i'
#define CHECKSMART		'c'
#define SMARTVERBOSEALL		'a'
#define SMARTVENDORATTRIB	'v'
#define GENERALSMARTVALUES	'g'
#define SMARTERRORLOG		'l'
#define SMARTSELFTESTLOG	'L'
#define SMARTDISABLE		'd'
#define SMARTENABLE		'e'
#define SMARTEXEOFFIMMEDIATE	'O'
#define SMARTSHORTSELFTEST	'S'
#define SMARTEXTENDSELFTEST	'X'
#define SMARTSHORTCAPSELFTEST	's'
#define SMARTEXTENDCAPSELFTEST	'x'
#define SMARTSELFTESTABORT	'A'
#define SMARTAUTOOFFLINEENABLE  't'
#define SMARTAUTOOFFLINEDISABLE 'T'
#define SMARTAUTOSAVEENABLE     'f'
#define SMARTAUTOSAVEDISABLE    'F'
#define PRINTCOPYLEFT           'p'
#define SMART009MINUTES         'm'


/* Boolean Values */
#define TRUE 0x01
#define FALSE 0x00

#endif
