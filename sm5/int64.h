/*
 * int64.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2004 Christian Franke
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
 */

#ifndef INT64_H_
#define INT64_H_

#define INT64_H_CVSID "$Id: int64.h,v 1.7 2004/08/30 06:19:31 ballen4705 Exp $\n"

#ifndef CONFIG_H_CVSID
// need HAVE_STDINT_H, HAVE_INTTYPES_H
#include "config.h"
#endif

// 64 bit integer typedefs

#ifdef HAVE_SYS_INT_TYPES_H
#include <sys/int_types.h>
#else
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_SYS_INTTYPES_H
#include <sys/inttypes.h>
#else
#if defined(_WIN32) && defined(_MSC_VER)
// for MSVC 6.0
typedef          __int64    int64_t;
typedef unsigned __int64   uint64_t;
#else
// default is GCC style
typedef          long long  int64_t;
typedef unsigned long long uint64_t;
#endif // _WIN32 && _MSC_VER
#endif // HAVE_SYS_INTTYPES_H
#endif // HAVE_STDINT_H
#endif // HAVE_SYS_INT_TYPES_H

// 64 bit integer format strings

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if defined(_WIN32) && defined(_MSC_VER)
// for MSVC 6.0
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#endif // _WIN32 && _MSC_VER
#endif // HAVE_INTTYPES_H

// If macros not defined in inttypes.h, fix here.  Default is GCC
// style
#ifndef PRId64		
#define PRId64 "lld"
#endif // ndef PRId64

#ifndef PRIu64
#define PRIu64 "llu"
#endif // ndef PRIu64

#ifndef PRIx64
#define PRIx64 "llx"
#endif // ndef PRIx64


#if defined(_WIN32) && defined(_MSC_VER)
// for MSVC 6.0: "unsigned __int64 -> double" conversion not implemented
// replacement function implemented in os_win32/int64_vc6.c
double uint64_to_double(unsigned __int64 ull);
#else
#define uint64_to_double(ull) ((double)(ull))
#endif // _WIN32 && _MSC_VER


#endif // INT64_H
