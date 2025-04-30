/*
 * popen_as_ugid.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef POPEN_AS_UGID_H_CVSID
#define POPEN_AS_UGID_H_CVSID "$Id$"

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string>

// Wrapper for popen(3) which prevents that unneeded file descriptors
// are inherited to the command run by popen() and optionally drops
// privileges of root user:
// If uid != 0, popen() is run as this user.
// If gid != 0, popen() is run as this group and no supplemental groups.
// Only mode "r" is supported.  Only one open stream at a time is supported.
FILE * popen_as_ugid(const char * cmd, const char * mode, uid_t uid, gid_t gid);

// Call corresponding pclose(3) and return its result.
int pclose_as_ugid(FILE * f);

// Parse "USER[:GROUP]" string and set uid, gid, uname and gname accordingly.
// USER and GROUP may be specified as numeric ids or names.
// If a numeric id is used and the corresponding user (or group) does not
// exist, the function succeeds but leaves uname (or gname) unchanged.
// If no GROUP is specified, the default group of USER is used instead.
// Returns nullptr on success or a message string on error.
const char * parse_ugid(const char * s, uid_t & uid, gid_t & gid,
                        std::string & uname, std::string & gname );

#endif // POPEN_AS_UGID_H_CVSID
