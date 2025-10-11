/*
 * os_openbsd.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-8 David Snyder
 *
 * Derived from os_netbsd.c by Sergey Svishchev, Copyright (C) 2003-8
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OS_OPENBSD_H_
#define OS_OPENBSD_H_

#define OS_OPENBSD_H_CVSID "$Id: os_openbsd.h 4760 2018-08-19 18:45:53Z chrfranke $\n"

/* from NetBSD: atareg.h,v 1.17, by Manuel Bouyer */
/* Actually fits _perfectly_ into OBSDs wdcreg.h, but... */
/* Subcommands for SMART (features register) */
#define WDSMART_CYL             0xc24f

#include <sys/device.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <sys/scsiio.h>
#include <sys/ataio.h>

#define ata_smart_selftestlog __openbsd_ata_smart_selftestlog
#include <dev/ata/atareg.h>
#if HAVE_DEV_ATA_ATAVAR_H
#include <dev/ata/atavar.h>
#endif
#include <dev/ic/wdcreg.h>
#undef ata_smart_selftestlog

#include <err.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>

#endif /* OS_OPENBSD_H_ */
