/*
 * dev_ata_cmd_set.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2008 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
#include "atacmds.h"
#include "dev_ata_cmd_set.h"

#include <errno.h>

const char * dev_ata_cmd_set_cpp_cvsid = "$Id$"
  DEV_ATA_CMD_SET_H_CVSID;


/////////////////////////////////////////////////////////////////////////////
// ata_device_with_command_set

// Adapter routine to implement new ATA pass through with old interface

bool ata_device_with_command_set::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_ok(in, true)) // data_out_support
    return false;

  smart_command_set command = (smart_command_set)-1;
  int select = 0;
  char * data = (char *)in.buffer;
  char buffer[512];
  switch (in.in_regs.command) {
    case ATA_IDENTIFY_DEVICE:
      command = IDENTIFY;
      break;
    case ATA_IDENTIFY_PACKET_DEVICE:
      command = PIDENTIFY;
      break;
    case ATA_CHECK_POWER_MODE:
      command = CHECK_POWER_MODE;
      data = buffer; data[0] = 0;
      break;
    case ATA_SMART_CMD:
      switch (in.in_regs.features) {
        case ATA_SMART_ENABLE:
          command = ENABLE;
          break;
        case ATA_SMART_READ_VALUES:
          command = READ_VALUES;
          break;
        case ATA_SMART_READ_THRESHOLDS:
          command = READ_THRESHOLDS;
          break;
        case ATA_SMART_READ_LOG_SECTOR:
          command = READ_LOG;
          select = in.in_regs.lba_low;
          break;
        case ATA_SMART_WRITE_LOG_SECTOR:
          command = WRITE_LOG;
          select = in.in_regs.lba_low;
          break;
        case ATA_SMART_DISABLE:
          command = DISABLE;
          break;
        case ATA_SMART_STATUS:
          command = (in.out_needed.lba_high ? STATUS_CHECK : STATUS);
          break;
        case ATA_SMART_AUTO_OFFLINE:
          command = AUTO_OFFLINE;
          select = in.in_regs.sector_count;
          break;
        case ATA_SMART_AUTOSAVE:
          command = AUTOSAVE;
          select = in.in_regs.sector_count;
          break;
        case ATA_SMART_IMMEDIATE_OFFLINE:
          command = IMMEDIATE_OFFLINE;
          select = in.in_regs.lba_low;
          break;
        default:
          return set_err(ENOSYS, "Unknown SMART command");
      }
      break;
    default:
      return set_err(ENOSYS, "Non-SMART commands not implemented");
  }

  clear_err(); errno = 0;
  int rc = ata_command_interface(command, select, data);
  if (rc < 0) {
    if (!get_errno())
      set_err(errno);
    return false;
  }

  switch (command) {
    case CHECK_POWER_MODE:
      out.out_regs.sector_count = data[0];
      break;
    case STATUS_CHECK:
      switch (rc) {
        case 0: // Good SMART status
          out.out_regs.lba_high = 0xc2; out.out_regs.lba_mid = 0x4f;
          break;
        case 1: // Bad SMART status
          out.out_regs.lba_high = 0x2c; out.out_regs.lba_mid = 0xf4;
          break;
      }
      break;
    default:
      break;
  }
  return true;
}

