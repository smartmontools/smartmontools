/*
 * int64.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2004-11 Christian Franke
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

// 64 bit integer typedefs and format strings

#ifdef HAVE_INTTYPES_H
// The ISO C99 standard specifies that in C++ implementations the PRI* macros
// from <inttypes.h> should only be defined if explicitly requested
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> // PRId64, PRIu64, PRIx64 (also includes <stdint.h>)
#else
#ifdef HAVE_STDINT_H
#include <stdint.h> // int64_t, uint64_t (usually included above)
#else
#ifdef HAVE_SYS_INTTYPES_H
#include <sys/inttypes.h>
#else
#ifdef HAVE_SYS_INT_TYPES_H
#include <sys/int_types.h>
#else
#if defined(_WIN32) && defined(_MSC_VER)
// for MSVC <= 9 (MSVC10 and MinGW provide <stdint.h>)
typedef          __int64    int64_t;
typedef unsigned __int64   uint64_t;
#else
// for systems with above includes missing (like ix86-pc-linux-gnulibc1),
// default to GCC if types are undefined in types.h
#include <sys/types.h>
#ifndef HAVE_INT64_T
typedef          long long  int64_t;
#endif
#ifndef HAVE_UINT64_T
typedef unsigned long long uint64_t;
#endif
#endif // _WIN32 && _MSC_VER
#endif // HAVE_SYS_INT_TYPES_H
#endif // HAVE_SYS_INTTYPES_H
#endif // HAVE_STDINT_H
#endif // HAVE_INTTYPES_H

#if defined(_WIN32) && !defined(PRId64)
// for MSVC (MinGW provides <inttypes.h>)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#endif // _WIN32 && !PRId64

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

#endif // INT64_H
