/*
 * dev_intelliprop.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2016 Casey Biemiller  <cbiemiller@intelliprop.com>
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
#include "int64.h"
#include "atacmds.h" //ATTR_PACKED and ASSERT_SIZEOF_STRUCT
#include "dev_interface.h"
#include "dev_intelliprop.h"
#include "dev_tunnelled.h"
#include <errno.h>

const char * dev_intelliprop_cpp_cvsid = "$Id$"
  DEV_INTELLIPROP_H_CVSID;

//Vendor Specific log addresses
#define LOG_C0           0xc0

// VS LOG MODE CONTROL BITS
enum {
  IPROP_VS_LOG_MODE_CTL_AUTO_SUPPORTED   = (0 << 0), // NOTE: Not supported
  IPROP_VS_LOG_MODE_CTL_MANUAL_SUPPORTED = (1 << 1),
  IPROP_VS_LOG_MODE_CTL_AUTO_ENABLED     = (0 << 2), // NOTE: Not supported
  IPROP_VS_LOG_MODE_CTL_MANUAL_ENABLED   = (1 << 3),
};

// VS LOG PORT SETTING BITS
enum {
  IPROP_VS_LOG_PORT_WRITE_ENABLE_MASK  = 0xC000,
  IPROP_VS_LOG_PORT_WRITE_ENABLE_VALID = 0x8000,
  IPROP_VS_LOG_PORT_RX_DC_GAIN_MASK    = 0x3000,
  IPROP_VS_LOG_PORT_RX_DC_GAIN_SHIFT   = 12,
  IPROP_VS_LOG_PORT_RX_EQ_MASK         = 0x0F00,
  IPROP_VS_LOG_PORT_RX_EQ_SHIFT        = 8,
  IPROP_VS_LOG_PORT_TX_PREEMP_MASK     = 0x00F8,
  IPROP_VS_LOG_PORT_TX_PREEMP_SHIFT    = 3,
  IPROP_VS_LOG_PORT_TX_VOD_MASK        = 0x0007,
  IPROP_VS_LOG_PORT_TX_VOD_SHIFT       = 0,
};

//This struct is used for the Vendor Specific log C0 on devices that support it.
#pragma pack(1)
struct iprop_internal_log
{
  uint32_t drive_select;       // Bytes - [  3:  0] of Log C0
  uint32_t obsolete;           // Bytes - [  7:  4] of Log C0
  uint8_t  mode_control;       // Byte  - [      8] of Log C0
  uint8_t  log_passthrough;    // Byte  - [      9] of Log C0
  uint16_t tier_id;            // Bytes - [ 11: 10] of Log C0
  uint32_t hw_version;         // Bytes - [ 15: 12] of Log C0
  uint32_t fw_version;         // Bytes - [ 19: 16] of Log C0
  uint8_t  variant[8];         // Bytes - [ 27: 20] of Log C0
  uint8_t  reserved[228];      // Bytes - [255: 28] of Log C0
  uint16_t port_0_settings[3]; // Bytes - [263:256] of Log C0
  uint16_t port_0_reserved;
  uint16_t port_1_settings[3]; // Bytes - [271:264] of Log C0
  uint16_t port_1_reserved;
  uint16_t port_2_settings[3]; // Bytes - [279:272] of Log C0
  uint16_t port_2_reserved;
  uint16_t port_3_settings[3]; // Bytes - [287:280] of Log C0
  uint16_t port_3_reserved;
  uint16_t port_4_settings[3]; // Bytes - [295:288] of Log C0
  uint16_t port_4_reserved;
  uint8_t  reserved2[214];     // Bytes - [509:296] of Log C0
  uint16_t crc;                // Bytes - [511:510] of Log C0
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(iprop_internal_log, 512);

/**
 * buffer is a pointer to a buffer of bytes, which should include data and
 *   also CRC if the function is being used to check CRC
 * len is the number of bytes in the buffer (including CRC if it is present)
 * check_crc is a boolean value, set true to check an existing CRC, false
 *   to calculate a new CRC
 */
static uint16_t iprop_crc16_1(uint8_t * buffer, uint32_t len, bool check_crc)
{
  uint8_t crc[16];
  uint16_t crc_final = 0;
  uint8_t crc_msb;
  uint8_t data_msb;
  uint32_t total_len;

  // Initialize CRC array
  for (uint32_t ii = 0; ii < 16; ii++) {
    crc[ii] = 0;
    //crc[ii] = (crc_in >> ii) & 1;
  }

  // If calculating a new CRC, we need to pad the data with extra zeroes
  total_len = check_crc ? len : len + 2;

  // Loop for each byte, plus extra for the CRC itself
  for (uint32_t ii = 0; ii < total_len; ii++) {
    uint8_t data = (ii < len) ? buffer[ii] : 0;

    // Loop for each bit
    for (uint32_t jj = 0; jj < 8; jj++) {
      crc_msb = crc[15];
      data_msb = (data >> (8 - jj - 1)) & 1;

      crc[15] = crc[14] ^ crc_msb;
      crc[14] = crc[13];
      crc[13] = crc[12];
      crc[12] = crc[11];
      crc[11] = crc[10] ^ crc_msb;
      crc[10] = crc[9];
      crc[9] = crc[8] ^ crc_msb;
      crc[8] = crc[7] ^ crc_msb;
      crc[7] = crc[6] ^ crc_msb;
      crc[6] = crc[5];
      crc[5] = crc[4] ^ crc_msb;
      crc[4] = crc[3] ^ crc_msb;
      crc[3] = crc[2];
      crc[2] = crc[1] ^ crc_msb;
      crc[1] = crc[0] ^ crc_msb;
      crc[0] = data_msb ^ crc_msb;
    }
  }

  // Convert CRC array to final value
  for (uint32_t ii = 0; ii < 16; ii++) {
    if (crc[ii] == 1) {
      crc_final |= (1 << ii);
    } else {
      crc_final &= ~(1 << ii);
    }
  }

  return crc_final;
}

static void iprop_dump_log_structure(struct iprop_internal_log const * const log)
{
  pout("Dumping LOG Structure:\n");
  pout("  drive_select:           0x%08x\n", log->drive_select);
  pout("  obsolete:               0x%08x\n", log->obsolete);
  pout("  mode_control:           0x%02x\n", log->mode_control);
  pout("  log_passthrough:        0x%02x\n", log->log_passthrough);
  pout("  tier_id:                0x%04x\n", log->tier_id);
  pout("  hw_version:             0x%08x\n", log->hw_version);
  pout("  fw_version:             0x%08x\n", log->fw_version);
  pout("  variant:                \"");
  for (int ii = 0; ii < 8; ii++) {
    pout("%c", (char)log->variant[ii]);
  }
  pout("\"\n");
  pout("  port_0_settings(Gen 1): 0x%08x\n", log->port_0_settings[0]);
  pout("  port_0_settings(Gen 2): 0x%08x\n", log->port_0_settings[1]);
  pout("  port_0_settings(Gen 3): 0x%08x\n", log->port_0_settings[2]);
  pout("  port_1_settings(Gen 1): 0x%08x\n", log->port_1_settings[0]);
  pout("  port_1_settings(Gen 2): 0x%08x\n", log->port_1_settings[1]);
  pout("  port_1_settings(Gen 3): 0x%08x\n", log->port_1_settings[2]);
  pout("  port_2_settings(Gen 1): 0x%08x\n", log->port_2_settings[0]);
  pout("  port_2_settings(Gen 2): 0x%08x\n", log->port_2_settings[1]);
  pout("  port_2_settings(Gen 3): 0x%08x\n", log->port_2_settings[2]);
  pout("  port_3_settings(Gen 1): 0x%08x\n", log->port_3_settings[0]);
  pout("  port_3_settings(Gen 2): 0x%08x\n", log->port_3_settings[1]);
  pout("  port_3_settings(Gen 3): 0x%08x\n", log->port_3_settings[2]);
  pout("  port_4_settings(Gen 1): 0x%08x\n", log->port_4_settings[0]);
  pout("  port_4_settings(Gen 2): 0x%08x\n", log->port_4_settings[1]);
  pout("  port_4_settings(Gen 3): 0x%08x\n", log->port_4_settings[2]);
  pout("  crc:                    0x%04x\n", log->crc);
  pout("\n");
}

static bool iprop_switch_routed_drive(ata_device * device, int drive_select)
{
  // Declare a log page buffer and initialize it with what is on the drive currently
  iprop_internal_log write_payload;
  if (!ataReadLogExt(device, LOG_C0, 0, 0, &write_payload, 1))
    return device->set_err(EIO, "intelliprop: Initial Read Log failed: %s", device->get_errmsg());

  // Check the returned data is good
  uint16_t const crc_check = iprop_crc16_1((uint8_t *)&write_payload,
                                           sizeof(struct iprop_internal_log),
                                           false);


   //If this first read fails the crc check, the log can be still sent with routing information
   //as long as everything else in the log is zeroed. So there is no need to return false.
  if (crc_check != 0) {
    if (ata_debugmode)
      pout("Intelliprop WARNING: Received log crc(0x%04X) is invalid!\n", crc_check);
    iprop_dump_log_structure(&write_payload);
    memset(&write_payload, 0, sizeof(struct iprop_internal_log));
  }

  //The option to read the log, even if successful, could be useful
  if (ata_debugmode)
    iprop_dump_log_structure(&write_payload);

  // Modify the current drive select to what we were given
  write_payload.drive_select = (uint32_t)drive_select;
  if (ata_debugmode)
    pout("Intelliprop - Change to port 0x%08X.\n", write_payload.drive_select);
  write_payload.log_passthrough = 0; // TEST (Set to 1, non hydra member drive will abort --> test error handling)
  write_payload.tier_id = 0; // TEST (Set to non-zero, non hydra member drive will abort --> test error handling)

  // Update the CRC area
  uint16_t const crc_new = iprop_crc16_1((uint8_t *)&write_payload,
                                         sizeof(struct iprop_internal_log) - sizeof(uint16_t),
                                         false);
  write_payload.crc = (crc_new >> 8) | (crc_new << 8);

  // Check our CRC work
  uint16_t const crc_check2 = iprop_crc16_1((uint8_t *)&write_payload,
                                            sizeof(struct iprop_internal_log),
                                            false);
  if (crc_check2 != 0)
    return device->set_err(EIO, "intelliprop: Re-calculated log crc(0x%04X) is invalid!", crc_check2);

  // Apply the Write LOG
  if (!ataWriteLogExt(device, LOG_C0, 0, &write_payload, 1))
    return device->set_err(EIO, "intelliprop: Write Log failed: %s", device->get_errmsg());

  // Check that the Write LOG was applied
  iprop_internal_log check_payload;
  if (!ataReadLogExt(device, LOG_C0, 0, 0, &check_payload, 1))
    return device->set_err(EIO, "intelliprop: Secondary Read Log failed: %s", device->get_errmsg());

  if (check_payload.drive_select != write_payload.drive_select) {
    if (ata_debugmode > 1)
      iprop_dump_log_structure(&check_payload);
    return device->set_err(EIO, "intelliprop: Current drive select val(0x%08X) is not expected(0x%08X)",
         check_payload.drive_select,
         write_payload.drive_select);
  }

  return true;
}

namespace intelliprop {

class intelliprop_device
: public tunnelled_device<
    /*implements*/ ata_device,
    /*by using an*/ ata_device
  >
{
public:
  intelliprop_device(smart_interface * intf, unsigned phydrive, ata_device * atadev);

  virtual ~intelliprop_device() throw();

  virtual bool open();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

private:
  unsigned m_phydrive;
};


intelliprop_device::intelliprop_device(smart_interface * intf, unsigned phydrive, ata_device * atadev)
: smart_device(intf, atadev->get_dev_name(), "intelliprop", "intelliprop"),
  tunnelled_device<ata_device, ata_device>(atadev),
  m_phydrive(phydrive)
{
  set_info().info_name = strprintf("%s [intelliprop_disk_%u]", atadev->get_info_name(), phydrive);
}

intelliprop_device::~intelliprop_device() throw()
{
}

bool intelliprop_device::open()
{
  if (!tunnelled_device<ata_device, ata_device>::open())
    return false;

  ata_device * atadev = get_tunnel_dev();
  if (!iprop_switch_routed_drive(atadev, m_phydrive)) {
    close();
    return set_err(atadev->get_err());
  }

  return true;
}

bool intelliprop_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  return get_tunnel_dev()->ata_pass_through(in, out);
}
}//namespace

ata_device * get_intelliprop_device(smart_interface * intf, unsigned phydrive, ata_device * atadev)
{
  return new intelliprop::intelliprop_device(intf, phydrive, atadev);
}
