/*
 * knowndrives.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003 Bruce Allen, Philip Williams 
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
 */

#include <stdio.h>
#include <regex.h>
#include "atacmds.h"
#include "ataprint.h"
#include "knowndrives.h"
#include "utility.h"

#define MODEL_STRING_LENGTH       40
#define FIRMWARE_STRING_LENGTH     8

const char *knowndrives_c_cvsid="$Id: knowndrives.cpp,v 1.3 2003/04/14 18:48:35 pjwilliams Exp $" ATACMDS_H_CVSID ATAPRINT_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

/* Table of settings for known drives terminated by an element containing all
 * zeros.  The drivesettings structure is described in knowndrives.h */
const drivesettings knowndrives[] = {
  /*------------------------------------------------------------
   *  IBM Deskstar 60GXP series   
   *------------------------------------------------------------ */
  {
    "IC35L0[12346]0AVER07",
    NULL,
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/index.html#rel",
    NULL,
    NULL
  },
  /*------------------------------------------------------------
   *  IBM Deskstar 40GV & 75GXP series
   *------------------------------------------------------------ */
  {
    "(IBM-)?DTLA-30[57]0[123467][05]",
    NULL,
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/",
    NULL,
    NULL
  },
  /*------------------------------------------------------------
   *  Phil's IBM Deskstar 120GXP (FOR TESTING)
   *------------------------------------------------------------ */
/*
  {
     "^IC35L060AVVA07-0$",
     NULL,
     "Hey, you've got an IBM Deskstar 120GXP!\n",
     NULL,
     NULL,
  },
*/
  /*------------------------------------------------------------
   *  End of table.  Please add entries above this marker.
   *------------------------------------------------------------ */
  {0, 0, 0, 0, 0}
};

// Searches knowndrives[] for a drive with the given model number and firmware
// string.  If either the drive's model or firmware strings are not set by the
// manufacturer then values of NULL may be used.  Returns the index of the
// first match in knowndrives[] or -1 if no match if found.
int lookupdrive(const char *model, const char *firmware)
{
  regex_t regex;
  int i, index;
  const char *empty = "";

  model = model ? model : empty;
  firmware = firmware ? firmware : empty;

  for (i = 0, index = -1; index == -1 && knowndrives[i].modelregexp; i++) {
    // Attempt to compile regular expression.
    if (compileregex(&regex, knowndrives[i].modelregexp, REG_EXTENDED))
      goto CONTINUE;

    // Check whether model matches the regular expression in knowndrives[i].
    if (!regexec(&regex, model, 0, NULL, 0)) {
      // model matches, now check firmware.
      if (!knowndrives[i].firmwareregexp)
        // The firmware regular expression in knowndrives[i] is NULL, which is
        // considered a match.
        index = i;
      else {
        // Compare firmware against the regular expression in knowndrives[i].
        regfree(&regex);  // Recycle regex.
        if (compileregex(&regex, knowndrives[i].firmwareregexp, REG_EXTENDED))
          goto CONTINUE;
        if (!regexec(&regex, firmware, 0, NULL, 0))
          index = i;
      }
    }
  CONTINUE:
    regfree(&regex);
  }

  return index;
}

// Shows all presets for drives in knowndrives[].
void showallpresets(void)
{
  int i;

  for (i = 0; knowndrives[i].modelregexp; i++) {
    const int (* presets)[2] = knowndrives[i].vendoropts;
    int first_preset = 1;
    int width = 11;

    if (!presets)
      continue;

    pout("%-*s %s\n", width, "MODEL", knowndrives[i].modelregexp);
    pout("%-*s %s\n", width, "FIRMWARE", knowndrives[i].firmwareregexp ?
                                           knowndrives[i].firmwareregexp : "");

    while (1) {
      char out[64];
      const int attr = (*presets)[0], val  = (*presets)[1];

      if (!attr)  
        break;

      ataPrintSmartAttribName(out, attr, val);
      // Use leading zeros instead of spaces so that everything lines up.
      out[0] = (out[0] == ' ') ? '0' : out[0];
      out[1] = (out[1] == ' ') ? '0' : out[1];
      pout("%-*s %s\n", width, first_preset ? "PRESETS" : "", out);
      first_preset = 0;
      presets++;
    }
    if (knowndrives[i].specialpurpose)
      pout("%-*s There is a special-purpose function defined for this drive\n",           width, "");
    pout("\n");
  }
}

// Shows the presets (if any) that are available for the given drive.
void showpresets(const struct hd_driveid *drive)
{
  int i;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1], out[64];

  formatdriveidstring(model, drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);

  if ((i = lookupdrive(model, firmware)) >= 0 && knowndrives[i].vendoropts) {
    const int (* presets)[2] = knowndrives[i].vendoropts;
    while (1) {
      const int attr = (*presets)[0];
      const int val  = (*presets)[1];

      if (!attr)  
        break;

      ataPrintSmartAttribName(out, attr, val);
      pout("%s\n", out);
      presets++;
    }
  } else {
    pout("No presets are defined for this drive\n");
  }
}

// Sets preset vendor attribute options in opts by finding the entry (if any)
// for the given drive in knowndrives[].  Values that have already been set in
// opts will not be changed.
void applypresets(const struct hd_driveid *drive, unsigned char opts[256])
{
  int i;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];

  formatdriveidstring(model, drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);

  // Look up the drive in knowndrives[] and check vendoropts is non-NULL.
  if ((i = lookupdrive(model, firmware)) >= 0 && knowndrives[i].vendoropts) {
    const int (* presets)[2];

    // For each attribute in list of attribute/val pairs...
    presets = knowndrives[i].vendoropts;
    while (1) {
      const int attr = (*presets)[0];
      const int val  = (*presets)[1];

      if (!attr)  
        break;

      // ... set attribute if user hasn't already done so.
      if (!opts[attr])
        opts[attr] = val;
      presets++;
    }

    // If a function is defined for this drive then call it.
    if (knowndrives[i].specialpurpose)
      (*knowndrives[i].specialpurpose)();
  }
}
