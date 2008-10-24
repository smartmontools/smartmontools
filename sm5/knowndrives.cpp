/*
 * knowndrives.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-8 Philip Williams, Bruce Allen
 * Copyright (C) 2008   Christian Franke <smartmontools-support@lists.sourceforge.net>
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
#include "extern.h"
#include "knowndrives.h"
#include "utility.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h> // access()
#endif

#include <stdexcept>

const char *knowndrives_c_cvsid="$Id: knowndrives.cpp,v 1.185 2008/10/24 22:33:33 manfred99 Exp $"
ATACMDS_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID KNOWNDRIVES_H_CVSID UTILITY_H_CVSID;

#define MODEL_STRING_LENGTH                         40
#define FIRMWARE_STRING_LENGTH                       8
#define TABLEPRINTWIDTH                             19


/* Table of settings for known drives terminated by an element containing all
 * zeros.  The drivesettings structure is described in knowndrives.h.  Note
 * that lookupdrive() will search knowndrives[] from the start to end or
 * until it finds the first match, so the order in knowndrives[] is important
 * for distinct entries that could match the same drive. */

// Note that the table just below uses EXTENDED REGULAR EXPRESSIONS.
// A good on-line reference for these is:
// http://www.zeus.com/extra/docsystem/docroot/apps/web/docs/modules/access/regex.html

// Starting with CVS version 1.179 of this file, the regular expressions
// for drive model and firmware must match the full string.  The effect of
// "^FULLSTRING$" is identical to "FULLSTRING".  The special characters '^'
// and '$' are no longer required, but still allowed. The form ".*SUBSTRING.*"
// can be used if substring match is desired.

static const drive_settings builtin_knowndrives[] = {
// BEGIN drivedb.h (DO NOT DELETE - used by Makefile)
  { "Asus-Phison SSD",
    "ASUS-PHISON SSD",
    "", "", ""
  },
  { "IBM Deskstar 60GXP series",  // ER60A46A firmware
    "(IBM-|Hitachi )?IC35L0[12346]0AVER07.*",
    "ER60A46A",
    "", ""
  },
  { "IBM Deskstar 60GXP series",  // All other firmware
    "(IBM-|Hitachi )?IC35L0[12346]0AVER07.*",
    "",
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
    "Please see http://www.geocities.com/dtla_update/index.html#rel and\n"
    "http://www-3.ibm.com/pc/support/site.wss/document.do?lndocid=MIGR-42215 or\n"
    "http://www-1.ibm.com/support/docview.wss?uid=psg1MIGR-42215",
    ""
  },
  { "IBM Deskstar 40GV & 75GXP series (A5AA/A6AA firmware)",
    "(IBM-)?DTLA-30[57]0[123467][05].*",
    "T[WX][123468AG][OF]A[56]AA",
    "", ""
  },
  { "IBM Deskstar 40GV & 75GXP series (all other firmware)",
    "(IBM-)?DTLA-30[57]0[123467][05].*",
    "",
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
    "Please see http://www.geocities.com/dtla_update/ and\n"
    "http://www-3.ibm.com/pc/support/site.wss/document.do?lndocid=MIGR-42215 or\n"
    "http://www-1.ibm.com/support/docview.wss?uid=psg1MIGR-42215",
    ""
  },
  { "", // ExcelStor J240, J340, J360, J680, and J880
    "ExcelStor Technology J(24|34|36|68|88)0",
    "", "", ""
  },
  { "", // Fujitsu M1623TAU
    "FUJITSU M1623TAU",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHG series",
    "FUJITSU MHG2...ATU?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHH series",
    "FUJITSU MHH2...ATU?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHJ series",
    "FUJITSU MHJ2...ATU?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHK series",
    "FUJITSU MHK2...ATU?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "",  // Fujitsu MHL2300AT
    "FUJITSU MHL2300AT",
    "",
    "This drive's firmware has a harmless Drive Identity Structure\n"
      "checksum error bug.",
    "-v 9,seconds"
  },
  { "",  // MHM2200AT, MHM2150AT, MHM2100AT, MHM2060AT
    "FUJITSU MHM2(20|15|10|06)0AT",
    "",
    "This drive's firmware has a harmless Drive Identity Structure\n"
      "checksum error bug.",
    "-v 9,seconds"
  },
  { "Fujitsu MHN series",
    "FUJITSU MHN2...AT",
    "",
    "",
    "-v 9,seconds"
  },
  { "", // Fujitsu MHR2020AT
    "FUJITSU MHR2020AT",
    "",
    "",
    "-v 9,seconds"
  },
  { "", // Fujitsu MHR2040AT
    "FUJITSU MHR2040AT",
    "",    // Tested on 40BA
    "",
    "-v 9,seconds -v 192,emergencyretractcyclect "
    "-v 198,offlinescanuncsectorct -v 200,writeerrorcount"
  },
  { "Fujitsu MHSxxxxAT family",
    "FUJITSU MHS20[6432]0AT(  .)?",
    "",
    "",
    "-v 9,seconds -v 192,emergencyretractcyclect "
    "-v 198,offlinescanuncsectorct -v 200,writeerrorcount "
    "-v 201,detectedtacount"
  },
  { "Fujitsu MHT series",
    "FUJITSU MHT2...(AH|AS|AT|BH)U?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHU series",
    "FUJITSU MHU2...ATU?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHV series",
    "FUJITSU MHV2...(AH|AS|AT|BH|BS|BT).*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MPA..MPG series",
    "FUJITSU MP[A-G]3...A[HTEV]U?.*",
    "",
    "",
    "-v 9,seconds"
  },
  { "Fujitsu MHY2 BH series",
    "FUJITSU MHY2(04|06|08|10|12|16|20|25)0BH.*",
    "", "", ""
  },
  { "Fujitsu MHW2 BH series",
    "FUJITSU MHW2(04|06|08|10|12|16)0BH.*",
    "", "", ""
  },
  { "Fujitsu MHZ2 BS series",
    "FUJITSU MHZ2(12|25)0BS.*",
    "", "", ""
  },
  { "", // Samsung SV4012H (known firmware)
    "SAMSUNG SV4012H",
    "RM100-08",
    "",
    "-v 9,halfminutes -F samsung"
  },
  { "", // Samsung SV4012H (all other firmware)
    "SAMSUNG SV4012H",
    "",
    "May need -F samsung disabled; see manual for details.",
    "-v 9,halfminutes -F samsung"
  },
  { "", // Samsung SV0412H (known firmware)
    "SAMSUNG SV0412H",
    "SK100-01",
    "",
    "-v 9,halfminutes -v 194,10xCelsius -F samsung"
  },
  { "", // Samsung SV0412H (all other firmware)
    "SAMSUNG SV0412H",
    "",
    "May need -F samsung disabled; see manual for details.",
    "-v 9,halfminutes -v 194,10xCelsius -F samsung"
  },
  { "", // Samsung SV1204H (known firmware)
    "SAMSUNG SV1204H",
    "RK100-1[3-5]",
    "",
    "-v 9,halfminutes -v 194,10xCelsius -F samsung"
  },
  { "", // Samsung SV1204H (all other firmware)
    "SAMSUNG SV1204H",
    "",
    "May need -F samsung disabled; see manual for details.",
    "-v 9,halfminutes -v 194,10xCelsius -F samsung"
  },
  { "", // SAMSUNG SV0322A tested with FW JK200-35
    "SAMSUNG SV0322A",
    "", "", ""
  },
  { "", // SAMSUNG SP40A2H with RR100-07 firmware
    "SAMSUNG SP40A2H",
    "RR100-07",
    "",
    "-v 9,halfminutes -F samsung"
  },
  { "", // SAMSUNG SP80A4H with RT100-06 firmware
    "SAMSUNG SP80A4H",
    "RT100-06",
    "",
    "-v 9,halfminutes -F samsung"
  },
  { "", // SAMSUNG SP8004H with QW100-61 firmware
    "SAMSUNG SP8004H",
    "QW100-61",
    "",
    "-v 9,halfminutes -F samsung"
  },
  { "SAMSUNG SpinPoint T133 series", // tested with HD300LJ/ZT100-12, HD400LJ/ZZ100-14, HD401LJ/ZZ100-15
    "SAMSUNG HD(250KD|(30[01]|320|40[01])L[DJ])",
    "", "", ""
  },
  { "SAMSUNG SpinPoint T166 series", // tested with HD501LJ/CR100-10
    "SAMSUNG HD(080G|160H|32[01]K|403L|50[01]L)J",
    "", "", ""
  },
  { "SAMSUNG SpinPoint P120 series", // VF100-37 firmware, tested with SP2514N/VF100-37
    "SAMSUNG SP(16[01]3|2[05][01]4)[CN]",
    "VF100-37",
    "",
    "-F samsung3"
  },
  { "SAMSUNG SpinPoint P120 series", // other firmware, tested with SP2504C/VT100-33
    "SAMSUNG SP(16[01]3|2[05][01]4)[CN]",
    "",
    "May need -F samsung3 enabled; see manual for details.",
    ""
  },
  { "SAMSUNG SpinPoint P80 SD series", // tested with HD160JJ/ZM100-33
    "SAMSUNG HD(080H|120I|160J)J",
    "", "", ""
  },
  { "SAMSUNG SpinPoint P80 series", // BH100-35 firmware, tested with SP0842N/BH100-35
    "SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]",
    "BH100-35",
    "",
    "-F samsung3"
  },
  { "SAMSUNG SpinPoint P80 series", // firmware *-35 or later
    "SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]",
    ".*-3[5-9]",
    "May need -F samsung3 enabled; see manual for details.",
    ""
  },
  { "SAMSUNG SpinPoint P80 series", // firmware *-25...34, tested with SP1614C/SW100-25 and -34
    "SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]",
    ".*-(2[5-9]|3[0-4])",
    "",
    "-v 9,halfminutes"
  },
  { "", // Any other Samsung disk with *-23 *-24 firmware
    // SAMSUNG SP1213N (TL100-23 firmware)
    // SAMSUNG SP0802N (TK100-23 firmware)
    // Samsung SP1604N, tested with FW TM100-23 and TM100-24
    "SAMSUNG .*",
    ".*-2[34]",
    "",
    "-v 9,halfminutes -F samsung2"
  },
  { "", // All Samsung drives with '.*-25' firmware
    "SAMSUNG.*",
    ".*-25",
    "May need -F samsung2 disabled; see manual for details.",
    "-v 9,halfminutes -F samsung2"
  },
  { "", // All Samsung drives with '.*-26 or later (currently to -39)' firmware
    "SAMSUNG.*",
    ".*-(2[6789]|3[0-9])",
    "",
    "-v 9,halfminutes"
  },
  { "", // Samsung ALL OTHER DRIVES
    "SAMSUNG.*",
    "",
    "May need -F samsung or -F samsung2 enabled; see manual for details.",
    ""
  },
  { "Maxtor Fireball 541DX family",
    "Maxtor 2B0(0[468]|1[05]|20)H1",
    "",
    "",
    "-v 9,minutes -v 194,unknown"
  },
  { "Maxtor Fireball 3 family",
    "Maxtor 2F0[234]0[JL]0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 1280 ATA family",  // no self-test log, ATA2-Fast
    "Maxtor 8(1280A2|2160A4|2560A4|3840A6|4000A6|5120A8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 2160 Ultra ATA family",
    "Maxtor 8(2160D2|3228D3|3240D3|4320D4|6480D6|8400D8|8455D8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 2880 Ultra ATA family",
    "Maxtor 9(0510D4|0576D4|0648D5|0720D5|0840D6|0845D6|0864D6|1008D7|1080D8|1152D8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 3400 Ultra ATA family",
    "Maxtor 9(1(360|350|202)D8|1190D7|10[12]0D6|0840D5|06[48]0D4|0510D3|1(350|202)E8|1010E6|0840E5|0640E4)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax D540X-4G family",
    "Maxtor 4G(120J6|160J[68])",
    "",
    "",
    "-v 9,minutes -v 194,unknown"
  },
  { "Maxtor DiamondMax D540X-4K family",
    "MAXTOR 4K(020H1|040H2|060H3|080H4)",
    "", "", ""
  },
  { "Maxtor DiamondMax Plus D740X family",
    "MAXTOR 6L0(20[JL]1|40[JL]2|60[JL]3|80[JL]4)",
    "", "", ""
  },
  { "Maxtor DiamondMax Plus 5120 Ultra ATA 33 family",
    "Maxtor 9(0512D2|0680D3|0750D3|0913D4|1024D4|1360D6|1536D6|1792D7|2048D8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax Plus 6800 Ultra ATA 66 family",
    "Maxtor 9(2732U8|2390U7|2049U6|1707U5|1366U4|1024U3|0845U3|0683U2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax D540X-4D",
    "Maxtor 4D0(20H1|40H2|60H3|80H4)",
    "",
    "",
    "-v 9,minutes -v 194,unknown"
  },
  { "Maxtor DiamondMax 16 family",
    "Maxtor 4(R0[68]0[JL]0|R1[26]0L0|A160J0|R120L4)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 4320 Ultra ATA family",
    "Maxtor (91728D8|91512D7|91303D6|91080D5|90845D4|90645D3|90648D[34]|90432D2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 17 VL family",
    "Maxtor 9(0431U1|0641U2|0871U2|1301U3|1741U4)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 20 VL family",
    "Maxtor (94091U8|93071U6|92561U5|92041U4|91731U4|91531U3|91361U3|91021U2|90841U2|90651U2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax VL 30 family",  // U: ATA66, H: ATA100
    "Maxtor (33073U4|32049U3|31536U2|30768U1|33073H4|32305H3|31536H2|30768H1)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 36 family",
    "Maxtor (93652U8|92739U6|91826U4|91369U3|90913U2|90845U2|90435U1)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 40 ATA 66 series",
    "Maxtor 9(0684U2|1024U2|1362U3|1536U3|2049U4|2562U5|3073U6|4098U8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax Plus 40 series (Ultra ATA 66 and Ultra ATA 100)",
    "Maxtor (54098[UH]8|53073[UH]6|52732[UH]6|52049[UH]4|51536[UH]3|51369[UH]3|51024[UH]2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 40 VL Ultra ATA 100 series",
    "Maxtor 3(1024H1|1535H2|2049H2|3073H3|4098H4)( B)?",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax Plus 45 Ulta ATA 100 family",
    "Maxtor 5(4610H6|4098H6|3073H4|2049H3|1536H2|1369H2|1023H2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 60 ATA 66 family",
    "Maxtor 9(1023U2|1536U2|2049U3|2305U3|3073U4|4610U6|6147U8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 60 ATA 100 family",
    "Maxtor 9(1023H2|1536H2|2049H3|2305H3|3073H4|4098H6|4610H6|6147H8)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax Plus 60 family",
    "Maxtor 5T0(60H6|40H4|30H3|20H2|10H1)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 80 family",
    "Maxtor (98196H8|96147H6)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 536DX family",
    "Maxtor 4W(100H6|080H6|060H4|040H3|030H2)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax Plus 8 family",
    "Maxtor 6(E0[234]|K04)0L0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 10 family (ATA/133 and SATA/150)",
    "Maxtor 6(B(30|25|20|16|12|10|08)0[MPRS]|L(080[MLP]|(100|120)[MP]|160[MP]|200[MPRS]|250[RS]|300[RS]))0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 10 family (SATA/300)",
    "Maxtor 6V(080E|160E|200E|250F|300F|320F)0",
    "", "", ""
  },
  { "Maxtor DiamondMax Plus 9 family",
    "Maxtor 6Y((060|080|120|160)L0|(060|080|120|160|200|250)P0|(060|080|120|160|200|250)M0)",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor DiamondMax 11 family",
    "Maxtor 6H[45]00[FR]0",
    "", "", ""
  },
  { "Maxtor DiamondMax 17",
    "Maxtor 6G(080L|160[PE])0",
    "", "", ""
  },
  { "Seagate Maxtor DiamondMax 20",
    "MAXTOR STM3(40|80|160)[28]1[12]0?AS?",
    "", "", ""
  },
  { "Seagate Maxtor DiamondMax 21",
    "MAXTOR STM3(160215|(250|320)820|320620)AS?",
    "", "", ""
  },
  { "Seagate Maxtor DiamondMax 22",
    "MAXTOR STM3(320613|500320|750330|1000340)AS?",
    "", "", ""
  },
  { "Maxtor MaXLine Plus II",
    "Maxtor 7Y250[PM]0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor MaXLine II family",
    "Maxtor [45]A(25|30|32)0[JN]0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor MaXLine III family (ATA/133 and SATA/150)",
    "Maxtor 7L(25|30)0[SR]0",
    "",
    "",
    "-v 9,minutes"
  },
  { "Maxtor MaXLine III family (SATA/300)",
    "Maxtor 7V(25|30)0F0",
    "", "", ""
  },
  { "Maxtor MaXLine Pro 500 family",  // There is also a 7H500R0 model, but I
    "Maxtor 7H500F0",               // haven't added it because I suspect
    "",                               // it might need vendoropts_9_minutes
    "", ""                            // and nobody has submitted a report yet
  },
  { "", // HITACHI_DK14FA-20B
    "HITACHI_DK14FA-20B",
    "",
    "",
    "-v 9,minutes -v 193,loadunload"
  },
  { "HITACHI Travelstar DK23XX/DK23XXB series",
    "HITACHI_DK23..-..B?",
    "",
    "",
    "-v 9,minutes -v 193,loadunload"
  },
  { "Hitachi Endurastar J4K20/N4K20 (formerly DK23FA-20J)",
    "(HITACHI_DK23FA-20J|HTA422020F9AT[JN]0)",
    "",
    "",
    "-v 9,minutes -v 193,loadunload"
  },
  { "IBM Deskstar 14GXP and 16GP series",
    "IBM-DTTA-3(7101|7129|7144|5032|5043|5064|5084|5101|5129|5168)0",
    "", "", ""
  },
  { "IBM Deskstar 25GP and 22GXP family",
    "IBM-DJNA-3(5(101|152|203|250)|7(091|135|180|220))0",
    "", "", ""
  },
  { "IBM Travelstar 4GT family",
    "IBM-DTCA-2(324|409)0",
    "", "", ""
  },
  { "IBM Travelstar 25GS, 18GT, and 12GN family",
    "IBM-DARA-2(25|18|15|12|09|06)000",
    "", "", ""
  },
  { "IBM Travelstar 48GH, 30GN, and 15GN family",
    "(IBM-|Hitachi )?IC25(T048ATDA05|N0(30|20|15|12|10|07|06|05)ATDA04)-.",
    "", "", ""
  },
  { "IBM Travelstar 32GH, 30GT, and 20GN family",
    "IBM-DJSA-2(32|30|20|10|05)",
    "", "", ""
  },
  { "IBM Travelstar 4GN family",
    "IBM-DKLA-2(216|324|432)0",
    "", "", ""
  },
  { "IBM Deskstar 37GP and 34GXP family",
    "IBM-DPTA-3(5(375|300|225|150)|7(342|273|205|136))0",
    "", "", ""
  },
  { "IBM/Hitachi Travelstar 60GH and 40GN family",
    "(IBM-|Hitachi )?IC25(T060ATC[SX]05|N0[4321]0ATC[SX]04)-.",
    "", "", ""
  },
  { "IBM/Hitachi Travelstar 40GNX family",
    "(IBM-|Hitachi )?IC25N0[42]0ATC[SX]05-.",
    "", "", ""
  },
  { "Hitachi Travelstar 80GN family",
    "(Hitachi )?IC25N0[23468]0ATMR04-.",
    "", "", ""
  },
  { "Hitachi Travelstar 4K40",
    "(Hitachi )?HTS4240[234]0M9AT00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K80 family",
    "(Hitachi )?HTS5480[8642]0M9AT00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K100 series",
    "(Hitachi )?HTS5410[1864]0G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar E5K100 series",
    "(Hitachi )?HTE541040G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K120",
    "(Hitachi )?HTS5412(60|80|10|12)H9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K160 series",
    "(Hitachi )?HTS5416([468]0|1[26])J9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K250 series",
    "(Hitachi |HITACHI )?HTS5425(80|12|16|20|25)K9(A3|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 7K60",
    "(Hitachi )?HTS726060M9AT00",
    "", "", ""
  },
  { "Hitachi Travelstar E7K60",
    "(Hitachi )?HTE7260[46]0M9AT00",
    "", "", ""
  },
  { "Hitachi Travelstar 7K100",
    "(Hitachi )?HTS7210[168]0G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar E7K100",
    "(Hitachi )?HTE7210[168]0G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 7K200",
    "(Hitachi )?HTS7220(80|10|12|16|20)K9(A3|SA)00",
    "", "", ""
  },
  { "IBM/Hitachi Deskstar 120GXP family",
    "(IBM-)?IC35L((020|040|060|080|120)AVVA|0[24]0AVVN)07-[01]",
    "", "", ""
  },
  { "IBM/Hitachi Deskstar GXP-180 family",
    "(IBM-)?IC35L(030|060|090|120|180)AVV207-[01]",
    "", "", ""
  },
  { "IBM Travelstar 14GS",
    "IBM-DCYA-214000",
    "", "", ""
  },
  { "IBM Travelstar 4LP",
    "IBM-DTNA-2(180|216)0",
    "", "", ""
  },
  { "Hitachi Deskstar 7K80 series",
    "(Hitachi )?HDS7280([48]0PLAT20|(40)?PLA320|80PLA380)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K160",
    "(Hitachi )?HDS7216(80|16)PLA[3T]80",
    "", "", ""
  },
  { "Hitachi Deskstar 7K250 series",
    "(Hitachi )?HDS7225((40|80|12|16)VLAT20|(12|16|25)VLAT80|(80|12|16|25)VLSA80)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K250 (SUN branded)",
    "HITACHI HDS7225SBSUN250G.*",
    "", "", ""
  },
  { "Hitachi Deskstar T7K250 series",
    "(Hitachi )?HDT7225((25|20|16)DLA(T80|380))",
    "", "", ""
  },
  { "Hitachi Deskstar 7K400 series",
    "(Hitachi )?HDS724040KL(AT|SA)80",
    "", "", ""
  },
  { "Hitachi Deskstar 7K500 series",
    "(Hitachi )?HDS725050KLA(360|T80)",
    "", "", ""
  },
  { "Hitachi Deskstar P7K500 series",
    "(Hitachi )?HDP7250(16|25|32|40|50)GLA(36|38|T8)0",
    "", "", ""
  },
  { "Hitachi Deskstar T7K500",
    "(Hitachi )?HDT7250(25|32|40|50)VLA(360|380|T80)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K1000",
    "(Hitachi )?HDS7210(50|75|10)KLA330",
    "", "", ""
  },
  { "Hitachi Ultrastar 7K1000",
    "(Hitachi )?HUA7210(50|75|10)KLA330",
    "", "", ""
  },
  { "Toshiba 2.5\" HDD series (30-60 GB)",
    "TOSHIBA MK((6034|4032)GSX|(6034|4032)GAX|(6026|4026|4019|3019)GAXB?|(6025|6021|4025|4021|4018|3025|3021|3018)GAS|(4036|3029)GACE?|(4018|3017)GAP)",
    "", "", ""
  },
  { "Toshiba 2.5\" HDD series (80 GB and above)",
    "TOSHIBA MK(80(25GAS|26GAX|32GAX|32GSX)|10(31GAS|32GAX)|12(33GAS|34G[AS]X)|2035GSS)",
    "", "", ""
  },
  { "Toshiba 2.5\" HDD MK..52GSX series",
    "TOSHIBA MK(80|12|16|25|32)52GSX",
    "", "", ""
  },
  { "Toshiba 1.8\" HDD series",
    "TOSHIBA MK[23468]00[4-9]GA[HL]",
    "", "", ""
  },
  { "", // TOSHIBA MK6022GAX
    "TOSHIBA MK6022GAX",
    "", "", ""
  },
  { "", // TOSHIBA MK6409MAV
    "TOSHIBA MK6409MAV",
    "", "", ""
  },
  { "", // TOS MK3019GAXB SUN30G
    "TOS MK3019GAXB SUN30G",
    "", "", ""
  },
  { "", // TOSHIBA MK2016GAP, MK2017GAP, MK2018GAP, MK2018GAS, MK2023GAS
    "TOSHIBA MK20(1[678]GAP|(18|23)GAS)",
    "", "", ""
  },
  { "Seagate Momentus family",
    "ST9(20|28|40|48)11A",
    "", "", ""
  },
  { "Seagate Momentus 42 family",
    "ST9(2014|3015|4019)A",
    "", "", ""
  },
  { "Seagate Momentus 4200.2 series",
    "ST9(100822|808210|60821|50212|402113|30219)A",
    "", "", ""
  },
  { "Seagate Momentus 5400.2 series",
    "ST9(808211|60822|408114|308110|120821|10082[34]|8823|6812|4813|3811)AS?",
    "", "", ""
  },
  { "Seagate Momentus 5400.3 series",
    "ST9(4081[45]|6081[35]|8081[15]|100828|120822|160821)AS?",
    "", "", ""
  },
  { "Seagate Momentus 5400.3 ED series",
    "ST9(4081[45]|6081[35]|8081[15]|100828|120822|160821)AB",
    "", "", ""
  },
  { "Seagate Momentus 5400 PSD series", // Hybrid drives
    "ST9(808212|(120|160)8220)AS",
    "", "", ""
  },
  { "Seagate Momentus 7200.1 series",
    "ST9(10021|80825|6023|4015)AS?",
    "", "", ""
  },
  { "Seagate Momentus 7200.2 series",
    "ST9(80813|100821|120823|160823|200420)ASG?",
    "", "", ""
  },
  { "Seagate Momentus 7200.3 series",
    "ST9((80|120|160)411|(250|320)421)ASG?",
    "", "", ""
  },
  { "Seagate Medalist 1010, 1721, 2120, 3230 and 4340",  // ATA2, with -t permissive
    "ST3(1010|1721|2120|3230|4340)A",
    "", "", ""
  },
  { "Seagate Medalist 2110, 3221, 4321, 6531, and 8641",
    "ST3(2110|3221|4321|6531|8641)A",
    "", "", ""
  },
  { "Seagate U Series X family",
    "ST3(10014A(CE)?|20014A)",
    "", "", ""
  },
  { "Seagate U7 family",
    "ST3(30012|40012|60012|80022|120020)A",
    "", "", ""
  },
  { "Seagate U Series 6 family",
    "ST3(8002|6002|4081|3061|2041)0A",
    "", "", ""
  },
  { "Seagate U Series 5 family",
    "ST3(40823|30621|20413|15311|10211)A",
    "", "", ""
  },
  { "Seagate U4 family",
    "ST3(2112|4311|6421|8421)A",
    "", "", ""
  },
  { "Seagate U8 family",
    "ST3(8410|4313|17221|13021)A",
    "", "", ""
  },
  { "Seagate U10 family",
    "ST3(20423|15323|10212)A",
    "", "", ""
  },
  { "Seagate Barracuda ATA family",
    "ST3(2804|2724|2043|1362|1022|681)0A",
    "", "", ""
  },
  { "Seagate Barracuda ATA II family",
    "ST3(3063|2042|1532|1021)0A",
    "", "", ""
  },
  { "Seagate Barracuda ATA III family",
    "ST3(40824|30620|20414|15310|10215)A",
    "", "", ""
  },
  { "Seagate Barracuda ATA IV family",
    "ST3(20011|30011|40016|60021|80021)A",
    "", "", ""
  },
  { "Seagate Barracuda ATA V family",
    "ST3(12002(3A|4A|9A|3AS)|800(23A|15A|23AS)|60(015A|210A)|40017A)",
    "", "", ""
  },
  { "Seagate Barracuda 5400.1",
    "ST340015A",
    "", "", ""
  },
  { "Seagate Barracuda 7200.7 and 7200.7 Plus family",
    "ST3(200021A|200822AS?|16002[13]AS?|12002[26]AS?|1[26]082[78]AS|8001[13]AS?|8081[79]AS|60014A|40111AS|40014AS?)",
    "", "", ""
  },
  { "Seagate Barracuda 7200.8 family",
    "ST3(400[68]32|300[68]31|250[68]23|200826)AS?",
    "", "", ""
  },
  { "Seagate Barracuda 7200.9 family",
    "ST3(402111?|80[28]110?|120[28]1[0134]|160[28]1[012]|200827|250[68]24|300[68]22|(320|400)[68]33|500[68](32|41))AS?.*",
    "", "", ""
  },
  { "Seagate Barracuda 7200.10 family",
    "ST3((80|160)[28]15|200820|250[34]10|(250|300|320|400)[68]20|500[68]30|750[68]40)AS?",
    "", "", ""
  },
  { "Seagate Barracuda 7200.11",
    "ST3(500[368]2|750[36]3|1000[36]4)0AS?",
    "", "", ""
  },
  { "Seagate Barracuda ES",
    "ST3(250[68]2|32062|40062|50063|75064)0NS",
    "", "", ""
  },
  { "Seagate Barracuda ES.2",  // no SAS versions added for now
    "ST3(25031|50032|75033|100034)0NS",
    "", "", ""
  },
  { "Seagate Medalist 17240, 13030, 10231, 8420, and 4310",
    "ST3(17240|13030|10231|8420|4310)A",
    "", "", ""
  },
  { "Seagate Medalist 17242, 13032, 10232, 8422, and 4312",
    "ST3(1724|1303|1023|842|431)2A",
    "", "", ""
  },
  { "Seagate NL35 family",
    "ST3(250623|250823|400632|400832|250824|250624|400633|400833|500641|500841)NS",
    "", "", ""
  },
  { "Seagate SV35.2 Series",
    "ST3(160815|250820|320620|500630|750640)(A|S)V",
    "", "", ""
  },
  { "Seagate DB35.3 Series",
    "ST3(750640SCE|((80|160)215|(250|320|400)820|500830|750840)(A|S)CE)",
    "", "", ""
  },
  { "Western Digital Protege",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "WDC WD([2468]00E|1[26]00A)B-.*",
    "", "", ""
  },
  { "Western Digital Caviar family",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "WDC WD(2|3|4|6|8|10|12|16|18|20|25)00BB-.*",
    "", "", ""
  },
  { "Western Digital Caviar WDxxxAB series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "WDC WD(3|4|6)00AB-.*",
    "", "", ""
  },
  { "Western Digital Caviar WDxxxAA series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "WDC WD...?AA(-.*)?",
    "", "", ""
  },
  { "Western Digital Caviar WDxxxBA series",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ */
    "WDC WD...BA",
    "", "", ""
  },
  { "Western Digital Caviar AC series", // add only 5400rpm/7200rpm (ata33 and faster)
    "WDC AC((116|121|125|225|132|232)|([1-4][4-9][0-9])|([1-4][0-9][0-9][0-9]))00[A-Z]?.*",
    "", "", ""
  },
  { "Western Digital Caviar SE family",
  /* Western Digital drives with this comment all appear to use Attribute 9 in
   * a  non-standard manner.  These entries may need to be updated when it
   * is understood exactly how Attribute 9 should be interpreted.
   * UPDATE: this is probably explained by the WD firmware bug described in the
   * smartmontools FAQ 
   * UPDATE 2: this does not apply to more recent models, at least WD3200AAJB */
    "WDC WD(4|6|8|10|12|16|18|20|25|30|32|40|50)00(JB|PB)-.*",
    "", "", ""
  },
  { "Western Digital Caviar Blue EIDE family",  // WD Caviar SE EIDE family
    /* not completely accurate: at least also WD800JB, WD(4|8|20|25)00BB sold as Caviar Blue */
    "WDC WD(16|25|32|40|50)00AAJB-.*",
    "", "", ""
  },
  { "Western Digital Caviar Blue EIDE family",  // WD Caviar SE16 EIDE family
    "WDC WD(25|32|40|50)00AAKB-.*",
    "", "", ""
  },
  { "Western Digital RE EIDE family",
    "WDC WD(12|16|25|32)00SB-.*",
    "", "", ""
  },
  { "Western Digital Caviar Serial ATA family",
    "WDC WD(4|8|20|32)00BD-.*",
    "", "", ""
  },
  { "Western Digital Caviar SE Serial ATA family",
    "WDC WD(4|8|12|16|20|25|32|40)00(JD|KD|PD)-.*",
    "", "", ""
  },
  { "Western Digital Caviar SE Serial ATA family",
    "WDC WD(8|12|16|20|25|30|32|40|50)00JS-.*",
    "", "", ""
  },
  { "Western Digital Caviar SE16 Serial ATA family",
    "WDC WD(16|20|25|32|40|50|75)00KS-.*",
    "", "", ""
  },
  { "Western Digital Caviar Blue Serial ATA family",  // WD Caviar SE Serial ATA family
    /* not completely accurate: at least also WD800BD, (4|8)00JD sold as Caviar Blue */
    "WDC WD((8|12|16|25|32)00AABS|(12|16|25|32|40|50)00AAJS)-.*",
    "", "", ""
  },
  { "Western Digital Caviar Blue Serial ATA family",  // WD Caviar SE16 Serial ATA family
    "WDC WD(16|20|25|32|40|50|64|75)00AAKS-.*",
    "", "", ""
  },
  { "Western Digital RE Serial ATA family",
    "WDC WD(12|16|25|32)00(SD|YD|YS)-.*",
    "", "", ""
  },
  { "Western Digital RE2 Serial ATA family",
    "WDC WD((40|50|75)00(YR|YS|AYYS)|(16|32|40|50)0[01]ABYS)-.*",
    "", "", ""
  },
  { "Western Digital RE3 Serial ATA family",
    "WDC WD((25|32|50)02ABYS)-.*",
    "", "", ""
  },
  { "Western Digital Caviar Green family",
    "WDC WD((50|64|75)00AA|10EA)CS-.*",
    "", "", ""
  },
  { "Western Digital AV ATA family",
    "WDC WD(8|16|50)00AV(B|J)B-.*",
    "", "", ""
  },
  { "Western Digital Raptor family",
    "WDC WD((360|740|800)GD|(360|740|1500)ADFD)-.*",
    "", "", ""
  },
  { "Western Digital VelociRaptor family",
    "WDC WD((1500|3000)B|3000G)LFS-.*",
    "", "", ""
  },
  { "Western Digital Scorpio EIDE family",
    "WDC WD(4|6|8|10|12|16)00(UE|VE)-.*",
    "", "", ""
  },
  { "Western Digital Scorpio Blue EIDE family",
    "WDC WD(4|6|8|10|12|16|25)00BEVE-.*",
    "", "", ""
  },
  { "Western Digital Scorpio Serial ATA family",
    "WDC WD(4|6|8|10|12|16|25)00BEAS-.*",
    "", "", ""
  },
  { "Western Digital Scorpio Blue Serial ATA family",
    "WDC WD((4|6|8|10|12|16|25)00BEVS|3200BEVT)-.*",
    "", "", ""
  },
  { "Quantum Bigfoot series",
    "QUANTUM BIGFOOT TS10.0A",
    "", "", ""
  },
  { "Quantum Fireball lct15 series",
    "QUANTUM FIREBALLlct15 [123]0",
    "", "", ""
  },
  { "Quantum Fireball lct20 series",
    "QUANTUM FIREBALLlct20 [234]0",
    "", "", ""
  },
  { "Quantum Fireball CX series", 
    "QUANTUM FIREBALL CX10.2A",
    "", "", ""
  },
  { "Quantum Fireball CR series",
    "QUANTUM FIREBALL CR(4.3|8.4|13.0)A",
    "", "", ""
  },
  { "Quantum Fireball EX series",
    "QUANTUM FIREBALL EX(3.2|6.4)A",
    "", "", ""
  },
  { "Quantum Fireball ST series",
    "QUANTUM FIREBALL ST(3.2|4.3|4300)A",
    "", "", ""
  },
  { "Quantum Fireball SE series",
    "QUANTUM FIREBALL SE4.3A",
    "", "", ""
  },
  { "Quantum Fireball Plus LM series",
    "QUANTUM FIREBALLP LM(10.2|15|20.5|30)",
    "", "", ""
  },
  { "Quantum Fireball Plus AS series",
    "QUANTUM FIREBALLP AS(10.2|20.5|30.0|40.0)",
    "", "", ""
  },
  { "Quantum Fireball Plus KX series",
    "QUANTUM FIREBALLP KX27.3",
    "", "", ""
  },
  { "Quantum Fireball Plus KA series",
    "QUANTUM FIREBALLP KA(9|10).1",
    "", "", ""
  },
// END drivedb.h (DO NOT DELETE - used by Makefile)
};


/// Drive database class. Stores custom entries read from file.
/// Provides transparent access to concatenation of custom and
/// default table.
class drive_database
{
public:
  drive_database();

  ~drive_database();

  /// Get total number of entries.
  unsigned size() const
    { return m_custom_tab.size() + m_builtin_size; }

  /// Get number of custom entries.
  unsigned custom_size() const
    { return m_custom_tab.size(); }

  /// Array access.
  const drive_settings & operator[](unsigned i);

  /// Append new custom entry.
  void push_back(const drive_settings & src);

  /// Append builtin table.
  void append(const drive_settings * builtin_tab, unsigned builtin_size)
    { m_builtin_tab = builtin_tab; m_builtin_size = builtin_size; }

private:
  const drive_settings * m_builtin_tab;
  unsigned m_builtin_size;

  std::vector<drive_settings> m_custom_tab;
  std::vector<char *> m_custom_strings;

  const char * copy_string(const char * str);

  drive_database(const drive_database &);
  void operator=(const drive_database &);
};

drive_database::drive_database()
: m_builtin_tab(0), m_builtin_size(0)
{
}

drive_database::~drive_database()
{
  for (unsigned i = 0; i < m_custom_strings.size(); i++)
    delete [] m_custom_strings[i];
}

const drive_settings & drive_database::operator[](unsigned i)
{
  return (i < m_custom_tab.size() ? m_custom_tab[i]
          : m_builtin_tab[i - m_custom_tab.size()] );
}

void drive_database::push_back(const drive_settings & src)
{
  drive_settings dest;
  dest.modelfamily    = copy_string(src.modelfamily);
  dest.modelregexp    = copy_string(src.modelregexp);
  dest.firmwareregexp = copy_string(src.firmwareregexp);
  dest.warningmsg     = copy_string(src.warningmsg);
  dest.presets        = copy_string(src.presets);
  m_custom_tab.push_back(dest);
}

const char * drive_database::copy_string(const char * src)
{
  char * dest = new char[strlen(src)+1];
  try {
    m_custom_strings.push_back(dest);
  }
  catch (...) {
    delete [] dest; throw;
  }
  return strcpy(dest, src);
}


/// The drive database.
static drive_database knowndrives;


// Compile regular expression, print message on failure.
static bool compile(regular_expression & regex, const char *pattern)
{
  if (!regex.compile(pattern, REG_EXTENDED)) {
    pout("Internal error: unable to compile regular expression \"%s\": %s\n"
         "Please inform smartmontools developers at " PACKAGE_BUGREPORT "\n",
      pattern, regex.get_errmsg());
    return false;
  }
  return true;
}

// Compile & match a regular expression.
static bool match(const char * pattern, const char * str)
{
  regular_expression regex;
  if (!compile(regex, pattern))
    return false;
  return regex.full_match(str);
}

// Searches knowndrives[] for a drive with the given model number and firmware
// string.  If either the drive's model or firmware strings are not set by the
// manufacturer then values of NULL may be used.  Returns the entry of the
// first match in knowndrives[] or 0 if no match if found.
const drive_settings * lookup_drive(const char * model, const char * firmware)
{
  if (!model)
    model = "";
  if (!firmware)
    firmware = "";

  for (unsigned i = 0; i < knowndrives.size(); i++) {
    // Check whether model matches the regular expression in knowndrives[i].
    if (!match(knowndrives[i].modelregexp, model))
      continue;

    // Model matches, now check firmware. "" matches always.
    if (!(  !*knowndrives[i].firmwareregexp
          || match(knowndrives[i].firmwareregexp, firmware)))
      continue;

    // Found
    return &knowndrives[i];
  }

  // Not found
  return 0;
}

// Parse '-v' and '-F' options in preset string, return false on error.
static bool parse_presets(const char * presets, unsigned char * opts, unsigned char & fix_firmwarebug)
{
  for (int i = 0; ; ) {
    i += strspn(presets+i, " \t");
    if (!presets[i])
      break;
    char opt, arg[40+1+13]; int len = -1;
    if (!(sscanf(presets+i, "-%c %40[^ ]%n", &opt, arg, &len) >= 2 && len > 0))
      return false;
    if (opt == 'v') {
      // Parse "-v N,option"
      unsigned char newopts[MAX_ATTRIBUTE_NUM] = {0, };
      if (parse_attribute_def(arg, newopts))
        return false;
      // Set only if not set by user
      for (int j = 0; j < MAX_ATTRIBUTE_NUM; j++)
        if (newopts[j] && !opts[j])
          opts[j] = newopts[j];
    }
    else if (opt == 'F') {
      unsigned char fix;
      if (!strcmp(arg, "samsung"))
        fix = FIX_SAMSUNG;
      else if (!strcmp(arg, "samsung2"))
        fix = FIX_SAMSUNG2;
      else if (!strcmp(arg, "samsung3"))
        fix = FIX_SAMSUNG3;
      else
        return false;
      // Set only if not set by user
      if (fix_firmwarebug == FIX_NOTSPECIFIED)
        fix_firmwarebug = fix;
    }
    else
      return false;

    i += len;
  }
  return true;
}

// Shows one entry of knowndrives[], returns #errors.
static int showonepreset(const drive_settings * dbentry)
{
  // Basic error check
  if (!(   dbentry
        && dbentry->modelfamily
        && dbentry->modelregexp && *dbentry->modelregexp
        && dbentry->firmwareregexp
        && dbentry->warningmsg
        && dbentry->presets                             )) {
    pout("Invalid drive database entry. Please report\n"
         "this error to smartmontools developers at " PACKAGE_BUGREPORT ".\n");
    return 1;
  }
  
  // print and check model and firmware regular expressions
  int errcnt = 0;
  regular_expression regex;
  pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL REGEXP:", dbentry->modelregexp);
  if (!compile(regex, dbentry->modelregexp))
    errcnt++;

  pout("%-*s %s\n", TABLEPRINTWIDTH, "FIRMWARE REGEXP:", *dbentry->firmwareregexp ?
    dbentry->firmwareregexp : ".*"); // preserve old output (TODO: Change)
  if (*dbentry->firmwareregexp && !compile(regex, dbentry->firmwareregexp))
    errcnt++;

  pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL FAMILY:", dbentry->modelfamily);

  // if there are any presets, then show them
  unsigned char fix_firmwarebug = 0;
  bool first_preset = true;
  if (*dbentry->presets) {
    unsigned char opts[MAX_ATTRIBUTE_NUM] = {0,};
    if (!parse_presets(dbentry->presets, opts, fix_firmwarebug)) {
      pout("Syntax error in preset option string \"%s\"\n", dbentry->presets);
      errcnt++;
    }
    for (int i = 0; i < MAX_ATTRIBUTE_NUM; i++) {
      char out[256];
      if (opts[i]) {
        ataPrintSmartAttribName(out, i, opts);
        // Use leading zeros instead of spaces so that everything lines up.
        out[0] = (out[0] == ' ') ? '0' : out[0];
        out[1] = (out[1] == ' ') ? '0' : out[1];
        pout("%-*s %s\n", TABLEPRINTWIDTH, first_preset ? "ATTRIBUTE OPTIONS:" : "", out);
        first_preset = false;
      }
    }
  }
  if (first_preset)
    pout("%-*s %s\n", TABLEPRINTWIDTH, "ATTRIBUTE OPTIONS:", "None preset; no -v options are required.");

  // describe firmwarefix
  if (fix_firmwarebug) {
    const char * fixdesc;
    switch (fix_firmwarebug) {
      case FIX_SAMSUNG:
        fixdesc = "Fixes byte order in some SMART data (same as -F samsung)";
        break;
      case FIX_SAMSUNG2:
        fixdesc = "Fixes byte order in some SMART data (same as -F samsung2)";
        break;
      case FIX_SAMSUNG3:
        fixdesc = "Fixes completed self-test reported as in progress (same as -F samsung3)";
        break;
      default:
        fixdesc = "UNKNOWN"; errcnt++;
        break;
    }
    pout("%-*s %s\n", TABLEPRINTWIDTH, "OTHER PRESETS:", fixdesc);
  }

  // Print any special warnings
  if (*dbentry->warningmsg)
    pout("%-*s %s\n", TABLEPRINTWIDTH, "WARNINGS:", dbentry->warningmsg);
  return errcnt;
}

// Shows all presets for drives in knowndrives[].
// Returns #syntax errors.
int showallpresets()
{
  // loop over all entries in the knowndrives[] table, printing them
  // out in a nice format
  int errcnt = 0;
  for (unsigned i = 0; i < knowndrives.size(); i++) {
    errcnt += showonepreset(&knowndrives[i]);
    pout("\n");
  }

  pout("Total number of entries  :%5u\n"
       "Entries read from file(s):%5u\n\n",
    knowndrives.size(), knowndrives.custom_size());

  pout("For information about adding a drive to the database see the FAQ on the\n");
  pout("smartmontools home page: " PACKAGE_HOMEPAGE "\n");

  if (errcnt > 0)
    pout("\nFound %d syntax error(s) in database.\n"
         "Please inform smartmontools developers at " PACKAGE_BUGREPORT "\n", errcnt);
  return errcnt;
}

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware)
{
  int cnt = 0;
  const char * firmwaremsg = (firmware ? firmware : "(any)");

  for (unsigned i = 0; i < knowndrives.size(); i++) {
    if (!match(knowndrives[i].modelregexp, model))
      continue;
    if (   firmware && *knowndrives[i].firmwareregexp
        && !match(knowndrives[i].firmwareregexp, firmware))
        continue;
    // Found
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
  if (cnt == 0)
    pout("No presets are defined for this drive.  Its identity strings:\n"
         "MODEL:    %s\n"
         "FIRMWARE: %s\n"
         "do not match any of the known regular expressions.\n",
         model, firmwaremsg);
  return cnt;
}

// Shows the presets (if any) that are available for the given drive.
void showpresets(const ata_identify_device * drive)
{
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];

  // get the drive's model/firmware strings
  format_ata_string(model, drive->model, MODEL_STRING_LENGTH);
  format_ata_string(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);
  
  // and search to see if they match values in the table
  const drive_settings * dbentry = lookup_drive(model, firmware);
  if (!dbentry) {
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
  showonepreset(dbentry);
}

// Sets preset vendor attribute options in opts by finding the entry
// (if any) for the given drive in knowndrives[].  Values that have
// already been set in opts will not be changed.  Returns false if drive
// not recognized.
bool apply_presets(const ata_identify_device *drive, unsigned char * opts,
                   unsigned char & fix_firmwarebug)
{
  // get the drive's model/firmware strings
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];
  format_ata_string(model, drive->model, MODEL_STRING_LENGTH);
  format_ata_string(firmware, drive->fw_rev, FIRMWARE_STRING_LENGTH);
  
  // Look up the drive in knowndrives[].
  const drive_settings * dbentry = lookup_drive(model, firmware);
  if (!dbentry)
    return false;

  if (*dbentry->presets) {
    // Apply presets
    if (!parse_presets(dbentry->presets, opts, fix_firmwarebug))
      pout("Syntax error in preset option string \"%s\"\n", dbentry->presets);
  }
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// Parser for drive database files

// Abstract pointer to read file input.
// Operations supported: c = *p; c = p[1]; ++p;
class stdin_iterator
{
public:
  explicit stdin_iterator(FILE * f)
    : m_f(f) { get(); get(); }

  stdin_iterator & operator++()
    { get(); return *this; }

  char operator*() const
    { return m_c; }

  char operator[](int i) const
    {
      if (i != 1)
        fail();
      return m_next;
    }

private:
  FILE * m_f;
  char m_c, m_next;
  void get();
  void fail() const;
};

void stdin_iterator::get()
{
  m_c = m_next;
  int ch = getc(m_f);
  m_next = (ch != EOF ? ch : 0);
}

void stdin_iterator::fail() const
{
  throw std::runtime_error("stdin_iterator: wrong usage");
}


// Use above as parser input 'pointer'. Can easily be changed later
// to e.g. 'const char *' if above is too slow.
typedef stdin_iterator parse_ptr;

// Skip whitespace and comments.
static parse_ptr skip_white(parse_ptr src, const char * path, int & line)
{
  for ( ; ; ++src) switch (*src) {
    case ' ': case '\t':
      continue;

    case '\n':
      ++line;
      continue;

    case '/':
      switch (src[1]) {
        case '/':
          // skip '// comment'
          ++src; ++src;
          while (*src && *src != '\n')
            ++src;
          if (*src)
            ++line;
          break;
        case '*':
          // skip '/* comment */'
          ++src; ++src;
          for (;;) {
            if (!*src) {
              pout("%s(%d): Missing '*/'\n", path, line);
              return src;
            }
            char c = *src; ++src;
            if (c == '\n')
              ++line;
            else if (c == '*' && *src == '/')
              break;
          }
          break;
        default:
          return src;
      }
      continue;

    default:
      return src;
  }
}

// Info about a token.
struct token_info
{
  char type;
  int line;
  std::string value;

  token_info() : type(0), line(0) { }
};

// Get next token.
static parse_ptr get_token(parse_ptr src, token_info & token, const char * path, int & line)
{
  src = skip_white(src, path, line);
  switch (*src) {
    case '{': case '}': case ',':
      // Simple token
      token.type = *src; token.line = line;
      ++src;
      break;

    case '"':
      // String constant
      token.type = '"'; token.line = line;
      token.value = "";
      do {
        for (++src; *src != '"'; ++src) {
          char c = *src;
          if (!c || c == '\n' || (c == '\\' && !src[1])) {
            pout("%s(%d): Missing terminating '\"'\n", path, line);
            token.type = '?'; token.line = line;
            return src;
          }
          if (c == '\\') {
            c = *++src;
            switch (c) {
              case 'n' : c = '\n'; break;
              case '\n': ++line; break;
              case '\\': case '"': break;
              default:
                pout("%s(%d): Unknown escape sequence '\\%c'\n", path, line, c);
                token.type = '?'; token.line = line;
                continue;
            }
          }
          token.value += c;
        }
        // Lookahead to detect string constant concatentation
        src = skip_white(++src, path, line);
      } while (*src == '"');
      break;

    case 0:
      // EOF
      token.type = 0; token.line = line;
      break;

    default:
      pout("%s(%d): Syntax error, invalid char '%c'\n", path, line, *src);
      token.type = '?'; token.line = line;
      while (*src && *src != '\n')
        ++src;
      break;
  }

  return src;
}

// Parse drive database from abstract input pointer.
static bool parse_drive_database(parse_ptr src, drive_database & db, const char * path)
{
  int state = 0, field = 0;
  std::string values[5];
  bool ok = true;

  token_info token; int line = 1;
  src = get_token(src, token, path, line);
  for (;;) {
    // EOF is ok after '}', trailing ',' is also allowed.
    if (!token.type && (state == 0 || state == 4))
      break;

    // Check expected token
    const char expect[] = "{\",},";
    if (token.type != expect[state]) {
      if (token.type != '?')
        pout("%s(%d): Syntax error, '%c' expected\n", path, token.line, expect[state]);
      ok = false;
      // Skip to next entry
      while (token.type && token.type != '{')
        src = get_token(src, token, path, line);
      state = 0;
      if (token.type)
        continue;
      break;
    }

    // Interpret parser state
    switch (state) {
      case 0: // ... ^{...}
        state = 1; field = 0;
        break;
      case 1: // {... ^"..." ...}
        switch (field) {
          case 1: case 2:
            if (!token.value.empty()) {
              regular_expression regex;
              if (!regex.compile(token.value.c_str(), REG_EXTENDED)) {
                pout("%s(%d): Error in regular expression: %s\n", path, token.line, regex.get_errmsg());
                ok = false;
              }
            }
            else if (field == 1) {
              pout("%s(%d): Missing regular expression for drive model\n", path, token.line);
              ok = false;
            }
            break;
          case 4:
            if (!token.value.empty()) {
              unsigned char opts[MAX_ATTRIBUTE_NUM] = {0, }; unsigned char fix = 0;
              if (!parse_presets(token.value.c_str(), opts, fix)) {
                pout("%s(%d): Syntax error in preset option string\n", path, token.line);
                ok = false;
              }
            }
            break;
        }
        values[field] = token.value;
        state = (++field < 5 ? 2 : 3);
        break;
      case 2: // {... "..."^, ...}
        state = 1;
        break;
      case 3: // {...^}, ...
        {
          drive_settings entry;
          entry.modelfamily    = values[0].c_str();
          entry.modelregexp    = values[1].c_str();
          entry.firmwareregexp = values[2].c_str();
          entry.warningmsg     = values[3].c_str();
          entry.presets        = values[4].c_str();
          db.push_back(entry);
        }
        state = 4;
        break;
      case 4: // {...}^, ...
        state = 0;
        break;
      default:
        pout("Bad state %d\n", state);
        return false;
    }
    src = get_token(src, token, path, line);
  }
  return ok;
}

// Read drive database from file.
bool read_drive_database(const char * path)
{
  stdio_file f(path, "r");
  if (!f) {
    pout("%s: cannot open drive database file\n", path);
    return false;
  }

  return parse_drive_database(parse_ptr(f), knowndrives, path);
}

// Read drive databases from standard places.
bool read_default_drive_databases()
{
#ifndef _WIN32
  // Read file for local additions: /{,usr/local/}etc/smart_drivedb.h
  static const char db1[] = SMARTMONTOOLS_SYSCONFDIR"/smart_drivedb.h";
#else
  static const char db1[] = "./smart_drivedb.h";
#endif
  if (!access(db1, 0)) {
    if (!read_drive_database(db1))
      return false;
  }

#ifdef SMARTMONTOOLS_DRIVEDBDIR
  // Read file from package: // /usr/{,local/}share/smartmontools/drivedb.h
  static const char db2[] = SMARTMONTOOLS_DRIVEDBDIR"/drivedb.h";
  if (!access(db2, 0)) {
    if (!read_drive_database(db2))
      return false;
  }
  else
#endif
  {
    // Append builtin table.
    knowndrives.append(builtin_knowndrives,
      sizeof(builtin_knowndrives)/sizeof(builtin_knowndrives[0]));
  }

  return true;
}
