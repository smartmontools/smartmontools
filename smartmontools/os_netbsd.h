/*
 * os_netbsd.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Sergey Svishchev <smartmontools-support@lists.sourceforge.net>
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

#ifndef OS_NETBSD_H_
#define OS_NETBSD_H_

#define OS_NETBSD_H_CVSID "$Id$\n"

#include <sys/device.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <sys/scsiio.h>
#include <sys/ataio.h>

#define ata_smart_selftestlog __netbsd_ata_smart_selftestlog
#include <dev/ata/atareg.h>
#if HAVE_DEV_ATA_ATAVAR_H
#include <dev/ata/atavar.h>
#endif
#include <dev/ic/wdcreg.h>
#undef ata_smart_selftestlog

#include <err.h>
#include <fcntl.h>
#include <util.h>

#ifndef	WDSM_RD_THRESHOLDS	/* pre-1.6.2 system */
#define	WDSM_RD_THRESHOLDS	0xd1
#endif
#ifndef	WDSMART_CYL
#define	WDSMART_CYL		0xc24f
#endif

#endif /* OS_NETBSD_H_ */
