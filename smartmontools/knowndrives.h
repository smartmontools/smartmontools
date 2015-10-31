/*
 * knowndrives.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-11 Philip Williams, Bruce Allen
 * Copyright (C) 2008-15 Christian Franke
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

#ifndef KNOWNDRIVES_H_
#define KNOWNDRIVES_H_

#define KNOWNDRIVES_H_CVSID "$Id$\n"

// Structure to store drive database entries, see drivedb.h for a description.
struct drive_settings {
  const char * modelfamily;
  const char * modelregexp;
  const char * firmwareregexp;
  const char * warningmsg;
  const char * presets;
};

// info returned by lookup_usb_device()
struct usb_dev_info
{
  std::string usb_device; // Device name, empty if unknown
  std::string usb_bridge; // USB bridge name, empty if unknown
  std::string usb_type;   // Type string ('-d' option).
};

// Search drivedb for USB device with vendor:product ID.
int lookup_usb_device(int vendor_id, int product_id, int bcd_device,
                      usb_dev_info & info, usb_dev_info & info2);

// Shows the presets (if any) that are available for the given drive.
void show_presets(const ata_identify_device * drive);

// Shows all presets for drives in knowndrives[].
// Returns #syntax errors.
int showallpresets();

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware);

// Searches drive database and sets preset vendor attribute
// options in defs and firmwarebugs.
// Values that have already been set will not be changed.
// Returns pointer to database entry or nullptr if none found.
const drive_settings * lookup_drive_apply_presets(
  const ata_identify_device * drive, ata_vendor_attr_defs & defs,
  firmwarebug_defs & firmwarebugs);

// Get path for additional database file
const char * get_drivedb_path_add();

#ifdef SMARTMONTOOLS_DRIVEDBDIR
// Get path for default database file
const char * get_drivedb_path_default();
#endif

// Read drive database from file.
bool read_drive_database(const char * path);

// Init default db entry and optionally read drive databases from standard places.
bool init_drive_database(bool use_default_db);

// Get vendor attribute options from default db entry.
const ata_vendor_attr_defs & get_default_attr_defs();

#endif
