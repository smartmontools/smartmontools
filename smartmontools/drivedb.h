/*
 * drivedb.h - smartmontools drive database file
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-11 Philip Williams, Bruce Allen
 * Copyright (C) 2008-16 Christian Franke
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
  {
    "StorFly CFast SATA 6Gbps SSDs",
    // http://datasheet.octopart.com/VSFCS2CC060G-100-Virtium-datasheet-82287733.pdf
    // tested with StorFly VSFCS2CC060G-100/0409-000
    "StorFly VSFCS2C[CI](016|030|060|120|240)G-...",
    // C - commercial, I industrial
    "", "",
    "-v 192,raw48,Unsafe_Shutdown_Count "
    "-v 160,raw48,Uncorrectable_Error_Cnt "
    // 0729 - remaining in block life. In 0828  remaining is normalized to 100% then decreases
    "-v 161,raw48,Spares_Remaining " 
    "-v 241,raw48,Host_Writes_32MiB "
    "-v 242,raw48,Host_Reads_32MiB "
    "-v 169,raw48,Lifetime_Remaining% "
    "-v 248,raw48,Lifetime_Remaining% " //  later then 0409 FW.
    "-v 249,raw48,Spares_Remaining_Perc " //  later then 0409 FW.
  },
  {
    "StorFly TuffDrive eUSB", 
    ".*VTDU31X[CI]008G-A90[12].*",
    // X - XE Class
    // I - Industrial
    "", "",
    "-v 12,raw48,Power_Cycle_Count "
    "-v 196,raw48,Reallocated_Event_Count "
    "-v 199,raw48,UDMA_CRC_Error_Count "
    "-v 203,raw48,Total_ECC_Errors "
    "-v 204,raw48,Corrected_ECC_Error "
    "-v 213,raw48,Spare_Worst_Threshold "
    "-v 214,raw48,Anchor_Block_Status "
    "-v 215,raw48,Trim_Status "
    "-v 229,raw48,Erase_Count "
    "-v 232,raw48,Total_Number_of_Reads "
    "-v 241,raw48,Total_LBAs_Written "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 249,raw48,Total_NAND_Written "
  },
  {
    "StorFly SATA 6Gbps pSLC SSD", 
    ".*VSF205[XC][CI](050|100)G-A902.*|"  // SlimSATA
    ".*VSFDM[4|6|8][XC][CI](050|100)G-A902.*",  // M.2
    // X - XE Class, C - CE Class
    // C - commercial, I industrial
    "0705-000", "",
    "-v 1,raw48,Raw_Read_Error_Rate " //
    "-v 5,raw48,Reallocated_Blk_Cnt "
    "-v 9,raw24(raw8),Pwr_On_Hours "
    "-v 12,raw48,Pwr_Cycle_Cnt "
    "-v 14,raw48,Device_Raw_Capacity "
    "-v 15,raw48,Device_User_Capacity "
    "-v 16,raw48,pSLC_Spare_Blk_Cnt "
    "-v 17,raw48,pSLC_Spare_Blk_Left "
    "-v 100,raw48,Total_pSLC_Erase_Cnt "
    "-v 168,raw48,SATA_PHY_Error_Cnt "
    "-v 170,raw48,Total_Bad_Blk_Cnt "
    "-v 172,raw48,Total_Blk_Erase_Failures "
    "-v 173,raw48,Max_pSLC_PE_Cycle_Cnt "
    "-v 174,raw48,Unexpected_Pwr_Loss_Cnt "
    "-v 175,raw48,Average_pSLC_Erase_Cnt "
    "-v 176,raw48,pSLC_NAND_PE_Endurance "
    "-v 181,raw48,Total_Program_Fail "
    "-v 187,raw48,Uncorrectable_Error_Cnt "
    "-v 194,raw48,Temperature_Celsius "
    "-v 198,raw48,RAID_Recovery_Cnt "
    "-v 199,raw48,UDMA_CRC_Error_Cnt "
    "-v 202,raw48,Drive_Live_Used "
    "-v 232,raw48,RAID_Trigger_Cnt "
    "-v 234,raw48,Total_Flash_Sectors_Read "
    "-v 235,raw48,Total_Flash_Sectors_Wrtn "
    "-v 241,raw48,Total_LBAs_Wrtn "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 248,raw48,Remaining_Life_Left "
    "-v 249,raw48,Remaining_Spare_Blk_Cnt "
  },
  {
    "StorFly SATA 6Gbps TLC SSD", 
    ".*VSF205CI100G-A902.*|"  // SlimSATA
    ".*VSFDM6CC400G-A901.*|"  // M.2
    ".*VSFDM8CC960G-A901.*|"  // M.2
    ".*VSFDM8CC100G-A902.*|"  // M.2
    ".*VSFDM4CC200G-A902.*",
    // X - XE Class, C - CE Class
    // C - commercial, I industrial
    "", "",
    "-v 1,raw48,Raw_Read_Error_Rate " //
    "-v 5,raw48,Reallocated_Blk_Cnt "
    "-v 9,raw24(raw8),Pwr_On_Hours "
    "-v 12,raw48,Pwr_Cycle_Cnt "
    "-v 14,raw48,Device_Raw_Capacity "
    "-v 15,raw48,Device_User_Capacity "
    "-v 16,raw48,TLC_Spare_Blk_Cnt "
    "-v 17,raw48,TLC_Spare_Blk_Left "
    "-v 100,raw48,Total_TLC_Erase_Cnt "
    "-v 168,raw48,SATA_PHY_Error_Cnt "
    "-v 170,raw48,Total_Bad_Blk_Cnt "
    "-v 172,raw48,Total_Blk_Erase_Failures "
    "-v 173,raw48,Max_TLC_PE_Cycle_Cnt "
    "-v 174,raw48,Unexpected_Pwr_Loss_Cnt "
    "-v 175,raw48,Average_TLC_Erase_Cnt "
    "-v 181,raw48,Total_Program_Fail "
    "-v 187,raw48,Uncorrectable_Error_Cnt "
    "-v 194,raw48,Temperature_Celsius "
    "-v 198,raw48,RAID_Recovery_Cnt "
    "-v 199,raw48,UDMA_CRC_Error_Cnt "
    "-v 202,raw48,Drive_Live_Used "
    "-v 232,raw48,RAID_Trigger_Cnt "
    "-v 234,raw48,Total_Flash_Sectors_Read "
    "-v 235,raw48,Total_Flash_Sectors_Wrtn "
    "-v 241,raw48,Total_LBAs_Wrtn "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 248,raw48,Remaining_Life_Left "
    "-v 249,raw48,Remaining_Spare_Blk_Cnt "
  },
  {
    "StorFly SATA 6Gbps SSDs", 
    ".*VSFBM6CC025G-JUN.*|"
    ".*VSFBM6CC025G-JUN1.*",
    "0626-000", "",
    "-v 1,raw24/raw32,Raw_Read_Error_Rate "
    "-v 5,raw48,Reallocated_Sector_Count "
    "-v 9,raw24(raw8),Power_On_Hours "
    "-v 160,raw48,Uncorrectable_Count "
    "-v 161,raw48,Spares_Remaining "
    "-v 169,raw48,Remaining_Life_Left "
    "-v 194,raw48,Temperature "
    "-v 199,raw48,CRC_Error_Count "
    "-v 241,raw48,Total_LBAs_Written "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 248,raw48,Remaining_Life_Left "
    "-v 249,raw48,Spare_Blocks_Remaining "
  },
  {
    "StorFly SATA 6Gbps SSDs", 
    ".*VSFBM6CC400G-JUN.*|"
    ".*VSFBM6CC400G-JUN1.*",
    "0629-000", "",
    "-v 1,raw24/raw32,Raw_Read_Error_Rate "
    "-v 5,raw48,Reallocated_Sector_Count "
    "-v 9,raw24(raw8),Power_On_Hours "
    "-v 12,raw48,Power_Cycle_Count "
    "-v 16,raw48,Valid_Spare_Blocks "
    "-v 17,raw48,Remaining_Spare_Blocks "
    "-v 160,raw48,Uncorrectable_Count "
    "-v 161,raw48,Spares_Remaining "
    "-v 163,raw48,Initial_Invalid_Blocks "
    "-v 164,raw48,Total_Erase_Count "
    "-v 165,raw48,Maximum_Erase_Count "
    "-v 166,raw48,Minimum_Erase_Count "
    "-v 167,raw48,Average_Erase_Count "
    "-v 168,raw48,NAND_Endurance "
    "-v 177,raw48,Total_Wear_Level_Count "
    "-v 178,raw48,Runtime_Invalid_Blocks "
    "-v 181,raw48,Total_Program_Fail "
    "-v 182,raw48,Total_Erase_Fail "
    "-v 192,raw48,Unsafe_Power_Down "
    "-v 194,raw48,Temperature "
    "-v 195,raw48,Hardware_ECC_Recovered "
    "-v 196,raw48,Reallocation_Event_Count "
    "-v 198,raw48,Offline_Uncorrectable "
    "-v 199,raw48,CRC_Error_Count "
    "-v 231,raw48,Remaining_Life_Left "
    "-v 232,raw48,Available_Reserve_Space "
    "-v 241,raw48,Total_LBAs_Written "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 248,raw48,Remaining_Life_Left "
    "-v 249,raw48,Spare_Blocks_Remaining "
  },
  {
    "StorFly SATA 6Gbps SSDs", 
    // tested with Virtium StorFly VSFB25CC050G-JUN/0202-000
    "StorFly VSFBM8C[CI](060|120|240)G-...|"
    "StorFly VSFBM8C[CI](050|100|200)G-...", 
    // C - commercial, I industrial
    "0202-000", "",
    "-v 1,raw24/raw32,Raw_Read_Error_Rate "
    "-v 5,raw48,Reallocated_Sector_Count "
    "-v 9,raw24(raw8),Power_On_Hours "
    "-v 12,raw48,Power_Cycle_Count "
    "-v 160,raw48,Uncorrectable_Count "
    "-v 161,raw48,Spares_Remaining "
    "-v 163,raw48,Initial_Invalid_Blocks "
    "-v 164,raw48,Total_Erase_Count "
    "-v 165,raw48,Maximum_Erase_Count "
    "-v 166,raw48,Minimum_Erase_Count "
    "-v 167,raw48,Average_Erase_Count "
    "-v 168,raw48,NAND_Endurance "
    "-v 177,raw48,Total_Wear_Level_Count "
    "-v 178,raw48,Runtime_Invalid_Blocks "
    "-v 181,raw48,Total_Program_Fail "
    "-v 182,raw48,Total_Erase_Fail "
    "-v 187,raw48,UECC_Count "
    "-v 192,raw48,Unsafe_Power_Down "
    "-v 194,raw48,Temperature "
    "-v 195,raw48,Hardware_ECC_Recovered "
    "-v 196,raw48,Reallocation_Event_Count "
    "-v 198,raw48,Offline_Uncorrectable "
    "-v 199,raw48,CRC_Error_Count "
    "-v 232,raw48,Available_Reserve_Space "
    "-v 241,raw48,Total_LBAs_Written "
    "-v 242,raw48,Total_LBAs_Read "
    "-v 248,raw48,Remaining_Life_Left "
    "-v 249,raw48,Spare_Blocks_Remaining "
  },