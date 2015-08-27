/*
 * utility.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008-14 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#define UTILITY_H_CVSID "$Id$"

#include <time.h>
#include <sys/types.h> // for regex.h (according to POSIX)
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

#ifndef __GNUC__
#define __attribute_format_printf(x, y)  /**/
#elif defined(__MINGW32__) && __USE_MINGW_ANSI_STDIO
// Check format of __mingw_*printf() instead of MSVCRT.DLL:*printf()
#define __attribute_format_printf(x, y)  __attribute__((format (gnu_printf, x, y)))
#define HAVE_WORKING_SNPRINTF 1
#else
#define __attribute_format_printf(x, y)  __attribute__((format (printf, x, y)))
#endif

// Make version information string
std::string format_version_info(const char * prog_name, bool full = false);

// return (v)sprintf() formated std::string
std::string strprintf(const char * fmt, ...)
    __attribute_format_printf(1, 2);
std::string vstrprintf(const char * fmt, va_list ap);

// Return true if STR starts with PREFIX
inline bool str_starts_with(const char * str, const char * prefix)
  { return !strncmp(str, prefix, strlen(prefix)); }

inline bool str_starts_with(const std::string & str, const char * prefix)
  { return !strncmp(str.c_str(), prefix, strlen(prefix)); }

#ifndef HAVE_WORKING_SNPRINTF
// Substitute by safe replacement functions
int safe_snprintf(char *buf, int size, const char *fmt, ...)
    __attribute_format_printf(3, 4);
int safe_vsnprintf(char *buf, int size, const char *fmt, va_list ap);
#define snprintf  safe_snprintf
#define vsnprintf safe_vsnprintf
#endif

#ifndef HAVE_STRTOULL
// Replacement for missing strtoull() (Linux with libc < 6, MSVC)
uint64_t strtoull(const char * p, char * * endp, int base);
#endif

// Utility function prints current date and time and timezone into a
// character buffer of length>=64.  All the fuss is needed to get the
// right timezone info (sigh).
#define DATEANDEPOCHLEN 64
void dateandtimezone(char *buffer);
// Same, but for time defined by epoch tval
void dateandtimezoneepoch(char *buffer, time_t tval);

// like printf() except that we can control it better. Note --
// although the prototype is given here in utility.h, the function
// itself is defined differently in smartctl and smartd.  So the
// function definition(s) are in smartd.c and in smartctl.c.
void pout(const char *fmt, ...)  
    __attribute_format_printf(1, 2);

// replacement for perror() with redirected output.
void syserror(const char *message);

// Function for processing -r option in smartctl and smartd
int split_report_arg(char *s, int *i);

// Function for processing -t selective... option in smartctl
int split_selective_arg(char *s, uint64_t *start, uint64_t *stop, int *mode);

// Replacement for exit(status)
// (exit is not compatible with C++ destructors)
#define EXIT(status) { throw (int)(status); }


#ifdef OLD_INTERFACE

// Utility function to free memory
void *FreeNonZero(void* address, int size, int whatline, const char* file);

// A custom version of strdup() that keeps track of how much memory is
// being allocated. If mustexist is set, it also throws an error if we
// try to duplicate a NULL string.
char *CustomStrDup(const char *ptr, int mustexist, int whatline, const char* file);

#endif // OLD_INTERFACE

// Compile time check of byte ordering
// (inline const function allows compiler to remove dead code)
inline bool isbigendian()
{
#ifdef WORDS_BIGENDIAN
  return true;
#else
  return false;
#endif
}

// Runtime check of ./configure result, throws on error.
void check_config();

// This value follows the peripheral device type value as defined in
// SCSI Primary Commands, ANSI INCITS 301:1997.  It is also used in
// the ATA standard for packet devices to define the device type.
const char *packetdevicetype(int type);

// returns true if any of the n bytes are nonzero, else zero.
bool nonempty(const void * data, int size);

// needed to fix glibc bug
void FixGlibcTimeZoneBug();

// Format integer with thousands separator
const char * format_with_thousands_sep(char * str, int strsize, uint64_t val,
                                       const char * thousands_sep = 0);

// Format capacity with SI prefixes
const char * format_capacity(char * str, int strsize, uint64_t val,
                             const char * decimal_point = 0);

// Wrapper class for a raw data buffer
class raw_buffer
{
public:
  explicit raw_buffer(unsigned sz, unsigned char val = 0)
    : m_data(new unsigned char[sz]),
      m_size(sz)
    { memset(m_data, val, m_size); }

  ~raw_buffer()
    { delete [] m_data; }

  unsigned size() const
    { return m_size; }

  unsigned char * data()
    { return m_data; }
  const unsigned char * data() const
    { return m_data; }

private:
  unsigned char * m_data;
  unsigned m_size;

  raw_buffer(const raw_buffer &);
  void operator=(const raw_buffer &);
};

/// Wrapper class for FILE *.
class stdio_file
{
public:
  explicit stdio_file(FILE * f = 0, bool owner = false)
    : m_file(f), m_owner(owner) { }

  stdio_file(const char * name, const char * mode)
    : m_file(fopen(name, mode)), m_owner(true) { }

  ~stdio_file()
    {
      if (m_file && m_owner)
        fclose(m_file);
    }

  bool open(const char * name, const char * mode)
    {
      m_file = fopen(name, mode);
      m_owner = true;
      return !!m_file;
    }

  void open(FILE * f, bool owner = false)
    {
      m_file = f;
      m_owner = owner;
    }

  bool close()
    {
      if (!m_file)
        return true;
      bool ok = !ferror(m_file);
      if (fclose(m_file))
        ok = false;
      m_file = 0;
      return ok;
    }

  operator FILE * ()
    { return m_file; }

  bool operator!() const
    { return !m_file; }

private:
  FILE * m_file;
  bool m_owner;

  stdio_file(const stdio_file &);
  void operator=(const stdio_file &);
};

/// Wrapper class for regex(3).
/// Supports copy & assignment and is compatible with STL containers.
class regular_expression
{
public:
  // Construction & assignment
  regular_expression();

  regular_expression(const char * pattern, int flags,
                     bool throw_on_error = true);

  ~regular_expression();

  regular_expression(const regular_expression & x);

  regular_expression & operator=(const regular_expression & x);

  /// Set and compile new pattern, return false on error.
  bool compile(const char * pattern, int flags);

  // Get pattern from last compile().
  const char * get_pattern() const
    { return m_pattern.c_str(); }

  /// Get error message from last compile().
  const char * get_errmsg() const
    { return m_errmsg.c_str(); }

  // Return true if pattern is not set or bad.
  bool empty() const
    { return (m_pattern.empty() || !m_errmsg.empty()); }

  /// Return true if substring matches pattern
  bool match(const char * str, int flags = 0) const
    { return !regexec(&m_regex_buf, str, 0, (regmatch_t*)0, flags); }

  /// Return true if full string matches pattern
  bool full_match(const char * str, int flags = 0) const
    {
      regmatch_t range;
      return (   !regexec(&m_regex_buf, str, 1, &range, flags)
              && range.rm_so == 0 && range.rm_eo == (int)strlen(str));
    }

  /// Return true if substring matches pattern, fill regmatch_t array.
  bool execute(const char * str, unsigned nmatch, regmatch_t * pmatch, int flags = 0) const
    { return !regexec(&m_regex_buf, str, nmatch, pmatch, flags); }

private:
  std::string m_pattern;
  int m_flags;
  regex_t m_regex_buf;
  std::string m_errmsg;

  void free_buf();
  void copy(const regular_expression & x);
  bool compile();
};

#ifdef _WIN32
// Get exe directory
//(implemented in os_win32.cpp)
std::string get_exe_dir();
#endif


#ifdef OLD_INTERFACE
// remaining controller types in old interface modules
#define CONTROLLER_UNKNOWN              0x00
#define CONTROLLER_ATA                  0x01
#define CONTROLLER_SCSI                 0x02
#endif

#endif

