/*
 * os_openbsd.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004 David Snyder <smartmontools-support@lists.sourceforge.net>
 *
 * Derived from os_netbsd.c by Sergey Svishchev <smartmontools-support@lists.sourceforge.net>, Copyright (C) 2003-4 
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
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef OS_OPENBSD_H_
#define OS_OPENBSD_H_

#define OS_OPENBSD_H_CVSID "$Id: os_openbsd.h,v 1.1 2004/08/29 07:10:44 snyderx Exp $\n"

/* from NetBSD: atareg.h,v 1.17, by Manuel Bouyer */
/* Actually fits _perfectly_ into OBSDs wdcreg.h, but... */
/* Subcommands for SMART (features register) */
#define WDSM_RD_DATA            0xd0
#define WDSM_RD_THRESHOLDS      0xd1
#define WDSM_ATTR_AUTOSAVE_EN   0xd2
#define WDSM_SAVE_ATTR          0xd3
#define WDSM_EXEC_OFFL_IMM      0xd4
#define WDSM_RD_LOG             0xd5
#define WDSM_ENABLE_OPS         0xd8
#define WDSM_DISABLE_OPS        0xd9
#define WDSM_STATUS             0xda

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

#endif /* OS_OPENBSD_H_ */
