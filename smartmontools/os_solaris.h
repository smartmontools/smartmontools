/*
 * os_solaris.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 SAWADA Keiji <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-8 Casper Dik <smartmontools-support@lists.sourceforge.net>
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
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef OS_SOLARIS_H_
#define OS_SOLARIS_H_

#define OS_SOLARIS_H_CVSID "$Id$\n"

// Additional material should start here.  Note: to keep the '-V' CVS
// reporting option working as intended, you should only #include
// system include files <something.h>.  Local #include files
// <"something.h"> should be #included in os_solaris.c

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// function prototypes for functions defined in os_solaris_ata.s
extern "C" {
  int smart_read_data(int fd, void *data);
  int smart_read_thresholds(int fd, void *data);
  int smart_read_log(int fd, int s, int count, void *data);
  int ata_identify(int fd, void *data);
  int ata_pidentify(int fd, void *data);
  int smart_enable(int fd);
  int smart_disable(int fd);
  int smart_status(int fd);
  int smart_auto_offline(int fd, int s);
  int smart_auto_save(int fd, int s);
  int smart_immediate_offline(int fd, int s);
  int smart_status_check(int fd);
}

// wrapper macros
#define smart_enable_auto_save(fd)	smart_auto_save(fd, 0xf1)
#define smart_disable_auto_save(fd)	smart_auto_save(fd, 0x00)

#endif /* OS_SOLARIS_H_ */
