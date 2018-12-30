/*
 * dev_ata_cmd_set.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2008 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEV_ATA_CMD_SET_H
#define DEV_ATA_CMD_SET_H

#define DEV_ATA_CMD_SET_H_CVSID "$Id$"

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
