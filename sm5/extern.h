/*
 * extern.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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


#ifndef EXTERN_H_CVSID
#define EXTERN_H_CVSID "$Id: extern.h,v 1.27 2003/08/27 21:16:19 pjwilliams Exp $\n"
#endif

// For development and testing of Selective self-test code
#define DEVELOP_SELECTIVE_SELF_TEST 0

// Possible values for fixfirmwarebug
#define FIX_NONE             0
#define FIX_SAMSUNG          1

// Block used for global control/communications.  If you need more
// global variables, this should be the only place that you need to
// add them.
typedef struct smartmonctrl_s {
  unsigned char driveinfo;
  unsigned char checksmart;
  unsigned char smartvendorattrib;
  unsigned char generalsmartvalues;
  unsigned char smartlogdirectory;
  unsigned char smartselftestlog;
  unsigned char smarterrorlog;
  unsigned char smartdisable;
  unsigned char smartenable; 
  unsigned char smartstatus;
  unsigned char smartexeoffimmediate;
  unsigned char smartshortselftest;
  unsigned char smartextendselftest;
  unsigned char smartconveyanceselftest;
#if DEVELOP_SELECTIVE_SELF_TEST
  unsigned char smartselectiveselftest;
#endif
  unsigned char smartshortcapselftest;
  unsigned char smartextendcapselftest;
  unsigned char smartconveyancecapselftest;
#if DEVELOP_SELECTIVE_SELF_TEST
  unsigned char smartselectivecapselftest;
#endif
  unsigned char smartselftestabort;
  unsigned char smartautoofflineenable;
  unsigned char smartautoofflinedisable;
  unsigned char smartautosaveenable;
  unsigned char smartautosavedisable;
#if DEVELOP_SELECTIVE_SELF_TEST
  unsigned long long smartselectivespan[5][2];
  int smartselectivenumspans;
#endif
  int           testcase;
  unsigned char quietmode;
  unsigned char veryquietmode;
  unsigned char permissive;
  unsigned char conservative;
  unsigned char checksumfail;
  unsigned char checksumignore;
  unsigned char reportataioctl;
  unsigned char reportscsiioctl;
  unsigned char fixfirmwarebug;
  // If nonzero, escalade is 1 plus the disk number behind an escalade
  // controller
  unsigned char escalade;
  unsigned char ignorepresets;
  unsigned char showpresets;
  // The i'th entry in this array will modify the printed meaning of
  // the i'th SMART attribute.  The default definitions of the
  // Attributes are obtained by having the array be all zeros.  If
  // attributedefs[i] is nonzero, it means that the i'th attribute has
  // a non-default meaning.  See the ataPrintSmartAttribName and
  // and parse_attribute_def functions.
  unsigned char attributedefs[256];
} smartmonctrl;

#endif
