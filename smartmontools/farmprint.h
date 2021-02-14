/*
 * Copyright (c) 2020-2021 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the GNU General Public License v2.0 or later
 * SPDX-Licence-Identifier: GPL-2.0-or-later
 * You may obtain a copy of the License at
 *
 *     https://spdx.org/licenses/GPL-2.0-or-later.html
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 */

#ifndef FARMPRINT_H
#define FARMPRINT_H

#include "farmcmds.h"

/*
 *  Prints parsed FARM log (GP Log 0xA6) data from Seagate
 *  drives already present in ataFarmLog structure
 *  
 *  @param  farmLog:  Constant reference to parsed farm log (const ataFarmLog&)
 *  @return True if printing occurred without errors, otherwise false (bool)
 */
bool ataPrintFarmLog(const ataFarmLog& farmLog);

/*
 *  Prints parsed FARM log (SCSI log page 0x3D, sub-page 0x3) data from Seagate
 *  drives already present in scsiFarmLog structure
 *  
 *  @param  farmLog:  Constant reference to parsed farm log (const scsiFarmLog&)
 *  @return True if printing occurred without errors, otherwise false (bool)
 */
bool scsiPrintFarmLog(const scsiFarmLog& farmLog);

#endif
