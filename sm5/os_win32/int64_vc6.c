/*
 * os_win32/int64_vc6.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#include "int64.h"

#include <stdio.h>
#include <errno.h>

const char *int64_vc6_c_cvsid = "$Id: int64_vc6.c,v 1.3 2004/07/29 21:05:26 chrfranke Exp $" \
INT64_H_CVSID;


// Missing (why?-) "unsigned __int64 -> double" conversion

double uint64_to_double(unsigned __int64 ull)
{
	if ((__int64)ull >= 0)
		return (double)(__int64)ull;
	else
		return (double)(__int64)(ull - 9223372036854775808I64)
		                             + 9223372036854775808.0;
		//    ~((~(uint64_t)0) >> 1) == 0x8000000000000000I64
}

