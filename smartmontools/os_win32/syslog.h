/*
 * os_win32/syslog.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-8 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef SYSLOG_H
#define SYSLOG_H

#define SYSLOG_H_CVSID "$Id$\n"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EVENTLOG_ERROR_TYPE: */
#define LOG_EMERG       0
#define LOG_ALERT       1
#define LOG_CRIT        2
#define LOG_ERR         3
/* EVENTLOG_WARNING_TYPE: */
#define LOG_WARNING     4
/* EVENTLOG_INFORMATION_TYPE: */
#define LOG_NOTICE      5
#define LOG_INFO        6
#define LOG_DEBUG       7

/* event log: */
#define LOG_DAEMON      ( 3<<3)
/* ident.log: */
#define LOG_LOCAL0      (16<<3)
/* ident1-7.log: */
#define LOG_LOCAL1      (17<<3)
#define LOG_LOCAL2      (18<<3)
#define LOG_LOCAL3      (19<<3)
#define LOG_LOCAL4      (20<<3)
#define LOG_LOCAL5      (21<<3)
#define LOG_LOCAL6      (22<<3)
#define LOG_LOCAL7      (23<<3)

#define LOG_FACMASK     0x03f8
#define LOG_FAC(f)      (((f) & LOG_FACMASK) >> 3)
 
#define LOG_PID         0x01

void openlog(const char * ident, int option, int facility);

void closelog(void);

void vsyslog(int priority, const char * message, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* SYSLOG_H */
