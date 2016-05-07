/*
 * atacmdnames.h
 *
 * This module is based on the T13/1532D Volume 1 Revision 3 (ATA/ATAPI-7)
 * specification, which is available from http://www.t13.org/#FTP_site
 *
 * Home page of code is: http://www.smartmontools.org
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-8 Philip Williams
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef ATACMDNAMES_H_
#define ATACMDNAMES_H_

#define ATACMDNAMES_H_CVSID "$Id$\n"

/* Returns the name of the command (and possibly sub-command) with the given
   command code and feature register values. */
const char *look_up_ata_command(unsigned char c_code, unsigned char f_reg);

#endif
