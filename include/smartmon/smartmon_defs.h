/*
 * smartmon_defs.h - libsmartmon internal defines
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_DEFS_H
#define SMARTMON_DEFS_H

#include <smartmon/smartmon_config.h>

#include <type_traits>

// Enable format check of printf()-like functions
#ifndef __GNUC__
#define SMARTMON_FORMAT_PRINTF(x, y)  /**/
#elif defined(__MINGW32__) && !defined(__clang__) && __USE_MINGW_ANSI_STDIO
// Check format of __mingw_*printf() instead of MSVCRT.DLL:*printf()
#define SMARTMON_FORMAT_PRINTF(x, y)  __attribute__((format (gnu_printf, x, y)))
#else
#define SMARTMON_FORMAT_PRINTF(x, y)  __attribute__((format (printf, x, y)))
#endif

// Temporarily disable warnings
#ifndef __GNUC__
#define SMARTMON_DIAGNOSTIC_FORMAT_NONLITERAL_IGNORE  /**/
#define SMARTMON_DIAGNOSTIC_FORMAT_NONLITERAL_RESTORE /**/
#else // This also works for CLang
#define SMARTMON_DIAGNOSTIC_FORMAT_NONLITERAL_IGNORE \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define SMARTMON_DIAGNOSTIC_FORMAT_NONLITERAL_RESTORE \
  _Pragma("GCC diagnostic pop")
#endif

// Add __attribute__((packed)) if compiler supports it
// because some older gcc versions ignore #pragma pack()
#ifdef SMARTMON_HAVE_ATTR_PACKED
#define SMARTMON_ATTR_PACKED __attribute__((packed))
#else
#define SMARTMON_ATTR_PACKED /**/
#endif

// Static assert macros
// Don't use single argument 'static_assert(x)' because it requires C++17
#define SMARTMON_STATIC_ASSERT(x) static_assert((x), #x)
// Ensure portable data structure size
#define SMARTMON_ASSERT_SIZEOF(t, n) \
  SMARTMON_STATIC_ASSERT(std::is_standard_layout<t>::value && sizeof(t) == (n))

#endif // SMARTMON_DEFS_H
