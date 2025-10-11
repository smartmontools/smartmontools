/*
 * atacmdnames.h
 *
 * This module is based on the T13/1532D Volume 1 Revision 3 (ATA/ATAPI-7)
 * specification, which is available from http://www.t13.org/#FTP_site
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Philip Williams
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ATACMDNAMES_H_
#define ATACMDNAMES_H_

#define ATACMDNAMES_H_CVSID "$Id: atacmdnames.h 4760 2018-08-19 18:45:53Z chrfranke $\n"

/* Returns the name of the command (and possibly sub-command) with the given
   command code and feature register values. */
const char *look_up_ata_command(unsigned char c_code, unsigned char f_reg);

#endif
