/*
 * os_win32/daemon_win32.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <io.h>

#define WIN32_LEAN_AND_MEAN
#ifdef _DEBUG
// IsDebuggerPresent() only included if compiling for >= NT4, >= Win98
#define _WIN32_WINDOWS 0x0800
#endif
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include "daemon_win32.h"

const char *daemon_win32_c_cvsid = "$Id: daemon_win32.c,v 1.3 2004/07/31 16:56:20 chrfranke Exp $"
DAEMON_WIN32_H_CVSID;


/////////////////////////////////////////////////////////////////////////////

// Prevent spawning of child process if debugging
#ifdef _DEBUG
#define debugging() IsDebuggerPresent()
#else
#define debugging() FALSE
#endif


#define EVT_NAME_LEN 260

// Internal events (must be > SIGUSRn)
#define EVT_RUNNING   100 // Exists when running, signaled on creation
#define EVT_DETACHED  101 // Signaled when child detaches from console
#define EVT_RESTART   102 // Signaled when child should restart

static void make_name(char * name, int sig)
{
	int i;
	if (!GetModuleFileNameA(NULL, name, EVT_NAME_LEN-10))
		strcpy(name, "DaemonEvent");
	for (i = 0; name[i]; i++) {
		char c = name[i];
		if (!(   ('0' <= c && c <= '9')
		      || ('A' <= c && c <= 'Z')
		      || ('a' <= c && c <= 'z')))
			  name[i] = '_';
	}
	sprintf(name+strlen(name), "-%d", sig);
}


static HANDLE create_event(int sig, BOOL initial, BOOL errmsg, BOOL * exists)
{
	char name[EVT_NAME_LEN];
	HANDLE h;
	make_name(name, sig);
	if (exists)
		*exists = FALSE;
	if (!(h = CreateEventA(NULL, FALSE, initial, name))) {
		if (errmsg)
			fprintf(stderr, "CreateEvent(.,\"%s\"): Error=%ld\n", name, GetLastError());
		return 0;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		if (!exists) {
			if (errmsg)
				fprintf(stderr, "CreateEvent(.,\"%s\"): Exists\n", name);
			CloseHandle(h);
			return 0;
		}
		*exists = TRUE;
	}
	return h;
}


static HANDLE open_event(int sig)
{
	char name[EVT_NAME_LEN];
	make_name(name, sig);
	return OpenEventA(EVENT_MODIFY_STATE, FALSE, name);
}


static int event_exists(int sig)
{
	char name[EVT_NAME_LEN];
	HANDLE h;
	make_name(name, sig);
	if (!(h = OpenEventA(EVENT_MODIFY_STATE, FALSE, name)))
		return 0;
	CloseHandle(h);
	return 1;
}


static int sig_event(int sig)
{
	char name[EVT_NAME_LEN];
	HANDLE h;
	make_name(name, sig);
	if (!(h = OpenEventA(EVENT_MODIFY_STATE, FALSE, name))) {
		make_name(name, EVT_RUNNING);
		if (!(h = OpenEvent(EVENT_MODIFY_STATE, FALSE, name)))
			return -1;
		CloseHandle(h);
		return 0;
	}
	SetEvent(h);
	CloseHandle(h);
	return 1;
}


static void daemon_help(FILE * f, const char * ident, const char * message)
{
	fprintf(f,
		"%s: %s.\n"
		"Use \"%s status|stop|reload|restart|sigusr1|sigusr2\" to control daemon.\n",
		ident, message, ident);
	fflush(f);
}


/////////////////////////////////////////////////////////////////////////////
// Parent Process


static BOOL WINAPI parent_console_handler(DWORD event)
{
	switch (event) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			return TRUE; // Ignore
	}
	return FALSE; // continue with next handler ...
}


static int parent_main(HANDLE rev)
{
	HANDLE dev;
	HANDLE ht[2];
	char * cmdline;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD rc, exitcode;

	// Ignore ^C, ^BREAK in parent
	SetConsoleCtrlHandler(parent_console_handler, TRUE/*add*/);

	// Create event used by child to signal daemon_detach()
	if (!(dev = create_event(EVT_DETACHED, FALSE/*not signaled*/, TRUE, NULL/*must not exist*/))) {
		CloseHandle(rev);
		return 101;
	}

	// Restart process with same args
	cmdline = GetCommandLineA();
	memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
	
	if (!CreateProcessA(
		NULL, cmdline,
		NULL, NULL, TRUE/*inherit*/,
		0, NULL, NULL, &si, &pi)) {
		fprintf(stderr, "CreateProcess(.,\"%s\",.) failed, Error=%ld\n", cmdline, GetLastError());
		CloseHandle(rev); CloseHandle(dev);
		return 101;
	}
	CloseHandle(pi.hThread);

	// Wait for daemon_detach() or exit()
	ht[0] = dev; ht[1] = pi.hProcess;
	rc = WaitForMultipleObjects(2, ht, FALSE/*or*/, INFINITE);
	if (!(/*WAIT_OBJECT_0(0) <= rc && */ rc < WAIT_OBJECT_0+2)) {
		fprintf(stderr, "WaitForMultipleObjects returns %lX\n", rc);
		TerminateProcess(pi.hProcess, 200);
	}
	CloseHandle(rev); CloseHandle(dev);

	// Get exit code
	if (!GetExitCodeProcess(pi.hProcess, &exitcode))
		exitcode = 201;
	else if (exitcode == STILL_ACTIVE) // detach()ed, assume OK
		exitcode = 0;

	CloseHandle(pi.hProcess);
	return exitcode;
}


/////////////////////////////////////////////////////////////////////////////
// Child Process


// Tables of signal handler and corresponding events
typedef void (*sigfunc_t)(int);

#define MAX_SIG_HANDLERS 8

static int num_sig_handlers = 0;
static sigfunc_t sig_handlers[MAX_SIG_HANDLERS];
static int sig_numbers[MAX_SIG_HANDLERS];
static HANDLE sig_events[MAX_SIG_HANDLERS];

static HANDLE sigint_handle, sigbreak_handle, sigterm_handle;

static HANDLE running_event;

static int reopen_stdin, reopen_stdout, reopen_stderr;


// Handler for windows console events

static BOOL WINAPI child_console_handler(DWORD event)
{
	// Caution: runs in a new thread
	// TODO: Guard with a mutex
	HANDLE h = 0;
	switch (event) {
		case CTRL_C_EVENT: // <CONTROL-C> (SIGINT)
			h = sigint_handle; break;
		case CTRL_BREAK_EVENT: // <CONTROL-Break> (SIGBREAK/SIGQUIT)
		case CTRL_CLOSE_EVENT: // User closed console or abort via task manager
			h = sigbreak_handle; break;
		case CTRL_LOGOFF_EVENT: // Logout/Shutdown (SIGTERM)
		case CTRL_SHUTDOWN_EVENT:
			h = sigterm_handle; break;
	}
	if (!h)
		return FALSE; // continue with next handler
	// Signal event
	if (!SetEvent(h))
		return FALSE;
	return TRUE;
}


static void child_exit(void)
{
	int i;
	char * cmdline;
	HANDLE rst;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	for (i = 0; i < num_sig_handlers; i++)
		CloseHandle(sig_events[i]);
	num_sig_handlers = 0;
	CloseHandle(running_event); running_event = 0;

	// Restart?
	if (!(rst = open_event(EVT_RESTART)))
		return; // No => normal exit

	// Yes => Signal exit and restart process
	Sleep(500);
	SetEvent(rst);
	CloseHandle(rst);
	Sleep(500);

	cmdline = GetCommandLineA();
	memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;

	if (!CreateProcessA(
		NULL, cmdline,
		NULL, NULL, TRUE/*inherit*/,
		0, NULL, NULL, &si, &pi)) {
		fprintf(stderr, "CreateProcess(.,\"%s\",.) failed, Error=%ld\n", cmdline, GetLastError());
	}
	CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}

static int child_main(HANDLE hev)
{
	// Keep EVT_RUNNING open until exit
	running_event = hev;

	// Install console handler
	SetConsoleCtrlHandler(child_console_handler, TRUE/*add*/);

	// Install restart handler
	atexit(child_exit);

	// Continue in main() to do the real work
	return -1;
}


// Simulate signal()

sigfunc_t daemon_signal(int sig, sigfunc_t func)
{
	int i;
	HANDLE h;
	if (func == SIG_DFL || func == SIG_IGN)
		return func; // TODO
	for (i = 0; i < num_sig_handlers; i++) {
		if (sig_numbers[i] == sig) {
			sigfunc_t old = sig_handlers[i];
			sig_handlers[i] = func;
			return old;
		}
	}
	if (num_sig_handlers >= MAX_SIG_HANDLERS)
		return SIG_ERR;
	if (!(h = create_event(sig, FALSE, TRUE, NULL)))
		return SIG_ERR;
	sig_events[num_sig_handlers]   = h;
	sig_numbers[num_sig_handlers]  = sig;
	sig_handlers[num_sig_handlers] = func;
	switch (sig) {
		case SIGINT:   sigint_handle   = h; break;
		case SIGTERM:  sigterm_handle  = h; break;
		case SIGBREAK: sigbreak_handle = h; break;
	}
	num_sig_handlers++;
	return SIG_DFL;
}


// strsignal()

const char * daemon_strsignal(int sig)
{
	switch (sig) {
		case SIGHUP:  return "SIGHUP";
		case SIGINT:  return "SIGINT";
		case SIGTERM: return "SIGTERM";
		case SIGBREAK:return "SIGBREAK";
		case SIGUSR1: return "SIGUSR1";
		case SIGUSR2: return "SIGUSR2";
		default:      return "*UNKNOWN*";
	}
}


// Simulate sleep()

void daemon_sleep(int seconds)
{
	if (num_sig_handlers <= 0) {
		Sleep(seconds*1000L);
	}
	else {
		// Wait for any signal or timeout
		DWORD rc = WaitForMultipleObjects(num_sig_handlers, sig_events,
			FALSE/*OR*/, seconds*1000L);
		if (rc == WAIT_TIMEOUT)
			return;
		if (!(/*WAIT_OBJECT_0(0) <= rc && */ rc < WAIT_OBJECT_0+(unsigned)num_sig_handlers)) {
			fprintf(stderr,"WaitForMultipleObjects returns %lu\n", rc);
			return;
		}
		// Call Handler
		sig_handlers[rc-WAIT_OBJECT_0](sig_numbers[rc-WAIT_OBJECT_0]);
	}
}


// Disable/Enable console

void daemon_disable_console()
{
	SetConsoleCtrlHandler(child_console_handler, FALSE/*remove*/);
	reopen_stdin = reopen_stdout = reopen_stderr = 0;
	if (isatty(fileno(stdin))) {
		fclose(stdin); reopen_stdin = 1;
	}
	if (isatty(fileno(stdout))) {
		fclose(stdout); reopen_stdout = 1;
	}
	if (isatty(fileno(stderr))) {
		fclose(stderr); reopen_stderr = 1;
	}
	FreeConsole();
	SetConsoleCtrlHandler(child_console_handler, TRUE/*add*/);
}

int daemon_enable_console(const char * title)
{
	BOOL ok;
	SetConsoleCtrlHandler(child_console_handler, FALSE/*remove*/);
	ok = AllocConsole();
	SetConsoleCtrlHandler(child_console_handler, TRUE/*add*/);
	if (!ok)
		return -1;
	if (title)
		SetConsoleTitleA(title);
	if (reopen_stdin)
		freopen("conin$",  "r", stdin);
	if (reopen_stdout)
		freopen("conout$", "w", stdout);
	if (reopen_stderr)
		freopen("conout$", "w", stderr);
	reopen_stdin = reopen_stdout = reopen_stderr = 0;
	return 0;
}


// Detach daemon from console & parent

int daemon_detach(const char * ident)
{
	// Signal detach to parent
	if (sig_event(EVT_DETACHED) != 1) {
		if (!debugging())
			return -1;
	}
	if (ident) {
		// Print help
		FILE * f = ( isatty(fileno(stdout)) ? stdout
		           : isatty(fileno(stderr)) ? stderr : NULL);
		if (f)
			daemon_help(f, ident, "now detaches from console into background mode");
	}
	daemon_disable_console();
	return 0;
}


// Display a message box

int daemon_messagebox(int system, const char * title, const char * text)
{
	if (MessageBoxA(NULL, text, title,
	                MB_OK|MB_ICONWARNING|(system?MB_SYSTEMMODAL:MB_APPLMODAL)) != IDOK)
		return -1;
	return 0;
}


// Spawn a command and redirect <inpbuf >outbuf
// return command's exitcode or -1 on error

int daemon_spawn(const char * cmd,
                 const char * inpbuf, int inpsize,
                 char *       outbuf, int outsize )
{
	HANDLE pipe_inp_r, pipe_inp_w, pipe_out_r, pipe_out_w, pipe_err_w, h;
	DWORD num_io, exitcode; int i;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si; PROCESS_INFORMATION pi;
	HANDLE self = GetCurrentProcess();

	// Create stdin pipe with inheritable read side
	memset(&sa, 0, sizeof(sa)); sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	if (!CreatePipe(&pipe_inp_r, &h, &sa, inpsize*2+13))
		return -1;
	if (!DuplicateHandle(self, h, self, &pipe_inp_w,
		0, FALSE/*!inherit*/, DUPLICATE_SAME_ACCESS)) {
		CloseHandle(pipe_inp_r); CloseHandle(pipe_inp_w);
		return -1;
	}
	CloseHandle(h);

	// Create stdout pipe with inheritable write side
	memset(&sa, 0, sizeof(sa)); sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	if (!CreatePipe(&h, &pipe_out_w, &sa, outsize)) {
		CloseHandle(pipe_inp_r); CloseHandle(pipe_inp_w);
		return -1;
	}
	if (!DuplicateHandle(self, h, self, &pipe_out_r,
		0, FALSE/*!inherit*/, DUPLICATE_SAME_ACCESS)) {
		CloseHandle(h);          CloseHandle(pipe_out_w);
		CloseHandle(pipe_inp_r); CloseHandle(pipe_inp_w);
		return -1;
	}
	CloseHandle(h);

	// Create stderr handle as dup of stdout write side
	if (!DuplicateHandle(self, pipe_out_w, self, &pipe_err_w,
		0, TRUE/*inherit*/, DUPLICATE_SAME_ACCESS)) {
		CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
		CloseHandle(pipe_inp_r); CloseHandle(pipe_inp_w);
		return -1;
	}

	// Create process with pipes as stdio
	memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
	si.hStdInput  = pipe_inp_r;
	si.hStdOutput = pipe_out_w;
	si.hStdError  = pipe_err_w;
	si.dwFlags = STARTF_USESTDHANDLES;
	if (!CreateProcessA(
		NULL, (char*)cmd,
		NULL, NULL, TRUE/*inherit*/,
		DETACHED_PROCESS/*no new console*/,
		NULL, NULL, &si, &pi)) {
		CloseHandle(pipe_err_w);
		CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
		CloseHandle(pipe_inp_r); CloseHandle(pipe_inp_w);
		return -1;
	}
	CloseHandle(pi.hThread);
	// Close inherited handles
	CloseHandle(pipe_inp_r);
	CloseHandle(pipe_out_w);
	CloseHandle(pipe_err_w);

	// Copy inpbuf to stdin
	// convert \n => \r\n 
	for (i = 0; i < inpsize; ) {
		int len = 0;
		while (i+len < inpsize && inpbuf[i+len] != '\n')
			len++;
		if (len > 0)
			WriteFile(pipe_inp_w, inpbuf+i, len, &num_io, NULL);
		i += len;
		if (i < inpsize) {
			WriteFile(pipe_inp_w, "\r\n", 2, &num_io, NULL);
			i++;
		}
	}
	CloseHandle(pipe_inp_w);

	// Copy stdout to output buffer until full, rest to /dev/null
	// convert \r\n => \n
	for (i = 0; ; ) {
		char buf[256];
		int j;
		if (!ReadFile(pipe_out_r, buf, sizeof(buf), &num_io, NULL) || num_io == 0)
			break;
		for (j = 0; i < outsize-1 && j < (int)num_io; j++) {
			if (buf[j] != '\r')
				outbuf[i++] = buf[j];
		}	
	}
	outbuf[i] = 0;
	CloseHandle(pipe_out_r);

	// Wait for process exitcode
	WaitForSingleObject(pi.hProcess, INFINITE);
	exitcode = 42;
	GetExitCodeProcess(pi.hProcess, &exitcode);
	CloseHandle(pi.hProcess);
	return exitcode;
}


/////////////////////////////////////////////////////////////////////////////
// Initd Functions

static int wait_signaled(HANDLE h, int seconds)
{
	int i;
	for (i = 0; ; ) {
		if (WaitForSingleObject(h, 1000L) == WAIT_OBJECT_0)
			return 0;
		if (++i >= seconds)
			return -1;
		fputchar('.'); fflush(stdout);
	}
}


static int wait_evt_running(int seconds, int exists)
{
	int i;
	if (event_exists(EVT_RUNNING) == exists)
		return 0;
	for (i = 0; ; ) {
		Sleep(1000);
		if (event_exists(EVT_RUNNING) == exists)
			return 0;
		if (++i >= seconds)
			return -1;
		fputchar('.'); fflush(stdout);
	}
}


static int is_initd_command(char * s)
{
	if (!strcmp(s, "status"))
		return EVT_RUNNING;
	if (!strcmp(s, "stop"))
		return SIGTERM;
	if (!strcmp(s, "reload"))
		return SIGHUP;
	if (!strcmp(s, "sigusr1"))
		return SIGUSR1;
	if (!strcmp(s, "sigusr2"))
		return SIGUSR2;
	if (!strcmp(s, "restart"))
		return EVT_RESTART;
	return -1;
}


static int initd_main(const char * ident, int argc, char **argv)
{
	int rc;
	if (argc < 2)
		return -1;
	if ((rc = is_initd_command(argv[1])) < 0)
		return -1;
	if (argc != 2) {
		printf("%s: no arguments allowed for command %s\n", ident, argv[1]);
		return 1;
	}

	switch (rc) {
		default:
		case EVT_RUNNING:
			printf("Checking for %s:", ident); fflush(stdout);
			rc = event_exists(EVT_RUNNING);
			puts(rc ? " running" : " not running");
			return (rc ? 0 : 1);

		case SIGTERM:
			printf("Stopping %s:", ident); fflush(stdout);
			rc = sig_event(SIGTERM);
			if (rc <= 0) {
				puts(rc < 0 ? " not running" : " error");
				return (rc < 0 ? 0 : 1);
			}
			rc = wait_evt_running(10, 0);
			puts(!rc ? " done" : " timeout");
			return (!rc ? 0 : 1);

		case SIGHUP:
			printf("Reloading %s:", ident); fflush(stdout);
			rc = sig_event(SIGHUP);
			puts(rc > 0 ? " done" : rc == 0 ? " error" : " not running");
			return (rc > 0 ? 0 : 1);

		case SIGUSR1:
		case SIGUSR2:
			printf("Sending SIGUSR%d to %s:", (rc-SIGUSR1+1), ident); fflush(stdout);
			rc = sig_event(rc);
			puts(rc > 0 ? " done" : rc == 0 ? " error" : " not running");
			return (rc > 0 ? 0 : 1);

		case EVT_RESTART:
			{
				HANDLE rst;
				printf("Stopping %s:", ident); fflush(stdout);
				if (event_exists(EVT_DETACHED)) {
					puts(" not detached, cannot restart");
					return 1;
				}
				if (!(rst = create_event(EVT_RESTART, FALSE, FALSE, NULL))) {
					puts(" error");
					return 1;
				}
				rc = sig_event(SIGTERM);
				if (rc <= 0) {
					puts(rc < 0 ? " not running" : " error");
					CloseHandle(rst);
					return 1;
				}
				rc = wait_signaled(rst, 10);
				CloseHandle(rst);
				if (rc) {
					puts(" timeout");
					return 1;
				}
				puts(" done");
				Sleep(100);

				printf("Starting %s:", ident); fflush(stdout);
				rc = wait_evt_running(10, 1);
				puts(!rc ? " done" : " error");
				return (!rc ? 0 : 1);
			}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Main Function

// This function must be called from main()

int daemon_main(const char * ident, int argc, char **argv)
{
	int rc;
	HANDLE rev;
	BOOL exists;

#ifdef _DEBUG
	// Enable Debug heap checks
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)
		|_CRTDBG_ALLOC_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_LEAK_CHECK_DF);
#endif

	// Check for [status|stop|reload|restart|sigusr1|sigusr2] parameters
	if ((rc = initd_main(ident, argc, argv)) >= 0)
		return rc;

	// Create main event to detect process type:
	// 1. new: parent process => start child and wait for detach() or exit() of child.
	// 2. exists && signaled: child process => do the real work, signal detach() to parent
	// 3. exists && !signaled: already running => exit()
	if (!(rev = create_event(EVT_RUNNING, TRUE/*signaled*/, TRUE, &exists)))
		return 100;

	if (!exists && !debugging()) {
		// Event new => parent process
		return parent_main(rev);
	}

	if (WaitForSingleObject(rev, 0) == WAIT_OBJECT_0) {
		// Event was signaled => In child process
		return child_main(rev);
	}

	// Event no longer signaled => Already running!
	daemon_help(stdout, ident, "already running");
	CloseHandle(rev);
	return 1;
}


/////////////////////////////////////////////////////////////////////////////
// Test Program

#ifdef TEST

static volatile sig_atomic_t caughtsig = 0;

static void sig_handler(int sig)
{
	caughtsig = sig;
}

static void main_exit(void)
{
	printf("Main exit\n");
}

int main(int argc, char **argv)
{
	int i;
	int debug = 0;
	if ((i = daemon_main("testd", argc, argv)) >= 0)
		return i;

	printf("PID=%ld\n", GetCurrentProcessId());
	for (i = 0; i < argc; i++) {
		printf("%d: \"%s\"\n", i, argv[i]);
		if (!strcmp(argv[i],"-d"))
			debug = 1;
	}

	daemon_signal(SIGINT, sig_handler);
	daemon_signal(SIGBREAK, sig_handler);
	daemon_signal(SIGTERM, sig_handler);
	daemon_signal(SIGHUP, sig_handler);
	daemon_signal(SIGUSR2, sig_handler);

	atexit(main_exit);

	if (!debug) {
		printf("Preparing to detach...\n");
		Sleep(2000);
		daemon_detach();
		printf("Detached!\n");
	}

	for (;;) {
		daemon_sleep(1);
		printf("."); fflush(stdout);
		if (caughtsig) {
			if (caughtsig == SIGUSR2) {
				debug ^= 1;
				daemon_console(debug, "Daemon[Debug]");
			}
			printf("[PID=%ld: Signal=%d]", GetCurrentProcessId(), caughtsig); fflush(stdout);
			if (caughtsig == SIGTERM || caughtsig == SIGBREAK)
				break;
			caughtsig = 0;
		}
	}
	printf("\nExiting on signal %d\n", caughtsig);
	return 0;
}

#endif
