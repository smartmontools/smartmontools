/*
 * os_win32/hostname_win32.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-8 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#ifndef HOSTNAME_WIN32_H
#define HOSTNAME_WIN32_H

#define HOSTNAME_WIN32_H_CVSID "$Id: hostname_win32.h,v 1.5 2008/03/04 22:09:48 ballen4705 Exp $\n"

#ifdef __cplusplus
extern "C" {
#endif

int gethostname(char * name, int len);
int getdomainname(char * name, int len);

#ifdef __cplusplus
}
#endif

#endif // HOSTNAME_WIN32_H
