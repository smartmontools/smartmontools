/*
 * int64.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2004-18 Christian Franke
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef INT64_H_
#define INT64_H_

#define INT64_H_CVSID "$Id$"

// TODO: Remove this file, use <inttypes.h> or <stdint.h> instead.

// The ISO C99 standard specifies that in C++ implementations the PRI* macros
// from <inttypes.h> should only be defined if explicitly requested
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> // PRId64, PRIu64, PRIx64 (also includes <stdint.h>)

#endif // INT64_H
