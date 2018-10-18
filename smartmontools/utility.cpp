/*
 * utility.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-12 Bruce Allen
 * Copyright (C) 2008-18 Christian Franke
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// THIS FILE IS INTENDED FOR UTILITY ROUTINES THAT ARE APPLICABLE TO
// BOTH SCSI AND ATA DEVICES, AND THAT MAY BE USED IN SMARTD,
// SMARTCTL, OR BOTH.

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef _WIN32
#include <mbstring.h> // _mbsinc()
#endif

#include <stdexcept>

#include "svnversion.h"
#include "utility.h"

#include "atacmds.h"
#include "dev_interface.h"
#include "sg_unaligned.h"

const char * utility_cpp_cvsid = "$Id$"
  UTILITY_H_CVSID;

const char * packet_types[] = {
        "Direct-access (disk)",
        "Sequential-access (tape)",
        "Printer",
        "Processor",
        "Write-once (optical disk)",
        "CD/DVD",
        "Scanner",
        "Optical memory (optical disk)",
        "Medium changer",
        "Communications",
        "Graphic arts pre-press (10)",
        "Graphic arts pre-press (11)",
        "Array controller",
        "Enclosure services",
        "Reduced block command (simplified disk)",
        "Optical card reader/writer"
};

// BUILD_INFO can be provided by package maintainers
#ifndef BUILD_INFO
#define BUILD_INFO "(local build)"
#endif

// Make version information string
std::string format_version_info(const char * prog_name, bool full /*= false*/)
{
  std::string info = strprintf(
    "%s " PACKAGE_VERSION " "
#ifdef SMARTMONTOOLS_SVN_REV
      SMARTMONTOOLS_SVN_DATE " r" SMARTMONTOOLS_SVN_REV
#else
      "(build date " __DATE__ ")" // checkout without expansion of Id keywords
#endif
      " [%s] " BUILD_INFO "\n"
    "Copyright (C) 2002-18, Bruce Allen, Christian Franke, www.smartmontools.org\n",
    prog_name, smi()->get_os_version_str().c_str()
  );
  if (!full)
    return info;

  info += "\n";
  info += prog_name;
  info += " comes with ABSOLUTELY NO WARRANTY. This is free\n"
    "software, and you are welcome to redistribute it under\n"
    "the terms of the GNU General Public License; either\n"
    "version 2, or (at your option) any later version.\n"
    "See http://www.gnu.org for further details.\n"
    "\n"
    "smartmontools release " PACKAGE_VERSION
      " dated " SMARTMONTOOLS_RELEASE_DATE " at " SMARTMONTOOLS_RELEASE_TIME "\n"
#ifdef SMARTMONTOOLS_SVN_REV
    "smartmontools SVN rev " SMARTMONTOOLS_SVN_REV
      " dated " SMARTMONTOOLS_SVN_DATE " at " SMARTMONTOOLS_SVN_TIME "\n"
#else
    "smartmontools SVN rev is unknown\n"
#endif
    "smartmontools build host: " SMARTMONTOOLS_BUILD_HOST "\n"
    "smartmontools build with: "

#define N2S_(s) #s
#define N2S(s) "(" N2S_(s) ")"
#if   __cplusplus >  201703
                               "C++2x" N2S(__cplusplus)
#elif __cplusplus == 201703
                               "C++17"
#elif __cplusplus >  201402
                               "C++14" N2S(__cplusplus)
#elif __cplusplus == 201402
                               "C++14"
#elif __cplusplus >  201103
                               "C++11" N2S(__cplusplus)
#elif __cplusplus == 201103
                               "C++11"
#elif __cplusplus >  199711
                               "C++98" N2S(__cplusplus)
#elif __cplusplus == 199711
                               "C++98"
#else
                               "C++"   N2S(__cplusplus)
#endif
#undef N2S
#undef N2S_

#if defined(__GNUC__) && defined(__VERSION__) // works also with CLang
                                     ", GCC " __VERSION__
#endif
                                                          "\n"
    "smartmontools configure arguments:"
  ;
  info += (sizeof(SMARTMONTOOLS_CONFIGURE_ARGS) > 1 ?
           SMARTMONTOOLS_CONFIGURE_ARGS : " [no arguments given]");
  info += '\n';

  return info;
}

// Solaris only: Get site-default timezone. This is called from
// UpdateTimezone() when TZ environment variable is unset at startup.
#if defined (__SVR4) && defined (__sun)
static const char *TIMEZONE_FILE = "/etc/TIMEZONE";

static char *ReadSiteDefaultTimezone(){
  FILE *fp;
  char buf[512], *tz;
  int n;

  tz = NULL;
  fp = fopen(TIMEZONE_FILE, "r");
  if(fp == NULL) return NULL;
  while(fgets(buf, sizeof(buf), fp)) {
    if (strncmp(buf, "TZ=", 3))    // searches last "TZ=" line
      continue;
    n = strlen(buf) - 1;
    if (buf[n] == '\n') buf[n] = 0;
    if (tz) free(tz);
    tz = strdup(buf);
  }
  fclose(fp);
  return tz;
}
#endif

// Make sure that this executable is aware if the user has changed the
// time-zone since the last time we polled devices. The cannonical
// example is a user who starts smartd on a laptop, then flies across
// time-zones with a laptop, and then changes the timezone, WITHOUT
// restarting smartd. This is a work-around for a bug in
// GLIBC. Yuk. See bug number 48184 at http://bugs.debian.org and
// thanks to Ian Redfern for posting a workaround.

// Please refer to the smartd manual page, in the section labeled LOG
// TIMESTAMP TIMEZONE.
void FixGlibcTimeZoneBug(){
#if __GLIBC__  
  if (!getenv("TZ")) {
    putenv((char *)"TZ=GMT"); // POSIX prototype is 'int putenv(char *)'
    tzset();
    putenv((char *)"TZ");
    tzset();
  }
#elif _WIN32
  if (!getenv("TZ")) {
    putenv("TZ=GMT");
    tzset();
    putenv("TZ=");  // empty value removes TZ, putenv("TZ") does nothing
    tzset();
  }
#elif defined (__SVR4) && defined (__sun)
  // In Solaris, putenv("TZ=") sets null string and invalid timezone.
  // putenv("TZ") does nothing.  With invalid TZ, tzset() do as if
  // TZ=GMT.  With TZ unset, /etc/TIMEZONE will be read only _once_ at
  // first tzset() call.  Conclusion: Unlike glibc, dynamic
  // configuration of timezone can be done only by changing actual
  // value of TZ environment value.
  enum tzstate { NOT_CALLED_YET, USER_TIMEZONE, TRACK_TIMEZONE };
  static enum tzstate state = NOT_CALLED_YET;

  static struct stat prev_stat;
  static char *prev_tz;
  struct stat curr_stat;
  char *curr_tz;

  if(state == NOT_CALLED_YET) {
    if(getenv("TZ")) {
      state = USER_TIMEZONE; // use supplied timezone
    } else {
      state = TRACK_TIMEZONE;
      if(stat(TIMEZONE_FILE, &prev_stat)) {
	state = USER_TIMEZONE;	// no TZ, no timezone file; use GMT forever
      } else {
	prev_tz = ReadSiteDefaultTimezone(); // track timezone file change
	if(prev_tz) putenv(prev_tz);
      }
    }
    tzset();
  } else if(state == TRACK_TIMEZONE) {
    if(stat(TIMEZONE_FILE, &curr_stat) == 0
       && (curr_stat.st_ctime != prev_stat.st_ctime
	    || curr_stat.st_mtime != prev_stat.st_mtime)) {
      // timezone file changed
      curr_tz = ReadSiteDefaultTimezone();
      if(curr_tz) {
	putenv(curr_tz);
	if(prev_tz) free(prev_tz);
	prev_tz = curr_tz; prev_stat = curr_stat; 
      }
    }
    tzset();
  }
#endif
  // OTHER OS/LIBRARY FIXES SHOULD GO HERE, IF DESIRED.  PLEASE TRY TO
  // KEEP THEM INDEPENDENT.
  return;
}

#ifdef _WIN32
// Fix strings in tzname[] to avoid long names with non-ascii characters.
// If TZ is not set, tzset() in the MSVC runtime sets tzname[] to the
// national language timezone names returned by GetTimezoneInformation().
static char * fixtzname(char * dest, int destsize, const char * src)
{
  int i = 0, j = 0;
  while (src[i] && j < destsize-1) {
    int i2 = (const char *)_mbsinc((const unsigned char *)src+i) - src;
    if (i2 > i+1)
      i = i2; // Ignore multibyte chars
    else {
      if ('A' <= src[i] && src[i] <= 'Z')
        dest[j++] = src[i]; // "Pacific Standard Time" => "PST"
      i++;
    }
  }
  if (j < 2)
    j = 0;
  dest[j] = 0;
  return dest;
}
#endif // _WIN32

// This value follows the peripheral device type value as defined in
// SCSI Primary Commands, ANSI INCITS 301:1997.  It is also used in
// the ATA standard for packet devices to define the device type.
const char *packetdevicetype(int type){
  if (type<0x10)
    return packet_types[type];
  
  if (type<0x20)
    return "Reserved";
  
  return "Unknown";
}

// Utility function prints date and time and timezone into a character
// buffer of length 64.  All the fuss is needed to get the right
// timezone info (sigh).
void dateandtimezoneepoch(char (& buffer)[DATEANDEPOCHLEN], time_t tval)
{
  struct tm *tmval;
  const char *timezonename;
  char datebuffer[DATEANDEPOCHLEN];
  int lenm1;

  FixGlibcTimeZoneBug();
  
  // Get the time structure.  We need this to determine if we are in
  // daylight savings time or not.
  tmval=localtime(&tval);
  
  // Convert to an ASCII string, put in datebuffer
  // same as: asctime_r(tmval, datebuffer);
  strncpy(datebuffer, asctime(tmval), DATEANDEPOCHLEN);
  datebuffer[DATEANDEPOCHLEN-1]='\0';
  
  // Remove newline
  lenm1=strlen(datebuffer)-1;
  datebuffer[lenm1>=0?lenm1:0]='\0';

#if defined(_WIN32) && defined(_MSC_VER)
  // tzname is missing in MSVC14
  #define tzname _tzname
#endif

  // correct timezone name
  if (tmval->tm_isdst==0)
    // standard time zone
    timezonename=tzname[0];
  else if (tmval->tm_isdst>0)
    // daylight savings in effect
    timezonename=tzname[1];
  else
    // unable to determine if daylight savings in effect
    timezonename="";

#ifdef _WIN32
  // Fix long non-ascii timezone names
    // cppcheck-suppress variableScope
  char tzfixbuf[6+1] = "";
  if (!getenv("TZ"))
    timezonename=fixtzname(tzfixbuf, sizeof(tzfixbuf), timezonename);
#endif
  
  // Finally put the information into the buffer as needed.
  snprintf(buffer, DATEANDEPOCHLEN, "%s %s", datebuffer, timezonename);
  
  return;
}

// A replacement for perror() that sends output to our choice of
// printing. If errno not set then just print message.
void syserror(const char *message){
  
  if (errno) {
    // Get the correct system error message:
    const char *errormessage=strerror(errno);
    
    // Check that caller has handed a sensible string, and provide
    // appropriate output. See perrror(3) man page to understand better.
    if (message && *message)
      pout("%s: %s\n",message, errormessage);
    else
      pout("%s\n",errormessage);
  }
  else if (message && *message)
    pout("%s\n",message);
  
  return;
}

// Check regular expression for non-portable features.
//
// POSIX extended regular expressions interpret unmatched ')' ordinary:
// "The close-parenthesis shall be considered special in this context
//  only if matched with a preceding open-parenthesis."
//
// GNU libc and BSD libc support unmatched ')', Cygwin reports an error.
//
// POSIX extended regular expressions do not define empty subexpressions:
// "A vertical-line appearing first or last in an ERE, or immediately following
//  a vertical-line or a left-parenthesis, or immediately preceding a
//  right-parenthesis, produces undefined results."
//
// GNU libc and Cygwin support empty subexpressions, BSD libc reports an error.
//
static const char * check_regex(const char * pattern)
{
  int level = 0;
  char c;

  for (int i = 0; (c = pattern[i]); i++) {
    // Skip "\x"
    if (c == '\\') {
      if (!pattern[++i])
        break;
      continue;
    }

    // Skip "[...]"
    if (c == '[') {
      if (pattern[++i] == '^')
        i++;
      if (!pattern[i++])
        break;
      while ((c = pattern[i]) && c != ']')
        i++;
      if (!c)
        break;
      continue;
    }

    // Check "(...)" nesting
    if (c == '(')
      level++;
    else if (c == ')' && --level < 0)
      return "Unmatched ')'";

    // Check for leading/trailing '|' or "||", "|)", "|$", "(|", "^|"
    char c1;
    if (   (c == '|' && (   i == 0 || !(c1 = pattern[i+1])
                          || c1 == '|' || c1 == ')' || c1 == '$'))
        || ((c == '(' || c == '^') && pattern[i+1] == '|')       )
      return "Empty '|' subexpression";
  }

  return (const char *)0;
}

// Wrapper class for POSIX regex(3) or std::regex

#ifndef WITH_CXX11_REGEX

regular_expression::regular_expression()
{
  memset(&m_regex_buf, 0, sizeof(m_regex_buf));
}

regular_expression::~regular_expression()
{
  free_buf();
}

regular_expression::regular_expression(const regular_expression & x)
: m_pattern(x.m_pattern),
  m_errmsg(x.m_errmsg)
{
  memset(&m_regex_buf, 0, sizeof(m_regex_buf));
  copy_buf(x);
}

regular_expression & regular_expression::operator=(const regular_expression & x)
{
  m_pattern = x.m_pattern;
  m_errmsg = x.m_errmsg;
  free_buf();
  copy_buf(x);
  return *this;
}

void regular_expression::free_buf()
{
  if (nonempty(&m_regex_buf, sizeof(m_regex_buf))) {
    regfree(&m_regex_buf);
    memset(&m_regex_buf, 0, sizeof(m_regex_buf));
  }
}

void regular_expression::copy_buf(const regular_expression & x)
{
  if (nonempty(&x.m_regex_buf, sizeof(x.m_regex_buf))) {
    // There is no POSIX compiled-regex-copy command.
    if (!compile())
      throw std::runtime_error(strprintf(
        "Unable to recompile regular expression \"%s\": %s",
        m_pattern.c_str(), m_errmsg.c_str()));
  }
}

#endif // !WITH_CXX11_REGEX

regular_expression::regular_expression(const char * pattern)
: m_pattern(pattern)
{
  if (!compile())
    throw std::runtime_error(strprintf(
      "error in regular expression \"%s\": %s",
      m_pattern.c_str(), m_errmsg.c_str()));
}

bool regular_expression::compile(const char * pattern)
{
#ifndef WITH_CXX11_REGEX
  free_buf();
#endif
  m_pattern = pattern;
  return compile();
}

bool regular_expression::compile()
{
#ifdef WITH_CXX11_REGEX
  try {
    m_regex.assign(m_pattern, std::regex_constants::extended);
  }
  catch (std::regex_error & ex) {
    m_errmsg = ex.what();
    return false;
  }

#else
  int errcode = regcomp(&m_regex_buf, m_pattern.c_str(), REG_EXTENDED);
  if (errcode) {
    char errmsg[512];
    regerror(errcode, &m_regex_buf, errmsg, sizeof(errmsg));
    m_errmsg = errmsg;
    free_buf();
    return false;
  }
#endif

  const char * errmsg = check_regex(m_pattern.c_str());
  if (errmsg) {
    m_errmsg = errmsg;
#ifdef WITH_CXX11_REGEX
    m_regex = std::regex();
#else
    free_buf();
#endif
    return false;
  }

  m_errmsg.clear();
  return true;
}

bool regular_expression::full_match(const char * str) const
{
#ifdef WITH_CXX11_REGEX
  return std::regex_match(str, m_regex);
#else
  match_range range;
  return (   !regexec(&m_regex_buf, str, 1, &range, 0)
          && range.rm_so == 0 && range.rm_eo == (int)strlen(str));
#endif
}

bool regular_expression::execute(const char * str, unsigned nmatch, match_range * pmatch) const
{
#ifdef WITH_CXX11_REGEX
  std::cmatch m;
  if (!std::regex_search(str, m, m_regex))
    return false;
  unsigned sz = m.size();
  for (unsigned i = 0; i < nmatch; i++) {
    if (i < sz && *m[i].first) {
      pmatch[i].rm_so = m[i].first  - str;
      pmatch[i].rm_eo = m[i].second - str;
    }
    else
      pmatch[i].rm_so = pmatch[i].rm_eo = -1;
  }
  return true;

#else
  return !regexec(&m_regex_buf, str, nmatch, pmatch, 0);
#endif
}

// Splits an argument to the -t option that is assumed to be of the form
// "selective,%lld-%lld" (prefixes of "0" (for octal) and "0x"/"0X" (for hex)
// are allowed).  The first long long int is assigned to *start and the second
// to *stop.  Returns zero if successful and non-zero otherwise.
int split_selective_arg(char *s, uint64_t *start,
                        uint64_t *stop, int *mode)
{
  char *tailptr;
  if (!(s = strchr(s, ',')))
    return 1;
  bool add = false;
  if (!isdigit((int)(*++s))) {
    *start = *stop = 0;
    if (!strncmp(s, "redo", 4))
      *mode = SEL_REDO;
    else if (!strncmp(s, "next", 4))
      *mode = SEL_NEXT;
    else if (!strncmp(s, "cont", 4))
      *mode = SEL_CONT;
    else
      return 1;
    s += 4;
    if (!*s)
      return 0;
    if (*s != '+')
      return 1;
  }
  else {
    *mode = SEL_RANGE;
    errno = 0;
    // Last argument to strtoull (the base) is 0 meaning that decimal is assumed
    // unless prefixes of "0" (for octal) or "0x"/"0X" (for hex) are used.
    *start = strtoull(s, &tailptr, 0);
    s = tailptr;
    add = (*s == '+');
    if (!(!errno && (add || *s == '-')))
      return 1;
    if (!strcmp(s, "-max")) {
      *stop = ~(uint64_t)0; // replaced by max LBA later
      return 0;
    }
  }

  errno = 0;
  *stop = strtoull(s+1, &tailptr, 0);
  if (errno || *tailptr != '\0')
    return 1;
  if (add) {
    if (*stop > 0)
      (*stop)--;
    *stop += *start; // -t select,N+M => -t select,N,(N+M-1)
  }
  return 0;
}

// Returns true if region of memory contains non-zero entries
bool nonempty(const void * data, int size)
{
  for (int i = 0; i < size; i++)
    if (((const unsigned char *)data)[i])
      return true;
  return false;
}

// Copy not null terminated char array to null terminated string.
// Replace non-ascii characters.  Remove leading and trailing blanks.
const char * format_char_array(char * str, int strsize, const char * chr, int chrsize)
{
  int b = 0;
  while (b < chrsize && chr[b] == ' ')
    b++;
  int n = 0;
  while (b+n < chrsize && chr[b+n])
    n++;
  while (n > 0 && chr[b+n-1] == ' ')
    n--;

  if (n >= strsize)
    n = strsize-1;

  for (int i = 0; i < n; i++) {
    char c = chr[b+i];
    str[i] = (' ' <= c && c <= '~' ? c : '?');
  }

  str[n] = 0;
  return str;
}

// Format integer with thousands separator
const char * format_with_thousands_sep(char * str, int strsize, uint64_t val,
                                       const char * thousands_sep /* = 0 */)
{
  if (!thousands_sep) {
    thousands_sep = ",";
#ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
    const struct lconv * currentlocale = localeconv();
    if (*(currentlocale->thousands_sep))
      thousands_sep = currentlocale->thousands_sep;
#endif
  }

  char num[64];
  snprintf(num, sizeof(num), "%" PRIu64, val);
  int numlen = strlen(num);

  int i = 0, j = 0;
  do
    str[j++] = num[i++];
  while (i < numlen && (numlen - i) % 3 != 0 && j < strsize-1);
  str[j] = 0;

  while (i < numlen && j < strsize-1) {
    j += snprintf(str+j, strsize-j, "%s%.3s", thousands_sep, num+i);
    i += 3;
  }

  return str;
}

// Format capacity with SI prefixes
const char * format_capacity(char * str, int strsize, uint64_t val,
                             const char * decimal_point /* = 0 */)
{
  if (!decimal_point) {
    decimal_point = ".";
#ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
    const struct lconv * currentlocale = localeconv();
    if (*(currentlocale->decimal_point))
      decimal_point = currentlocale->decimal_point;
#endif
  }

  const unsigned factor = 1000; // 1024 for KiB,MiB,...
  static const char prefixes[] = " KMGTP";

  // Find d with val in [d, d*factor)
  unsigned i = 0;
  uint64_t d = 1;
  for (uint64_t d2 = d * factor; val >= d2; d2 *= factor) {
    d = d2;
    if (++i >= sizeof(prefixes)-2)
      break;
  }

  // Print 3 digits
  uint64_t n = val / d;
  if (i == 0)
    snprintf(str, strsize, "%u B", (unsigned)n);
  else if (n >= 100) // "123 xB"
    snprintf(str, strsize, "%" PRIu64 " %cB", n, prefixes[i]);
  else if (n >= 10)  // "12.3 xB"
    snprintf(str, strsize, "%" PRIu64 "%s%u %cB", n, decimal_point,
        (unsigned)(((val % d) * 10) / d), prefixes[i]);
  else               // "1.23 xB"
    snprintf(str, strsize, "%" PRIu64 "%s%02u %cB", n, decimal_point,
        (unsigned)(((val % d) * 100) / d), prefixes[i]);

  return str;
}

// return (v)sprintf() formatted std::string
__attribute_format_printf(1, 0)
std::string vstrprintf(const char * fmt, va_list ap)
{
  char buf[512];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  buf[sizeof(buf)-1] = 0;
  return buf;
}

std::string strprintf(const char * fmt, ...)
{
  va_list ap; va_start(ap, fmt);
  std::string str = vstrprintf(fmt, ap);
  va_end(ap);
  return str;
}

#if defined(HAVE___INT128)
// Compiler supports '__int128'.

// Recursive 128-bit to string conversion function
static int snprint_uint128(char * str, int strsize, unsigned __int128 value)
{
  if (strsize <= 0)
    return -1;

  if (value <= 0xffffffffffffffffULL) {
    // Print leading digits as 64-bit value
    return snprintf(str, (size_t)strsize, "%" PRIu64, (uint64_t)value);
  }
  else {
    // Recurse to print leading digits
    const uint64_t e19 = 10000000000000000000ULL; // 2^63 < 10^19 < 2^64
    int len1 = snprint_uint128(str, strsize, value / e19);
    if (len1 < 0)
      return -1;

    // Print 19 digits remainder as 64-bit value
    int len2 = snprintf(str + (len1 < strsize ? len1 : strsize - 1),
                        (size_t)(len1 < strsize ? strsize - len1 : 1),
                        "%019" PRIu64, (uint64_t)(value % e19)        );
    if (len2 < 0)
      return -1;
    return len1 + len2;
  }
}

// Convert 128-bit unsigned integer provided as two 64-bit halves to a string.
const char * uint128_hilo_to_str(char * str, int strsize, uint64_t value_hi, uint64_t value_lo)
{
  snprint_uint128(str, strsize, ((unsigned __int128)value_hi << 64) | value_lo);
  return str;
}

#elif defined(HAVE_LONG_DOUBLE_WIDER_PRINTF)
// Compiler and *printf() support 'long double' which is wider than 'double'.

const char * uint128_hilo_to_str(char * str, int strsize, uint64_t value_hi, uint64_t value_lo)
{
  snprintf(str, strsize, "%.0Lf", value_hi * (0xffffffffffffffffULL + 1.0L) + value_lo);
  return str;
}

#else // !HAVE_LONG_DOUBLE_WIDER_PRINTF
// No '__int128' or 'long double' support, use 'double'.

const char * uint128_hilo_to_str(char * str, int strsize, uint64_t value_hi, uint64_t value_lo)
{
  snprintf(str, strsize, "%.0f", value_hi * (0xffffffffffffffffULL + 1.0) + value_lo);
  return str;
}

#endif // HAVE___INT128

// Runtime check of byte ordering, throws on error.
static void check_endianness()
{
  const union {
    // Force compile error if int type is not 32bit.
    unsigned char c[sizeof(int) == 4 ? 8 : -1];
    uint64_t i;
  } x = {{1, 2, 3, 4, 5, 6, 7, 8}};
  const uint64_t le = 0x0807060504030201ULL;
  const uint64_t be = 0x0102030405060708ULL;

  if (!(   x.i == (isbigendian() ? be : le)
        && sg_get_unaligned_le16(x.c)   == (uint16_t)le
        && sg_get_unaligned_be16(x.c+6) == (uint16_t)be
        && sg_get_unaligned_le32(x.c)   == (uint32_t)le
        && sg_get_unaligned_be32(x.c+4) == (uint32_t)be
        && sg_get_unaligned_le64(x.c)   == le
        && sg_get_unaligned_be64(x.c)   == be          ))
    throw std::logic_error("CPU endianness does not match compile time test");
}

#ifndef HAVE_WORKING_SNPRINTF
// Some versions of (v)snprintf() don't append null char (MSVCRT.DLL),
// and/or return -1 on output truncation (glibc <= 2.0.6).
// Below are sane replacements substituted by #define in utility.h.

#undef vsnprintf
#if defined(_WIN32) && defined(_MSC_VER)
#define vsnprintf _vsnprintf
#endif

int safe_vsnprintf(char *buf, int size, const char *fmt, va_list ap)
{
  int i;
  if (size <= 0)
    return 0;
  i = vsnprintf(buf, size, fmt, ap);
  if (0 <= i && i < size)
    return i;
  buf[size-1] = 0;
  return strlen(buf); // Note: cannot detect for overflow, not necessary here.
}

int safe_snprintf(char *buf, int size, const char *fmt, ...)
{
  int i; va_list ap;
  va_start(ap, fmt);
  i = safe_vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return i;
}

static void check_snprintf() {}

#elif defined(__GNUC__) && (__GNUC__ >= 7)

// G++ 7+: Assume sane implementation and avoid -Wformat-truncation warning
static void check_snprintf() {}

#else

static void check_snprintf()
{
  char buf[] =              "ABCDEFGHI";
  int n1 = snprintf(buf, 8, "123456789");
  int n2 = snprintf(buf, 0, "X");
  if (!(!strcmp(buf, "1234567") && n1 == 9 && n2 == 1))
    throw std::logic_error("Function snprintf() does not conform to C99");
}

#endif // HAVE_WORKING_SNPRINTF

// Runtime check of ./configure result, throws on error.
void check_config()
{
  check_endianness();
  check_snprintf();
}
