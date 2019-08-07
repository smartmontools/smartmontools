/*
 * WTSSendMessage() command line tool
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2012-19 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define WINVER 0x0501
#define _WIN32_WINNT WINVER

char svnid[] = "$Id$";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>


static int usage()
{
  printf("wtssendmsg $Revision$ - Display a message box on client desktops\n"
         "Copyright (C) 2012-19 Christian Franke, www.smartmontools.org\n\n"
         "Usage: wtssendmsg [-cas] [-t TIMEOUT] [-w 0..5] [-v] [\"Caption\"] \"Message\"|-\n"
         "       wtssendmsg -v\n\n"
         "  -c    Console session [default]\n"
         "  -a    Active sessions\n"
         "  -s    Connected sessions\n"
         "  -t    Remove message box after TIMEOUT seconds\n"
         "  -w    Select buttons and wait for response or timeout\n"
         "  -v    List sessions\n"
  );
  return 1;
}

static int getnum(const char * s)
{
  char * endp;
  int n = strtol(s, &endp, 10);
  if (*endp)
    return -1;
  return n;
}

int main(int argc, const char **argv)
{
  int mode = 0, timeout = 0, buttons = -1, verbose = 0, i;

  for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
    int j;
    for (j = 1; j > 0 && argv[i][j]; j++)
      switch (argv[i][j]) {
        case 'c': mode = 0; break;
        case 'a': mode = 1; break;
        case 's': mode = 2; break;
        case 't':
          if (argv[i][j+1] || ++i >= argc)
            return usage();
          timeout = getnum(argv[i]);
          if (timeout < 0)
            return usage();
          j = 0;
          break;
        case 'w':
          if (argv[i][j+1] || ++i >= argc)
            return usage();
          buttons = getnum(argv[i]);
          if (!(MB_OK <= buttons && buttons <= MB_RETRYCANCEL)) // 0..5
            return usage();
          j = 0;
          break;
        case 'v': verbose = 1; break;
        default: return usage();
      }
  }

  const char * message = 0, * caption = "";
  char msgbuf[1024];
  if (i < argc) {
    if (i+1 < argc)
      caption = argv[i++];

    message = argv[i++];
    if (i < argc)
      return usage();

    if (!strcmp(message, "-")) {
      // Read message from stdin
      // The message is also written to a Windows event log entry, so
      // don't convert '\r\n' to '\n' (the MessageBox works with both)
      DWORD size = 0;
      if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE),
                    msgbuf, sizeof(msgbuf)-1, &size, (OVERLAPPED*)0)) {
        fprintf(stderr, "Read from stdin failed\n");
        return 1;
      }
      msgbuf[size] = 0;
      message = msgbuf;
    }
  }
  else {
      if (!verbose)
        return usage();
  }

  // Get session list
  WTS_SESSION_INFOA * sessions; DWORD count;
  if (!WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
    fprintf(stderr, "WTSEnumerateSessions() failed\n");
    return 1;
  }

  int status = 0;
  for (i = 0; i < (int)count; i++) {

    if (verbose) {
      printf("Session %d (\"%s\", State=%d)%s",
             i, sessions[i].pWinStationName, sessions[i].State,
             (!message ? "\n" : ": "));
      if (!message)
        continue; // List sessions only
      fflush(stdout);
    }

    // Check session state
    if (!(   !strcmpi(sessions[i].pWinStationName, "Console")
          || (mode >= 1 && sessions[i].State == WTSActive)
          || (mode >= 2 && sessions[i].State == WTSConnected))) {
      if (verbose)
        printf("ignored\n");
      continue;
    }

    // Send Message
    DWORD response = ~0;
    if (!WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId,
                         (char *)caption, strlen(caption),
                         (char *)message, strlen(message),
                         (buttons <= MB_OK ? MB_OK|MB_ICONEXCLAMATION
                          : buttons|MB_DEFBUTTON2|MB_ICONQUESTION    ),
                         timeout, &response, (buttons >= MB_OK) /*Wait?*/ )) {
      status |= 0x01;
      if (verbose)
        printf("WTSSendMessage() failed with error=%d\n", (int)GetLastError());
      else
        fprintf(stderr, "Session %d (\"%s\", State=%d): WTSSendMessage() failed with error=%d\n",
                i, sessions[i].pWinStationName, sessions[i].State, (int)GetLastError());
      continue;
    }

    if (buttons >= MB_OK) {
      switch (response) {
        case IDOK:
        case IDYES:    case IDABORT:   status |= 0x02; break;
        case IDNO:     case IDRETRY:   status |= 0x04; break;
        case IDCANCEL: case IDIGNORE:  status |= 0x08; break;
        case IDTIMEOUT:                status |= 0x10; break;
        default:                       status |= 0x01; break;
      }
      if (verbose)
        printf("response = %d, status = 0x%02x\n", (int)response, status);
    }
    else {
      // response == IDASYNC
      if (verbose)
        printf("message sent\n");
    }
  }

  WTSFreeMemory(sessions);

  return status;
}
