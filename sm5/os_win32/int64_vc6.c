/*
 * int64_vc6.c
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
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#include "int64.h"

#include <stdio.h>
#include <errno.h>

const char *int64_vc6_c_cvsid = "$Id: int64_vc6.c,v 1.1.2.1 2004/02/25 12:59:55 chrfranke Exp $" \
INT64_H_CVSID;


// strtoull() is missing in MSVC 6.0
// Used by utility:split_selective_arg()

__int64 strtoull(char * s, char ** end, int base)
{
	__int64 val; int n = -1;
	if (sscanf(s, "%I64i%n", &val, &n) != 1 && n <= 0) {
		if (end)
			*end = s;
		errno = EINVAL; return -1;
	}
	if (end)
		*end = s + n;
	return val;
}
