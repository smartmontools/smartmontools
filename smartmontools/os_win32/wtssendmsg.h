/*
 * os_win32/wtssendmsg.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2012 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#ifndef WTSSENDMSG_H
#define WTSSENDMSG_H

#define WTSSENDMSG_H_CVSID "$Id$"

// Send Message (via WTSSendMessage) to desktop of
// Console session (mode=0),
// and to all active sessions (mode=1)
// and to all connected sessions (mode=2).
// Returns number of messages sucessfully sent.
int wts_send_message(int mode, const char * title, const char * text,
                     int * perrcnt = 0);

#endif // WTSSENDMSG_H
