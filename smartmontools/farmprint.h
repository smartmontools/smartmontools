/*
 * farmprint.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 - 2023 Seagate Technology LLC and/or its Affiliates
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FARMPRINT_H
#define FARMPRINT_H

#include "farmcmds.h"

/*
 *  Prints parsed FARM log (GP Log 0xA6) data from Seagate
 *  drives already present in ataFarmLog structure
 *
 *  @param  farmLog:  Constant reference to parsed farm log (const ataFarmLog&)
 */
void ataPrintFarmLog(const ataFarmLog& farmLog);

/*
 *  Prints parsed FARM log (SCSI log page 0x3D, sub-page 0x3) data from Seagate
 *  drives already present in scsiFarmLog structure
 *
 *  @param  farmLog:  Constant reference to parsed farm log (const scsiFarmLog&)
 */
void scsiPrintFarmLog(const scsiFarmLog& farmLog);

#endif
