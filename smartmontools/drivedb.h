/*
 * drivedb.h - smartmontools drive database file
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-10 Philip Williams, Bruce Allen
 * Copyright (C) 2008-10 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Structure used to store drive database entries:
 *
 * struct drive_settings {
 *   const char * modelfamily;
 *   const char * modelregexp;
 *   const char * firmwareregexp;
 *   const char * warningmsg;
 *   const char * presets;
 * };
 *
 * The elements are used in the following ways:
 *
 *  modelfamily     Informal string about the model family/series of a
 *                  device. Set to "" if no info (apart from device id)
 *                  known.  The entry is ignored if this string starts with
 *                  a dollar sign.  Must not start with "USB:", see below.
 *  modelregexp     POSIX extended regular expression to match the model of
 *                  a device.  This should never be "".
 *  firmwareregexp  POSIX extended regular expression to match a devices's
 *                  firmware.  This is optional and should be "" if it is not
 *                  to be used.  If it is nonempty then it will be used to
 *                  narrow the set of devices matched by modelregexp.
 *  warningmsg      A message that may be displayed for matching drives.  For
 *                  example, to inform the user that they may need to apply a
 *                  firmware patch.
 *  presets         String with vendor-specific attribute ('-v') and firmware
 *                  bug fix ('-F') options.  Same syntax as in smartctl command
 *                  line.  The user's own settings override these.
 *
 * The regular expressions for drive model and firmware must match the full
 * string.  The effect of "^FULLSTRING$" is identical to "FULLSTRING".
 * The form ".*SUBSTRING.*" can be used if substring match is desired.
 *
 * The table will be searched from the start to end or until the first match,
 * so the order in the table is important for distinct entries that could match
 * the same drive.
 *
 *
 * Format for USB ID entries:
 *
 *  modelfamily     String with format "USB: DEVICE; BRIDGE" where
 *                  DEVICE is the name of the device and BRIDGE is
 *                  the name of the USB bridge.  Both may be empty
 *                  if no info known.
 *  modelregexp     POSIX extended regular expression to match the USB
 *                  vendor:product ID in hex notation ("0x1234:0xabcd").
 *                  This should never be "".
 *  firmwareregexp  POSIX extended regular expression to match the USB
 *                  bcdDevice info.  Only compared during search if other
 *                  entries with same USB vendor:product ID exist.
 *  warningmsg      Not used yet.
 *  presets         String with one device type ('-d') option.
 *
 */

/*
const drive_settings builtin_knowndrives[] = {
 */
  { "$Id$",
    "-", "-",
    "This is a dummy entry to hold the SVN-Id of drivedb.h",
    ""
  },
  { "Apple SSD SM128",
    "APPLE SSD SM128",
    "", "", ""
  },
  { "Asus-Phison SSD",
    "ASUS-PHISON SSD",
    "", "", ""
  },
  { "SuperTalent UltraDrive GX SSD",
    "STT_FT[MD](28|32|56|64)GX25H",
    "", "",
    " -v 1,raw64"
    " -v 9,raw64"
    " -v 12,raw64"
    " -v 184,raw64,Initial_Bad_Block_Count"
    " -v 195,raw64,Program_Failure_Blk_Ct"
    " -v 196,raw64,Erase_Failure_Blk_Ct"
    " -v 197,raw64,Read_Failure_Blk_Ct"
    " -v 198,raw64,Read_Sectors_Tot_Ct"
    " -v 199,raw64,Write_Sectors_Tot_Ct"
    " -v 200,raw64,Read_Commands_Tot_Ct"
    " -v 201,raw64,Write_Commands_Tot_Ct"
    " -v 202,raw64,Error_Bits_Flash_Tot_Ct"
    " -v 203,raw64,Corr_Read_Errors_Tot_Ct"
    " -v 204,raw64,Bad_Block_Full_Flag"
    " -v 205,raw64,Max_PE_Count_Spec"
    " -v 206,raw64,Min_Erase_Count"
    " -v 207,raw64,Max_Erase_Count"
    " -v 208,raw64,Average_Erase_Count"
    " -v 209,raw64,Remaining_Lifetime_Perc"
    " -v 210,raw64"
    " -v 211,raw64"
    " -v 212,raw64"
    " -v 213,raw64"
  },
  { "Patriot Torqx SSD",
    "Patriot[ -]Torqx.*",
    "", "",
    " -v 1,raw64"
    " -v 9,raw64"
    " -v 12,raw64"
    " -v 184,raw64,Initial_Bad_Block_Count"
    " -v 195,raw64,Program_Failure_Blk_Ct"
    " -v 196,raw64,Erase_Failure_Blk_Ct"
    " -v 197,raw64,Read_Failure_Blk_Ct"
    " -v 198,raw64,Read_Sectors_Tot_Ct"
    " -v 199,raw64,Write_Sectors_Tot_Ct"
    " -v 200,raw64,Read_Commands_Tot_Ct"
    " -v 201,raw64,Write_Commands_Tot_Ct"
    " -v 202,raw64,Error_Bits_Flash_Tot_Ct"
    " -v 203,raw64,Corr_Read_Errors_Tot_Ct"
    " -v 204,raw64,Bad_Block_Full_Flag"
    " -v 205,raw64,Max_PE_Count_Spec"
    " -v 206,raw64,Min_Erase_Count"
    " -v 207,raw64,Max_Erase_Count"
    " -v 208,raw64,Average_Erase_Count"
    " -v 209,raw64,Remaining_Lifetime_Perc"
    " -v 210,raw64"
    " -v 211,raw64"
    " -v 212,raw64"
    " -v 213,raw64"
  },
  { "OCZ Vertex SSD",
    "OCZ[ -]VERTEX.*",
    "", "",
    " -v 1,raw64"
    " -v 9,raw64"
    " -v 12,raw64"
    " -v 184,raw64,Initial_Bad_Block_Count"
    " -v 195,raw64,Program_Failure_Blk_Ct"
    " -v 196,raw64,Erase_Failure_Blk_Ct"
    " -v 197,raw64,Read_Failure_Blk_Ct"
    " -v 198,raw64,Read_Sectors_Tot_Ct"
    " -v 199,raw64,Write_Sectors_Tot_Ct"
    " -v 200,raw64,Read_Commands_Tot_Ct"
    " -v 201,raw64,Write_Commands_Tot_Ct"
    " -v 202,raw64,Error_Bits_Flash_Tot_Ct"
    " -v 203,raw64,Corr_Read_Errors_Tot_Ct"
    " -v 204,raw64,Bad_Block_Full_Flag"
    " -v 205,raw64,Max_PE_Count_Spec"
    " -v 206,raw64,Min_Erase_Count"
    " -v 207,raw64,Max_Erase_Count"
    " -v 208,raw64,Average_Erase_Count"
    " -v 209,raw64,Remaining_Lifetime_Perc"
    " -v 210,raw64"
    " -v 211,raw64"
    " -v 212,raw64"
    " -v 213,raw64"
  },
  { "OCZ Agility SSD",
    "OCZ[ -]AGILITY.*",
    "", "",
    " -v 1,raw64"
    " -v 9,raw64"
    " -v 12,raw64"
    " -v 184,raw64,Initial_Bad_Block_Count"
    " -v 195,raw64,Program_Failure_Blk_Ct"
    " -v 196,raw64,Erase_Failure_Blk_Ct"
    " -v 197,raw64,Read_Failure_Blk_Ct"
    " -v 198,raw64,Read_Sectors_Tot_Ct"
    " -v 199,raw64,Write_Sectors_Tot_Ct"
    " -v 200,raw64,Read_Commands_Tot_Ct"
    " -v 201,raw64,Write_Commands_Tot_Ct"
    " -v 202,raw64,Error_Bits_Flash_Tot_Ct"
    " -v 203,raw64,Corr_Read_Errors_Tot_Ct"
    " -v 204,raw64,Bad_Block_Full_Flag"
    " -v 205,raw64,Max_PE_Count_Spec"
    " -v 206,raw64,Min_Erase_Count"
    " -v 207,raw64,Max_Erase_Count"
    " -v 208,raw64,Average_Erase_Count"
    " -v 209,raw64,Remaining_Lifetime_Perc"
    " -v 210,raw64"
    " -v 211,raw64"
    " -v 212,raw64"
    " -v 213,raw64"
  },
  { "Crucial M225 SSD",
    "CRUCIAL_CT(64|128|256)M225",
    "", "",
    " -v 1,raw64"
    " -v 9,raw64"
    " -v 12,raw64"
    " -v 184,raw64,Initial_Bad_Block_Count"
    " -v 195,raw64,Program_Failure_Blk_Ct"
    " -v 196,raw64,Erase_Failure_Blk_Ct"
    " -v 197,raw64,Read_Failure_Blk_Ct"
    " -v 198,raw64,Read_Sectors_Tot_Ct"
    " -v 199,raw64,Write_Sectors_Tot_Ct"
    " -v 200,raw64,Read_Commands_Tot_Ct"
    " -v 201,raw64,Write_Commands_Tot_Ct"
    " -v 202,raw64,Error_Bits_Flash_Tot_Ct"
    " -v 203,raw64,Corr_Read_Errors_Tot_Ct"
    " -v 204,raw64,Bad_Block_Full_Flag"
    " -v 205,raw64,Max_PE_Count_Spec"
    " -v 206,raw64,Min_Erase_Count"
    " -v 207,raw64,Max_Erase_Count"
    " -v 208,raw64,Average_Erase_Count"
    " -v 209,raw64,Remaining_Lifetime_Perc"
    " -v 210,raw64"
    " -v 211,raw64"
    " -v 212,raw64"
    " -v 213,raw64"
  },
  { "Intel X25-E SSD",
    "SSDSA2SH(032|064)G1.* INTEL",  // G1 = first generation
    "", "",
    "-v 225,raw48,Host_Writes_Count"
  },
  { "Intel X25-M SSD",
    "INTEL SSDSA2MH(080|160)G1.*",  // G1 = first generation, 50nm
    "", "",
    "-v 225,raw48,Host_Writes_Count"
  },
  { "Intel X25-M SSD",
    "INTEL SSDSA2M(080|160)G2.*",  // G2 = second generation, 34nm
    "", "",
    "-v 225,raw48,Host_Writes_Count"
  },
  { "Transcend IDE Solid State Drive",
    "TS(8|16|32|64|128)GSSD25-(M|S)",
    "", "", ""
  },
  { "Transcend SATA Solid State Drive",
    "TS(8|16|32|64|128|192)GSSD25S-(M|S)",
    "", "",
    "-v 229,hex64,Halt_System_ID "
    "-v 232,hex64,Firmware_Version_information "
    "-v 233,hex64,ECC_Fail_Record "
    "-v 234,raw24/raw24,Erase_Count_Avg/Max "
    "-v 235,raw24/raw24,Block_Count_Good/System"
  },
  { "Transcend Ultra Series Solid State Drive (SATA II)",
    "TS(60|120)GSSD25D-M",
    "", "", ""
  },
  { "Marvell SSD SD88SA024BA0 (SUN branded)",
    "MARVELL SD88SA024BA0 SUN24G 0902M0054V",
    "", "", ""
  },
  { "HP 1TB SATA disk GB1000EAFJL",
    "GB1000EAFJL",
    "", "", ""
  },
  { "HP 500GB SATA disk MM0500EANCR",
    "MM0500EANCR",
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
    "http://www.ibm.com/pc/support/site.wss/MIGR-42215.html",
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
    "http://www.ibm.com/pc/support/site.wss/MIGR-42215.html",
    ""
  },
  { "", // ExcelStor J240, J340, J360, J680, J880 and J8160
    "ExcelStor Technology J(24|34|36|68|88|816)0",
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
    "", "",
    "-v 240,raw48,Transfer_Error_Rate"
  },
  { "Fujitsu MHW2 BH series",
    "FUJITSU MHW2(04|06|08|10|12|16)0BH.*",
    "", "", ""
  },
  { "Fujitsu MHW2 BJ series",
    "FUJITSU MHW2(08|12|16)0BJ.*",
    "", "", ""
  },
  { "Fujitsu MHZ2 BH series",
    "FUJITSU MHZ2(04|08|12|16|20|25|32)0BH.*",
    "", "", ""
  },
  { "Fujitsu MHZ2 BJ series",
    "FUJITSU MHZ2(08|12|16|20|25|32)0BJ.*",
    "",
    "",
    "-v 9,minutes"
  },
  { "Fujitsu MHZ2 BS series",
    "FUJITSU MHZ2(12|25)0BS.*",
    "", "", ""
  },
  { "Fujitsu MHZ2 BK series",
    "FUJITSU MHZ2(08|12|16|25)0BK.*",
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
  { "SAMSUNG SpinPoint V80 series", // tested with SV1604N/TR100-23
    "SAMSUNG SV(0211|0401|0612|0802|1203|1604)N",
    "",
    "",
    "-v 9,halfminutes -F samsung2"
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
  { "SAMSUNG SpinPoint F1 DT series", // tested with HD103UJ/1AA01113
    "SAMSUNG HD(083G|16[12]G|25[12]H|32[12]H|50[12]I|642J|75[23]L|10[23]U)J",
    "", "", ""
  },
  { "SAMSUNG SpinPoint F1 RE series", // tested with HE103UJ/1AA01113
    "SAMSUNG HE(252H|322H|502I|642J|753L|103U)J",
    "", "", ""
  },
  { "SAMSUNG SpinPoint F2 EG series", // tested with HD154UI/1AG01118
    "SAMSUNG HD(502H|10[23]S|15[34]U)I",
    "", "", ""
  },
  { "SAMSUNG SpinPoint F3 series", // tested with HD502HJ/1AJ100E4
    "SAMSUNG HD(502H|754J|103S)J",
    "", "", ""
  },
  { "SAMSUNG SpinPoint F3 EG series", // tested with HD503HI/1AJ100E4
    "SAMSUNG HD(253G|(324|503)H|754J|105S)I",
    "", "", ""
  },
  { "SAMSUNG SpinPoint S250 series", // tested with HD200HJ/KF100-06
    "SAMSUNG HD(162|200|250)HJ",
    "", "", ""
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
    "-v 9,halfminutes -v 198,increasing"
  },
  { "SAMSUNG SpinPoint P80 series", // firmware *-23...24, tested with
    // SP0802N/TK100-23,
    // SP1213N/TL100-23,
    // SP1604N/TM100-23 and -24
    "SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]",
    ".*-2[34]",
    "",
    "-v 9,halfminutes -F samsung2"
  },
  { "SAMSUNG SpinPoint P80 series", // unknown firmware
    "SAMSUNG SP(0451|08[0124]2|12[0145]3|16[0145]4)[CN]",
    "",
    "May need -F samsung2 or -F samsung3 enabled; see manual for details.",
    ""
  },
  { "SAMSUNG SpinPoint M40/60/80 series", // tested with HM160JI/AD100-16
    "SAMSUNG HM(0[468]0H|1[026]0J)[CI]",
    "",
    "",
    "-v 9,halfminutes"
  },
  { "SAMSUNG SpinPoint M5 series", // tested with HM160HI/HH100-12
    "SAMSUNG HM((061|080)G|(121|160)H|250J)I",
    "", "", ""
  },
  { "SAMSUNG SpinPoint M7 series", // tested with HM500JI/2AC101C4
    "SAMSUNG HM(250H|320I|[45]00J)I",
    "", "", ""
  },
  { "SAMSUNG SpinPoint M series", // tested with MP0402H/UC100-11
    "SAMSUNG MP0(302|402|603|804)H",
    "",
    "",
    "-v 9,halfminutes"
  },
/*
  // TODO: Make the entries below more specific.
  // These entries produce misleading results, because newer
  // Samsung disks reuse the version numbers *-NN.
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
*/
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
    "Maxtor 9(2732U8|2390U7|204[09]U6|1707U5|1366U4|1024U3|0845U3|0683U2)",
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
    "MAXTOR STM3(160215|(250|320)820|320620|500630)AS?",
    "", "", ""
  },
  { "Seagate Maxtor DiamondMax 22", // fixed firmware
    "(MAXTOR )?STM3(500320|750330|1000340)AS?",
    "MX1A", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207969
    "", ""
  },
  { "Seagate Maxtor DiamondMax 22", // fixed firmware
    "(MAXTOR )?STM3(160813|320614|640323|1000334)AS?",
    "MX1B", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207975
    "", ""
  },
  { "Seagate Maxtor DiamondMax 22", // buggy firmware
    "(MAXTOR )?STM3(500320|750330|1000340)AS?",
    "MX15",
    "There are known problems with these drives,\n"
    "AND THIS FIRMWARE VERSION IS AFFECTED,\n"
    "see the following Seagate web pages:\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207931\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207969",
    ""
  },
  { "Seagate Maxtor DiamondMax 22", // unknown firmware
    "(MAXTOR )?STM3(160813|32061[34]|500320|640323|750330|10003(34|40))AS?",
    "",
    "There are known problems with these drives,\n"
    "see the following Seagate web pages:\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207931\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207969\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207975",
    ""
  },
  { "Seagate Maxtor DiamondMax 23",
    "STM3((160|250)31|(320|500)41|(750|1000)52)8AS?",
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
  { "Hitachi Endurastar J4K30/N4K30",
    "HE[JN]4230[23]0F9AT00",
    "",
    "",
    "-v 9,minutes -v 193,loadunload"
  },
  { "Hitachi Travelstar C4K60 family",  // 1.8" slim drive
    "HTC4260[23]0G5CE00|HTC4260[56]0G8CE00",
    "",
    "",
    "-v 9,minutes -v 193,loadunload"
  },
  { "IBM Travelstar 4GT family",
    "IBM-DTCA-2(324|409)0",
    "", "", ""
  },
  { "IBM Travelstar 6GN family",
    "IBM-DBCA-20(324|486|648)0",
    "", "", ""
  },
  { "IBM Travelstar 25GS, 18GT, and 12GN family",
    "IBM-DARA-2(25|18|15|12|09|06)000",
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
  { "Hitachi Travelstar 4K120",
    "(Hitachi )?(HTS4212(60|80|10|12)H9AT00|HTS421260G9AT00)",
    "", "", ""
  },
  { "Hitachi Travelstar 5K80",
    "(Hitachi )?HTS5480[8642]0M9AT00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K100",
    "(Hitachi )?HTS5410[1864]0G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar E5K100",
    "(Hitachi )?HTE541040G9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K120",
    "(Hitachi )?HTS5412(60|80|10|12)H9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K160",
    "(Hitachi |HITACHI )?HTS5416([468]0|1[26])J9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar E5K160",
    "(Hitachi )?HTE5416(12|16|60|80)J9(AT|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K250",
    "(Hitachi |HITACHI )?HTS5425(80|12|16|20|25)K9(A3|SA)00",
    "", "", ""
  },
  { "Hitachi Travelstar 5K320",
    "(Hitachi |HITACHI )?HT(S|E)5432(80|12|16|25|32)L9(A3(00)?|SA01)",
    "", "", ""
  },
  { "Hitachi Travelstar 5K500.B",
    "(Hitachi )?HT[ES]5450(12|16|25|32|40|50)B9A30[01]",
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
  { "Hitachi Travelstar 7K320", // tested with HTS723225L9A360/FCDOC30F
    "(Hitachi )?HT[ES]7232(80|12|16|25|32)L9(A300|A360|SA61)",
    "", "", ""
  },
  { "Hitachi Travelstar 7K500",
    "(Hitachi )?HT[ES]7250(12|16|25|32|50)A9A36[45]",
    "", "", ""
  },
  { "IBM Deskstar 14GXP and 16GP series",
    "IBM-DTTA-3(7101|7129|7144|5032|5043|5064|5084|5101|5129|5168)0",
    "", "", ""
  },
  { "IBM Deskstar 25GP and 22GXP family",
    "IBM-DJNA-3(5(101|152|203|250)|7(091|135|180|220))0",
    "", "", ""
  },
  { "IBM Deskstar 37GP and 34GXP family",
    "IBM-DPTA-3(5(375|300|225|150)|7(342|273|205|136))0",
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
  { "Hitachi Deskstar 7K80",
    "(Hitachi )?HDS7280([48]0PLAT20|(40)?PLA320|80PLA380).*",
    "", "", ""
  },
  { "Hitachi Deskstar 7K160",
    "(Hitachi )?HDS7216(80|16)PLA[3T]80.*",
    "", "", ""
  },
  { "Hitachi Deskstar 7K250",
    "(Hitachi )?HDS7225((40|80|12|16)VLAT20|(12|16|25)VLAT80|(80|12|16|25)VLSA80)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K250 (SUN branded)",
    "HITACHI HDS7225SBSUN250G.*",
    "", "", ""
  },
  { "Hitachi Deskstar T7K250",
    "(Hitachi )?HDT7225((25|20|16)DLA(T80|380))",
    "", "", ""
  },
  { "Hitachi Deskstar 7K400",
    "(Hitachi )?HDS724040KL(AT|SA)80",
    "", "", ""
  },
  { "Hitachi Deskstar 7K500",
    "(Hitachi )?HDS725050KLA(360|T80)",
    "", "", ""
  },
  { "Hitachi Deskstar P7K500",
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
  { "Hitachi Deskstar 7K1000.B",
    "(Hitachi )?HDT7210((16|25)SLA380|(32|50|64|75|10)SLA360)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K1000.C",
    "(Hitachi )?HDS7210((16|25)CLA382|(32|50)CLA362|(64|75|10)CLA332)",
    "", "", ""
  },
  { "Hitachi Deskstar 7K2000",
    "Hitachi HDS722020ALA330",
    "", "", ""
  },
  { "Hitachi Ultrastar 7K1000",
    "(Hitachi )?HUA7210(50|75|10)KLA330",
    "", "", ""
  },
  { "Hitachi Ultrastar A7K2000",
    "(Hitachi )?HUA7220((50|10)C|20A)LA33[01]",
    "", "", ""
  },
  { "Toshiba 2.5\" HDD series (10-20 GB)",
    "TOSHIBA MK(101[67]GAP|15[67]GAP|20(1[678]GAP|(18|23)GAS))",
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
  { "Toshiba MKx019GAXB (SUN branded)",
    "TOS MK[34]019GAXB SUN[34]0G",
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
  { "Seagate Momentus 5400.4 series",
    "ST9(120817|(160|200|250)827)AS",
    "", "", ""
  },
  { "Seagate Momentus 5400.5 series",
    "ST9((80|120|160)310|(250|320)320)AS",
    "", "", ""
  },
  { "Seagate Momentus 5400.6 series",
    "ST9(80313|160(301|314)|(12|25)0315|250317|(320|500)325|500327|640320)ASG?",
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
  { "Seagate Momentus 7200.4 series",
    "ST9(160412|250410|320423|500420)ASG?",
    "", "", ""
  },
  { "Seagate Momentus 7200 FDE.2 series",
    "ST9((160413|25041[12]|320426|50042[12])AS|(16041[89]|2504[16]4|32042[67]|500426)ASG)",
    "", "", ""
  },
  { "Seagate Medalist 1010, 1720, 1721, 2120, 3230 and 4340",  // ATA2, with -t permissive
    "ST3(1010|1720|1721|2120|3230|4340)A",
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
  { "Seagate U8 family",
    "ST3(4313|6811|8410|13021|17221)A",
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
  { "Seagate Barracuda 7200.11 family", // unaffected firmware
    "ST3(160813|320[68]13|500[368]20|640[36]23|640[35]30|750[36]30|1000(333|[36]40)|1500341)AS?",
    "CC.?.?", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207957
    "", ""
  },
  { "Seagate Barracuda 7200.11 family", // fixed firmware
    "ST3(500[368]20|750[36]30|1000340)AS?",
    "SD1A", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207951
    "", ""
  },
  { "Seagate Barracuda 7200.11 family", // fixed firmware
    "ST3(160813|320[68]13|640[36]23|1000333|1500341)AS?",
    "SD[12]B", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207957
    "", ""
  },
  { "Seagate Barracuda 7200.11 family", // buggy firmware
    "ST3(500[368]20|640[35]30|750[36]30|1000340)AS?",
    "(AD14|SD1[5-9])",
    "There are known problems with these drives,\n"
    "AND THIS FIRMWARE VERSION IS AFFECTED,\n"
    "see the following Seagate web pages:\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207931\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207951",
    ""
  },
  { "Seagate Barracuda 7200.11 family", // unknown firmware
    "ST3(160813|320[68]13|500[368]20|640[36]23|640[35]30|750[36]30|1000(333|[36]40)|1500341)AS?",
    "",
    "There are known problems with these drives,\n"
    "see the following Seagate web pages:\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207931\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207951\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207957",
    ""
  },
  { "Seagate Barracuda 7200.12 family",
    "ST3((160|250)318|(320|500)418|500410|(750|1000)528)AS",
    "", "", ""
  },
  { "Seagate Barracuda ES",
    "ST3(250[68]2|32062|40062|50063|75064)0NS",
    "", "", ""
  },
  { "Seagate Barracuda ES.2", // fixed firmware
    "ST3(25031|50032|75033|100034)0NS",
    "SN[01]6", // http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207963
    "", ""
  },
  { "Seagate Barracuda ES.2", // unknown firmware
    "ST3(25031|50032|75033|100034)0NS",
    "",
    "There are known problems with these drives,\n"
    "see the following Seagate web pages:\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207931\n"
    "http://seagate.custkb.com/seagate/crm/selfservice/search.jsp?DocId=207963",
    ""
  },
  { "Seagate Barracuda LP",
    "ST3(500412|1000520|1500541|2000542)AS",
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
    "WDC WD(3|4|6|8|25)00AB-.*",
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
  { "Western Digital RE2-GP family",
    "WDC WD(5000AB|7500AY|1000FY)PS-.*",
    "", "", ""
  },
  { "Western Digital RE3 Serial ATA family",
    "WDC WD((25|32|50)02A|(75|10)02F)BYS-.*",
    "", "", ""
  },
  { "Western Digital Caviar Green family",
    "WDC WD((50|64|75)00AA(C|V)S|(50|64|75)00AADS|10EA(C|V)S|(10|15|20)EADS)-.*",
    "", "", ""
  },
  { "Western Digital Caviar Green (Adv. Format) family",
    "WDC WD((64|80)00A|(10|15|20)E)ARS-.*",
    "", "", ""
  },
  { "Western Digital Caviar Black family",
    "WDC WD((500|640|750)1AA|1001FA)LS-.*",
    "", "", ""
  },
  { "Western Digital AV ATA family",
    "WDC WD(8|16|50)00AV(B|J)B-.*",
    "", "", ""
  },
  { "Western Digital AV-GP family",
    "WDC WD((16|25|32|50|64|75)00AVVS|(50|75)00AVCS|10EVVS|(10|20)EVCS|(10|15|20)EVDS)-.*",
    "", "", ""
  },
  { "Western Digital Raptor family",
    "WDC WD((360|740|800)GD|(360|740|800|1500)ADF[DS])-.*",
    "", "", ""
  },
  { "Western Digital Raptor X",
    "WDC WD1500AHFD-.*",
    "", "", ""
  },
  { "Western Digital VelociRaptor family",
    "WDC WD(800H|(1500|3000)[BH]|1600H|3000G)LFS-.*",
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
    "WDC WD((4|6|8|10|12|16|25)00BEVS|(8|12|16|25|32|40|50|64)00BEVT|7500KEVT|10TEVT)-.*",
    "", "", ""
  },
  { "Western Digital Scorpio Black Serial ATA family",
    "WDC WD(8|12|16|25|32)00B[EJ]KT-.*",
    "", "", ""
  },
  { "Western Digital My Passport Essential hard drive (USB interface)",
    "WDC WD(25|32|40|50)00BMVU-.*",
    "", "", ""
  },
  { "Western Digital My Passport Essential SE hard drive (USB interface)",
    "WDC WD(7500K|10T)MVV-.*",
    "", "", ""
  },
  { "Western Digital My Passport hard drive (USB interface)",
    "WDC WD3200BMVV-.*",
    "", "", ""
  },
  { "Quantum Bigfoot series",
    "QUANTUM BIGFOOT TS10.0A",
    "", "", ""
  },
  { "Quantum Fireball lct15 series",
    "QUANTUM FIREBALLlct15 ([123]0|22)",
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
    "QUANTUM FIREBALL CR(4.3|6.4|8.4|13.0)A",
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
    "QUANTUM FIREBALLP LM(10.2|15|20.[45]|30)",
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

  ////////////////////////////////////////////////////
  // USB ID entries
  ////////////////////////////////////////////////////

  // ALi
  { "USB: ; ALi M5621", // USB->PATA
    "0x0402:0x5621",
    "",
    "",
    "" // unsupported
  },
  // Buffalo / Melco
  { "USB: Buffalo JustStore Portable HD-PVU2; ",
    "0x0411:0x0181",
    "",
    "",
    "-d sat"
  },
  // Cypress
  { "USB: ; Cypress CY7C68300A (AT2)",
    "0x04b4:0x6830",
    "0x0001",
    "",
    "" // unsupported
  },
  { "USB: ; Cypress CY7C68300B/C (AT2LP)",
    "0x04b4:0x6830",
    "0x0240",
    "",
    "-d usbcypress"
  },
  // Myson Century
  { "USB: ; Myson Century CS8818",
    "0x04cf:0x8818",
    "0xb007",
    "",
    "" // unsupported
  },
  // Samsung
  { "USB: Samsung Story Station; ",
    "0x04e8:0x5f06",
    "",
    "",
    "-d sat"
  },
  // Sunplus
  { "USB: ; SunPlus SPDIF215",
    "0x04fc:0x0c15",
    "0xf615",
    "",
    "-d usbsunplus"
  },
  { "USB: ; SunPlus SPDIF225", // USB+SATA->SATA
    "0x04fc:0x0c25",
    "0x0103",
    "",
    "-d usbsunplus"
  },
  // Iomega
  { "USB: Iomega LPHD080-0; ",
    "0x059b:0x0272",
    "",
    "",
    "-d usbcypress"
  },
  { "USB: Iomega MDHD500-U; ",
    "0x059b:0x0275",
    "0x0001",
    "",
    "" // unsupported
  },
  { "USB: Iomega LDHD-UP; Sunplus",
    "0x059b:0x0370",
    "",
    "",
    "-d usbsunplus"
  },
  // LaCie
  { "USB: LaCie hard disk (FA Porsche design);",
    "0x059f:0x0651",
    "",
    "",
    "" // unsupported
  },
  { "USB: LaCie hard disk; JMicron",
    "0x059f:0x0951",
    "",
    "",
    "-d usbjmicron"
  },
  { "USB: LaCie hard disk (Neil Poulton design);",
    "0x059f:0x1018",
    "",
    "",
    "-d sat"
  },
  { "USB: LaCie Desktop Hard Drive; JMicron",
    "0x059f:0x1019",
    "",
    "",
    "-d usbjmicron"
  },
  { "USB: LaCie Rugged Hard Drive; JMicron",
    "0x059f:0x101d",
    "0x0001",
    "",
    "-d usbjmicron,x"
  },
  // In-System Design
  { "USB: ; In-System/Cypress ISD-300A1",
    "0x05ab:0x0060",
    "0x1101",
    "",
    "-d usbcypress"
  },
  // Genesys Logic
  { "USB: ; Genesys Logic GL881E",
    "0x05e3:0x0702",
    "",
    "",
    "" // unsupported
  },
  { "USB: ; Genesys Logic", // TODO: requires '-T permissive'
    "0x05e3:0x0718",
    "0x0041",
    "",
    "-d sat"
  },
  // Prolific
  { "USB: ; Prolific PL2507", // USB->PATA
    "0x067b:0x2507",
    "",
    "",
    "" // unsupported
  },
  { "USB: ; Prolific PL3507", // USB+IEE1394->PATA
    "0x067b:0x3507",
    "0x0001",
    "",
    "" // unsupported
  },
  // Freecom
  { "USB: Freecom Hard Drive XS; Sunplus",
    "0x07ab:0xfc8e",
    "0x010f",
    "",
    "-d usbsunplus"
  },
  // Toshiba
  { "USB: Toshiba PX1270E-1G16; Sunplus",
    "0x0930:0x0b03",
    "",
    "",
    "-d usbsunplus"
  },
  { "USB: Toshiba PX1396E-3T01; Sunplus", // similar to Dura Micro 501
    "0x0930:0x0b09",
    "",
    "",
    "-d usbsunplus"
  },
  // Seagate
  { "USB: Seagate FreeAgent Go; ",
    "0x0bc2:0x2(000|100|101)",
    "",
    "",
    "-d sat"
  },
  { "USB: Seagate FreeAgent Go FW; ",
    "0x0bc2:0x2200",
    "",
    "",
    "-d sat"
  },
  { "USB: Seagate Expansion Portable; ",
    "0x0bc2:0x2300",
    "",
    "",
    "-d sat"
  },
  { "USB: Seagate FreeAgent Desktop; ",
    "0x0bc2:0x3000",
    "",
    "",
    "-d sat"
  },
  { "USB: Seagate FreeAgent Desk; ",
    "0x0bc2:0x3001",
    "",
    "",
    "-d sat"
  },
  // Dura Micro
  { "USB: Dura Micro 509; Sunplus",
    "0x0c0b:0xb159",
    "0x0103",
    "",
    "-d usbsunplus"
  },
  // Maxtor
  { "USB: Maxtor OneTouch; ",
    "0x0d49:0x7300",
    "0x0121",
    "",
    "-d sat"
  },
  { "USB: Maxtor OneTouch 4; ",
    "0x0d49:0x7310",
    "0x0125",
    "",
    "-d sat"
  },
  { "USB: Maxtor OneTouch 4 Mini; ",
    "0x0d49:0x7350",
    "0x0125",
    "",
    "-d sat"
  },
  { "USB: Maxtor Basics Desktop; ",
    "0x0d49:0x7410",
    "0x0122",
    "",
    "-d sat"
  },
  { "USB: Maxtor Basics Portable; ",
    "0x0d49:0x7450",
    "0x0122",
    "",
    "-d sat"
  },
  // Western Digital
  { "USB: WD My Passport (IDE); Cypress",
    "0x1058:0x0701",
    "0x0240",
    "",
    "-d usbcypress"
  },
  { "USB: WD My Passport Portable; ",
    "0x1058:0x0702",
    "0x0102",
    "",
    "-d sat"
  },
  { "USB: WD My Passport Essential; ",
    "0x1058:0x0704",
    "0x0175",
    "",
    "-d sat"
  },
  { "USB: WD My Passport Elite; ",
    "0x1058:0x0705",
    "0x0175",
    "",
    "-d sat"
  },
  { "USB: WD My Passport 070A; ",
    "0x1058:0x070a",
    "0x1028",
    "",
    "-d sat"
  },
  { "USB: WD My Book ES; ",
    "0x1058:0x0906",
    "0x0012",
    "",
    "-d sat"
  },
  { "USB: WD Elements Desktop; ",
    "0x1058:0x1001",
    "0x0104",
    "",
    "-d sat"
  },
  { "USB: WD Elements Desktop WDE1UBK...; ",
    "0x1058:0x1003",
    "0x0175",
    "",
    "-d sat"
  },
  { "USB: WD Elements; ",
    "0x1058:0x1010",
    "0x0105",
    "",
    "-d sat"
  },
  { "USB: WD Elements Desktop; ", // 2TB
    "0x1058:0x1021",
    "0x2002",
    "",
    "-d sat"
  },
  { "USB: WD My Book Essential; ",
    "0x1058:0x1100",
    "0x0165",
    "",
    "-d sat"
  },
  { "USB: WD My Book; ",
    "0x1058:0x1102",
    "0x1028",
    "",
    "-d sat"
  },
  { "USB: WD My Book Essential; ",
    "0x1058:0x1110",
    "0x1030",
    "",
    "-d sat"
  },
  // A-DATA
  { "USB: A-DATA SH93; Cypress",
    "0x125f:0xa93a",
    "0x0150",
    "",
    "-d usbcypress"
  },
  // Initio
  { "USB: ; Initio 316000",
    "0x13fd:0x0540",
    "",
    "",
    "" // unsupported
  },
  { "USB: ; Initio", // USB->SATA
    "0x13fd:0x1240",
    "0x0104",
    "",
    "-d sat"
  },
  { "USB: ; Initio", // USB+SATA->SATA
    "0x13fd:0x1340",
    "0x0208",
    "",
    "-d sat"
  },
  // JMicron
  { "USB: ; JMicron JM20329", // USB->SATA
    "0x152d:0x2329",
    "0x0100",
    "",
    "-d usbjmicron"
  },
  { "USB: ; JMicron JM20336", // USB+SATA->SATA, USB->2xSATA
    "0x152d:0x2336",
    "0x0100",
    "",
    "-d usbjmicron,x"
  },
  { "USB: ; JMicron JM20337/8", // USB->SATA+PATA, USB+SATA->PATA
    "0x152d:0x2338",
    "0x0100",
    "",
    "-d usbjmicron"
  },
  { "USB: ; JMicron JM20339", // USB->SATA
    "0x152d:0x2339",
    "0x0100",
    "",
    "-d usbjmicron,x"
  },
  { "USB: ; JMicron", // USB->SATA
    "0x152d:0x2352",
    "0x0100",
    "",
    "-d usbjmicron,x"
  },
  // Verbatim
  { "USB: Verbatim FW/USB160; Oxford OXUF934SSA-LQAG", // USB+IEE1394->SATA
    "0x18a5:0x0215",
    "0x0001",
    "",
    "-d sat"
  },
  { "USB: Verbatim External Hard Drive 47519; Sunplus", // USB->SATA
    "0x18a5:0x0216",
    "",
    "",
    "-d usbsunplus"
  },
  // SunplusIT
  { "USB: ; SunplusIT",
    "0x1bcf:0x0c31",
    "",
    "",
    "-d usbsunplus"
  },
  // Hitachi/SimpleTech
  { "USB: Hitachi/SimpleTech; JMicron", // 1TB
    "0x4971:0xce17",
    "",
    "",
    "-d usbjmicron,x"
  },
  // OnSpec
  { "USB: ; OnSpec", // USB->PATA
    "0x55aa:0x2b00",
    "0x0100",
    "",
    "" // unsupported
  },
/*
}; // builtin_knowndrives[]
 */
