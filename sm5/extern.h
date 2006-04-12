/*
 * extern.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-6 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#ifndef EXTERN_H_
#define EXTERN_H_

#define EXTERN_H_CVSID "$Id: extern.h,v 1.41 2006/04/12 14:54:28 ballen4705 Exp $\n"

// Possible values for fixfirmwarebug.  If use has NOT specified -F at
// all, then value is 0.
#define FIX_NOTSPECIFIED     0
#define FIX_NONE             1
#define FIX_SAMSUNG          2
#define FIX_SAMSUNG2         3

// Block used for global control/communications.  If you need more
// global variables, this should be the only place that you need to
// add them.
typedef struct smartmonctrl_s {
  // spans for selective self-test
  uint64_t smartselectivespan[5][2];
  // number of spans
  int smartselectivenumspans;
  int           testcase;
  // one plus time in minutes to wait after powerup before restarting
  // interrupted offline scan after selective self-test.
  int  pendingtime;
  // run offline scan after selective self-test.  0: don't change, 1:
  // turn off scan after selective self-test, 2: turn on scan after
  // selective self-test.
  unsigned char scanafterselect;
  unsigned char driveinfo;
  unsigned char checksmart;
  unsigned char smartvendorattrib;
  unsigned char generalsmartvalues;
  unsigned char smartlogdirectory;
  unsigned char smartselftestlog;
  unsigned char selectivetestlog;
  unsigned char smarterrorlog;
  unsigned char smartdisable;
  unsigned char smartenable; 
  unsigned char smartstatus;
  unsigned char smartexeoffimmediate;
  unsigned char smartshortselftest;
  unsigned char smartextendselftest;
  unsigned char smartconveyanceselftest;
  unsigned char smartselectiveselftest;
  unsigned char smartshortcapselftest;
  unsigned char smartextendcapselftest;
  unsigned char smartconveyancecapselftest;
  unsigned char smartselectivecapselftest;
  unsigned char smartselftestabort;
  unsigned char smartautoofflineenable;
  unsigned char smartautoofflinedisable;
  unsigned char smartautosaveenable;
  unsigned char smartautosavedisable;
  unsigned char printing_switchable;
  unsigned char dont_print;
  unsigned char permissive;
  unsigned char conservative;
  unsigned char checksumfail;
  unsigned char checksumignore;
  unsigned char reportataioctl;
  unsigned char reportscsiioctl;
  unsigned char fixfirmwarebug;
  // 3Ware controller type, but also extensible to other contoller types
  unsigned char controller_type;
  // For 3Ware controllers, nonzero value is 1 plus the disk number
  unsigned char controller_port;
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
