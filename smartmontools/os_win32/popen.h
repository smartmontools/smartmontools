/*
 * os_win32/popen.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2018-21 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef POPEN_H
#define POPEN_H

#define POPEN_H_CVSID "$Id$"

#include <stdio.h>

// MinGW <stdio.h> defines these to _popen/_pclose
#undef popen
#undef pclose

#ifdef __cplusplus
extern "C" {
#endif

// popen(3) reimplementation for Windows
//
// The _popen() from MSVCRT is not useful as it always opens a new
// console window if parent process has none.
//
// Differences to popen(3):
// - Only modes "r[bt]" are supported
// - stdin and stderr from parent are not inherited to child process
//   but redirected to null device
// - Only one child process can be run at a time

FILE * popen(const char * command, const char * mode);

int pclose(FILE * f);

#ifdef __cplusplus
}
#endif

// Enhanced version of popen() with ability to modify the access token.
// If 'restricted' is set, the child process is run with a restricted access
// token.  The local Administrator group and most privileges (all except
// SeChangeNotifyPrivilege) are removed.
FILE * popen_as_restr_user(const char * cmd, const char * mode, bool restricted);

// Check whether the access token of the current user could be effectively
// restricted.
// Returns false if the current user is the local SYSTEM or Administrator account.
bool popen_as_restr_check();

// wait(3) macros from <sys/wait.h>
#ifndef WIFEXITED
#define WIFEXITED(status)   (((status) & 0xff) == 0x00)
#define WIFSIGNALED(status) (((status) & 0xff) != 0x00)
#define WIFSTOPPED(status)  (0)
#define WEXITSTATUS(status) ((status) >> 8)
#define WTERMSIG(status)    ((status) & 0xff)
#define WSTOPSIG(status)    (0)
#endif // WIFEXITED

#endif // POPEN_H
