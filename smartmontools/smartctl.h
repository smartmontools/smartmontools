/*
 * smartctl.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-10 Bruce Allen
 * Copyright (C) 2008-17 Christian Franke
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTCTL_H_
#define SMARTCTL_H_

#define SMARTCTL_H_CVSID "$Id: smartctl.h 4842 2018-12-02 16:07:26Z chrfranke $\n"

// Return codes (bitmask)

// command line did not parse, or internal error occurred in smartctl
#define FAILCMD   (0x01<<0)

// device open failed
#define FAILDEV   (0x01<<1)

// device is in low power mode and -n option requests to exit
#define FAILPOWER (0x01<<1)

// read device identity (ATA only) failed
#define FAILID    (0x01<<1)

// smart command failed, or ATA identify device structure missing information
#define FAILSMART (0x01<<2)

// SMART STATUS returned FAILURE
#define FAILSTATUS (0x01<<3)

// Attributes found <= threshold with prefail=1
#define FAILATTR (0x01<<4)

// SMART STATUS returned GOOD but age attributes failed or prefail
// attributes have failed in the past
#define FAILAGE (0x01<<5)

// Device had Errors in the error log
#define FAILERR (0x01<<6)

// Device had Errors in the self-test log
#define FAILLOG (0x01<<7)

// Classes of SMART commands.  Here 'mandatory' means "Required by the
// ATA/ATAPI-5 Specification if the device implements the S.M.A.R.T.
// command set."  The 'mandatory' S.M.A.R.T.  commands are: (1)
// Enable/Disable Attribute Autosave, (2) Enable/Disable S.M.A.R.T.,
// and (3) S.M.A.R.T. Return Status.  All others are optional.
enum failure_type {
  OPTIONAL_CMD,
  MANDATORY_CMD,
};

// Globals to set failuretest() policy
extern bool failuretest_conservative;
extern unsigned char failuretest_permissive;

// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
void failuretest(failure_type type, int returnvalue);

// Globals to control printing
extern bool printing_is_switchable;
extern bool printing_is_off;

// Printing control functions
inline void print_on()
{
  if (printing_is_switchable)
    printing_is_off = false;
}
inline void print_off()
{
  if (printing_is_switchable)
    printing_is_off = true;
}

// The singleton global JSON object
#include "json.h"
extern json jglb;

#include "utility.h" // __attribute_format_printf()
// TODO: move this to a new include file?

// Version of pout() for items already included in JSON output
void jout(const char *fmt, ...)
  __attribute_format_printf(1, 2);
// Version of pout() for info/warning/error messages
void jinf(const char *fmt, ...)
  __attribute_format_printf(1, 2);
void jwrn(const char *fmt, ...)
__attribute_format_printf(1, 2);
void jerr(const char *fmt, ...)
__attribute_format_printf(1, 2);

#endif
