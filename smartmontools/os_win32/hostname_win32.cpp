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

#include "hostname_win32.h"

const char * hostname_win32_cpp_cvsid = "$Id$"
  HOSTNAME_WIN32_H_CVSID;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Call GetComputerNameEx() if available (Win2000/XP)

static BOOL CallGetComputerNameExA(int type, LPSTR name, LPDWORD size)
{
	HINSTANCE hdll;
	BOOL (WINAPI * GetComputerNameExA_p)(int/*enum COMPUTER_NAME_FORMAT*/, LPSTR, LPDWORD);
	BOOL ret;
	if (!(hdll = LoadLibraryA("KERNEL32.DLL")))
		return FALSE;
	if (!(GetComputerNameExA_p = (BOOL (WINAPI *)(int, LPSTR, LPDWORD))GetProcAddress(hdll, "GetComputerNameExA")))
		ret = FALSE;
	else
		ret = GetComputerNameExA_p(type, name, size);
	FreeLibrary(hdll);
	return ret;
}


// Get host/domainname from registry (NT4/2000/XP)

static DWORD GetNamesFromRegistry(BOOL domain, char * name, int len)
{
	HKEY hk; DWORD size, type;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
	    "System\\CurrentControlSet\\Services\\Tcpip\\Parameters",
	    0, KEY_READ, &hk) != ERROR_SUCCESS)
		return 0;
	size = len-1;
	if (!(RegQueryValueExA(hk, (!domain?"HostName":"Domain"), 0, &type, (unsigned char *)name, &size) == ERROR_SUCCESS && type == REG_SZ))
		size = 0;
	if (size == 0 && domain) {
		size = len-1;
		if (!(RegQueryValueExA(hk, "DhcpDomain", 0, &type, (unsigned char *)name, &size) == ERROR_SUCCESS && type == REG_SZ))
			size = 0;
	}
	RegCloseKey(hk);
	return size;
}


static int gethostdomname(int domain, char * name, int len)
{
	// try KERNEL32.dll::GetComputerNameEx()
	DWORD size = len - 1;
	if (CallGetComputerNameExA((!domain ? 1:2/*ComputerNameDnsHost:Domain*/), name, &size))
		return 0;

	// try registry
	if (GetNamesFromRegistry(domain, name, len))
		return 0;

	if (domain)
		return -1;

	// last resort: get NETBIOS name
	size = len - 1;
	if (GetComputerNameA(name, &size))
		return 0;

	return -1;
}


int gethostname(char * name, int len)
{
	return gethostdomname(0, name, len);
}


int getdomainname(char * name, int len)
{
	return gethostdomname(1, name, len);
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
