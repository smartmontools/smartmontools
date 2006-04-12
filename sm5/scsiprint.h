/*
 * scsiprint.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-6 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-6 Douglas Gilbert <dougg@torque.net>
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


/* scsismart version number */
#ifndef SCSI_PRINT_H_
#define SCSI_PRINT_H_

#define SCSIPRINT_H_CVSID "$Id: scsiprint.h,v 1.20 2006/04/12 14:54:28 ballen4705 Exp $\n"

int scsiPrintMain(int fd);

#endif
