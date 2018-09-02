/*
 * dev_intelliprop.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2016 Casey Biemiller <cbiemiller@intelliprop.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEV_INTELLIPROP_H
#define DEV_INTELLIPROP_H

#define DEV_INTELLIPROP_H_CVSID "$Id: dev_intelliprop.h 4763 2018-09-02 13:11:48Z chrfranke $"

ata_device * get_intelliprop_device(smart_interface * intf, unsigned phydrive, ata_device * atadev);

#endif
