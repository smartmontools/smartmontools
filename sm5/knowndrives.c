/*
 * knowndrives.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-4 Philip Williams, Bruce Allen
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
#include "atacmds.h"
#include "ataprint.h"
#include "extern.h"
#include "int64.h"
#include "knowndrives.h"
#include "utility.h" // includes <regex.h>
#include "config.h"

const char *knowndrives_c_cvsid="$Id: knowndrives.c,v 1.107 2004/06/01 21:12:27 pjwilliams Exp $"
ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

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
  { // IBM Deskstar 60GXP series
    "IC35L0[12346]0AVER07",
    ".*",
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/index.html#rel",
    NULL, NULL, NULL
  },
  { // IBM Deskstar 40GV & 75GXP series (A5AA/A6AA firmware)
    "(IBM-)?DTLA-30[57]0[123467][05]$",
    "^T[WX][123468A]OA[56]AA$",
    NULL, NULL, NULL, NULL
  },
  { // IBM Deskstar 40GV & 75GXP series (all other firmware)
    "(IBM-)?DTLA-30[57]0[123467][05]$",
    ".*",
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/",
    NULL, NULL, NULL
  },
  { // ExcelStor J240
    "^ExcelStor Technology J240$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Fujitsu MPB series
    "^FUJITSU MPB....ATU?$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MPD and MPE series
    "^FUJITSU MP[DE]....A[HTE]$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MPF series
    "^FUJITSU MPF3(102A[HT]|153A[HT]|204A[HT])$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MPG series
    "^FUJITSU MPG3(102A(H|T  E)|204A(H|[HT]  E)|307A(H  E|T)|409A[HT]  E)$",
    ".*",
    NULL,
    vendoropts_9_seconds,
    NULL, NULL
  },
  { // Fujitsu MPC series
    "^FUJITSU MPC3(032AT|043AT|045AH|064A[HT]|084AT|096AT|102AT)$",
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
  { // Fujitsu MHSxxxxAT family
    "^FUJITSU MHS20[6432]0AT(  .)?$",
    ".*",
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
  { // Fujitsu MHTxxxxAT family
    "^FUJITSU MHT20[23468]0AT$",
    ".*",
    NULL,
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
  { // Samsung SV4012H (all other firmware)
    "^SAMSUNG SV4012H$",
    ".*",
    may_need_minus_F_disabled,
    vendoropts_Samsung_SV4012H,
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
  { //SAMSUNG SV0322A tested with FW JK200-35
    "^SAMSUNG SV0322A$",
    ".*",
    NULL,
    NULL,
    NULL,
    NULL
  },
  { // SAMSUNG SP40A2H with RR100-07 firmware
    "^SAMSUNG SP40A2H$",
    "^RR100-07$",
    NULL,
    vendoropts_9_halfminutes,
    specialpurpose_reverse_samsung,
    same_as_minus_F
  },
  { 
    // Any other Samsung disk with *-23 *-24 firmware
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
  { // All Samsung drives with '.*-25' firmware
    "^SAMSUNG.*",
    ".*-25$",
    may_need_minus_F2_disabled,
    vendoropts_Samsung_SV4012H,
    specialpurpose_reverse_samsung2,
    same_as_minus_F2
  },
  { // All Samsung drives with '.*-26 or later (currently to -39)' firmware
    "^SAMSUNG.*",
    ".*-(2[6789]|3[0-9])$",
    NULL,
    vendoropts_Samsung_SV4012H,
    NULL,
    NULL
  },
  { // Samsung ALL OTHER DRIVES
    "^SAMSUNG.*",
    ".*",
    may_need_minus_F_enabled,
    NULL, NULL, NULL
  },
  { // Maxtor Fireball 541DX family
    "^Maxtor 2B0(0[468]|1[05]|20)H1$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { // Maxtor DiamondMax 3400 Ultra ATA family
    "^Maxtor 9(1(360|350|202)D8|1190D7|10[12]0D6|0840D5|06[48]0D4|0510D3|1(350|202)E8|1010E6|0840E5|0640E4)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax D540X-4G family
    "^Maxtor 4G(120J6|160J[68])$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Maxtor DiamondMax D540X-4K family
    "^MAXTOR 4K(020H1|040H2|060H3|080H4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Maxtor DiamondMax Plus D740X family
    "^MAXTOR 6L0(20[JL]1|40[JL]2|60[JL]3|80[JL]4)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Maxtor DiamondMax Plus 6800 Ultra ATA 66 family
    "^Maxtor 9(2732U8|2390U7|2049U6|1707U5|1366U4|1024U3|0845U3|0683U2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor 4D080H4
    "^Maxtor (4D080H4|4G120J6)$",
    ".*",
    NULL,
    vendoropts_Maxtor_4D080H4,
    NULL, NULL
  },
  { // Maxtor DiamondMax 16 family
    "^Maxtor 4(R0[68]0[JL]|R1[26]0L|A160J)0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax 4320 family
    "^Maxtor (91728D8|91512D7|91303D6|91080D5|90845D4|90645D3|90648D4|90432D2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax 20 VL family
    "^Maxtor (94091U8|93071U6|92561U5|92041U4|91731U4|91531U3|91361U3|91021U2|90841U2|90651U2)$",
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
  { // Maxtor DiamondMax 36 family
    "^Maxtor (93652U8|92739U6|91826U4|91369U3|90913U2|90845U2|90435U1)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax 40 ATA 66 series
    "^Maxtor 9(0684U2|1024U2|1362U3|1536U3|2049U4|2562U5|3073U6|4098U8)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax Plus 40 series (Ultra ATA 66 and Ultra ATA 100)
    "^Maxtor (54098[UH]8|53073[UH]6|52732[UH]6|52049[UH]4|51536[UH]3|51369[UH]3|51024[UH]2)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax 40 VL Ultra ATA 100 series
    "^Maxtor 3(1024H1|1535H2|2049H2|3073H3|4098H4)( B)?$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor DiamondMax Plus 45 Ulta ATA 100 family
    "^Maxtor 5(4610H6|4098H6|3073H4|2049H3|1536H2|1369H2|1023H2)$",
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
  { // Maxtor DiamondMax 536DX family
    "^Maxtor 4W(100H6|080H6|060H4|040H3|030H2)$",
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
    "^Maxtor 6Y((060|080|120|160)L0|(060|080|120|160|200|250)P0|(060|080|120|160|200|250)M0)$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor MaXLine Plus II
    "^Maxtor 7Y250[PM]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // Maxtor MaXLine II family
    "^Maxtor [45]A(25|32)0[JN]0$",
    ".*",
    NULL,
    vendoropts_9_minutes,
    NULL, NULL
  },
  { // HITACHI_DK14FA-20B
    "^HITACHI_DK14FA-20B$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23XX,
    NULL, NULL
  },
  { // HITACHI Travelstar DK23XX/DK23XXB series
    "^HITACHI_DK23..-..B?$",
    ".*",
    NULL,
    vendoropts_Hitachi_DK23XX,
    NULL, NULL
  },
  { // IBM Deskstar 14GXP and 16GP series
    "^IBM-DTTA-3(7101|7129|7144|5032|5043|5064|5084|5101|5129|5168)0$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { // IBM Deskstar 25GP and 22GXP family
    "^IBM-DJNA-3(5(101|152|203|250)|7(091|135|180|220))0$",
    ".*",
    NULL, NULL, NULL, NULL
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
  { // IBM/Hitachi Travelstar 40GNX family
    "^IC25N0[42]0ATC[SX]05-.$",
    ".*",
    NULL, NULL, NULL, NULL 
  },
  { // Hitachi Travelstar 80GN family
    "^IC25N0[23468]0ATMR04-.$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // IBM/Hitachi Deskstar 120GXP family
    "^IC35L((020|040|060|080|120)AVVA|0[24]0AVVN)07-[01]$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // IBM/Hitachi Deskstar GXP-180 family 
    "^IC35L(030|060|090|120|180)AVV207-[01]$",
    ".*", 
    NULL, NULL, NULL, NULL 
  },
  { // IBM Travelstar 14GS
    "^IBM-DCYA-214000$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // IBM Travelstar 4LP
    "^IBM-DTNA-2(180|216)0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Hitachi Deskstar 7K250 series
    "^HDS7225((40|80|12|16)VLAT20|(12|16|25)VLAT80|(80|12|16|25)VLSA80)$",
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
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK4019GAX/MK4019GAXB
    "^TOSHIBA MK4019GAXB?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK6409MAV
    "^TOSHIBA MK6409MAV$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOS MK3019GAXB SUN30G
    "^TOS MK3019GAXB SUN30G$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK2017GAP
    "^TOSHIBA MK2017GAP$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // TOSHIBA MK4018GAS
    "^TOSHIBA MK4018GAS$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Medalist 8641 family
    "^ST3(2110|3221|4312|6531|8641)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate U Series X family
    "^ST3(10014A(CE)?|20014A)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate U Series 6 family
    "^ST3(8002|6002|4081|3061|2041)0A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate U Series 5 family
    "^ST3(40823|30621|20413|15311|10211)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate U8 family
    "^ST3(8410|4313|17221|13021)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda ATA II family
    "^ST3(3063|2042|1532|1021)0A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda ATA III family
    "^ST3(40824|30620|20414|15310|10215)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda ATA IV family
    "^ST3(20011|40016|60021|80021)A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda ATA V family
    "^ST3(12002(3A|4A|9A|3AS)|800(23A|15A|23AS)|60(015A|210A)|40017A)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Barracuda 7200.7 and 7200.7 Plus family
    "^ST3(200822AS?|16002[13]AS?|12002[26]AS?|8001[13]AS?|60014A|40014AS?)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Seagate Medalist 17242, 13032, 10232, 8422, and 4312
    "^ST3(1724|1303|1023|842|431)2A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Protege
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD([2468]00E|1[26]00A)B-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar family
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(2|3|4|6|8|10|12|16|18|20|25)00BB-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar WDxxxAB series
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(3|4|6)00AB-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar WDxxxAA series
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(64|84|102|136|205|307)AA(-.*)?$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar WDxxxBA series
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(102|136|153|205)BA$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar AC12500, AC24300, AC25100, AC36400, AC38400
    "^WDC AC(125|243|251|364|384)00",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar SE family
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted. */
    "^WDC WD(4|6|8|10|12|16|18|20|25)00JB-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar SE (Serial ATA) family
    "^WDC WD(4|8|12|16|20|25)00JD-.*$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar AC38400
    "^WDC AC38400L$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // Western Digital Caviar AC23200L
    "^WDC AC23200L$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALLlct15 20 and QUANTUM FIREBALLlct15 30
    "^QUANTUM FIREBALLlct15 [23]0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALLlct20 series
    "^QUANTUM FIREBALLlct20 [234]0$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALL CX10.2A
    "^QUANTUM FIREBALL CX10.2A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALLP LM15 and LM30
    "^QUANTUM FIREBALLP LM(15|30)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALL CR4.3A
    "^QUANTUM FIREBALL CR4.3A$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALLP AS10.2 and AS40.0
    "^QUANTUM FIREBALLP AS(10.2|40.0)$",
    ".*",
    NULL, NULL, NULL, NULL
  },
  { // QUANTUM FIREBALL EX6.4A
    "^QUANTUM FIREBALL EX6.4A$",
    ".*",
    NULL, NULL, NULL, NULL
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
