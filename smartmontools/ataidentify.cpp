/*
 * ataidentify.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2012-15 Christian Franke
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

#include "config.h"
#include "ataidentify.h"

const char * ataidentify_cpp_cvsid = "$Id$"
  ATAIDENTIFY_H_CVSID;

#include "int64.h"
#include "utility.h"


// Table 12 of X3T10/0948D (ATA-2) Revision 4c, March 18, 1996
// Table 9 of X3T13/2008D (ATA-3) Revision 7b, January 27, 1997
// Tables 11 and 13 of T13/1153D (ATA/ATAPI-4) revision 18, August 19, 1998
// Tables 20 and 22 of T13/1321D (ATA/ATAPI-5) Revision 3, February 29, 2000
// Tables 27 and 29 of T13/1410D (ATA/ATAPI-6) Revision 3b, February 26, 2002
// Tables 16 and 18 of T13/1532D (ATA/ATAPI-7) Volume 1 Revision 4b, April 21, 2004
// Tables 29 and 39 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
// Tables 50 and 61 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
// Tables 45 and 50 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
// Table 44 of T13/BSR INCITS 529 (ACS-4) Revision 08, April 28, 2015 (ATAPI removed)

const char * const identify_descriptions[] = {
  "  0 General configuration",
    ". 15 Device identifier: 0 = ATA, 1 = ATAPI",
    ". 14:8 ATA: Vendor specific [RET-3]",
    ". 14 ATAPI: Must be set to 0",
    ". 13 ATAPI: Reserved",
    ". 12:8 ATAPI: Command set: 0x05 = CD/DVD",
    ". 7 Removable media device [OBS-8]",
    ". 6 ATA: Not removable controller and/or device [OBS-6]",
    ". 5:3 ATA: Vendor specific [RET-3]",
    ". 6:5 ATAPI: DRQ after PACKET cmd: 0x0 = 3ms, 0x2 = 50us",
    ". 4:3 ATAPI: Reserved",
    ". 2 Response incomplete",
    ". 1 ATA: Vendor specific [RET-3]",
    ". 0 ATA: Reserved",
    ". 1:0 ATAPI: Packet size: 0x0 = 12 byte, 0x1 = 16 byte",

  "  1 Cylinders [OBS-6]",
  "  2 Specific configuration (0x37c8/738c/8c73/c837)",
  "  3 Heads [OBS-6]",
  "  4 Vendor specific [RET-3]",
  "  5 Vendor specific [RET-3]",
  "  6 Sectors per track [OBS-6]",
  "  7-8 Reserved for CFA (Sectors per card)",
  "  9 Vendor specific [RET-4]",
  " 10-19 Serial number (String)",
  " 20 Vendor specific [RET-3]",
  " 21 Vendor specific [RET-3]",
  " 22 Vendor specific bytes on READ/WRITE LONG [OBS-4]",
  " 23-26 Firmware revision (String)",
  " 27-46 Model number (String)",

  " 47 READ/WRITE MULTIPLE support",
    ". 15:8 Must be set to 0x80",
    ". 7:0 Maximum sectors per DRQ on READ/WRITE MULTIPLE",

  " 48 Trusted Computing feature set options",
    ". 15:14 Must be set to 0x1",
    ". 13:1 Reserved for the Trusted Computing Group",
    ". 0 Trusted Computing feature set supported",

  " 49 Capabilities",
    ". 15:14 ATA: Reserved for IDENTIFY PACKET DEVICE",
    ". 15 ATAPI: Interleaved DMA supported [OBS-8]",
    ". 14 ATAPI: Command queuing supported [OBS-8]",
    ". 13 ATA: Standard standby timer values supported",
    ". 13 ATAPI: Overlap operation supported [OBS-8]",
    ". 12 ATA: Reserved for IDENTIFY PACKET DEVICE",
    ". 12 ATAPI: ATA software reset required [OBS-5]",
    ". 11 IORDY supported",
    ". 10 IORDY may be disabled",
    ". 9 LBA supported",
    ". 8 DMA supported",
    ". 7:2 Reserved", // ATA-3: Vendor specific, ATA-8: Retired
    ". 1:0 Long Phy Sector Alignment Error reporting", // ACS-2

  " 50 Capabilities",
    ". 15:14 Must be set to 0x1",
    ". 13:2 Reserved",
    ". 1 Reserved [OBS-6]",
    ". 0 Vendor specific minimum standby timer value",

  " 51 PIO data transfer mode [OBS-5]",
  " 52 Single Word DMA data transfer mode [OBS-3]",

  " 53 Field validity / Free-fall Control",
    ". 15:8 Free-fall Control sensitivity",
    ". 7:3 Reserved",
    ". 2 Word 88 (Ultra DMA modes) is valid",
    ". 1 Words 64-70 (PIO modes) are valid",
    ". 0 Words 54-58 (CHS) are valid [OBS-6]",

  " 54 Current cylinders [OBS-6]",
  " 55 Current heads [OBS-6]",
  " 56 Current sectors per track [OBS-6]",
  " 57-58 Current capacity in sectors (DWord) [OBS-6]",

  " 59 Sanitize Device - READ/WRITE MULTIPLE support",
    ". 15 BLOCK ERASE EXT supported",
    ". 14 OVERWRITE EXT supported",
    ". 13 CRYPTO SCRAMBLE EXT supported",
    ". 12 Sanitize Device feature set supported",
    ". 11 Cmds during sanitize as specified by this standard", // ACS-3
    ". 10 SANITIZE ANTIFREEZE LOCK EXT supported", // ACS-3
    ". 9 Reserved",
    ". 8 Bits 7:0 are valid",
    ". 7:0 Current sectors per DRQ on READ/WRITE MULTIPLE",

  " 60-61 User addressable sectors for 28-bit commands (DWord)",
  " 62 Single Word DMA modes [OBS-3]",

  " 63 Multiword DMA modes",
    ". 15:11 Reserved",
    ". 10 Multiword DMA mode 2 selected",
    ". 9 Multiword DMA mode 1 selected",
    ". 8 Multiword DMA mode 0 selected",
    ". 7:3 Reserved",
    ". 2 Multiword DMA mode 2 and below supported",
    ". 1 Multiword DMA mode 1 and below supported",
    ". 0 Multiword DMA mode 0 supported",

  " 64 PIO modes",
    ". 15:2 Reserved",
    ". 1 PIO mode 4 supported",
    ". 0 PIO mode 3 supported",

  " 65 Minimum Multiword DMA cycle time per word in ns",
  " 66 Recommended Multiword DMA cycle time in ns",
  " 67 Minimum PIO cycle time without flow control in ns",
  " 68 Minimum PIO cycle time with IORDY flow control in ns",

  " 69 Additional support",
    ". 15 CFast specification supported",
    ". 14 Deterministic data after trim supported",
    ". 13 LPS Alignment Error Reporting Control supported",
    ". 12 DCO IDENTIFY/SET DMA supported [OBS-ACS-3]",
    ". 11 READ BUFFER DMA supported",
    ". 10 WRITE BUFFER DMA supported",
    ". 9 SET MAX SET PASSWORD/UNLOCK DMA supported [OBS-ACS-3]",
    ". 8 DOWNLOAD MICROCODE DMA supported",
    ". 7 Reserved for IEEE 1667",
    ". 6 Optional ATA device 28-bit commands supported",
    ". 5 Trimmed LBA range(s) returning zeroed data supported",
    ". 4 Device encrypts all user data",
    ". 3 Extended number of user addressable sectors supported",
    ". 2 All write cache is non-volatile", // ACS-3
    ". 1:0 Zoned Capabilities", // ACS-4

  " 70 Reserved",
  " 71-74 ATA: Reserved for IDENTIFY PACKET DEVICE",
  " 71 ATAPI: Time in ns from PACKET to bus release [OBS-8]",
  " 72 ATAPI: Time in ns from SERVICE to BSY cleared [OBS-8]",
  " 73-74 ATAPI: Reserved",

  " 75 Queue depth",
    ". 15:5 Reserved",
    ". 4:0 Maximum queue depth - 1",

  " 76 Serial ATA capabilities",
    ". 15 READ LOG DMA EXT as equiv to READ LOG EXT supported",
    ". 14 Device Auto Partial to Slumber transitions supported",
    ". 13 Host Auto Partial to Slumber transitions supported",
    ". 12 NCQ priority information supported",
    ". 11 Unload while NCQ commands are outstanding supported",
    ". 10 Phy Event Counters supported",
    ". 9 Receipt of host initiated PM requests supported",
    ". 8 NCQ feature set supported",
    ". 7:4 Reserved for Serial ATA",
    ". 3 SATA Gen3 signaling speed (6.0 Gb/s) supported",
    ". 2 SATA Gen2 signaling speed (3.0 Gb/s) supported",
    ". 1 SATA Gen1 signaling speed (1.5 Gb/s) supported",
    ". 0 Must be set to 0",

  " 77 Serial ATA additional capabilities", // ACS-3
    ". 15:7 Reserved for Serial ATA",
    ". 6 RECEIVE/SEND FPDMA QUEUED supported",
    ". 5 NCQ Queue Management supported",
    ". 4 NCQ Streaming supported",
    ". 3:1 Current Serial ATA signal speed",
    ". 0 Must be set to 0",

  " 78 Serial ATA features supported",
    ". 15:8 Reserved for Serial ATA",
    ". 7 NCQ Autosense supported", // ACS-3
    ". 6 Software Settings Preservation supported",
    ". 5 Hardware Feature Control supported", // ACS-3
    ". 4 In-order data delivery supported",
    ". 3 Device initiated power management supported",
    ". 2 DMA Setup auto-activation supported",
    ". 1 Non-zero buffer offsets supported",
    ". 0 Must be set to 0",

  " 79 Serial ATA features enabled",
    ". 15:8 Reserved for Serial ATA",
    ". 7 Automatic Partial to Slumber transitions enabled", // ACS-3
    ". 6 Software Settings Preservation enabled",
    ". 5 Hardware Feature Control enabled", // ACS-3
    ". 4 In-order data delivery enabled",
    ". 3 Device initiated power management enabled",
    ". 2 DMA Setup auto-activation enabled",
    ". 1 Non-zero buffer offsets enabled",
    ". 0 Must be set to 0",

  " 80 Major version number",
    ". 15:12 Reserved",
    ". 11 ACS-4 supported",
    ". 10 ACS-3 supported",
    ". 9 ACS-2 supported",
    ". 8 ATA8-ACS supported",
    ". 7 ATA/ATAPI-7 supported",
    ". 6 ATA/ATAPI-6 supported",
    ". 5 ATA/ATAPI-5 supported",
    ". 4 ATA/ATAPI-4 supported [OBS-8]",
    ". 3 ATA-3 supported [OBS-7]",
    ". 2 ATA-2 supported [OBS-6]",
    ". 1 ATA-1 supported [OBS-5]",
    ". 0 Reserved",

  " 81 Minor version number",

  " 82 Commands and feature sets supported",
    ". 15 IDENTIFY DEVICE DMA supported [OBS-4]", // ATA-4 r07-r14 only
    ". 14 NOP supported",
    ". 13 READ BUFFER supported",
    ". 12 WRITE BUFFER supported",
    ". 11 WRITE VERIFY supported [OBS-4]", // ATA-4 r07-r13 only
    ". 10 HPA feature set supported [OBS-ACS-3]",
    ". 9 DEVICE RESET supported", // ATA:0, ATAPI:1
    ". 8 SERVICE interrupt supported [OBS-ACS-2]",
    ". 7 Release interrupt supported [OBS-ACS-2]",
    ". 6 Read look-ahead supported",
    ". 5 Volatile write cache supported",
    ". 4 PACKET feature set supported", // ATA:0, ATAPI:1
    ". 3 Power Management feature set supported",
    ". 2 Removable Media feature set supported [OBS-8]",
    ". 1 Security feature set supported",
    ". 0 SMART feature set supported",

  " 83 Commands and feature sets supported",
    ". 15:14 Must be set to 0x1",
    ". 13 FLUSH CACHE EXT supported",
    ". 12 FLUSH CACHE supported",
    ". 11 DCO feature set supported [OBS-ACS-3]",
    ". 10 48-bit Address feature set supported",
    ". 9 AAM feature set supported [OBS-ACS-2]",
    ". 8 SET MAX security extension supported [OBS-ACS-3]",
    ". 7 Reserved for Addr Offset Resvd Area Boot [OBS-ACS-3]",
    ". 6 SET FEATURES subcommand required to spin-up",
    ". 5 PUIS feature set supported",
    ". 4 Removable Media Status Notification supported [OBS-8]",
    ". 3 APM feature set supported",
    ". 2 CFA feature set supported",
    ". 1 TCQ feature set supported [OBS-ACS-2]",
    ". 0 DOWNLOAD MICROCODE supported",

  " 84 Commands and feature sets supported",
    ". 15:14 Must be set to 0x1",
    ". 13 IDLE IMMEDIATE with UNLOAD feature supported",
    ". 12:11 Reserved for TLC [OBS-ACS-3]",
    ". 10 URG bit for WRITE STREAM (DMA) EXT supported [OBS-8]",
    ". 9 URG bit for READ STREAM (DMA) EXT supported [OBS-8]",
    ". 8 64-bit World Wide Name supported",
    ". 7 WRITE DMA QUEUED FUA EXT supported",
    ". 6 WRITE DMA/MULTIPLE FUA EXT supported",
    ". 5 GPL feature set supported",
    ". 4 Streaming feature set supported [OBS-ACS-3]",
    ". 3 Media Card Pass Through Command supported [OBS-ACS-2]",
    ". 2 Media serial number supported [RES-ACS-3]",
    ". 1 SMART self-test supported",
    ". 0 SMART error logging supported",

  " 85 Commands and feature sets supported or enabled",
    ". 15 IDENTIFY DEVICE DMA supported [OBS-4]", // ATA-4 r07-r14 only
    ". 14 NOP supported",
    ". 13 READ BUFFER supported",
    ". 12 WRITE BUFFER supported",
    ". 11 WRITE VERIFY supported [OBS-4]", // ATA-4 r07-r13 only
    ". 10 HPA feature set supported [OBS-ACS-3]",
    ". 9 DEVICE RESET supported", // ATA:0, ATAPI:1
    ". 8 SERVICE interrupt enabled [OBS-ACS-2]",
    ". 7 Release interrupt enabled [OBS-ACS-2]",
    ". 6 Read look-ahead enabled",
    ". 5 Write cache enabled",
    ". 4 PACKET feature set supported", // ATA:0, ATAPI:1
    ". 3 Power Management feature set supported",
    ". 2 Removable Media feature set supported [OBS-8]",
    ". 1 Security feature set enabled",
    ". 0 SMART feature set enabled",

  " 86 Commands and feature sets supported or enabled",
    ". 15 Words 119-120 are valid",
    ". 14 Reserved",
    ". 13 FLUSH CACHE EXT supported",
    ". 12 FLUSH CACHE supported",
    ". 11 DCO feature set supported [OBS-ACS-3]",
    ". 10 48-bit Address features set supported",
    ". 9 AAM feature set enabled [OBS-ACS-2]",
    ". 8 SET MAX security extension enabled [OBS-ACS-3]",
    ". 7 Reserved for Addr Offset Resvd Area Boot [OBS-ACS-3]",
    ". 6 SET FEATURES subcommand required to spin-up",
    ". 5 PUIS feature set enabled",
    ". 4 Removable Media Status Notification enabled [OBS-8]",
    ". 3 APM feature set enabled",
    ". 2 CFA feature set supported",
    ". 1 TCQ feature set supported [OBS-ACS-2]",
    ". 0 DOWNLOAD MICROCODE supported",

  " 87 Commands and feature sets supported or enabled",
    ". 15:14 Must be set to 0x1",
    ". 13 IDLE IMMEDIATE with UNLOAD FEATURE supported",
    ". 12:11 Reserved for TLC [OBS-ACS-3]",
    ". 10 URG bit for WRITE STREAM (DMA) EXT supported [OBS-8]",
    ". 9 URG bit for READ STREAM (DMA) EXT supported [OBS-8]",
    ". 8 64-bit World Wide Name supported",
    ". 7 WRITE DMA QUEUED FUA EXT supported [OBS-ACS-2]",
    ". 6 WRITE DMA/MULTIPLE FUA EXT supported",
    ". 5 GPL feature set supported",
    ". 4 Valid CONFIGURE STREAM has been executed [OBS-8]",
    ". 3 Media Card Pass Through Command supported [OBS-ACS-2]",
    ". 2 Media serial number is valid",
    ". 1 SMART self-test supported",
    ". 0 SMART error logging supported",

  " 88 Ultra DMA modes",
    ". 15 Reserved",
    ". 14 Ultra DMA mode 6 selected",
    ". 13 Ultra DMA mode 5 selected",
    ". 12 Ultra DMA mode 4 selected",
    ". 11 Ultra DMA mode 3 selected",
    ". 10 Ultra DMA mode 2 selected",
    ". 9 Ultra DMA mode 1 selected",
    ". 8 Ultra DMA mode 0 selected",
    ". 7 Reserved",
    ". 6 Ultra DMA mode 6 and below supported",
    ". 5 Ultra DMA mode 5 and below supported",
    ". 4 Ultra DMA mode 4 and below supported",
    ". 3 Ultra DMA mode 3 and below supported",
    ". 2 Ultra DMA mode 2 and below supported",
    ". 1 Ultra DMA mode 1 and below supported",
    ". 0 Ultra DMA mode 0 supported",

  " 89 SECURITY ERASE UNIT time",
    ". 15 Bits 14:8 of value are valid", // ACS-3
    ". 14:0 SECURITY ERASE UNIT time value", // value*2 minutes

  " 90 ENHANCED SECURITY ERASE UNIT time",
    ". 15 Bits 14:8 of value are valid", // ACS-3
    ". 14:0 ENHANCED SECURITY ERASE UNIT time value", // value*2 minutes

  " 91 Current APM level",
    ". 15:8 Reserved", // ACS-3
    ". 7:0 Current APM level value",

  " 92 Master Password Identifier", // ATA-7: Master Password Revision Code

  " 93 Hardware reset result (PATA)",
    ". 15:14 Must be set to 0x1",
    ". 13 Device detected CBLID- above(1)/below(0) ViHB",
    ". 12 Reserved",
    ". 11 Device 1 asserted PDIAG-",
    ". 10:9 Device 1 detection method: -, Jumper, CSEL, other",
    ". 8 Must be set to 1",
    ". 7 Reserved",
    ". 6 Device 0 responds when device 1 selected",
    ". 5 Device 0 detected the assertion of DASP-",
    ". 4 Device 0 detected the assertion of PDIAG-",
    ". 3 Device 0 passed diagnostics",
    ". 2:1 Device 0 detection method: -, Jumper, CSEL, other",
    ". 0 Must be set to 1",

  " 94 AAM level [OBS-ACS-2]",
    ". 15:8 Recommended AAM level [OBS-ACS-2]",
    ". 7:0 Current AAM level [OBS-ACS-2]",

  " 95 Stream Minimum Request Size",
  " 96 Streaming Transfer Time - DMA",
  " 97 Streaming Access Latency - DMA and PIO",
  " 98-99 Streaming Performance Granularity (DWord)",
  "100-103 User addressable sectors for 48-bit commands (QWord)",
  "104 Streaming Transfer Time - PIO",
  "105 Max blocks of LBA Range Entries per DS MANAGEMENT cmd",

  "106 Physical sector size / logical sector size",
    ". 15:14 Must be set to 0x1",
    ". 13 Multiple logical sectors per physical sector",
    ". 12 Logical Sector longer than 256 words",
    ". 11:4 Reserved",
    ". 3:0 2^X logical sectors per physical sector",

  "107 Inter-seek delay for ISO 7779 acoustic testing",
  "108-111 64-bit World Wide Name",
  "112-115 Reserved", // ATA-7: Reserved for world wide name extension to 128 bits
  "116 Reserved for TLC [OBS-ACS-3]",
  "117-118 Logical sector size (DWord)",

  "119 Commands and feature sets supported",
    ". 15:14 Must be set to 0x1",
    ". 13:10 Reserved",
    ". 9 DSN feature set supported", // ACS-3
    ". 8 Accessible Max Address Config feature set supported", // ACS-3
    ". 7 Extended Power Conditions feature set supported",
    ". 6 Sense Data Reporting feature set supported",
    ". 5 Free-fall Control feature set supported",
    ". 4 DOWNLOAD MICROCODE with mode 3 supported",
    ". 3 READ/WRITE LOG DMA EXT supported",
    ". 2 WRITE UNCORRECTABLE EXT supported",
    ". 1 Write-Read-Verify feature set supported",
    ". 0 Reserved for DDT [OBS-ACS-3]",

  "120 Commands and feature sets supported or enabled",
    ". 15:14 Must be set to 0x1",
    ". 13:10 Reserved",
    ". 9 DSN feature set enabled", // ACS-3
    ". 8 Reserved",
    ". 7 Extended Power Conditions feature set enabled",
    ". 6 Sense Data Reporting feature set enabled",
    ". 5 Free-fall Control feature set enabled",
    ". 4 DOWNLOAD MICROCODE with mode 3 supported",
    ". 3 READ/WRITE LOG DMA EXT supported",
    ". 2 WRITE UNCORRECTABLE EXT supported",
    ". 1 Write-Read-Verify feature set enabled",
    ". 0 Reserved for DDT [OBS-ACS-3]",

  "121-126 ATA: Reserved",
  "121-124 ATAPI: Reserved",
  "125 ATAPI: Byte count = 0 behavior",
  "126 ATAPI: Byte count = 0 behavior [OBS-6]",

  "127 Removable Media Status Notification [OBS-8]",
    ". 15:1 Reserved",
    ". 0 Removable Media Status Notification supported",

  "128 Security status",
    ". 15:9 Reserved",
    ". 8 Master password capability: 0 = High, 1 = Maximum",
    ". 7:6 Reserved",
    ". 5 Enhanced security erase supported",
    ". 4 Security count expired",
    ". 3 Security frozen",
    ". 2 Security locked",
    ". 1 Security enabled",
    ". 0 Security supported",

  "129-159 Vendor specific",

  "160 CFA power mode",
  // ". 15 Word 160 supported",
  // ". 14 Reserved",
  // ". 13 CFA power mode 1 is required for some commands",
  // ". 12 CFA power mode 1 disabled",
  // ". 11:0 Maximum current in mA",
  "161-167 Reserved for CFA",

  "168 Form factor",
    ". 15:4 Reserved",
    ". 3:0 Nominal form factor: -, 5.25, 3.5, 2.5, 1.8, <1.8",

  "169 DATA SET MANAGEMENT command support",
    ". 15:1 Reserved",
    ". 0 Trim bit in DATA SET MANAGEMENT command supported",

  "170-173 Additional product identifier (String)",
  "174-175 Reserved",
  "176-205 Current media serial number (String)",

  "206 SCT Command Transport",
    ". 15:12 Vendor specific",
    ". 11:8 Reserved",
    ". 7 Reserved for Serial ATA",
    ". 6 Reserved",
    ". 5 SCT Data Tables supported",
    ". 4 SCT Feature Control supported",
    ". 3 SCT Error Recovery Control supported",
    ". 2 SCT Write Same supported",
    ". 1 SCT Read/Write Long supported [OBS-ACS-2]",
    ". 0 SCT Command Transport supported",

  "207-208 Reserved", // ATA-8: Reserved for CE-ATA

  "209 Alignment of logical sectors",
    ". 15:14 Must be set to 0x1",
    ". 13:0 Logical sector offset",

  "210-211 Write-Read-Verify sector count mode 3 (DWord)",
  "212-213 Write-Read-Verify sector count mode 2 (DWord)",

  "214 NV Cache capabilities [OBS-ACS-3]",
    ". 15:12 NV Cache feature set version [OBS-ACS-3]",
    ". 11:8 NV Cache Power Mode feature set version [OBS-ACS-3]",
    ". 7:5 Reserved [OBS-ACS-3]",
    ". 4 NV Cache feature set enabled [OBS-ACS-3]",
    ". 3:2 Reserved",
    ". 1 NV Cache Power Mode feature set enabled [OBS-ACS-3]",
    ". 0 NV Cache Power Mode feature set supported [OBS-ACS-3]",

  "215-216 NV Cache size in logical blocks (DWord) [OBS-ACS-3]",
  "217 Nominal media rotation rate",
  "218 Reserved",

  "219 NV Cache options [OBS-ACS-3]",
    ". 15:8 Reserved [OBS-ACS-3]",
    ". 7:0 Estimated time to spin up in seconds [OBS-ACS-3]",

  "220 Write-Read-Verify mode",
    ". 15:8 Reserved",
    ". 7:0 Write-Read-Verify feature set current mode",

  "221 Reserved",

  "222 Transport major version number",
    ". 15:12 Transport: 0x0 = Parallel, 0x1 = Serial, 0xe = PCIe", // PCIe: ACS-4
    ". 11:8 Reserved    | Reserved",
    ". 7 Reserved    | SATA 3.2",
    ". 6 Reserved    | SATA 3.1",
    ". 5 Reserved    | SATA 3.0",
    ". 4 Reserved    | SATA 2.6",
    ". 3 Reserved    | SATA 2.5",
    ". 2 Reserved    | SATA II: Extensions",
    ". 1 ATA/ATAPI-7 | SATA 1.0a",
    ". 0 ATA8-APT    | ATA8-AST",

  "223 Transport minor version number",
  "224-229 Reserved",
  "230-233 Extended number of user addressable sectors (QWord)",
  "234 Minimum blocks per DOWNLOAD MICROCODE mode 3 command",
  "235 Maximum blocks per DOWNLOAD MICROCODE mode 3 command",
  "236-254 Reserved",

  "255 Integrity word",
    ". 15:8 Checksum",
    ". 7:0 Signature"
};

const int num_identify_descriptions = sizeof(identify_descriptions)/sizeof(identify_descriptions[0]);

static inline unsigned short get_word(const void * id, int word)
{
  const unsigned char * p = ((const unsigned char *)id) + 2 * word;
  return p[0] + (p[1] << 8);
}

void ata_print_identify_data(const void * id, bool all_words, int bit_level)
{
  // ATA or ATAPI ?
  unsigned short w = get_word(id, 0);
  bool is_atapi = ((w & 0x8000) && (w != 0x848a/*CompactFlash Signature*/));

  int prev_word = -1, prev_bit = -1;
  pout("Word     %s Value   Description\n", (bit_level >= 0 ? "Bit    " : " "));

  for (int i = 0; i < num_identify_descriptions; i++) {
    // Parse table entry
    const char * desc = identify_descriptions[i];

    int word = prev_word, word2 = -1;
    int bit = -1, bit2 = -1;

    int nc;
    unsigned v1, v2;
    if (word >= 0 && sscanf(desc, ". %u:%u %n", &v1, &v2, (nc=-1, &nc)) == 2 && nc > 0 && 16 > v1 && v1 > v2) {
      bit = v1; bit2 = v2;
    }
    else if (word >= 0 && sscanf(desc, ". %u %n", &v1, (nc=-1, &nc)) == 1 && nc > 0 && v1 < 16) {
      bit = v1;
    }
    else if (sscanf(desc, "%u-%u %n", &v1, &v2, (nc=-1, &nc)) == 2 && nc > 0 && v1 < v2 && v2 < 256) {
      word = v1, word2 = v2;
    }
    else if (sscanf(desc, "%u %n", &v1, (nc=-1, &nc)) == 1 && nc > 0 && v1 < 256) {
      word = v1;
    }
    else {
      pout("Error: #%d: Syntax\n", i);
      continue;
    }
    desc += nc;

    // Check for ATA/ATAPI specific entries
    if (str_starts_with(desc, "ATA: ")) {
      if (is_atapi)
        continue;
      desc += sizeof("ATA: ")-1;
    }
    else if (str_starts_with(desc, "ATAPI: ")) {
      if (!is_atapi)
        continue;
    }

    // Check table entry
    if (bit < 0) {
      if (word != prev_word+1) {
        pout("Error: #%d: Missing word %d\n", i, prev_word+1);
        return;
      }
      else if (prev_bit > 0) {
        pout("Error: #%d: Missing bit 0 from word %d\n", i, prev_word);
        return;
      }
    }
    else if (!((prev_bit < 0 && bit == 15) || bit == prev_bit-1)) {
      pout("Error: #%d: Missing bit %d from word %d\n", i, bit+1, word);
      return;
    }

    w = get_word(id, word);
    bool w_is_set = (w != 0x0000 && w != 0xffff);

    if (bit >= 0) {
      int b;
      if (bit2 >= 0)
        b = (w >> bit2) & ~(~0 << (bit-bit2+1));
      else
        b = (w >> bit) & 1;

      if (   (bit_level >= 0 && b)
          || (bit_level >= 1 && w_is_set)
          || (bit_level >= 2 && all_words)) {
        if (bit2 >= 0) {
          // Print bitfield
          char valstr[20];
          snprintf(valstr, sizeof(valstr), "0x%0*x", (bit - bit2 + 4) >> 2, b);
          pout("%4d     %2d:%-2d  %6s   %s\n", word, bit, bit2, valstr, desc);
        }
        else {
          // Print bit
          pout("%4d     %2d          %u   %s\n", word, bit, b, desc);
        }
      }

      prev_bit = (bit2 >= 0 ? bit2 : bit);
    }
    else {
      if (word2 >= 0) {
        for (int j = word+1; !w_is_set && j <= word2; j++) {
           if (get_word(id, j) != w)
             w_is_set = true;
        }

        // Print word array
        if (all_words || w_is_set) {
          pout("%s%4d-%-3d  %s",
               (bit_level >= 0 ? "\n" : ""), word, word2,
               (bit_level >= 0 ? "-     " : ""));

          if (!w_is_set) {
            pout("0x%02x...  %s\n", w & 0xff, desc);
          }
          else {
            bool is_str = !!strstr(desc, "(String)");
            pout(".        %s", desc);

            for (int j = word; j <= word2; j += 4) {
              if (j + 2*4 < word2 && !nonempty((const unsigned char *)id + 2*j, 2*(word2-j+1))) {
                // Remaining words are null
                pout("\n%4d-%-3d  %s0x0000:0000:0000:00...", j, word2,
                     (bit_level >= 0 ? ".     " : ""));
                break;
              }
              // Print 4 words in a row
              pout("\n%4d-%-3d  %s0x", j, (j+3 <= word2 ? j+3 : word2),
                   (bit_level >= 0 ? ".     " : ""));
              int k;
              for (k = 0; k < 4 && j+k <= word2; k++)
                pout("%s%04x",  (k == 0 ? "" : ":"), get_word(id, j+k));

              if (is_str) {
                // Append little endian string
                pout("%*s  \"", 20 - 5 * k, "");
                for (k = 0; k < 4 && j+k <= word2; k++) {
                  char c2 = ((const char *)id)[2*(j+k)    ];
                  char c1 = ((const char *)id)[2*(j+k) + 1];
                  pout("%c%c", (' ' <= c1 && c1 <= '~' ? c1 : '.'),
                               (' ' <= c2 && c2 <= '~' ? c2 : '.') );
                }
                pout("\"");
              }
            }

            // Print decimal value of D/QWords
            if (word + 1 == word2 && strstr(desc, "(DWord)"))
              pout("  (%u)\n", ((unsigned)get_word(id, word2) << 16) | w);
            else if (word + 3 == word2 && strstr(desc, "(QWord)"))
              pout("  (%" PRIu64 ")\n", ((uint64_t)get_word(id, word + 3) << 48)
                                      | ((uint64_t)get_word(id, word + 2) << 32)
                                      | ((unsigned)get_word(id, word + 1) << 16) | (unsigned)w);
            else
              pout("\n");
          }
        }
      }
      else {
        // Print word
        if (all_words || w_is_set)
          pout("%s%4d      %s0x%04x   %s\n",
               (bit_level >= 0 ? "\n" : ""), word,
               (bit_level >= 0 ? "-     " : ""), w, desc);
      }

      prev_word = (word2 >= 0 ? word2 : word);
      prev_bit = -1;
    }
  }

  pout("\n");
}
