/*
 * knowndrives.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003 Philip Williams, Bruce Allen
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

const char *knowndrives_c_cvsid="$Id: knowndrives.c,v 1.31 2003/07/29 11:47:59 ballen4705 Exp $" ATACMDS_H_CVSID ATAPRINT_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

#define MODEL_STRING_LENGTH                         40
#define FIRMWARE_STRING_LENGTH                       8
#define TABLEPRINTWIDTH                             19

// See vendorattributeargs[] array in atacmds.c for definitions.
#define PRESET_9_MINUTES                   {   9,  1 }
#define PRESET_9_TEMP                      {   9,  2 }
#define PRESET_9_SECONDS                   {   9,  3 }
#define PRESET_9_HALFMINUTES               {   9,  4 }
#define PRESET_192_EMERGENCYRETRACTCYCLECT { 192,  1 }
#define PRESET_193_LOADUNLOAD              { 193,  1 }
#define PRESET_194_10XCELSIUS              { 194,  1 }
#define PRESET_194_UNKNOWN                 { 194,  2 }
#define PRESET_198_OFFLINESCANUNCSECTORCT  { 198,  1 }
#define PRESET_200_WRITEERRORCOUNT         { 200,  1 }
#define PRESET_201_DETECTEDTACOUNT         { 201,  1 }         
#define PRESET_220_TEMP                    { 220,  1 }

/* Arrays of preset vendor-specific attribute options for use in
 * knowndrives[]. */

// These two are common to several models.
const unsigned char vendoropts_9_minutes[][2] = {
  PRESET_9_MINUTES,
  {0,0}
};
const unsigned char vendoropts_9_seconds[][2] = {
  PRESET_9_SECONDS,
  {0,0}
};

const unsigned char vendoropts_Maxtor_4D080H4[][2] = {
  PRESET_9_MINUTES,
  PRESET_194_UNKNOWN,
  {0,0}
};

const unsigned char vendoropts_Fujitsu_MHS2020AT[][2] = {
  PRESET_9_SECONDS,
  PRESET_192_EMERGENCYRETRACTCYCLECT,
  PRESET_198_OFFLINESCANUNCSECTORCT,
  PRESET_200_WRITEERRORCOUNT,
  PRESET_201_DETECTEDTACOUNT,
  {0,0}
};

const unsigned char vendoropts_Samsung_SV4012H[][2] = {
  PRESET_9_HALFMINUTES,
  {0,0}
};

const unsigned char vendoropts_Samsung_SV1204H[][2] = {
  PRESET_9_HALFMINUTES,
  PRESET_194_10XCELSIUS,
  {0,0}
};

const unsigned char vendoropts_Hitachi_DK23EA[][2] = {
  PRESET_9_MINUTES,
  PRESET_193_LOADUNLOAD,
  {0,0}
};


/* Special-purpose functions for use in knowndrives[]. */
void specialpurpose_reverse_samsung(smartmonctrl *con)
{
  con->fixfirmwarebug = FIX_SAMSUNG;
}

/* Table of settings for known drives terminated by an element containing all
 * zeros.  The drivesettings structure is described in knowndrives.h.  Note
 * that lookupdrive() will search knowndrives[] from the start to end or
 * until it finds the first match, so the order in knowndrives[] is important
 * for distinct entries that could match the same drive. */

// Note that the table just below uses EXTENDED REGULAR EXPRESSIONS.
// A good on-line reference for these is:
// http://www.zeus.com/extra/docsystem/docroot/apps/web/docs/modules/access/regex.html

const drivesettings knowndrives[] = {
  { // IBM Deskstar 60GXP series
    "IC35L0[12346]0AVER07",
    ".*",
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/index.html#rel",
    NULL, NULL, NULL
  },
  { // IBM Deskstar 40GV & 75GXP series
    "(IBM-)?DTLA-30[57]0[123467][05]$",
    ".*",
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/",
    NULL, NULL, NULL
  },
  { // Fujitsu MPE3204AT
    "^FUJITSU MPE3204AT$",
    ".*",    // Tested on ED-03-04
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MPD3130AT
    "^FUJITSU MPD3130AT$",
    ".*",    // Tested on DD-04-47
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MHS2020AT
    "^FUJITSU MHS2020AT$",
    ".*",    // Tested on 8004
    NULL,
    vendoropts_Fujitsu_MHS2020AT,
    NULL, NULL
  },
  { // Samsung SV4012H (known firmware)
    "^SAMSUNG SV4012H$",
    "^RM100-08",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    "Fixes byte order in some SMART data (same as -F)"
  },
  { // Samsung SV4012H (all other firmware)
    "^SAMSUNG SV4012H$",
    ".*",
    "Contact developers; may need -F disabled",
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    "Fixes byte order in some SMART data (same as -F)"
  },
  { // Samsung SV1204H (known firmware)
    "^SAMSUNG SV1204H$",
    "^RK100-1[3-5]$",
    NULL,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    "Fixes byte order in some SMART data (same as -F)"
  },
  { //Samsung SV1204H (all other firmware)
    "^SAMSUNG SV1204H$",
    ".*",
    "Contact developers; may need -F disabled",
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    "Fixes byte order in some SMART data (same as -F)"
  },
  { //Samsung SP1604N, tested with FW TM100-23
    "^SAMSUNG SP1604N$",
    ".*",
    NULL,
    vendoropts_Samsung_SV4012H,
    NULL,NULL
  },
  { //SAMSUNG SV0322A with FW JK200-35
    "^SAMSUNG SV0322A$",
    ".*",
    NULL,
    NULL,
    NULL,
    NULL
  },
  { // Samsung ALL OTHER DRIVES
    "^SAMSUNG.*",
    ".*",
    "Contact developers; may need -F enabled.\n",
    NULL, NULL, NULL
  },
  { // Maxtor 6L080J4 and 4K080H4
    "^MAXTOR (6L080J4|4K080H4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Maxtor 4D080H4
    "^Maxtor (4D080H4|4G120J6)$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { // Maxtor 4R080J0
    "^Maxtor (4R080J0|4R080L0|6Y060L0|6Y160P0)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor 6Y120P0 (known firmware)
    "^Maxtor 6Y120P0$",
    "^YAR41VW0$",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor 6Y120P0 (any other firmware)
    "^Maxtor 6Y120P0$",
    ".*",
    "Contact developers; may need -v 9,minutes enabled.\n",
    NULL,
    NULL, NULL
  },
  { // HITACHI_DK23BA-20
    "^HITACHI_DK23BA-20$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // HITACHI_DK23EA-30
    "^HITACHI_DK23EA-30$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23EA,
    NULL, NULL
  },
  { // IBM GXP-180
    "^IC35L120AVV207-[01]$",
    ".*", 
    NULL, NULL, NULL, NULL 
  },
  {
    //  IBM Deskstar 120GXP  [Phil -- use for testing]
    "^IC35L060AVVA07-[01]$",
    ".*",
    NULL,
    NULL,
    NULL,
    NULL,
  },
  {
    // TOSHIBA MK6021GAS [Bruce -- use for testing on laptop]
    "^TOSHIBA MK6021GAS$",
    ".*",
    NULL,
    NULL,
    NULL,
    NULL,
  },
  /*------------------------------------------------------------
   *  End of table.  Do not add entries below this marker.
   *------------------------------------------------------------ */
  {NULL, NULL, NULL, NULL, NULL}
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
void showonepreset(const drivesettings *drivetable){
  
  const unsigned char (* presets)[2] = drivetable->vendoropts;
  int first_preset = 1;
  
  // Basic error check
  if (!drivetable || !drivetable->modelregexp){
    pout("Null known drive table pointer. Please report\n"
	 "this error to smartmontools developers.\n");
    return;
  }
  
  // print model and firmware regular expressions
  pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL REGEXP:", drivetable->modelregexp);
  pout("%-*s %s\n", TABLEPRINTWIDTH, "FIRMWARE REGEXP:", drivetable->firmwareregexp ?
       drivetable->firmwareregexp : "");
  
  // if there are any presets, then show them
  if (presets && (*presets)[0]) while (1) {
    char out[64];
    const int attr = (*presets)[0], val  = (*presets)[1];
    
    // if we are at the end of the attribute list, break out
    if (!attr)  
      break;
    
    ataPrintSmartAttribName(out, attr, val);
    // Use leading zeros instead of spaces so that everything lines up.
    out[0] = (out[0] == ' ') ? '0' : out[0];
    out[1] = (out[1] == ' ') ? '0' : out[1];
    pout("%-*s %s\n", TABLEPRINTWIDTH, first_preset ? "ATTRIBUTE OPTIONS:" : "", out);
    first_preset = 0;
    presets++;
  }
  else
    pout("%-*s %s\n", TABLEPRINTWIDTH, "ATTRIBUTE OPTIONS:", "None preset; no -v options are required.");

  
  // Is a special purpose function defined?  If so, describe it
  if (drivetable->specialpurpose){
    pout("%-*s ", TABLEPRINTWIDTH, "OTHER PRESETS:");
    pout("%s\n", drivetable->functiondesc ?
	 drivetable->functiondesc : "A special purpose function "
	 "is defined for this drive"); 
  }
  
  // Print any special warnings
  if (drivetable->warningmsg){
    pout("%-*s ", TABLEPRINTWIDTH, "WARNINGS:");
    pout("%s\n", drivetable->warningmsg);
  }
  
  return;
}

void showallpresets(void){
  int i;

  // loop over all entries in the knowndrives[] table, printing them
  // out in a nice format
  for (i=0; knowndrives[i].modelregexp; i++){
    showonepreset(&knowndrives[i]);
    pout("\n");
  }
  return;
}

// Shows the presets (if any) that are available for the given drive.
void showpresets(const struct hd_driveid *drive){
  int i;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];

  // get the drive's model/firmware strings
  formatdriveidstring(model, drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);
  
  // and search to see if they match values in the table
  if ((i = lookupdrive(model, firmware)) < 0) {
    // no matches found
    pout("No presets are defined for this drive.  Its identity strings:\n"
	 "MODEL:    %s\n"
	 "FIRMWARE: %s\n"
	 "do not match any of the known regular expressions.\n"
	 "Use -P showall to list all known regular expressions.\n",
	 model, firmware);
    return;
  }
  
  // We found a matching drive.  Print out all information about it.
  pout("Drive found in smartmontools Database.  Drive identity strings:\n"
       "%-*s %s\n"
       "%-*s %s\n"
       "match smartmontools Drive Database entry:\n",
       TABLEPRINTWIDTH, "MODEL:", model, TABLEPRINTWIDTH, "FIRMWARE:", firmware);
  showonepreset(&knowndrives[i]);
  return;
}

// Sets preset vendor attribute options in opts by finding the entry
// (if any) for the given drive in knowndrives[].  Values that have
// already been set in opts will not be changed.  Returns <0 if drive
// not recognized else index >=0 into drive database.
int applypresets(const struct hd_driveid *drive, unsigned char opts[256],
                  smartmonctrl *con) {
  int i;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];
  
  // get the drive's model/firmware strings
  formatdriveidstring(model, drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);

  // Look up the drive in knowndrives[] and check vendoropts is non-NULL.
  if ((i = lookupdrive(model, firmware)) >= 0 && knowndrives[i].vendoropts) {
    const unsigned char (* presets)[2];

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
      (*knowndrives[i].specialpurpose)(con);
  }
  
  // return <0 if drive wasn't recognized, or index>=0 into database
  // if it was
  return i;
}
