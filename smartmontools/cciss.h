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

#define CCISS_H_CVSID "$Id: cciss.h 4761 2018-08-20 19:33:04Z chrfranke $"

int cciss_io_interface(int device, int target,
			      struct scsi_cmnd_io * iop, int report);

#endif /* CCISS_H_ */
