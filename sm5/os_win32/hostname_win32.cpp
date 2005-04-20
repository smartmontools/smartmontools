/*
 * os_win32/hostname_win32.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-5 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#include "hostname_win32.h"

const char * hostname_win32_c_cvsid = "$Id: hostname_win32.cpp,v 1.3 2005/04/20 03:30:20 ballen4705 Exp $" HOSTNAME_WIN32_H_CVSID;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#ifndef MAX_HOSTNAME_LEN

// From IPHlpApi.dll:

#define MAX_HOSTNAME_LEN    132
#define MAX_DOMAIN_NAME_LEN 132
#define MAX_SCOPE_ID_LEN    260

typedef struct {
  char String[4 * 4];
} IP_ADDRESS_STRING, 
*PIP_ADDRESS_STRING, IP_MASK_STRING, *PIP_MASK_STRING;

typedef struct _IP_ADDR_STRING {
  struct _IP_ADDR_STRING* Next;
  IP_ADDRESS_STRING IpAddress;
  IP_MASK_STRING IpMask;
  DWORD Context;
} IP_ADDR_STRING, 
*PIP_ADDR_STRING;

typedef struct {
  char HostName[MAX_HOSTNAME_LEN];
  char DomainName[MAX_DOMAIN_NAME_LEN];
  PIP_ADDR_STRING CurrentDnsServer;
  IP_ADDR_STRING DnsServerList;
  UINT NodeType;
  char ScopeId[MAX_SCOPE_ID_LEN];
  UINT EnableRouting;
  UINT EnableProxy;
  UINT EnableDns;
} FIXED_INFO,
*PFIXED_INFO;

DWORD WINAPI GetNetworkParams(PFIXED_INFO info, PULONG size);

#endif // MAX_HOSTNAME_LEN


// Call GetComputerNameEx() if available (Win2000/XP)

static BOOL CallGetComputerNameExA(int type, LPSTR name, LPDWORD size)
{
	HANDLE hdll;
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


// Call GetNetworkParams() if available (Win98/ME/2000/XP)

static DWORD CallGetNetworkParams(PFIXED_INFO info, PULONG size)
{
	HANDLE hdll;
	DWORD (WINAPI * GetNetworkParams_p)(PFIXED_INFO, PULONG);
	DWORD ret;
	if (!(hdll = LoadLibraryA("IPHlpApi.dll")))
		return ERROR_NOT_SUPPORTED;
	if (!(GetNetworkParams_p = (DWORD (WINAPI *)(PFIXED_INFO, PULONG))GetProcAddress(hdll, "GetNetworkParams")))
		ret = ERROR_NOT_SUPPORTED;
	else
		ret = GetNetworkParams_p(info, size);
	FreeLibrary(hdll);
	return ret;
}


// Get host/domainname from registry (Win98/ME/NT4/2000/XP)

static DWORD GetNamesFromRegistry(BOOL domain, char * name, int len)
{
	HKEY hk; DWORD size, type;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
	    (GetVersion() & 0x80000000
	     ? "System\\CurrentControlSet\\Services\\VxD\\MSTCP" //Win9x/ME
	     : "System\\CurrentControlSet\\Services\\Tcpip\\Parameters"),
	    0, KEY_READ, &hk) != ERROR_SUCCESS)
		return 0;
	size = len-1;
	if (!(RegQueryValueExA(hk, (!domain?"HostName":"Domain"), 0, &type, name, &size) == ERROR_SUCCESS && type == REG_SZ))
		size = 0;
	if (size == 0 && domain) {
		size = len-1;
		if (!(RegQueryValueExA(hk, "DhcpDomain", 0, &type, name, &size) == ERROR_SUCCESS && type == REG_SZ))
			size = 0;
	}
	RegCloseKey(hk);
	return size;
}


static int gethostdomname(int domain, char * name, int len)
{
	DWORD size; FIXED_INFO info;

	// try KERNEL32.dll::GetComputerNameEx()
	size = len - 1;
	if (CallGetComputerNameExA((!domain ? 1:2/*ComputerNameDnsHost:Domain*/), name, &size))
		return 0;

	// try IPHlpApi.dll::GetNetworkParams() 
	size = sizeof(info);
	if (CallGetNetworkParams(&info, &size) == ERROR_SUCCESS) {
		strncpy(name, (!domain?info.HostName:info.DomainName), len-1); name[len-1] = 0;
		return 0;
	}

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
