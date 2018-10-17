/*
 * os_win32/popen_win32.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2018 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "popen.h"

const char * popen_win32_cpp_cvsid = "$Id: popen_win32.cpp 4818 2018-10-17 05:32:17Z chrfranke $"
  POPEN_H_CVSID;

#include <errno.h>
#include <fcntl.h>
#include <io.h> // _open_osfhandle()
#include <signal.h> // SIGSEGV
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static FILE * s_popen_file;
static HANDLE s_popen_process;

extern "C"
FILE * popen(const char * command, const char * mode)
{
  // Fail if previous run is still in progress
  if (s_popen_file) {
    errno = EEXIST;
    return (FILE *)0;
  }

  // mode "w" is not implemented
  if (!(mode[0] == 'r' && (!mode[1] || !mode[2]))) {
    errno = EINVAL;
    return (FILE *)0;
  }

  // Set flags for text or binary mode
  // Note: _open_osfhandle() ignores _fmode and defaults to O_BINARY
  int oflags; const char * fomode;
  switch (mode[1]) {
    case 0:
    case 't':
      oflags = O_RDONLY|O_TEXT;
      fomode = "rt";
      break;
    case 'b':
      oflags = O_RDONLY|O_BINARY;
      fomode = "rb";
      break;
    default:
      errno = EINVAL;
      return (FILE *)0;
  }

  // Create stdout pipe with inheritable write end
  HANDLE pipe_out_r, pipe_out_w;
  if (!CreatePipe(&pipe_out_r, &pipe_out_w, (SECURITY_ATTRIBUTES *)0, 1024)) {
    errno = EMFILE;
    return (FILE *)0;
  }
  if (!SetHandleInformation(pipe_out_w, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
    CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
    errno = EMFILE;
    return (FILE *)0;
  }

  // Connect pipe read end to new FD
  int fd = _open_osfhandle((intptr_t)pipe_out_r, oflags);
  if (fd < 0) {
    CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
    return (FILE *)0;
  }

  // Connect FD to new FILE
  FILE * f = fdopen(fd, fomode);
  if (!f) {
    int err = errno;
    close(fd); // CloseHandle(pipe_out_r)
    CloseHandle(pipe_out_w);
    errno = err;
    return (FILE *)0;
  }

  // Build command line "cmd /c COMMAND"
  int cmdlen = strlen(command);
  char * shellcmd = (char *)malloc(7 + cmdlen + 1);
  if (!shellcmd) {
    fclose(f); // CloseHandle(pipe_out_r)
    CloseHandle(pipe_out_w);
    errno = ENOMEM;
    return (FILE *)0;
  }
  memcpy(shellcmd, "cmd /c ", 7);
  memcpy(shellcmd + 7, command, cmdlen + 1);

  // Redirect stdin stderr to null device
  // Don't inherit parent's stdin, script may hang if parent has no console.
  SECURITY_ATTRIBUTES sa_inherit = { sizeof(sa_inherit), (SECURITY_DESCRIPTOR *)0, TRUE };
  HANDLE null_in  = CreateFile("nul", GENERIC_READ , 0, &sa_inherit, OPEN_EXISTING, 0, (HANDLE)0);
  HANDLE null_err = CreateFile("nul", GENERIC_WRITE, 0, &sa_inherit, OPEN_EXISTING, 0, (HANDLE)0);

  // Set stdio handles
  STARTUPINFO si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
  si.hStdInput  = null_in;
  si.hStdOutput = pipe_out_w;
  si.hStdError  = null_err;
  si.dwFlags = STARTF_USESTDHANDLES;

  // Create process
  PROCESS_INFORMATION pi;
  BOOL ok = CreateProcessA(
    getenv("COMSPEC"), // "C:\WINDOWS\system32\cmd.exe" or nullptr
    shellcmd, // "cmd /c COMMAND" ("cmd" searched in PATH if COMSPEC not set)
    (SECURITY_ATTRIBUTES *)0, (SECURITY_ATTRIBUTES *)0,
    TRUE, // inherit
    CREATE_NO_WINDOW, // DETACHED_PROCESS would open new console(s)
    (void *)0, (char *)0, &si, &pi
  );
  free(shellcmd);

  // Close inherited handles
  CloseHandle(null_err);
  CloseHandle(null_in);
  CloseHandle(pipe_out_w);

  if (!ok) {
    fclose(f); // CloseHandle(pipe_out_r)
    errno = ENOENT;
    return (FILE *)0;
  }

  // Store process and FILE for pclose()
  CloseHandle(pi.hThread);
  s_popen_process = pi.hProcess;
  s_popen_file = f;

  return f;
}

extern "C"
int pclose(FILE * f)
{
  if (f != s_popen_file) {
    errno = EBADF;
    return -1;
  }

  fclose(f);
  s_popen_file = 0;

  // Wait for process exitcode
  DWORD exitcode = 42;
  bool ok = (   WaitForSingleObject(s_popen_process, INFINITE) == WAIT_OBJECT_0
             && GetExitCodeProcess(s_popen_process, &exitcode));

  CloseHandle(s_popen_process);
  s_popen_process = 0;

  if (!ok) {
    errno = ECHILD;
    return -1;
  }

  // Modify exitcode for wait(3) macros
  if (exitcode >> 23)
    return ((exitcode << 9) >> 1) | SIGSEGV;
  else
    return exitcode << 8;
}
