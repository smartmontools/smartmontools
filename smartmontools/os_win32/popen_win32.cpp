/*
 * os_win32/popen_win32.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2018-21 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "popen.h"

const char * popen_win32_cpp_cvsid = "$Id$"
  POPEN_H_CVSID;

#include <errno.h>
#include <fcntl.h>
#include <io.h> // _open_osfhandle()
#include <signal.h> // SIGSEGV
#include <stdlib.h>
#include <string.h>

#include <windows.h>

static HANDLE create_restricted_token()
{
  // Create SIDs for SYSTEM and Local Adminstrator
  union {
    SID sid;
    char sid_space[32]; // 16
  } adm, sys; // "S-1-5-18", "S-1-5-32-544"
  DWORD adm_size = sizeof(adm), sys_size = sizeof(sys);
  if (!(   CreateWellKnownSid(WinBuiltinAdministratorsSid, (PSID)0, &adm.sid, &adm_size)
        && CreateWellKnownSid(WinLocalSystemSid, (PSID)0, &sys.sid, &sys_size)          )) {
    errno = ENOMEM;
    return (HANDLE)0;
  }

  // Open token of current process
  HANDLE proc_token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc_token)) {
    errno = EPERM;
    return (HANDLE)0;
  }

  // Get Owner of current process: S-1-5-21-SYSTEM-GUID-USERID
  union {
    TOKEN_USER user;
    char user_space[128]; // TODO: Max size?
  } usr;
  DWORD size = 0;
  if (!GetTokenInformation(proc_token, TokenUser, &usr, sizeof(usr), &size)) {
    CloseHandle(proc_token);
    errno = EPERM;
    return (HANDLE)0;
  }

  // Restricting token from SYSTEM or local Administrator is not effective
  if (EqualSid(usr.user.User.Sid, &sys.sid) || EqualSid(usr.user.User.Sid, &adm.sid)) {
    CloseHandle(proc_token);
    errno = EINVAL;
    return (HANDLE)0;
  }

  // The default DACL of an elevated process may not contain the user itself:
  // D:(A;;GA;;;BA)(A;;GA;;;SY)[(A;;GXGR;;;S-1-5-5-0-LOGON_ID)]
  // The restricted process then fails to start because the hidden
  // console cannot be accessed.  Use a standard default DACL instead:
  // D:(A;;GA;;;S-1-5-21-SYSTEM-GUID-USERID)(A;;GA;;;BA)(A;;GA;;;SY)
  union {
    ACL acl;
    char acl_space[256]; // 236
  } dacl;
  if (!(   InitializeAcl(&dacl.acl, sizeof(dacl), ACL_REVISION)
        && AddAccessAllowedAce(&dacl.acl, ACL_REVISION, GENERIC_ALL, usr.user.User.Sid)
        && AddAccessAllowedAce(&dacl.acl, ACL_REVISION, GENERIC_ALL, &adm.sid)
        && AddAccessAllowedAce(&dacl.acl, ACL_REVISION, GENERIC_ALL, &sys.sid)         )) {
    CloseHandle(proc_token);
    errno = ENOMEM;
    return (HANDLE)0;
  }

  // Create new token with local Administrator and most Privileges dropped
  SID_AND_ATTRIBUTES sid_to_disable = {&adm.sid, 0};
  HANDLE restr_token;
  BOOL ok = CreateRestrictedToken(proc_token,
    DISABLE_MAX_PRIVILEGE, // Keep only "SeChangeNotifyPrivilege"
    1, &sid_to_disable, // Disable "S-1-5-32-544" (changes group to deny only)
    0, (LUID_AND_ATTRIBUTES *)0, // No further privileges
    0, (SID_AND_ATTRIBUTES *)0, // No restricted SIDs
    &restr_token
  );
  CloseHandle(proc_token);

  if (!ok) {
    errno = EPERM;
    return (HANDLE)0;
  }

  // Set new Default DACL
  TOKEN_DEFAULT_DACL tdacl = { &dacl.acl };
  if (!SetTokenInformation(restr_token, TokenDefaultDacl, &tdacl, sizeof(tdacl))) {
    CloseHandle(restr_token);
    errno = EPERM;
    return (HANDLE)0;
  }

  return restr_token;
}

bool popen_as_restr_check()
{
  HANDLE restr_token = create_restricted_token();
  if (!restr_token)
    return false;
  CloseHandle(restr_token);
  return true;
}

static FILE * s_popen_file;
static HANDLE s_popen_process;

FILE * popen_as_restr_user(const char * cmd, const char * mode, bool restricted)
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
  int cmdlen = strlen(cmd);
  char * shellcmd = (char *)malloc(7 + cmdlen + 1);
  if (!shellcmd) {
    fclose(f); // CloseHandle(pipe_out_r)
    CloseHandle(pipe_out_w);
    errno = ENOMEM;
    return (FILE *)0;
  }
  memcpy(shellcmd, "cmd /c ", 7);
  memcpy(shellcmd + 7, cmd, cmdlen + 1);

  // Create a restricted token if requested
  HANDLE restr_token = 0;
  if (restricted) {
    restr_token = create_restricted_token();
    if (!restr_token) {
      int err = errno;
      fclose(f); // CloseHandle(pipe_out_r)
      CloseHandle(pipe_out_w);
      errno = err;
      return (FILE *)0;
    }
  }

  // Redirect stdin stderr to null device
  // Don't inherit parent's stdin, script may hang if parent has no console.
  SECURITY_ATTRIBUTES sa_inherit = { sizeof(sa_inherit), (SECURITY_DESCRIPTOR *)0, TRUE };
  HANDLE null_in  = CreateFileA("nul", GENERIC_READ , 0, &sa_inherit, OPEN_EXISTING, 0, (HANDLE)0);
  HANDLE null_err = CreateFileA("nul", GENERIC_WRITE, 0, &sa_inherit, OPEN_EXISTING, 0, (HANDLE)0);

  // Set stdio handles
  STARTUPINFO si{}; si.cb = sizeof(si);
  si.hStdInput  = null_in;
  si.hStdOutput = pipe_out_w;
  si.hStdError  = null_err;
  si.dwFlags = STARTF_USESTDHANDLES;

  // Create process
  PROCESS_INFORMATION pi;
  BOOL ok;
  const char * shell = getenv("COMSPEC");
  if (restr_token) {
    ok = CreateProcessAsUserA(
      restr_token,
      shell, // "C:\Windows\System32\cmd.exe" or nullptr
      shellcmd, // "cmd /c COMMAND" ("cmd" searched in PATH if COMSPEC not set)
      (SECURITY_ATTRIBUTES *)0, (SECURITY_ATTRIBUTES *)0,
      TRUE, // inherit
      CREATE_NO_WINDOW, // DETACHED_PROCESS would open new console(s)
      (void *)0, (char *)0, &si, &pi
    );
  }
  else {
    ok = CreateProcessA(
      shell, // "C:\Windows\System32\cmd.exe" or nullptr
      shellcmd, // "cmd /c COMMAND" ("cmd" searched in PATH if COMSPEC not set)
      (SECURITY_ATTRIBUTES *)0, (SECURITY_ATTRIBUTES *)0,
      TRUE, // inherit
      CREATE_NO_WINDOW, // DETACHED_PROCESS would open new console(s)
      (void *)0, (char *)0, &si, &pi
    );
  }
  free(shellcmd);

  // Close inherited handles
  CloseHandle(null_err);
  CloseHandle(null_in);
  if (restr_token)
    CloseHandle(restr_token);
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
FILE * popen(const char * cmd, const char * mode)
{
  return popen_as_restr_user(cmd, mode, false);
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

// Test program
#ifdef TEST

int main(int argc, char **argv)
{
  bool restricted = false;
  int ai = 1;
  if (argc > 1 && !strcmp(argv[ai], "-r")) {
    restricted = true;
    ai++;
  }
  if (ai + 1 != argc) {
    printf("Usage: %s [-r] \"COMMAND ARG...\"\n", argv[0]);
    return 1;
  }
  const char * cmd = argv[ai];

  printf("popen_as_restr_check() = %s\n", (popen_as_restr_check() ? "true" : "false"));
  printf("popen_as_restr_user(\"%s\", \"r\", %s):\n", cmd, (restricted ? "true" : "false"));
  FILE * f = popen_as_restr_user(cmd, "r", restricted);
  if (!f) {
    perror("popen_as_restr_user");
    return 1;
  }

  int cnt, c;
  for (cnt = 0; (c = getc(f)) != EOF; cnt++)
    putchar(c);
  printf("[EOF]\nread %d bytes\n", cnt);

  int status = pclose(f);

  if (status == -1) {
    perror("pclose");
    return 1;
  }
  printf("pclose() = 0x%04x (exit = %d, sig = %d)\n",
    status, WEXITSTATUS(status), WTERMSIG(status));
  return status;
}

#endif
