/*
 * os_win32/hostname_win32.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-12 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define WINVER 0x0500
#define _WIN32_WINNT WINVER

#include "hostname_win32.h"

const char * hostname_win32_cpp_cvsid = "$Id$"
  HOSTNAME_WIN32_H_CVSID;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int gethostname(char * name, int len)
{
	DWORD size = len - 1;
	if (GetComputerNameExA(ComputerNameDnsHostname, name, &size))
		return 0;

	// last resort: get NETBIOS name
	size = len - 1;
	if (GetComputerNameA(name, &size))
		return 0;

	return -1;
}


int getdomainname(char * name, int len)
{
	DWORD size = len - 1;
	if (GetComputerNameExA(ComputerNameDnsDomain, name, &size))
		return 0;

	return -1;
}


#ifdef TEST

#include <stdio.h>

main()
{
	char name[256];
	if (gethostname(name, sizeof(name)))
		strcpy(name, "Error");
	printf("hostname=\"%s\"\n", name);
	if (getdomainname(name, sizeof(name)))
		strcpy(name, "Error");
	printf("domainname=\"%s\"\n", name);
	return 0;
}

#endif
