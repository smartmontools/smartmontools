/*
 * WTSSendMessage() command line tool
 *
 * Home page of code is: http://www.smartmontools.org
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

char svnid[] = "$Id$";

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>


static int usage()
{
  printf("wtssendmsg $Revision$ - Display a message box on client desktops\n"
         "Copyright (C) 2012 Christian Franke, smartmontools.org\n\n"
         "Usage: wtssendmsg [-cas] [-v] [\"Caption\"] \"Message\"|-\n"
         "       wtssendmsg -v\n\n"
         "  -c    Console session [default]\n"
         "  -a    Active sessions\n"
         "  -s    Connected sessions\n"
         "  -v    List sessions\n"
  );
  return 1;
}

int main(int argc, const char **argv)
{
  int mode = 0, verbose = 0, status = 0, i;
  const char * message = 0, * caption = "";
  char msgbuf[1024];
  WTS_SESSION_INFOA * sessions; DWORD count;

  for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
    int j;
    for (j = 1; argv[i][j]; j++)
      switch (argv[i][j]) {
      case 'c': mode = 0; break;
      case 'a': mode = 1; break;
      case 's': mode = 2; break;
      case 'v': verbose = 1; break;
      default: return usage();
    }
  }

  if (i < argc) {
    if (i+1 < argc)
      caption = argv[i++];

    message = argv[i++];
    if (i < argc)
      return usage();

    if (!strcmp(message, "-")) {
      // Read message from stdin
      i = fread(msgbuf, 1, sizeof(msgbuf)-1, stdin);
      if (i < 0) {
        perror("stdin");
        return 1;
      }
      msgbuf[i] = 0;
      message = msgbuf;
    }
  }
  else {
      if (!verbose)
        return usage();
  }

  // Get session list
  if (!WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
    fprintf(stderr, "WTSEnumerateSessions() failed\n");
    return 1;
  }

  for (i = 0; i < (int)count; i++) {

    if (verbose) {
      printf("Session %d (\"%s\", State=%d)%s",
             i, sessions[i].pWinStationName, sessions[i].State,
             (!message ? "\n" : ": "));
      if (!message)
        continue; // List sessions only
      fflush(stdout);
    }

    if (   !strcmpi(sessions[i].pWinStationName, "Console")
        || (mode >= 1 && sessions[i].State == WTSActive)
        || (mode >= 2 && sessions[i].State == WTSConnected)) {

      // Send Message, don't wait for OK button
      DWORD result;
      if (WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId,
          (char *)caption, strlen(caption),
          (char *)message, strlen(message),
          MB_OK|MB_ICONEXCLAMATION, 0 /*Timeout*/,
          &result, FALSE /*!Wait*/)) {
        if (verbose)
          printf("message sent\n");
      }
      else {
        status = 1;
        if (verbose)
          printf("WTSSendMessage() failed with error=%d\n", (int)GetLastError());
        else
          fprintf(stderr, "Session %d (\"%s\", State=%d): WTSSendMessage() failed with error=%d\n",
                  i, sessions[i].pWinStationName, sessions[i].State, (int)GetLastError());
      }
    }
    else {
      if (verbose)
        printf("ignored\n");
    }
  }

  WTSFreeMemory(sessions);

  return status;
}
