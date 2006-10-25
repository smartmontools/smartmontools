/*
 * knowndrives.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-6 Philip Williams, Bruce Allen
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

#include "config.h"
#include "int64.h"
#include <stdio.h>
#include "atacmds.h"
#include "ataprint.h"
#include "extern.h"
#include "knowndrives.h"
#include "utility.h" // includes <regex.h>

const char *knowndrives_c_cvsid="$Id: knowndrives.cpp,v 1.155 2006/10/25 22:18:43 pjwilliams Exp $"
ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

#define MODEL_STRING_LENGTH                         40
#define FIRMWARE_STRING_LENGTH                       8
#define TABLEPRINTWIDTH                             19

// See vendorattributeargs[] array in atacmds.cpp for definitions.
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

extern int64_t bytes;

// to hold onto exit code for atexit routine
extern int exitstatus;

// These three are common to several models.
const unsigned char vendoropts_9_minutes[][2] = {
  PRESET_9_MINUTES,
  {0,0}
};
const unsigned char vendoropts_9_halfminutes[][2] = {
  PRESET_9_HALFMINUTES,
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

const unsigned char vendoropts_Hitachi_DK23XX[][2] = {
  PRESET_9_MINUTES,
  PRESET_193_LOADUNLOAD,
  {0,0}
};

const char same_as_minus_F[]="Fixes byte order in some SMART data (same as -F samsung)";
const char same_as_minus_F2[]="Fixes byte order in some SMART data (same as -F samsung2)";

const char may_need_minus_F_disabled[] ="May need -F samsung disabled; see manual for details.";
const char may_need_minus_F2_disabled[]="May need -F samsung2 disabled; see manual for details.";
const char may_need_minus_F2_enabled[] ="May need -F samsung2 enabled; see manual for details.";
const char may_need_minus_F_enabled[]  ="May need -F samsung or -F samsung2 enabled; see manual for details.";

/* Special-purpose functions for use in knowndrives[]. */
void specialpurpose_reverse_samsung(smartmonctrl *con)
{
  if (con->fixfirmwarebug==FIX_NOTSPECIFIED)
    con->fixfirmwarebug = FIX_SAMSUNG;
}
void specialpurpose_reverse_samsung2(smartmonctrl *con)
{
  if (con->fixfirmwarebug==FIX_NOTSPECIFIED)
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
  { "IBM Deskstar 60GXP series",  // ER60A46A firmware
    "(IBM-|Hitachi )?IC35L0[12346]0AVER07",
    "^ER60A46A$",
    NULL, NULL, NULL, NULL
  },
  { "IBM Deskstar 60GXP series",  // All other firmware
    "(IBM-|Hitachi )?IC35L0[12346]0AVER07",
    ".*",
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
    "Please see http://www.geocities.com/dtla_update/index.html#rel and\n"
    "http://www-3.ibm.com/pc/support/site.wss/document.do?lndocid=MIGR-42215 or\n"
    "http://www-1.ibm.com/support/docview.wss?uid=psg1MIGR-42215",
    NULL, NULL, NULL
  },
  { "IBM Deskstar 40GV & 75GXP series (A5AA/A6AA firmware)",
    "(IBM-)?DTLA-30[57]0[123467][05]",
    "^T[WX][123468AG][OF]A[56]AA$",
    NULL, NULL, NULL, NULL
  },
  { "IBM Deskstar 40GV & 75GXP series (all other firmware)",
    "(IBM-)?DTLA-30[57]0[123467][05]",
    ".*",
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
    "Please see http://www.geocities.com/dtla_update/ and\n"
    "http://www-3.ibm.com/pc/support/site.wss/document.do?lndocid=MIGR-42215 or\n"
    "http://www-1.ibm.com/support/docview.wss?uid=psg1MIGR-42215",
    NULL, NULL, NULL
  },
  { NULL, // ExcelStor J240, J340, J360, J680, and J880
    "^ExcelStor Technology J(24|34|36|68|88)0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // Fujitsu M1623TAU
    "^FUJITSU M1623TAU$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHG series",
    "^FUJITSU MHG2...ATU?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHH series",
    "^FUJITSU MHH2...ATU?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHJ series",
    "^FUJITSU MHJ2...ATU?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHK series",
    "^FUJITSU MHK2...ATU?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { NULL,  // Fujitsu MHL2300AT
    "^FUJITSU MHL2300AT$",
    ".*",
    "This drive's firmware has a harmless Drive Identity Structure\n"
      "checksum error bug.",
    vendoropts_9_seconds,
    NULL, NULL
  },
  { NULL,  // MHM2200AT, MHM2150AT, MHM2100AT, MHM2060AT
    "^FUJITSU MHM2(20|15|10|06)0AT$",
    ".*",
    "This drive's firmware has a harmless Drive Identity Structure\n"
      "checksum error bug.",
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHN series",
    "^FUJITSU MHN2...AT$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { NULL, // Fujitsu MHR2020AT
    "^FUJITSU MHR2020AT$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { NULL, // Fujitsu MHR2040AT
    "^FUJITSU MHR2040AT$",
    ".*",    // Tested on 40BA
    NULL,
    vendoropts_Fujitsu_MHR2040AT,
    NULL, NULL
  },
  { "Fujitsu MHSxxxxAT family",
    "^FUJITSU MHS20[6432]0AT(  .)?$",
    ".*",
    NULL,
    vendoropts_Fujitsu_MHS2020AT,
    NULL, NULL
  },
  { "Fujitsu MHT series",
    "^FUJITSU MHT2...(AH|AS|AT|BH)U?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHU series",
    "^FUJITSU MHU2...ATU?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MHV series",
    "^FUJITSU MHV2...(AH|AS|AT|BH|BS|BT)",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { "Fujitsu MPA..MPG series",
    "^FUJITSU MP[A-G]3...A[HTEV]U?",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { NULL, // Samsung SV4012H (known firmware)
    "^SAMSUNG SV4012H$",
    "^RM100-08$",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // Samsung SV4012H (all other firmware)
    "^SAMSUNG SV4012H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // Samsung SV0412H (known firmware)
    "^SAMSUNG SV0412H$",
    "^SK100-01$",
    NULL,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // Samsung SV0412H (all other firmware)
    "^SAMSUNG SV0412H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // Samsung SV1204H (known firmware)
    "^SAMSUNG SV1204H$",
    "^RK100-1[3-5]$",
    NULL,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // Samsung SV1204H (all other firmware)
    "^SAMSUNG SV1204H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV1204H,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // SAMSUNG SV0322A tested with FW JK200-35
    "^SAMSUNG SV0322A$",
    ".*",
    NULL,
    NULL,
    NULL,
    NULL
  },
  { NULL, // SAMSUNG SP40A2H with RR100-07 firmware
    "^SAMSUNG SP40A2H$",
    "^RR100-07$",
    NULL,
    vendoropts_9_halfminutes,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { NULL, // SAMSUNG SP8004H with QW100-61 firmware
    "^SAMSUNG SP8004H$",
    "^QW100-61$",
    NULL,
    vendoropts_9_halfminutes,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { "SAMSUNG SpinPoint T133 series", // tested with HD300LJ/ZT100-12, HD400LJ/ZZ100-14, HD401LJ/ZZ100-15
    "^SAMSUNG HD(250KD|(30[01]|320|40[01])L[DJ])$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "SAMSUNG SpinPoint P120 series", // tested with SP2504C/VT100-33
    "^SAMSUNG SP(16[01]3|2[05][01]4)[CN]$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "SAMSUNG SpinPoint P80 SD series", // tested with HD160JJ/ZM100-33
    "^SAMSUNG HD(080H|120I|160J)J$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "SAMSUNG SpinPoint P80 series", // firmware *-26 or later, tested with SP1614C/SW100-34
    "^SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]$",
    ".*-(2[6789]|3[0-9])$",
    NULL,
    vendoropts_9_halfminutes,
    NULL, NULL
  },
  { 
    NULL, // Any other Samsung disk with *-23 *-24 firmware
    // SAMSUNG SP1213N (TL100-23 firmware)
    // SAMSUNG SP0802N (TK100-23 firmware)
    // Samsung SP1604N, tested with FW TM100-23 and TM100-24
    "^SAMSUNG .*$",
    ".*-2[34]$",
    NULL,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { NULL, // All Samsung drives with '.*-25' firmware
    "^SAMSUNG.*",
    ".*-25$",
    may_need_minus_F2_disabled,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { NULL, // All Samsung drives with '.*-26 or later (currently to -39)' firmware
    "^SAMSUNG.*",
    ".*-(2[6789]|3[0-9])$",
    NULL,
    vendoropts_Samsung_SV4012H,
    NULL,
    NULL
  },
  { NULL, // Samsung ALL OTHER DRIVES
    "^SAMSUNG.*",
    ".*",
    may_need_minus_F_enabled,
    NULL, NULL, NULL
  },
  { "Maxtor Fireball 541DX family",
    "^Maxtor 2B0(0[468]|1[05]|20)H1$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { "Maxtor Fireball 3 family",
    "^Maxtor 2F0[234]0[JL]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 2160 Ultra ATA family",
    "^Maxtor 8(2160D2|3228D3|3240D3|4320D4|6480D6|8400D8|8455D8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 2880 Ultra ATA family",
    "^Maxtor 9(0510D4|0576D4|0648D5|0720D5|0840D6|0845D6|0864D6|1008D7|1080D8|1152D8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 3400 Ultra ATA family",
    "^Maxtor 9(1(360|350|202)D8|1190D7|10[12]0D6|0840D5|06[48]0D4|0510D3|1(350|202)E8|1010E6|0840E5|0640E4)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax D540X-4G family",
    "^Maxtor 4G(120J6|160J[68])$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { "Maxtor DiamondMax D540X-4K family",
    "^MAXTOR 4K(020H1|040H2|060H3|080H4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Maxtor DiamondMax Plus D740X family",
    "^MAXTOR 6L0(20[JL]1|40[JL]2|60[JL]3|80[JL]4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Maxtor DiamondMax Plus 5120 Ultra ATA 33 family",
    "^Maxtor 9(0512D2|0680D3|0750D3|0913D4|1024D4|1360D6|1536D6|1792D7|2048D8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax Plus 6800 Ultra ATA 66 family",
    "^Maxtor 9(2732U8|2390U7|2049U6|1707U5|1366U4|1024U3|0845U3|0683U2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax D540X-4D",
    "^Maxtor 4D0(20H1|40H2|60H3|80H4)$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { "Maxtor DiamondMax 16 family",
    "^Maxtor 4(R0[68]0[JL]0|R1[26]0L0|A160J0|R120L4)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 4320 Ultra ATA family",
    "^Maxtor (91728D8|91512D7|91303D6|91080D5|90845D4|90645D3|90648D[34]|90432D2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 17 VL family",
    "^Maxtor 9(0431U1|0641U2|0871U2|1301U3|1741U4)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 20 VL family",
    "^Maxtor (94091U8|93071U6|92561U5|92041U4|91731U4|91531U3|91361U3|91021U2|90841U2|90651U2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax VL 30 family",
    "^Maxtor (33073U4|32049U3|31536U2|30768U1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 36 family",
    "^Maxtor (93652U8|92739U6|91826U4|91369U3|90913U2|90845U2|90435U1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 40 ATA 66 series",
    "^Maxtor 9(0684U2|1024U2|1362U3|1536U3|2049U4|2562U5|3073U6|4098U8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax Plus 40 series (Ultra ATA 66 and Ultra ATA 100)",
    "^Maxtor (54098[UH]8|53073[UH]6|52732[UH]6|52049[UH]4|51536[UH]3|51369[UH]3|51024[UH]2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 40 VL Ultra ATA 100 series",
    "^Maxtor 3(1024H1|1535H2|2049H2|3073H3|4098H4)( B)?$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax Plus 45 Ulta ATA 100 family",
    "^Maxtor 5(4610H6|4098H6|3073H4|2049H3|1536H2|1369H2|1023H2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 60 ATA 66 family",
    "^Maxtor 9(1023U2|1536U2|2049U3|2305U3|3073U4|4610U6|6147U8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 60 ATA 100 family",
    "^Maxtor 9(1023H2|1536H2|2049H3|2305H3|3073H4|4610H6|6147H8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax Plus 60 family",
    "^Maxtor 5T0(60H6|40H4|30H3|20H2|10H1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 80 family",
    "^Maxtor (98196H8|96147H6)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 536DX family",
    "^Maxtor 4W(100H6|080H6|060H4|040H3|030H2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax Plus 8 family",
    "^Maxtor 6(E0[234]|K04)0L0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 10 family (ATA/133 and SATA/150)",
    "^Maxtor 6(B(30|25|20|16|12|08)0[MPRS]|L(080[MLP]|(100|120)[MP]|160[MP]|200[MPRS]|250[RS]|300[RS]))0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 10 family (SATA/300)",
    "^Maxtor 6V(080E|160E|200E|250F|300F|320F)0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Maxtor DiamondMax Plus 9 family",
    "^Maxtor 6Y((060|080|120|160)L0|(060|080|120|160|200|250)P0|(060|080|120|160|200|250)M0)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor DiamondMax 11 family",
    "^Maxtor 6H[45]00[FR]0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Maxtor MaXLine Plus II",
    "^Maxtor 7Y250[PM]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor MaXLine II family",
    "^Maxtor [45]A(25|30|32)0[JN]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor MaXLine III family (ATA/133 and SATA/150)",
    "^Maxtor 7L(25|30)0[SR]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { "Maxtor MaXLine III family (SATA/300)",
    "^Maxtor 7V(25|30)0F0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Maxtor MaXLine Pro 500 family",  // There is also a 7H500R0 model, but I
    "^Maxtor 7H500F0$",               // haven't added it because I suspect
    ".*",                             // it might need vendoropts_9_minutes
    NULL, NULL, NULL, NULL            // and nobody has submitted a report yet
  },
  { NULL, // HITACHI_DK14FA-20B
    "^HITACHI_DK14FA-20B$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23XX,
    NULL, NULL
  },
  { "HITACHI Travelstar DK23XX/DK23XXB series",
    "^HITACHI_DK23..-..B?$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23XX,
    NULL, NULL
  },
  { "Hitachi Endurastar J4K20/N4K20 (formerly DK23FA-20J)",
    "^(HITACHI_DK23FA-20J|HTA422020F9AT[JN]0)$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23XX,
    NULL, NULL
  },
  { "IBM Deskstar 14GXP and 16GP series",
    "^IBM-DTTA-3(7101|7129|7144|5032|5043|5064|5084|5101|5129|5168)0$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Deskstar 25GP and 22GXP family",
    "^IBM-DJNA-3(5(101|152|203|250)|7(091|135|180|220))0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "IBM Travelstar 4GT family",
    "^IBM-DTCA-2(324|409)0$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Travelstar 25GS, 18GT, and 12GN family",
    "^IBM-DARA-2(25|18|15|12|09|06)000$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Travelstar 48GH, 30GN, and 15GN family",
    "^(IBM-|Hitachi )?IC25(T048ATDA05|N0(30|20|15|12|10|07|06|05)ATDA04)-.$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Travelstar 32GH, 30GT, and 20GN family",
    "^IBM-DJSA-2(32|30|20|10|05)$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Travelstar 4GN family",
    "^IBM-DKLA-2(216|324|432)0$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM Deskstar 37GP and 34GXP family",
    "^IBM-DPTA-3(5(375|300|225|150)|7(342|273|205|136))0$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM/Hitachi Travelstar 60GH and 40GN family",
    "^(IBM-|Hitachi )?IC25(T060ATC[SX]05|N0[4321]0ATC[SX]04)-.$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM/Hitachi Travelstar 40GNX family",
    "^(IBM-|Hitachi )?IC25N0[42]0ATC[SX]05-.$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar 80GN family",
    "^(Hitachi )?IC25N0[23468]0ATMR04-.$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Travelstar 5K80 family",
    "^HTS5480[8642]0M9AT00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar 5K100 series",
    "^HTS5410[1864]0G9(AT|SA)00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar E5K100 series",
    "^HTE541040G9(AT|SA)00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar 7K60",
    "^HTS726060M9AT00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar 7K100",
    "^HTS7210[168]0G9(AT|SA)00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar E7K100",
    "^HTE7210[168]0G9(AT|SA)00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "Hitachi Travelstar E7K60 family",
    "^HTE7260[46]0M9AT00$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { "IBM/Hitachi Deskstar 120GXP family",
    "^(IBM-)?IC35L((020|040|060|080|120)AVVA|0[24]0AVVN)07-[01]$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "IBM/Hitachi Deskstar GXP-180 family",
    "^(IBM-)?IC35L(030|060|090|120|180)AVV207-[01]$",
    ".*", 
    NULL, NULL, NULL, NULL 
  },
  { "IBM Travelstar 14GS",
    "^IBM-DCYA-214000$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "IBM Travelstar 4LP",
    "^IBM-DTNA-2(180|216)0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Deskstar 7K80 series",
    "^(Hitachi )?HDS7280([48]0PLAT20|(40)?PLA320|80PLA380)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Deskstar 7K250 series",
    "^(Hitachi )?HDS7225((40|80|12|16)VLAT20|(12|16|25)VLAT80|(80|12|16|25)VLSA80)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Deskstar T7K250 series",
    "^(Hitachi )?HDT7225((25|20|16)DLA(T80|380))$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Deskstar 7K400 series",
    "^(Hitachi )?HDS724040KL(AT|SA)80$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Hitachi Deskstar 7K500 series",
    "^(Hitachi )?HDS725050KLA(360|T80)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Toshiba 2.5\" HDD series (30-60 GB)",
    "^TOSHIBA MK((6034|4032)GSX|(6034|4032)GAX|(6026|4026|4019|3019)GAXB?|(6025|6021|4025|4021|4018|3021|3018)GAS|(4036|3029)GACE?|(4018|3017)GAP)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Toshiba 2.5\" HDD series (80 GB and above)",
    "^TOSHIBA MK(80(25GAS|26GAX|32GAX|32GSX)|10(31GAS|32GAX)|12(33GAS|34G[AS]X)|2035GSS)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // TOSHIBA MK6022GAX
    "^TOSHIBA MK6022GAX$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // TOSHIBA MK6409MAV
    "^TOSHIBA MK6409MAV$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // TOS MK3019GAXB SUN30G
    "^TOS MK3019GAXB SUN30G$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // TOSHIBA MK2016GAP, MK2017GAP, MK2018GAP, MK2018GAS, MK2023GAS
    "^TOSHIBA MK20(1[678]GAP|(18|23)GAS)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Momentus family",
    "^ST9(20|28|40|48)11A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Momentus 42 family",
    "^ST9(2014|3015|4019)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Momentus 4200.2 Series",
    "^ST9(100822|808210|60821|50212|402113|30219)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Momentus 5400.2 series",
    "^ST9(9808211|960822|808211|408114|308110|120821|10082[34]|98823|96812|94813|93811|60822)AS?$", 
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Momentus 7200.1 series",
    "^ST9(10021|80825|6023|4015)AS?$", 
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Medalist 2110, 3221, 4321, 6531, and 8641",
    "^ST3(2110|3221|4321|6531|8641)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U Series X family",
    "^ST3(10014A(CE)?|20014A)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U7 family",
    "^ST3(30012|40012|60012|80022|120020)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U Series 6 family",
    "^ST3(8002|6002|4081|3061|2041)0A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U Series 5 family",
    "^ST3(40823|30621|20413|15311|10211)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U4 family",
    "^ST3(2112|4311|6421|8421)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U8 family",
    "^ST3(8410|4313|17221|13021)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate U10 family",
    "^ST3(20423|15323|10212)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda ATA II family",
    "^ST3(3063|2042|1532|1021)0A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda ATA III family",
    "^ST3(40824|30620|20414|15310|10215)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda ATA IV family",
    "^ST3(20011|30011|40016|60021|80021)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda ATA V family",
    "^ST3(12002(3A|4A|9A|3AS)|800(23A|15A|23AS)|60(015A|210A)|40017A)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda 5400.1",
    "^ST340015A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda 7200.7 and 7200.7 Plus family",
    "^ST3(200021A|200822AS?|16002[13]AS?|12002[26]AS?|1[26]082[78]AS|8001[13]AS?|80817AS|60014A|40111AS|40014AS?)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda 7200.8 family",
    "^ST3(400[68]32|300[68]31|250[68]23|200826)AS?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda 7200.9 family",
    "^ST3(500[68]41|400[68]33|300[68]22|250[68]24|250[68]24|200827|160[28]12|120814|120[28]13|80[28]110|402111)AS?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Barracuda 7200.10 family",
    "^ST3(750[68]4|500[68]3|400[68]2|320[68]2|300[68]2|250[68]2|20082)0AS?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Medalist 17240, 13030, 10231, 8420, and 4310",
    "^ST3(17240|13030|10231|8420|4310)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate Medalist 17242, 13032, 10232, 8422, and 4312",
    "^ST3(1724|1303|1023|842|431)2A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Seagate NL35 family",
    "^ST3(250623|250823|400632|400832|250824|250624|400633|400833|500641|500841)NS$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Protege",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD([2468]00E|1[26]00A)B-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar family",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD(2|3|4|6|8|10|12|16|18|20|25)00BB-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar WDxxxAB series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD(3|4|6)00AB-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar WDxxxAA series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD...?AA(-.*)?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar WDxxxBA series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD...BA$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // Western Digital Caviar AC12500, AC14300, AC23200, AC24300, AC25100,
          // AC36400, AC38400
    "^WDC AC(125|143|232|243|251|364|384)00.?",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar Serial ATA family",
    "^WDC WD(4|8|20|32)00BD-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar SE family",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "^WDC WD((4|6|8|10|12|16|18|20|25|30|32)00JB|(12|20|25)00PB)-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar SE (Serial ATA) family",
    "^WDC WD((4|8|12|16|20|25|32)00JD|(12|16|20|25|30|32)00JS|1600AAJS)-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar SE16 family",
    "^WDC WD((25|32|40|50)00KS|4000KD)-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Caviar RE Serial ATA series",
    "^WDC WD((12|16|25|32)00SD|2500YD|4000Y[RS]|5000YS)-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Raptor family",
    "^WDC WD((360|740|800)GD|(360|740|1500)ADFD)-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Western Digital Scorpio family",
    "^WDC WD((12|10|8|6|4)00(UE|VE|BEAS|BEVS))-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL,  // QUANTUM BIGFOOT TS10.0A
    "^QUANTUM BIGFOOT TS10.0A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALLlct15 20 and QUANTUM FIREBALLlct15 30
    "^QUANTUM FIREBALLlct15 [123]0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "QUANTUM FIREBALLlct20 series",
    "^QUANTUM FIREBALLlct20 [234]0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALL CX10.2A
    "^QUANTUM FIREBALL CX10.2A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Quantum Fireball Plus LM series",
    "^QUANTUM FIREBALLP LM(10.2|15|20.5|30)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Quantum Fireball CR series",
    "^QUANTUM FIREBALL CR(4.3|8.4)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALLP AS10.2, AS20.5, AS30.0, and AS40.0
    "^QUANTUM FIREBALLP AS(10.2|20.5|30.0|40.0)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALL EX6.4A
    "^QUANTUM FIREBALL EX6.4A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALL ST3.2A
    "^QUANTUM FIREBALL ST(3.2|4.3)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALL EX3.2A
    "^QUANTUM FIREBALL EX3.2A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { NULL, // QUANTUM FIREBALLP KX27.3
    "^QUANTUM FIREBALLP KX27.3$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Quantum Fireball Plus KA series",
    "^QUANTUM FIREBALLP KA(9|10).1$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { "Quantum Fireball SE series",
    "^QUANTUM FIREBALL SE4.3A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  /*------------------------------------------------------------
   *  End of table.  Do not add entries below this marker.
   *------------------------------------------------------------ */
  {NULL, NULL, NULL, NULL, NULL, NULL, NULL}
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
  pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL FAMILY:", drivetable->modelfamily ?
       drivetable->modelfamily : "");
  
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

// Shows all presets for drives in knowndrives[].
// Returns <0 on syntax error in regular expressions.
int showallpresets(void){
  int i;
  int rc = 0;
  regex_t regex;

  // loop over all entries in the knowndrives[] table, printing them
  // out in a nice format
  for (i=0; knowndrives[i].modelregexp; i++){
    showonepreset(&knowndrives[i]);
    pout("\n");
  }

  // Check all regular expressions
  for (i=0; knowndrives[i].modelregexp; i++){
    if (compileregex(&regex, knowndrives[i].modelregexp, REG_EXTENDED))
      rc = -1;
    if (knowndrives[i].firmwareregexp) {
      if (compileregex(&regex, knowndrives[i].firmwareregexp, REG_EXTENDED))
        rc = -1;
    }
  }
  pout("For information about adding a drive to the database see the FAQ on the\n");
  pout("smartmontools home page: " PACKAGE_HOMEPAGE "\n");
  return rc;
}

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware){
  int i;
  int cnt = 0;
  const char * firmwaremsg = (firmware ? firmware : "(any)");
  regex_t regex;

  for (i=0; knowndrives[i].modelregexp; i++){
    if (i > 0)
      regfree(&regex);
    if (compileregex(&regex, knowndrives[i].modelregexp, REG_EXTENDED))
      continue;
    if (regexec(&regex, model, 0, NULL, 0))
      continue;
    if (firmware && knowndrives[i].firmwareregexp) {
      regfree(&regex);
      if (compileregex(&regex, knowndrives[i].firmwareregexp, REG_EXTENDED))
        continue;
      if (regexec(&regex, firmware, 0, NULL, 0))
        continue;
    }
    if (++cnt == 1)
      pout("Drive found in smartmontools Database.  Drive identity strings:\n"
           "%-*s %s\n"
           "%-*s %s\n"
           "match smartmontools Drive Database entry:\n",
           TABLEPRINTWIDTH, "MODEL:", model, TABLEPRINTWIDTH, "FIRMWARE:", firmwaremsg);
    else if (cnt == 2)
      pout("and match these additional entries:\n");
    showonepreset(&knowndrives[i]);
    pout("\n");
  }
  regfree(&regex);
  if (cnt == 0)
    pout("No presets are defined for this drive.  Its identity strings:\n"
         "MODEL:    %s\n"
         "FIRMWARE: %s\n"
         "do not match any of the known regular expressions.\n",
         model, firmwaremsg);
  return cnt;
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
  
  // Look up the drive in knowndrives[].
  if ((i = lookupdrive(model, firmware)) >= 0) {
    
    // if vendoropts is non-NULL then Attribute interpretation presets
    if (knowndrives[i].vendoropts) {
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
    }
    
    // If a special-purpose function is defined for this drive then
    // call it. Note that if command line arguments or Directives
    // over-ride this choice, then the specialpurpose function that is
    // called must deal with this.
    if (knowndrives[i].specialpurpose)
      (*knowndrives[i].specialpurpose)(con);
  }
  
  // return <0 if drive wasn't recognized, or index>=0 into database
  // if it was
  return i;
}
