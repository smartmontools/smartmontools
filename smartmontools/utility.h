/*
 * utility.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2008-21 Christian Franke
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#define UTILITY_H_CVSID "$Id$"

#include <float.h> // *DBL_MANT_DIG
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <sys/types.h> // for regex.h (according to POSIX)
#ifdef WITH_CXX11_REGEX
#include <regex>
#else
#include <regex.h>
#endif

#ifndef __GNUC__
#define __attribute_format_printf(x, y)  /**/
#elif defined(__MINGW32__) && __USE_MINGW_ANSI_STDIO
// Check format of __mingw_*printf() instead of MSVCRT.DLL:*printf()
#define __attribute_format_printf(x, y)  __attribute__((format (gnu_printf, x, y)))
#else
#define __attribute_format_printf(x, y)  __attribute__((format (printf, x, y)))
#endif

// Make version information string
std::string format_version_info(const char * prog_name, bool full = false);

// return (v)sprintf() formatted std::string
std::string strprintf(const char * fmt, ...)
    __attribute_format_printf(1, 2);
std::string vstrprintf(const char * fmt, va_list ap);

// Return true if STR starts with PREFIX
inline bool str_starts_with(const char * str, const char * prefix)
  { return !strncmp(str, prefix, strlen(prefix)); }

inline bool str_starts_with(const std::string & str, const char * prefix)
  { return !strncmp(str.c_str(), prefix, strlen(prefix)); }

/* Replace space and non-alphanumerics with '_', upper to lower case */
std::string jsonify_name_s(const std::string & sin);
std::string jsonify_name(const char * in_a);

// Convert time to broken-down local time, throw on error.
struct tm * time_to_tm_local(struct tm * tp, time_t t);

// Utility function prints date and time and timezone into a character
// buffer of length 64.  All the fuss is needed to get the
// right timezone info (sigh).
#define DATEANDEPOCHLEN 64
void dateandtimezoneepoch(char (& buffer)[DATEANDEPOCHLEN], time_t tval);

// like printf() except that we can control it better. Note --
// although the prototype is given here in utility.h, the function
// itself is defined differently in smartctl and smartd.  So the
// function definition(s) are in smartd.c and in smartctl.c.
void pout(const char *fmt, ...)  
    __attribute_format_printf(1, 2);

// replacement for perror() with redirected output.
void syserror(const char *message);

// Function for processing -t selective... option in smartctl
int split_selective_arg(char *s, uint64_t *start, uint64_t *stop, int *mode);

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

void swap2(char *location);
void swap4(char *location);
void swap8(char *location);
// Typesafe variants using overloading
inline void swapx(unsigned short * p)
  { swap2((char*)p); }
inline void swapx(unsigned int * p)
  { swap4((char*)p); }
inline void swapx(uint64_t * p)
  { swap8((char*)p); }

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

// Copy not null terminated char array to null terminated string.
// Replace non-ascii characters.  Remove leading and trailing blanks.
const char * format_char_array(char * str, int strsize, const char * chr, int chrsize);

// Version for fixed size buffers.
template<size_t STRSIZE, size_t CHRSIZE>
inline const char * format_char_array(char (& str)[STRSIZE], const char (& chr)[CHRSIZE])
  { return format_char_array(str, (int)STRSIZE, chr, (int)CHRSIZE); }

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
      if (m_file && m_owner)
        fclose(m_file);
      m_file = fopen(name, mode);
      m_owner = true;
      return !!m_file;
    }

  void open(FILE * f, bool owner = false)
    {
      if (m_file && m_owner)
        fclose(m_file);
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

/// Wrapper class for POSIX regex(3) or std::regex
/// Supports copy & assignment and is compatible with STL containers.
class regular_expression
{
public:
  // Construction & assignment
#ifdef WITH_CXX11_REGEX
  regular_expression() = default;

#else
  regular_expression();

  ~regular_expression();

  regular_expression(const regular_expression & x);

  regular_expression & operator=(const regular_expression & x);
#endif

  /// Construct with pattern, throw on error.
  explicit regular_expression(const char * pattern);

  /// Set and compile new pattern, return false on error.
  bool compile(const char * pattern);

  // Get pattern from last compile().
  const char * get_pattern() const
    { return m_pattern.c_str(); }

  /// Get error message from last compile().
  const char * get_errmsg() const
    { return m_errmsg.c_str(); }

  // Return true if pattern is not set or bad.
  bool empty() const
    { return (m_pattern.empty() || !m_errmsg.empty()); }

  /// Return true if full string matches pattern
  bool full_match(const char * str) const;

#ifdef WITH_CXX11_REGEX
  struct match_range { int rm_so, rm_eo; };
#else
  typedef regmatch_t match_range;
#endif

  /// Return true if substring matches pattern, fill match_range array.
  bool execute(const char * str, unsigned nmatch, match_range * pmatch) const;

private:
  std::string m_pattern;
  std::string m_errmsg;

#ifdef WITH_CXX11_REGEX
  std::regex m_regex;
#else
  regex_t m_regex_buf;
  void free_buf();
  void copy_buf(const regular_expression & x);
#endif

  bool compile();
};

// 128-bit unsigned integer to string conversion.
// Provides full integer precision if compiler supports '__int128'.
// Otherwise precision depends on supported floating point data types.

#if defined(HAVE_LONG_DOUBLE_WIDER) && \
    (!defined(__MINGW32__) || defined(__USE_MINGW_ANSI_STDIO))
    // MinGW 'long double' type does not work with MSVCRT *printf()
#define HAVE_LONG_DOUBLE_WIDER_PRINTF 1
#else
#undef HAVE_LONG_DOUBLE_WIDER_PRINTF
#endif

// Return #bits precision provided by uint128_hilo_to_str().
inline int uint128_to_str_precision_bits()
{
#if defined(HAVE___INT128)
  return 128;
#elif defined(HAVE_LONG_DOUBLE_WIDER_PRINTF)
  return LDBL_MANT_DIG;
#else
  return DBL_MANT_DIG;
#endif
}

// Convert 128-bit unsigned integer provided as two 64-bit halves to a string.
const char * uint128_hilo_to_str(char * str, int strsize, uint64_t value_hi, uint64_t value_lo);

// Version for fixed size buffers.
template <size_t SIZE>
inline const char * uint128_hilo_to_str(char (& str)[SIZE], uint64_t value_hi, uint64_t value_lo)
  { return uint128_hilo_to_str(str, (int)SIZE, value_hi, value_lo); }

/// Get microseconds since some unspecified starting point.
/// Used only for command duration measurements in debug outputs.
/// Returns -1 if unsupported.
long long get_timer_usec();

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
