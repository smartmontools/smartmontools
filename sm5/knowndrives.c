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
#include <sys/types.h>
#include <regex.h>
#include "atacmds.h"
#include "ataprint.h"
#include "extern.h"
#include "knowndrives.h"
#include "utility.h"
#include "config.h"

const char *knowndrives_c_cvsid="$Id: knowndrives.c,v 1.74 2003/12/30 20:12:06 pjwilliams Exp $"
                                ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

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

extern long long bytes;

// to hold onto exit code for atexit routine
extern int exitstatus;

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

const unsigned char vendoropts_Fujitsu_MHR2040AT[][2] = {
  PRESET_9_SECONDS,
  PRESET_192_EMERGENCYRETRACTCYCLECT,
  PRESET_198_OFFLINESCANUNCSECTORCT,
  PRESET_200_WRITEERRORCOUNT,
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

const char same_as_minus_F[]="Fixes byte order in some SMART data (same as -F samsung)";
const char same_as_minus_F2[]="Fixes byte order in some SMART data (same as -F samsung2)";

const char may_need_minus_F_disabled[]="Contact developers at " PACKAGE_BUGREPORT "; may need -F samsung disabled";
const char may_need_minus_F2_disabled[]="Contact developers at " PACKAGE_BUGREPORT "; may need -F samsung2 disabled";

/* Special-purpose functions for use in knowndrives[]. */
void specialpurpose_reverse_samsung(smartmonctrl *con)
{
  con->fixfirmwarebug = FIX_SAMSUNG;
}
void specialpurpose_reverse_samsung2(smartmonctrl *con)
{
  con->fixfirmwarebug = FIX_SAMSUNG2;
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
  { // IBM Deskstar 40GV & 75GXP series (TXAOA5AA firmware)
    "(IBM-)?DTLA-30[57]0[123467][05]$",
    "^TXAOA5AA$",
    NULL, NULL, NULL, NULL
  },
  { // IBM Deskstar 40GV & 75GXP series (all other firmware)
    "(IBM-)?DTLA-30[57]0[123467][05]$",
    ".*",
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/",
    NULL, NULL, NULL
  },
  { // Fujitsu MPD and MPE series
    "^FUJITSU MP[DE]....A[HT]$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MHN2300AT
    "^FUJITSU MHN2300AT$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MHR2040AT
    "^FUJITSU MHR2040AT$",
    ".*",    // Tested on 40BA
    NULL,
    vendoropts_Fujitsu_MHR2040AT,
    NULL, NULL
  },
  { // Fujitsu MHS2020AT
    "^FUJITSU MHS2020AT$",
    ".*",    // Tested on 8004
    NULL,
    vendoropts_Fujitsu_MHS2020AT,
    NULL, NULL
  },
  { // Fujitsu MHL2300AT, MHM2200AT, MHM2100AT, MHM2150AT
    "^FUJITSU MH(L230|M2(20|10|15))0AT$",
    ".*",
    "This drive's firmware has a harmless Drive Identity Structure\n"
      "checksum error bug.",
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Samsung SV4012H (known firmware)
    "^SAMSUNG SV4012H$",
    "^RM100-08$",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { // SAMSUNG SP1213N (TL100-23 firmware)
    "^SAMSUNG SP1213N$",
    "^TL100-23$",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { // SAMSUNG SP0802N (TK100-23 firmware)
    "^SAMSUNG SP0802N$",
    "^TK100-23$",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { // Any other Samsung disk with *-23 firmware
    "^SAMSUNG .*$",
    ".*-23$",
    may_need_minus_F2_disabled,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { // Samsung SV4012H (all other firmware)
    "^SAMSUNG SV4012H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { // Samsung SV1204H (known firmware)
    "^SAMSUNG SV1204H$",
    "^RK100-1[3-5]$",
    NULL,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { //Samsung SV1204H (all other firmware)
    "^SAMSUNG SV1204H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { // Samsung SV0412H (known firmware)
    "^SAMSUNG SV0412H$",
    "^SK100-01$",
    NULL,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { // Samsung SV0412H (all other firmware)
    "^SAMSUNG SV0412H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
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
    "Contact developers at " PACKAGE_BUGREPORT "; may need -F samsung[2] enabled.\n",
    NULL, NULL, NULL
  },
  { // Maxtor DiamondMax Plus D740X family
    "^MAXTOR 6L0(20[JL]1|40[JL]2|60[JL]3|80[JL]4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Maxtor 4K080H4
    "^MAXTOR 4K080H4$",
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
  { // Maxtor 4R080J0 and 4R080L0
    "^Maxtor 4R080[JL]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor 4W040H3
    "^Maxtor 4W040H3$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax VL 30 family
    "^Maxtor (33073U4|32049U3|31536U2|30768U1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax Plus 60 family
    "^Maxtor 5T0(60H6|40H4|30H3|20H2|10H1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax 80 family
    "^Maxtor (98196H8|96147H6)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax Plus 8 family
    "^Maxtor 6E0[234]0L0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax Plus 9 family
    "^Maxtor 6Y((060|080|120|160)L0|(080|120|160|200)P0|(060|080|120|160|200)M0)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
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
  { // IBM Travelstar 25GS, 18GT, and 12GN family
    "^IBM-DARA-2(25|18|15|12|09|06)000$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { // IBM Travelstar 32GH, 30GT, and 20GN family
    "^IBM-DJSA-2(32|30|20|10|05)$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { // IBM/Hitachi Travelstar 60GH and 40GN family
    "^IC25(T060ATC[SX]05|N0[4321]0ATC[SX]04)-.$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { // IBM GXP-180
    "^IC35L120AVV207-[01]$",
    ".*", 
    NULL, NULL, NULL, NULL 
  },
  { // IBM Deskstar 120GXP 60GB [Phil -- use for testing]
    "^IC35L060AVVA07-[01]$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // IBM Deskstar 120GXP 40GB
    "^IC35L040AVVN07-0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK4025GAS
    "^TOSHIBA MK4025GAS$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK6021GAS [Bruce -- use for testing on laptop]
    "^TOSHIBA MK6021GAS$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // TOSHIBA MK4019GAXB
    "^TOSHIBA MK4019GAXB$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda U Series 20410
    "^ST320410A$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Seagate U Series 5 20413
    "^ST320413A$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Seagate Barracuda ATA II family
    "^ST3(3063|2042|1532|1021)0A$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Seagate Barracuda ATA IV family
    "^ST3(20011|40016|60021|80021)A$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Seagate Barracuda ATA V family
    "^ST3(12002(3A|4A|9A|3AS)|800(23A|15A|23AS)|60(015A|210A)|40017A)$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Seagate Barracuda 7200.7 and 7200.7 Plus family
    "^ST3(200822AS?|16002[13]AS?|12002[26]AS?|8001[13]AS?|60014A|40014AS?)$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Western Digital Protege WD400EB
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD400EB-.*$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Western Digital Caviar family
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(2|3|4|6|8|10|12|16|18|20|25)00BB-.*$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Western Digital Caviar SE family
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(4|6|8|10|12|16|18|20|25)00JB-.*$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Western Digital Caviar AC38400
    "^WDC AC38400L$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // Western Digital Caviar AC23200L
    "^WDC AC23200L$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // QUANTUM FIREBALLlct20 20
    "^QUANTUM FIREBALLlct20 20$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  { // QUANTUM FIREBALL CX10.2A
    "^QUANTUM FIREBALL CX10.2A$",
    ".*",
    NULL, NULL, NULL, NULL,
  },
  /*------------------------------------------------------------
   *  End of table.  Do not add entries below this marker.
   *------------------------------------------------------------ */
  {NULL, NULL, NULL, NULL, NULL, NULL}
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
         "this error to smartmontools developers at " PACKAGE_BUGREPORT ".\n");
    return;
  }
  
  // print model and firmware regular expressions
  pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL REGEXP:", drivetable->modelregexp);
  pout("%-*s %s\n", TABLEPRINTWIDTH, "FIRMWARE REGEXP:", drivetable->firmwareregexp ?
       drivetable->firmwareregexp : "");
  
  // if there are any presets, then show them
  if (presets && (*presets)[0]) while (1) {
    char out[256];
    const int attr = (*presets)[0], val  = (*presets)[1];
    unsigned char fakearray[MAX_ATTRIBUTE_NUM];

    // if we are at the end of the attribute list, break out
    if (!attr)  
      break;
    
    // This is a hack. ataPrintSmartAttribName() needs a pointer to an
    // "array" to dereference, so we provide such a pointer.
    fakearray[attr]=val;
    ataPrintSmartAttribName(out, attr, fakearray);

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
  pout("For information about adding a drive to the database see the FAQ on the\n");
  pout("smartmontools home page: " PACKAGE_HOMEPAGE "\n");
  return;
}

// Shows the presets (if any) that are available for the given drive.
void showpresets(const struct ata_identify_device *drive){
  int i;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];

  // get the drive's model/firmware strings
  formatdriveidstring(model, (char *)drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, (char *)drive->fw_rev, FIRMWARE_STRING_LENGTH);
  
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
int applypresets(const struct ata_identify_device *drive, unsigned char **optsptr,
                  smartmonctrl *con) {
  int i;
  unsigned char *opts;
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];
  
  if (*optsptr==NULL)
    bytes+=MAX_ATTRIBUTE_NUM;
  
  if (*optsptr==NULL && !(*optsptr=(unsigned char *)calloc(MAX_ATTRIBUTE_NUM,1))){
    pout("Unable to allocate memory in applypresets()");
    bytes-=MAX_ATTRIBUTE_NUM;
    EXIT(1);
  }
  
  opts=*optsptr;

  // get the drive's model/firmware strings
  formatdriveidstring(model, (char *)drive->model, MODEL_STRING_LENGTH);
  formatdriveidstring(firmware, (char *)drive->fw_rev, FIRMWARE_STRING_LENGTH);

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
