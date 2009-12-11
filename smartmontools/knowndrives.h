/*
 * knowndrives.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-9 Philip Williams, Bruce Allen
 * Copyright (C) 2008-9 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

// Searches knowndrives[] for a drive with the given model number and firmware
// string.
const drive_settings * lookup_drive(const char * model, const char * firmware);

// Shows the presets (if any) that are available for the given drive.
void show_presets(const ata_identify_device * drive, bool fix_swapped_id);

// Shows all presets for drives in knowndrives[].
// Returns #syntax errors.
int showallpresets();

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware);

// Sets preset vendor attribute options in opts by finding the entry
// (if any) for the given drive in knowndrives[].  Values that have
// already been set in opts will not be changed.  Also sets options in
// con.  Returns false if drive not recognized.
bool apply_presets(const ata_identify_device * drive, ata_vendor_attr_defs & defs,
                   unsigned char & fix_firmwarebug, bool fix_swapped_id);

// Read drive database from file.
bool read_drive_database(const char * path);

// Read drive databases from standard places.
bool read_default_drive_databases();

#endif
