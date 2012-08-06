/*
 * os_win32/wtssendmsg.cpp
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

#define WINVER 0x0500
#define _WIN32_WINNT WINVER

#include "wtssendmsg.h"

const char * wtssendmsg_cpp_cvsid = "$Id$"
  WTSSENDMSG_H_CVSID;

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>


int wts_send_message(int mode, const char * title, const char * text,
                     int * perrcnt /* = 0 */)
{
  if (perrcnt)
    *perrcnt = 0;

  // Get session list
  WTS_SESSION_INFOA * sessions; DWORD count;
  if (!WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count))
    return -1;

  int msgcnt = 0;
  for (unsigned i = 0; i < count; i++) {

    if (   !strcmpi(sessions[i].pWinStationName, "Console")
        || (mode >= 1 && sessions[i].State == WTSActive)
        || (mode >= 2 && sessions[i].State == WTSConnected)) {

      // Send Message, don't wait for OK button
      DWORD result;
      if (WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId,
          const_cast<char *>(title), strlen(title),
          const_cast<char *>(text), strlen(text),
          MB_OK|MB_ICONEXCLAMATION, 0 /*Timeout*/,
          &result, FALSE /*Wait*/))
        msgcnt++;
      else if (perrcnt)
        (*perrcnt)++;
    }
  }

  WTSFreeMemory(sessions);
  return msgcnt;
}
