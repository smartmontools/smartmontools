/*
 * os_netbsd.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Sergey Svishchev <smartmontools-support@lists.sourceforge.net>
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

#ifndef OS_NETBSD_H_
#define OS_NETBSD_H_

#define OS_NETBSD_H_CVSID "$Id: os_netbsd.h,v 1.5 2004/03/02 18:19:55 shattered Exp $\n"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <dev/ata/atareg.h>
#if HAVE_DEV_ATA_ATAVAR_H
#include <dev/ata/atavar.h>
#endif
#include <dev/ic/wdcreg.h>
#include <sys/ataio.h>
#include <sys/scsiio.h>

#include <err.h>
#include <util.h>

#ifndef	WDSM_RD_THRESHOLDS	/* pre-1.6.2 system */
#define	WDSM_RD_THRESHOLDS	0xd1
#endif
#ifndef	WDSMART_CYL
#define	WDSMART_CYL		0xc24f
#endif

#endif /* OS_NETBSD_H_ */
