/*
 * os_solaris.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 SAWADA Keiji
 * Copyright (C) 2003-8 Casper Dik
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OS_SOLARIS_H_
#define OS_SOLARIS_H_

#define OS_SOLARIS_H_CVSID "$Id: os_solaris.h 4760 2018-08-19 18:45:53Z chrfranke $\n"

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
