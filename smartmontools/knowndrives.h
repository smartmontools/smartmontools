/*
 * knowndrives.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-11 Philip Williams, Bruce Allen
 * Copyright (C) 2008-15 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KNOWNDRIVES_H_
#define KNOWNDRIVES_H_

#define KNOWNDRIVES_H_CVSID "$Id: knowndrives.h 4760 2018-08-19 18:45:53Z chrfranke $\n"

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
