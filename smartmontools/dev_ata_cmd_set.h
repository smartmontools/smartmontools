/*
 * dev_ata_cmd_set.h
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

#ifndef DEV_ATA_CMD_SET_H
#define DEV_ATA_CMD_SET_H

#define DEV_ATA_CMD_SET_H_CVSID "$Id: dev_ata_cmd_set.h,v 1.3 2008/08/23 21:32:12 chrfranke Exp $\n"

#include "atacmds.h" // smart_command_set
#include "dev_interface.h"

/////////////////////////////////////////////////////////////////////////////
// ata_device_with_command_set

/// Adapter class to implement new ATA pass through old interface.

class ata_device_with_command_set
: public /*implements*/ ata_device
{
public:
  /// ATA pass through mapped to ata_command_interface().
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  /// Old ATA interface called by ata_pass_through()
  virtual int ata_command_interface(smart_command_set command, int select, char * data) = 0;

  ata_device_with_command_set()
    : smart_device(never_called) { }
};

#endif // DEV_ATA_CMD_SET_H
