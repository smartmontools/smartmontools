/*
 * os_win32/syslogevt.c
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

static char rcsid[] = "$Id: syslogevt.c,v 1.1 2004/03/15 10:48:28 chrfranke Exp $";

#include <stdio.h>
#include <string.h>
#include <process.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _DEBUG
#include "syslogevt.h"
#endif


static int usage()
{
	puts(
		"syslogevt $Revision: 1.1 $ Copyright (C) 2004 Christian Franke\n"
		"Home page is http://smartmontools.sourceforge.net/\n"
		"\n"
		"Usage: syslogevt [-ru] name [ident ...]\n"
		"\n"
		"Creates registry files \"name-r.reg\" and \"name-u.reg\" to (un)register\n"
		"this program as an event message file for message source(s) \"ident\".\n"
		"If \"ident\" is ommited, \"name\" is used. Options:\n"
		"\n"
		"    -r    run \"regedit name-r.reg\" after creating files\n"
		"    -u    run \"regedit name-u.reg\" after creating files\n"
		"\n"
		"Examples:\n"
		"\n"
		"syslogevt smartd                     (Create smartd-r.reg and smartd-u.reg)\n"
		"regedit smartd-r.reg           (Register syslogevt.exe for smartd messages)\n"
		"\n"
		"syslogevt -r smartd                             (Same as above in one step)\n"
		"\n"
		"regedit smartd-u.reg                                (Undo the registration)\n"
		"\n"
		"CAUTION: A registry entry of an existing event source with the same \"ident\"\n"
		"         will be overwritten by regedit without notice."
	);
	return 1;
}

main(int argc, char ** argv)
{
	int regedit, a1, ai;
	char name1[30+1], name2[30+1], mypath[MAX_PATH+1];
	const char * ident;
	FILE * f1, * f2;

#ifdef _DEBUG
	if (MSG_SYSLOG != 0) {
		puts("Internal error: MSG_SYSLOG != 0"); return 1;
	}
#endif

	if (argc < 2)
		return usage();

	a1 = 1;
	regedit = 0;
	if (!strcmp(argv[a1], "-r")) {
		regedit = 1; a1++;
	}
	else if (!strcmp(argv[a1], "-u")) {
		regedit = -1; a1++;
	}

	for (ai = a1; ai < argc; ai++) {
		ident = argv[ai];
		if (!(ident[0] && strlen(ident) < sizeof(name1)-10
			  && strcspn(ident, "-.:/\\") == strlen(ident) )) {
			return usage();
		}
	}

	if (!GetModuleFileName(NULL, mypath, sizeof(mypath)-1)) {
		fputs("GetModuleFileName failed\n", stderr);
		return 1;
	}

	ident = argv[a1];
	strcpy(name1, ident); strcat(name1, "-r.reg");
	strcpy(name2, ident); strcat(name2, "-u.reg");

	if (!(f1 = fopen(name1, "w"))) {
		perror(name1); return 1;
	}
	if (!(f2 = fopen(name2, "w"))) {
		perror(name2); unlink(name1); return 1;
	}

	fputs("REGEDIT4\n\n", f1);
	fputs("REGEDIT4\n\n", f2);

	for (ai = (argc > a1+1 ? a1+1 : a1); ai < argc; ai++) {
		int i;
		ident = argv[ai];
		fputs("[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\", f1);
		fputs(ident, f1); fputs("]\n\"EventMessageFile\"=\"", f1);
		for (i = 0; mypath[i]; i++) {
			if (mypath[i] == '\\')
				fputc('\\', f1);
			fputc(mypath[i], f1);
		}
		fputs("\"\n\"TypesSupported\"=dword:00000007\n\n", f1);

		fputs("[-HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\", f2);
		fputs(ident, f2); fputs("]\n\n", f2);
	}

	fclose(f1);
	fclose(f2);

	if (GetVersion() & 0x80000000) {
		puts("Warning: Event log not supported on Win9x/ME\n");
		if (regedit)
			return 1;
	}

	if (regedit) {
		if (spawnlp(P_WAIT, "regedit", "regedit", (regedit > 0 ? name1 : name2), (const char *)0) == -1) {
			fputs("regedit: cannot execute\n", stderr);
			return 1;
		}
	}
	else {
		fputs("Files generated. Use\n\n    regedit ", stdout);
		puts(name1);
		fputs("\nto register event message file, and\n\n    regedit ", stdout);
		puts(name2);
		fputs("\nto remove registration later.\n\n"
			  "Do not remove this program when registered.\n", stdout);
	}

	return 0;
}
