/*
 * knowndrives.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003 Bruce Allen, Philip Williams
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

#ifndef __KNOWNDRIVES_H_
#define __KNOWNDRIVES_H_

#define KNOWNDRIVES_H_CVSID "$Id: knowndrives.h,v 1.1 2003/04/08 21:40:01 pjwilliams Exp $\n"

/* Structure used to store settings for specific drives in knowndrives[]. The
 * elements are used in the following ways:
 *
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
 */
typedef struct drivesettings_s {
  const char * const modelregexp;
  const char * const firmwareregexp;
  const char * const warningmsg;
  const int (* const vendoropts)[2];
  const int (* const specialpurpose)();
} drivesettings;

/* Table of settings for known drives.  Defined in knowndrives.c. */
extern const drivesettings knowndrives[];

#endif
