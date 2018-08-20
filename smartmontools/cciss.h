/*
 * cciss.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2007 Sergey Svishchev
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CCISS_H_
#define CCISS_H_

#define CCISS_H_CVSID "$Id$"

int cciss_io_interface(int device, int target,
			      struct scsi_cmnd_io * iop, int report);

#endif /* CCISS_H_ */
