/*
 * cciss.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2007 Sergey Svishchev
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CCISS_H_
#define CCISS_H_

#include <smartmon/smartmon_defs.h>

namespace smartmon {
SMARTMON_BEGIN_VERSION_NAMESPACE

int cciss_io_interface(int device, int target,
			      struct scsi_cmnd_io * iop, int report);

SMARTMON_END_VERSION_NAMESPACE
} // namespace smartmon

#endif /* CCISS_H_ */
