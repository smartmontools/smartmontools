/*
 * smartmon_defs.h - libsmartmon internal defines
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _SMARTMON_DEFS_H
#define _SMARTMON_DEFS_H

#include "smartmon_config.h"

// Enable format check of printf()-like functions
#ifndef __GNUC__
#define SMARTMON_FORMAT_PRINTF(x, y)  /**/
#elif defined(__MINGW32__) && __USE_MINGW_ANSI_STDIO
// Check format of __mingw_*printf() instead of MSVCRT.DLL:*printf()
#define SMARTMON_FORMAT_PRINTF(x, y)  __attribute__((format (gnu_printf, x, y)))
#else
#define SMARTMON_FORMAT_PRINTF(x, y)  __attribute__((format (printf, x, y)))
#endif

// Add __attribute__((packed)) if compiler supports it
// because some older gcc versions ignore #pragma pack()
#ifdef SMARTMON_HAVE_ATTR_PACKED
#define SMARTMON_ATTR_PACKED __attribute__((packed))
#else
#define SMARTMON_ATTR_PACKED /**/
#endif

#endif // _SMARTMON_DEFS_H
