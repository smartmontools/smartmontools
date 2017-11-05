/*
 * dev_intelliprop.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2012 Hank Wu <hank@areca.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef DEV_INTELLIPROP_H
#define DEV_INTELLIPROP_H

#define DEV_INTELLIPROP_H_CVSID "$Id$"

ata_device * get_intelliprop_device(smart_interface * intf, unsigned phydrive, ata_device * atadev);

#endif
