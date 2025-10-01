/*
 * os_netbsd.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Sergey Svishchev
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OS_NETBSD_H_
#define OS_NETBSD_H_

#define OS_NETBSD_H_CVSID "$Id: os_netbsd.h 4760 2018-08-19 18:45:53Z chrfranke $\n"

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
