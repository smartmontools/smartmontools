/*
 * knowndrives.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-6 Philip Williams, Bruce Allen
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

#define KNOWNDRIVES_H_CVSID "$Id: knowndrives.h,v 1.17 2006/08/09 20:40:19 chrfranke Exp $\n"

/* Structure used to store settings for specific drives in knowndrives[]. The
 * elements are used in the following ways:
 *
 *  modelfamily     Informal string about the model family/series of a 
 *                  device. Set to NULL if no info (apart from device id)
 *                  known.
 *  modelregexp     POSIX regular expression to match the model of a device.
 *                  This should never be NULL (except to terminate the
 *                  knowndrives array).
 *  firmwareregexp  POSIX regular expression to match a devices's firmware
 *                  version.  This is optional and should be NULL if it is not
 *                  to be used.  If it is non-NULL then it will be used to
 *                  narrow the set of devices matched by modelregexp.
 *  warningmsg      A message that may be displayed for matching drives.  For
 *                  example, to inform the user that they may need to apply a
 *                  firmware patch.
 *  vendoropts      Pointer to first element of an array of vendor-specific
 *                  option attribute/value pairs that should be set for a
 *                  matching device unless the user has requested otherwise.
 *                  The user's own settings override these.  The array should
 *                  be terminated with the entry {0,0}.
 *  specialpurpose  Pointer to a function that defines some additional action
 *                  that may be taken for matching devices.
 *  functiondesc    A description of the effect of the specialpurpose
 *                  function.  Used by showpresets() and showallpresets() to
 *                  make the output more informative.
 */
typedef struct drivesettings_s {
  const char * modelfamily;
  const char * modelregexp;
  const char * firmwareregexp;
  const char * warningmsg;
  const unsigned char (* vendoropts)[2];
  void (* specialpurpose)(smartmonctrl *);
  const char * functiondesc;
} drivesettings;

/* Table of settings for known drives.  Defined in knowndrives.c. */
extern const drivesettings knowndrives[];

// Searches knowndrives[] for a drive with the given model number and firmware
// string.
int lookupdrive(const char *model, const char *firmware);

// Shows the presets (if any) that are available for the given drive.
void showpresets(const struct ata_identify_device *drive);

// Shows all presets for drives in knowndrives[].
// Returns <0 on syntax error in regular expressions.
int showallpresets(void);

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware);

// Sets preset vendor attribute options in opts by finding the entry
// (if any) for the given drive in knowndrives[].  Values that have
// already been set in opts will not be changed.  Also sets options in
// con.  Returns <0 if drive not recognized else index of drive in
// database.
int applypresets(const struct ata_identify_device *drive, unsigned char **opts,
                  smartmonctrl *con);

#endif
