/*
 * os_win32/daemon_win32.h
 *
 * Home page of code is: http://www.smartmontools.org
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

#ifndef DAEMON_WIN32_H
#define DAEMON_WIN32_H

#define DAEMON_WIN32_H_CVSID "$Id$"

#include <signal.h>

// Additional non-ANSI signals
#define SIGHUP  (NSIG+1)
#define SIGUSR1 (NSIG+2)
#define SIGUSR2 (NSIG+3)


// Options for Windows service
typedef struct daemon_winsvc_options_s {
	const char * cmd_opt;  // argv[1] option for services
	// For service "install" command only:
	const char * svcname;  // Service name
	const char * dispname; // Service display name
	const char * descript; // Service description
} daemon_winsvc_options;


// This function must be called from main()
int daemon_main(const char * ident, const daemon_winsvc_options * svc_opts,
                int (*main_func)(int, char **), int argc, char **argv      );

// exit(code) returned by a service
extern int daemon_winsvc_exitcode;

// Simulate signal()
void (*daemon_signal(int sig, void (*func)(int)))(int);
const char * daemon_strsignal(int sig);

// Simulate sleep()
void daemon_sleep(int seconds);

// Disable/Enable console
void daemon_disable_console(void);
int daemon_enable_console(const char * title);

// Detach from console
int daemon_detach(const char * ident);

// Spawn a process and redirect stdio
int daemon_spawn(const char * cmd,
                 const char * inpbuf, int inpsize,
                 char *       outbuf, int outsize );

#endif // DAEMON_WIN32_H
