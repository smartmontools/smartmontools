/*
 * static_assert.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2019 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STATIC_ASSERT_H
#define STATIC_ASSERT_H

#define STATIC_ASSERT_H_CVSID "$Id$"

#if __cplusplus >= 201103 || _MSVC_LANG >= 201103
#define STATIC_ASSERT(x) static_assert((x), #x)
#elif __STDC_VERSION__ >= 201112
#define STATIC_ASSERT(x) _Static_assert((x), #x)
#elif __GNUC__ >= 4
#define STATIC_ASSERT(x) typedef char static_assertion[(x) ? 1 : -1] \
                         __attribute__((unused))
#else
#define STATIC_ASSERT(x) typedef char static_assertion[(x) ? 1 : -1]
#endif

#endif // STATIC_ASSERT_H
