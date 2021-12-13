/*
 * popen_as_ugid.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "popen_as_ugid.h"

const char * popen_as_ugid_cvsid = "$Id$"
  POPEN_AS_UGID_H_CVSID;

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static FILE * s_popen_file /* = 0 */;
static pid_t s_popen_pid /* = 0 */;

FILE * popen_as_ugid(const char * cmd, const char * mode, uid_t uid, gid_t gid)
{
  // Only "r" supported
  if (*mode != 'r') {
    errno = EINVAL;
    return (FILE *)0;
  }

  // Only one stream supported
  if (s_popen_file) {
    errno = EMFILE;
    return (FILE *)0;
  }

  int pd[2] = {-1, -1};
  int sd[2] = {-1, -1};
  FILE * fp = 0;
  pid_t pid;
  errno = 0;
  if (!(// Create stdout and status pipe ...
        !pipe(pd) && !pipe(sd) &&
        // ... connect stdout pipe to FILE ...
        !!(fp = fdopen(pd[0], "r")) &&
        // ... and then fork()
        (pid = fork()) != (pid_t)-1)  ) {
    int err = (errno ? errno : ENOSYS);
    if (fp) {
      fclose(fp); close(pd[1]);
    }
    else if (pd[0] >= 0) {
      close(pd[0]); close(pd[1]);
    }
    if (sd[0] >= 0) {
      close(sd[0]); close(sd[1]);
    }
    errno = err;
    return (FILE *)0;
  }

  if (!pid) { // Child
    // Do not inherit any unneeded file descriptors
    fclose(fp);
    for (int i = 0; i < getdtablesize(); i++) {
      if (i == pd[1] || i == sd[1])
        continue;
      close(i);
    }

    FILE * fc = 0;
    int err = errno = 0;
    if (!(// Connect stdio to /dev/null ...
          open("/dev/null", O_RDWR) == 0 &&
          dup(0) == 1 && dup(0) == 2 &&
          // ... don't inherit pipes ...
          !fcntl(pd[1], F_SETFD, FD_CLOEXEC) &&
          !fcntl(sd[1], F_SETFD, FD_CLOEXEC) &&
          // ... set group and user (assumes we are root) ...
          (!gid || (!setgid(gid) && !setgroups(1, &gid))) &&
          (!uid || !setuid(uid)) &&
          // ... and then call popen() from std library
          !!(fc = popen(cmd, mode))                         )) {
      err = (errno ? errno : ENOSYS);
    }

    // Send setup result to parent
    if (write(sd[1], &err, sizeof(err)) != (int)sizeof(err))
      err = EIO;
    close(sd[1]);
    if (!fc)
      _exit(127);

    // Send popen's FILE stream to parent's FILE
    int c;
    while (!err && (c = getc(fc)) != EOF) {
      char cb = (char)c;
      if (write(pd[1], &cb, 1) != 1)
        err = EIO;
    }

    // Return status or re-throw signal
    int status = pclose(fc);
    if (WIFSIGNALED(status))
      kill(getpid(), WTERMSIG(status));
    _exit(WIFEXITED(status) ? WEXITSTATUS(status) : 127);
  }

  // Parent
  close(pd[1]); close(sd[1]);

  // Get setup result from child
  int err = 0;
  if (read(sd[0], &err, sizeof(err)) != (int)sizeof(err))
    err = EIO;
  close(sd[0]);
  if (err) {
    fclose(fp);
    errno = err;
    return (FILE *)0;
  }

  // Save for pclose_as_ugid()
  s_popen_file = fp;
  s_popen_pid = pid;
  return fp;
}

int pclose_as_ugid(FILE * f)
{
  if (f != s_popen_file) {
    errno = EBADF;
    return -1;
  }

  fclose(f);
  s_popen_file = 0;

  pid_t pid; int status;
  do
    pid = waitpid(s_popen_pid, &status, 0);
  while (pid == (pid_t)-1 && errno == EINTR);
  s_popen_pid = 0;

  if (pid == (pid_t)-1)
    return -1;
  return status;
}

const char * parse_ugid(const char * s, uid_t & uid, gid_t & gid,
                        std::string & uname, std::string & gname )
{
  // Split USER:GROUP
  int len = strlen(s), n1 = -1, n2 = -1;
  char un[64+1] = "", gn[64+1] = "";
  if (!(  sscanf(s, "%64[^ :]%n:%64[^ :]%n", un, &n1, gn, &n2) >= 1
        && (n1 == len || n2 == len)                                )) {
    return "Syntax error";
  }

  // Lookup user
  const struct passwd * pwd;
  unsigned u = 0;
  if (sscanf(un, "%u%n", &u, (n1 = -1, &n1)) == 1 && n1 == (int)strlen(un)) {
    uid = (uid_t)u;
    pwd = getpwuid(uid);
  }
  else {
    pwd = getpwnam(un);
    if (!pwd)
      return "Unknown user name";
    uid = pwd->pw_uid;
  }
  if (pwd)
    uname = pwd->pw_name;

  const struct group * grp;
  if (gn[0]) {
    // Lookup group
    unsigned g = 0;
    if (sscanf(gn, "%u%n", &g, (n1 = -1, &n1)) == 1 && n1 == (int)strlen(gn)) {
      gid = (gid_t)g;
      grp = getgrgid(gid);
    }
    else {
      grp = getgrnam(gn);
      if (!grp)
        return "Unknown group name";
      gid = grp->gr_gid;
    }
  }
  else {
    // Use default group
    if (!pwd)
       return "Unknown default group";
    gid = pwd->pw_gid;
    grp = getgrgid(gid);
  }
  if (grp)
    gname = grp->gr_name;

  return (const char *)0;
}

// Test program
#ifdef TEST

int main(int argc, char **argv)
{
  const char * user_group, * cmd;
  switch (argc) {
    case 2: user_group = 0; cmd = argv[1]; break;
    case 3: user_group = argv[1]; cmd = argv[2]; break;
    default:
      printf("Usage: %s [USER[:GROUP]] \"COMMAND ARG...\"\n", argv[0]);
      return 1;
  }

  int leak = open("/dev/null", O_RDONLY);

  FILE * f;
  if (user_group) {
    uid_t uid; gid_t gid;
    std::string uname = "unknown", gname = "unknown";
    const char * err = parse_ugid(user_group, uid, gid, uname, gname);
    if (err) {
      fprintf(stderr, "Error: %s\n", err);
      return 1;
    }
    printf("popen_as_ugid(\"%s\", \"r\", %u(%s), %u(%s)):\n", cmd,
           (unsigned)uid, uname.c_str(), (unsigned)gid, gname.c_str());
    f = popen_as_ugid(cmd, "r", uid, gid);
  }
  else {
    printf("popen(\"%s\", \"r\"):\n", cmd);
    f = popen(cmd, "r");
  }
  fflush(stdout);
  close(leak);

  if (!f) {
    perror("popen");
    return 1;
  }

  int cnt, c;
  for (cnt = 0; (c = getc(f)) != EOF; cnt++)
    putchar(c);
  printf("[EOF]\nread %d bytes\n", cnt);

  int status;
  if (user_group)
    status = pclose_as_ugid(f);
  else
    status = pclose(f);

  if (status == -1) {
     perror("pclose");
     return 1;
  }
  printf("pclose() = 0x%04x (exit = %d, sig = %d)\n",
         status, WEXITSTATUS(status), WTERMSIG(status));
  return 0;
}

#endif
