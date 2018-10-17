/*
 * os_win32/daemon_win32.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-18 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DAEMON_WIN32_H
#define DAEMON_WIN32_H

#define DAEMON_WIN32_H_CVSID "$Id: daemon_win32.h 4818 2018-10-17 05:32:17Z chrfranke $"

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

#endif // DAEMON_WIN32_H
