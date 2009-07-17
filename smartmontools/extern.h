/*
 * extern.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
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

#ifndef EXTERN_H_
#define EXTERN_H_

#define EXTERN_H_CVSID "$Id: extern.h,v 1.63 2009/07/07 19:28:29 chrfranke Exp $\n"

// Block used for global control/communications.  If you need more
// global variables, this should be the only place that you need to
// add them.
typedef struct smartmonctrl_s {
  bool printing_switchable;
  bool dont_print;
  bool dont_print_serial;
  unsigned char permissive;
  bool conservative;
  unsigned char reportataioctl;
  unsigned char reportscsiioctl;
#ifdef OLD_INTERFACE
  // 3Ware controller type, but also extensible to other contoller types
  unsigned char controller_type; // TODO: Only needed for os_linux.cpp
  // For 3Ware controllers, nonzero value is 1 plus the disk number
  unsigned char controller_port;  // TODO: Only needed for os_linux.cpp
  // combined controller/channle/pmport for highpoint rocketraid controller
  unsigned char hpt_data[3]; // TODO: Only needed for os_linux.cpp
#endif
} smartmonctrl;

#endif
