#
# $Id$
#
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT(smartmontools, 6.5, smartmontools-support@lists.sourceforge.net)
AC_CONFIG_SRCDIR(smartctl.cpp)

smartmontools_cvs_tag=`echo '$Id$'`
smartmontools_release_date=2015-06-04
smartmontools_release_time="16:29:41 UTC"

AC_DEFINE_UNQUOTED(SMARTMONTOOLS_CONFIGURE_ARGS, "$ac_configure_args",            [smartmontools Configure Arguments])
AC_DEFINE_UNQUOTED(SMARTMONTOOLS_RELEASE_DATE,   "$smartmontools_release_date",   [smartmontools Release Date])
AC_DEFINE_UNQUOTED(SMARTMONTOOLS_RELEASE_TIME,   "$smartmontools_release_time",   [smartmontools Release Time])
AC_DEFINE_UNQUOTED(CONFIG_H_CVSID,               "$smartmontools_cvs_tag",        [smartmontools CVS Tag])
AC_DEFINE_UNQUOTED(PACKAGE_HOMEPAGE,             "http://smartmontools.sourceforge.net/", [smartmontools Home Page])

AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([foreign])

AM_MAINTAINER_MODE

AC_LANG([C++])
dnl Checks for programs.
AC_PROG_CXX
AM_PROG_AS
AC_PROG_INSTALL

m4_pattern_forbid([^PKG_PROG_])
if test "$cross_compiling" = "no"; then
  m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG],
    [AC_MSG_WARN([m4/pkg.m4 missing, systemd detection disabled])])
fi

AC_ARG_VAR(WINDMC, [Windows message compiler command])
AC_ARG_VAR(WINDRES, [Windows resource compiler command])
AC_ARG_VAR(MAKENSIS, [NSIS compiler command])

AC_CANONICAL_HOST
case "${host}" in
  *-*-mingw*)
    AC_CHECK_TOOL(WINDMC, [windmc])
    AC_CHECK_TOOL(WINDRES, [windres])

    AC_MSG_CHECKING([for makensis])
    if test -z "$MAKENSIS"; then
      if test -n "$PROGRAMFILES" && "$PROGRAMFILES/NSIS/makensis" -VERSION >/dev/null 2>&1; then
        MAKENSIS="$PROGRAMFILES/NSIS/makensis"
      elif makensis -VERSION >/dev/null 2>&1; then
        MAKENSIS=makensis
      fi
    fi
    AC_MSG_RESULT([${MAKENSIS:-no}])
    ;;
esac

# Check for SVN.
AC_MSG_CHECKING([whether this is a build from SVN])
is_svn_build=no
svn_deps=
if test -f "$srcdir/.svn/wc.db"; then
  # SVN 1.7, 1.8 working copy
  svn_deps='${srcdir}/.svn/wc.db'
elif test -f "${srcdir}/.svn/entries"; then
  # SVN <= 1.6 working copy (SVN 1.7 has empty entries file)
  svn_deps='${srcdir}/.svn/entries'
fi
if test -n "$svn_deps"; then
  is_svn_build=unknown
  if (cd "$srcdir" && svn --version && svnversion && svn info) >/dev/null 2>&1; then
    is_svn_build=yes
  fi
fi
AC_SUBST([svn_deps])
AM_CONDITIONAL(IS_SVN_BUILD, [test "$is_svn_build" = "yes"])
AC_MSG_RESULT([$is_svn_build])

# Note: On Linux, clock_gettime() requires -lrt which implies -lpthreads
# Check ommitted for now, gettimeofday() provides reasonable precision
# AC_SEARCH_LIBS(clock_gettime, rt)

dnl Checks for header files.
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([dev/ata/atavar.h])
AC_CHECK_HEADERS([netdb.h])
dnl we need [u]int64_t and friends.
AC_CHECK_HEADERS([inttypes.h])		dnl C99, UNIX98, solaris 2.6+
AC_CHECK_HEADERS([stdint.h])		dnl C99
AC_CHECK_HEADERS([sys/inttypes.h])	dnl pre-UNIX98
AC_CHECK_HEADERS([sys/int_types.h])	dnl pre-UNIX98, solaris 2.6+
dnl Check for FreeBSD twe include files...currently missing on 5.2, but should be there
AC_CHECK_HEADERS([sys/tweio.h])
AC_CHECK_HEADERS([sys/twereg.h])
dnl Check for FreeBSD twa include files...
AC_CHECK_HEADERS([sys/tw_osl_ioctl.h])
dnl This header file is needed for cciss_ioctl.h at least on SuSE LINUX
AC_CHECK_HEADERS([linux/compiler.h])
dnl Check for the FreeBSD CCISS system header and use internal one if not found
AC_CHECK_HEADERS([dev/ciss/cissio.h],
    [AC_DEFINE([CISS_LOCATION],[<dev/ciss/cissio.h>],[freebsd ciss header location])],
    [AC_DEFINE([CISS_LOCATION],["cissio_freebsd.h"],[freebsd ciss header location])]
    )
dnl Check for Linux CCISS include file
AC_CHECK_HEADERS([linux/cciss_ioctl.h], [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
])
dnl Check for Windows DDK and WMI header files
AC_CHECK_HEADERS([ntdddisk.h ddk/ntdddisk.h], [], [], [AC_INCLUDES_DEFAULT
#include <windows.h>
])
AC_CHECK_HEADERS([wbemcli.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_TYPES([int64_t, uint64_t])

dnl Checks for library functions.
AC_CHECK_FUNCS([getopt_long], [need_getopt_long=no], [need_getopt_long=yes])
AM_CONDITIONAL(NEED_GETOPT_LONG, [test "$need_getopt_long" = "yes"])
AC_CHECK_FUNCS([regcomp], [need_regex=no], [need_regex=yes])
AM_CONDITIONAL(NEED_REGEX, [test "$need_regex" = "yes"])

AC_CHECK_FUNCS([sigset])
AC_CHECK_FUNCS([strtoull])
AC_CHECK_FUNCS([uname])
AC_CHECK_FUNCS([clock_gettime ftime gettimeofday])

# Check byte ordering (defines WORDS_BIGENDIAN)
AC_C_BIGENDIAN

# check for __attribute__((packed))
# (sizeof() check is required to avoid false positives if other
# __attribute__((x)) are supported)
AC_MSG_CHECKING([whether $CXX supports __attribute__((packed))])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [[
    struct s { char a; short b; } __attribute__((packed));
    typedef char t[sizeof(struct s) == 3 ? 1 : -1];]])],
  [gcc_have_attr_packed=yes], [gcc_have_attr_packed=no])
AC_SUBST(gcc_have_attr_packed)
if test "$gcc_have_attr_packed" = "yes"; then
  AC_DEFINE(HAVE_ATTR_PACKED, 1, [Define to 1 if C++ compiler supports __attribute__((packed))])
fi
AC_MSG_RESULT([$gcc_have_attr_packed])

AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(ASFLAGS)

AC_ARG_WITH(systemdsystemunitdir,
  [AS_HELP_STRING([--with-systemdsystemunitdir@<:@=DIR|auto|yes|no@:>@], [Location of systemd service files [auto]])],
  [], [with_systemdsystemunitdir=auto])

systemdsystemunitdir=
case "$with_systemdsystemunitdir" in
 auto|yes)
   if test -n "$PKG_CONFIG"; then
     AC_MSG_CHECKING([for systemdsystemunitdir])
     systemdsystemunitdir=`$PKG_CONFIG --variable=systemdsystemunitdir systemd 2>/dev/null`
     AC_MSG_RESULT([${systemdsystemunitdir:-no}])
   fi
   case "$with_systemdsystemunitdir:$sysconfdir:$systemdsystemunitdir" in
     yes:*:) AC_MSG_ERROR([Location of systemd service files not found]) ;;
     yes:*:*|auto:*:|auto:/etc:*) ;;
     *) systemdsystemunitdir='${prefix}'$systemdsystemunitdir ;;
   esac ;;
 no) ;;
 *) systemdsystemunitdir="$with_systemdsystemunitdir" ;;
esac
AC_SUBST(systemdsystemunitdir)
AM_CONDITIONAL(INSTALL_SYSTEMDUNIT, [test -n "$systemdsystemunitdir"])

AC_ARG_WITH(systemdenvfile,
  [AS_HELP_STRING([--with-systemdenvfile@<:@=FILE|auto|yes|no@:>@], [Path of systemd EnvironmentFile [auto]])],
  [], [with_systemdenvfile=auto])

systemdenvfile=
case "$with_systemdenvfile:$cross_compiling:$systemdsystemunitdir" in
  auto:no:?*|yes:*:?*)
    AC_MSG_CHECKING([for path of systemd EnvironmentFile])
    for dir in sysconfig default; do
      if test -d /etc/$dir; then
        systemdenvfile='${sysconfdir}'/$dir/smartmontools
        break
      fi
    done
    AC_MSG_RESULT([${systemdenvfile:-no}])
    case "$with_systemdenvfile:$systemdenvfile" in
      yes:) AC_MSG_ERROR([Path of systemd EnvironmentFile not found]) ;;
    esac ;;
  auto:*|no:*) ;;
  *:*:) AC_MSG_ERROR([Location of systemd service files not found]) ;;
  *) systemdenvfile="$with_systemdenvfile"
esac
AC_SUBST(systemdenvfile)

AC_ARG_WITH(initscriptdir,
  [AS_HELP_STRING([--with-initscriptdir@<:@=DIR|auto|yes|no@:>@], [Location of init scripts [auto]])],
  [], [with_initscriptdir=auto])

initddir=
case "$with_initscriptdir:$cross_compiling:$systemdsystemunitdir" in
  auto:no:|yes:*)
    AC_MSG_CHECKING([for init (rc) directory])
    for dir in rc.d/init.d init.d rc.d; do
      if test -d /etc/$dir; then
        initddir='${sysconfdir}'/$dir
        break
      fi
    done
    AC_MSG_RESULT([${initddir:-no}])
    case "$with_initscriptdir:$initddir" in
      yes:) AC_MSG_ERROR([Location of init scripts not found]) ;;
    esac ;;
  auto:*|no:*) ;;
  *) initddir="$with_initscriptdir" ;;
esac
AC_SUBST(initddir)
AM_CONDITIONAL(INSTALL_INITSCRIPT, [test -n "$initddir"])

# use different init script templates for different OS
case "${host}" in
  *-*-freebsd*)
    initdfile="smartd.freebsd.initd"
    ;;
  *)
    initdfile="smartd.initd"
    ;;
esac
AC_SUBST(initdfile)

autoconf_25x=${docdir:-yes}
AC_ARG_WITH(docdir,
  [AS_HELP_STRING([--with-docdir=DIR], [Deprecated (use --docdir=DIR instead)])],
  [docdir="$withval"],
  [ if test -z "$docdir"; then
      # autoconf 2.5x without '--docdir' support
      docdir='${datadir}/doc/${PACKAGE}'
    fi
  ])
AC_SUBST(docdir)

AC_ARG_WITH(exampledir,
  [AS_HELP_STRING([--with-exampledir=DIR], [Location of example scripts [DOCDIR/examplescripts]])],
  [exampledir="$withval"], [exampledir='${docdir}/examplescripts'])
AC_SUBST(exampledir)

used_deprecated_option=no
AC_ARG_ENABLE(drivedb,
  [AS_HELP_STRING([--disable-drivedb], [Deprecated (use --without-drivedbdir instead)])],
  [used_deprecated_option=yes], [enable_drivedb=yes])

drivedbdir=
AC_ARG_WITH(drivedbdir,
  [AS_HELP_STRING([--with-drivedbdir=@<:@DIR|no@:>@], [Location of drive database file [DATADIR/smartmontools]])],
  [test "$withval" != "no" && drivedbdir="$withval"],
  [test "$enable_drivedb" != "no" && drivedbdir='${datadir}/${PACKAGE}'])
AC_SUBST(drivedbdir)
AM_CONDITIONAL(ENABLE_DRIVEDB, [test -n "$drivedbdir"])

AC_ARG_WITH(smartdscriptdir,
  [AS_HELP_STRING([--with-smartdscriptdir=DIR], [Location of smartd_warning.sh script [SYSCONFDIR]])],
  [smartdscriptdir="$withval"], [smartdscriptdir='${sysconfdir}'])
AC_SUBST(smartdscriptdir)

AC_ARG_WITH(smartdplugindir,
  [AS_HELP_STRING([--with-smartdplugindir=@<:@DIR|no@:>@],
    [Location of smartd_warning.sh plugin scripts [SMARTDSCRIPTDIR/smartd_warning.d]])],
  [smartdplugindir=; test "$withval" != "no" && smartdplugindir="$withval"],
  [smartdplugindir='${smartdscriptdir}/smartd_warning.d'])
AC_SUBST(smartdplugindir)

AC_ARG_ENABLE(savestates,
  [AS_HELP_STRING([--enable-savestates], [Deprecated (use --with-savestates@<:@=yes@:>@ instead)])],
  [used_deprecated_option=yes], [enable_savestates=no])

savestates='${localstatedir}/lib/${PACKAGE}/smartd.'
AC_ARG_WITH(savestates,
  [AS_HELP_STRING([--with-savestates@<:@=PREFIX|yes|no@:>@],
    [Enable default smartd state files [no] (yes=LOCALSTATEDIR/lib/smartmontools/smartd.)])],
  [case "$withval" in yes) ;; no) savestates= ;; *) savestates="$withval" ;; esac],
  [test "$enable_savestates" != "yes" && savestates=])
savestatesdir="${savestates%/*}"
AC_SUBST(savestates)
AC_SUBST(savestatesdir)
AM_CONDITIONAL(ENABLE_SAVESTATES, [test -n "$savestates"])

AC_ARG_ENABLE(attributelog,
  [AS_HELP_STRING([--enable-attributelog], [Deprecated (use --with-attributelog@<:@=yes@:>@ instead)])],
  [used_deprecated_option=yes], [enable_attributelog=no])

attributelog='${localstatedir}/lib/${PACKAGE}/attrlog.'
AC_ARG_WITH(attributelog,
  [AS_HELP_STRING([--with-attributelog@<:@=PREFIX|yes|no@:>@],
    [Enable default smartd attribute log files [no] (yes=LOCALSTATEDIR/lib/smartmontools/attrlog.)])],
  [case "$withval" in yes) ;; no) attributelog= ;; *) attributelog="$withval" ;; esac],
  [test "$enable_attributelog" != "yes" && attributelog=])
attributelogdir="${attributelog%/*}"
AC_SUBST(attributelog)
AC_SUBST(attributelogdir)
AM_CONDITIONAL(ENABLE_ATTRIBUTELOG, [test -n "$attributelog"])

AC_ARG_ENABLE(sample,
  [AS_HELP_STRING([--enable-sample], [Enables appending .sample to the installed smartd rc script and configuration file])],
  [smartd_suffix=; test "$enableval" = "yes" && smartd_suffix=".sample"],
  [smartd_suffix=;])
AC_SUBST(smartd_suffix)

AC_ARG_WITH(os-deps,
  [AS_HELP_STRING([--with-os-deps='os_module.o ...'], [Specify OS dependent module(s) [guessed]])],
  [ for x in $with_os_deps; do
      case $x in
        *.o) ;;
        *) AC_MSG_ERROR([non-object file specified by --with-os-deps]) ;;
      esac
    done
  ],[])

AC_ARG_WITH(selinux,
  [AS_HELP_STRING([--with-selinux@<:@=yes|no@:>@], [Enables SELinux support [no]])],
  [ if test "$withval" = "yes"; then
      AC_CHECK_HEADERS([selinux/selinux.h], [], [AC_MSG_ERROR([Missing SELinux header files])])
      AC_CHECK_LIB(selinux, matchpathcon, [], [AC_MSG_ERROR([Missing or incorrect SELinux library files])])
    fi
  ],[])
AC_SUBST(with_selinux)
if test "$with_selinux" = "yes"; then
  AC_DEFINE(WITH_SELINUX, 1, [Define to 1 if SELinux support is enabled])
fi

AC_ARG_WITH(libcap-ng,
  [AS_HELP_STRING([--with-libcap-ng@<:@=auto|yes|no@:>@], [Add Libcap-ng support to smartd [auto]])],
  [], [with_libcap_ng=auto])

use_libcap_ng=no
if test "$with_libcap_ng" != "no"; then
  AC_CHECK_LIB(cap-ng, capng_clear,
    [AC_DEFINE(HAVE_LIBCAP_NG, 1, [Define to 1 if you have the `cap-ng' library (-lcap-ng).]) dnl `vim syntax
     CAPNG_LDADD="-lcap-ng"; use_libcap_ng=yes])

  if test "$use_libcap_ng" = "yes"; then
    AC_CHECK_HEADER(cap-ng.h, [], [AC_MSG_ERROR([libcap-ng libraries found but headers are missing])])
  elif test "$with_libcap_ng" = "yes"; then
    AC_MSG_ERROR([libcap-ng support was requested but the library was not found])
  fi
fi

AC_MSG_CHECKING([whether to use libcap-ng])
AC_SUBST(CAPNG_LDADD)
AC_MSG_RESULT([$use_libcap_ng])

# Assume broken snprintf only on Windows with MSVCRT (MinGW without ANSI stdio support)
libc_have_working_snprintf=yes

case "$host" in
  *-*-mingw*)
    case " $CPPFLAGS $CXXFLAGS" in
      *\ -[[DU]]__USE_MINGW_ANSI_STDIO*)
        ;;
      *)
        # Older MinGW do not properly define PRI?64 if __USE_MINGW_ANSI_STDIO is set
        # Newer MinGW set __USE_MINGW_ANSI_STDIO in first C++ include which may be too late
        AC_MSG_CHECKING([whether __USE_MINGW_ANSI_STDIO is defined by C++ includes])
        AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
          #undef __USE_MINGW_ANSI_STDIO
          #include <iostream>
          #ifndef __USE_MINGW_ANSI_STDIO
            #error false
          #endif]])],
        [CXXFLAGS="-D__USE_MINGW_ANSI_STDIO $CXXFLAGS"],
        [libc_have_working_snprintf=no])
        AC_MSG_RESULT([$libc_have_working_snprintf])
        ;;
    esac ;;
esac

AC_ARG_WITH(working-snprintf,
  [AS_HELP_STRING([--with-working-snprintf@<:@=yes|no@:>@],
    [Function snprintf() handles output truncation as specified by C99 [MinGW:guessed,others:yes]])],
  [libc_have_working_snprintf=$withval])

if test "$libc_have_working_snprintf" = "yes"; then
  AC_DEFINE(HAVE_WORKING_SNPRINTF, 1, [Define to 1 if the `snprintf' function is sane.]) dnl `vim syntax
fi

if test "$prefix" = "NONE"; then
    # Fix mandir default set by autoconf 2.5x
    if test "$mandir" = '${prefix}/man'; then
    	AC_SUBST([mandir], ['${prefix}/share/man'])
    fi
fi

AC_SUBST(releaseversion,['${PACKAGE}-${VERSION}'])
AC_SUBST(smartmontools_release_date)
AC_SUBST(smartmontools_release_time)

# Set platform-specific modules and symbols
os_libs=
os_dltools='curl wget lynx'
os_mailer=mail
os_hostname="'hostname'"
os_dnsdomainname=
os_nisdomainname="'domainname'"
os_darwin=no
os_solaris=no
os_win32=no
os_win32_mingw=no
os_win64=no
os_man_filter=
case "${host}" in
  *-*-linux*)
    os_deps='os_linux.o cciss.o dev_areca.o'
    os_dnsdomainname="'dnsdomainname' 'hostname -d'"
    os_nisdomainname="'nisdomainname' 'hostname -y' 'domainname'"
    os_man_filter=Linux
    ;;
  *-*-freebsd*|*-*-dragonfly*|*-*-kfreebsd*-gnu*)
    os_deps='os_freebsd.o cciss.o dev_areca.o'
    os_libs='-lcam'
    os_dltools='curl wget lynx fetch'
    AC_CHECK_LIB(usb, libusb20_dev_get_device_desc)
    os_man_filter=FreeBSD
    ;;
  sparc-*-solaris*)
    os_deps='os_solaris.o os_solaris_ata.o'
    os_mailer='mailx'
    os_solaris=yes
    os_man_filter=Solaris
    ;;
  *-pc-solaris*)
    os_deps='os_solaris.o'
    os_mailer='mailx'
    os_solaris=yes
    os_man_filter=Solaris
    ;;
  *-*-netbsd*)
    os_deps='os_netbsd.o'
    os_libs='-lutil'
    os_man_filter=NetBSD
    ;;
  *-*-openbsd*)
    os_deps='os_openbsd.o'
    os_libs='-lutil'
    os_dltools='curl wget lynx ftp'
    os_man_filter=OpenBSD
    ;;
  *-*-cygwin*)
    os_deps='os_win32.o dev_areca.o'
    os_mailer='email'
    os_hostname="'hostname' 'echo "'"${HOSTNAME?unset}"'"'"
    os_dnsdomainname="'dnsdomainname' 'hostname -d' 'echo "'"${USERDNSDOMAIN?unset}"'"'"
    os_nisdomainname=
    os_win32=yes
    os_man_filter=Cygwin
    ;;
  x86_64-*-mingw*)
    os_deps='os_win32.o dev_areca.o'
    os_win32=yes
    os_win32_mingw=yes
    os_win64=yes
    os_man_filter=Windows
    ;;
  *-*-mingw*)
    os_deps='os_win32.o dev_areca.o'
    os_win32=yes
    os_win32_mingw=yes
    os_man_filter=Windows
    ;;
  *-*-darwin*)
    os_deps='os_darwin.o'
    os_libs='-framework CoreFoundation -framework IOKit'
    os_darwin=yes
    os_man_filter=Darwin
    ;;
  *-*-nto-qnx*)
    os_deps='os_qnxnto.o'
    ;;
  *)
    os_deps='os_generic.o'
    ;;
esac

# Replace if '--with-os-deps' was specified
test -z "$with_os_deps" || os_deps="$with_os_deps"

# Check if we need adapter to old interface (dev_legacy.cpp)
os_src=`echo "${os_deps}"|sed -n 's,^\([[^ .]]*\)\.o.*$,\1.cpp,p'`
AC_MSG_CHECKING([whether ${os_src} uses new interface])
if grep "smart_interface" "${srcdir}/${os_src}" >/dev/null 2>&1; then
  os_new_interface=yes
else
  os_new_interface=no
  os_deps="${os_deps} dev_legacy.o"
  AC_DEFINE(OLD_INTERFACE, 1, [Define to 1 if os_*.cpp still uses the old interface])
fi
AC_MSG_RESULT([$os_new_interface])

AC_SUBST([os_deps])
AC_SUBST([os_libs])
AC_SUBST([os_dltools])
AC_SUBST([os_mailer])
AC_SUBST([os_hostname])
AC_SUBST([os_dnsdomainname])
AC_SUBST([os_nisdomainname])
AC_SUBST([os_man_filter])

# Create drivedb.h update branch name from version: 5.41[.X] -> RELEASE_5_41_DRIVEDB
DRIVEDB_BRANCH=`echo $VERSION | sed 's,^\([[0-9]]*\.[[0-9]]*\)\..*$,\1,' \
                | sed -n 's,^\([[0-9]][[0-9]]*\)\.\([[0-9]][[0-9]]*\)$,RELEASE_\1_\2_DRIVEDB,p'`
if test -z "$DRIVEDB_BRANCH"; then
  AC_MSG_ERROR([Unable to create DRIVEDB_BRANCH from VERSION=$VERSION])
fi
AC_SUBST([DRIVEDB_BRANCH])

# Enable platform-specific makefile sections
AM_CONDITIONAL(OS_DARWIN, [test "$os_darwin" = "yes"])
AM_CONDITIONAL(OS_SOLARIS, [test "$os_solaris" = "yes"])
AM_CONDITIONAL(OS_WIN32, [test "$os_win32" = "yes"])
AM_CONDITIONAL(OS_WIN32_MINGW, [test "$os_win32_mingw" = "yes"])
AM_CONDITIONAL(OS_WIN32_NSIS, [test -n "$MAKENSIS"])
AM_CONDITIONAL(OS_WIN64, [test "$os_win64" = "yes"])

dnl Add -Wall and -W if using g++ and its not already specified.
if test "$GXX" = "yes"; then
  if test -z "`echo "$CXXFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
      CXXFLAGS="$CXXFLAGS -Wall"
  fi
# In the next line, do NOT delete the 2 spaces inside double quotes.
  if test -z "`echo "$CXXFLAGS " | grep "\-W " 2> /dev/null`" ; then
      CXXFLAGS="$CXXFLAGS -W"
  fi

else
 dnl We are NOT using gcc, so enable host-specific compiler flags
 case "${host}" in
	sparc*-*-solaris*) 
          dnl set CXXFLAGS for Solaris/SPARC C++ compiler
          if test -z "`echo "$CXXFLAGS" | grep "\-xmemalign" 2> /dev/null`" ; then
            dnl we have to tell the compilers about packed ATA structures
            CXXFLAGS="-xmemalign=1i $CXXFLAGS"
          fi
 esac
 case "${host}" in
	*-*-solaris*) 
          if test -z "`echo "$CXXFLAGS" | grep "\-xO" 2> /dev/null`" ; then
            dnl turn on optimization if user has not explicitly set its value
            CXXFLAGS="-xO2 $CXXFLAGS"
          fi
          if test -z "`echo "$CXXFLAGS" | grep "\-erroff" 2> /dev/null`" ; then
	    dnl suppress trivial warnings
	    CXXFLAGS="-erroff=%none,wbadinitl,wbadasgl,badargtypel2w,badargtype2w $CXXFLAGS"
	  fi
 esac
fi

AC_DEFINE_UNQUOTED(SMARTMONTOOLS_BUILD_HOST,     "${host}",                       [smartmontools Build Host])

AC_SUBST(CXXFLAGS)

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
AC_PROG_MAKE_SET

echo "-----------------------------------------------------------------------------" >&AS_MESSAGE_FD
echo "${PACKAGE}-${VERSION} configuration:" >&AS_MESSAGE_FD
echo "host operating system:  $host" >&AS_MESSAGE_FD
echo "C++ compiler:           $CXX" >&AS_MESSAGE_FD
echo "C compiler:             $CC" >&AS_MESSAGE_FD
echo "preprocessor flags:     $CPPFLAGS" >&AS_MESSAGE_FD
echo "C++ compiler flags:     $CXXFLAGS" >&AS_MESSAGE_FD
echo "C compiler flags:       $CFLAGS" >&AS_MESSAGE_FD
echo "linker flags:           $LDFLAGS" >&AS_MESSAGE_FD
echo "OS specific modules:    $os_deps $os_libs $LIBS" >&AS_MESSAGE_FD

case "$host_os" in
  mingw*)
    echo "resource compiler:      $WINDRES" >&AS_MESSAGE_FD
    echo "message compiler:       $WINDMC" >&AS_MESSAGE_FD
    echo "NSIS compiler:          $MAKENSIS" >&AS_MESSAGE_FD
    if test -n "$drivedbdir"; then
      echo "drive database file:    EXEDIR/drivedb.h" >&AS_MESSAGE_FD
      if test -n "$MAKENSIS"; then
        echo "database update tool:   EXEDIR/update-smart-drivedb.exe" >&AS_MESSAGE_FD
      fi
    else
      echo "drive database file:    [[disabled]]" >&AS_MESSAGE_FD
    fi
    if test -n "$savestates"; then
      echo "smartd save files:      `eval eval eval echo $savestates`MODEL-SERIAL.TYPE.state" >&AS_MESSAGE_FD
    fi
    if test -n "$attributelog"; then
      echo "smartd attribute logs:  `eval eval eval echo $attributelog`MODEL-SERIAL.TYPE.csv" >&AS_MESSAGE_FD
    fi
    ;;

  *)
    echo "binary install path:    `eval eval eval echo $sbindir`" >&AS_MESSAGE_FD
    echo "man page install path:  `eval eval eval echo $mandir`" >&AS_MESSAGE_FD
    echo "doc file install path:  `eval eval eval echo $docdir`" >&AS_MESSAGE_FD
    echo "examples install path:  `eval eval eval echo $exampledir`" >&AS_MESSAGE_FD
    if test -n "$drivedbdir"; then
      echo "drive database file:    `eval eval eval echo $drivedbdir`/drivedb.h" >&AS_MESSAGE_FD
      echo "database update script: `eval eval eval echo $sbindir`/update-smart-drivedb" >&AS_MESSAGE_FD
      echo "download tools:         `eval eval eval echo $os_dltools`" >&AS_MESSAGE_FD
    else
      echo "drive database file:    [[disabled]]" >&AS_MESSAGE_FD
    fi
    echo "local drive database:   `eval eval eval echo $sysconfdir`/smart_drivedb.h" >&AS_MESSAGE_FD
    echo "smartd config file:     `eval eval eval echo $sysconfdir`/smartd.conf${smartd_suffix}" >&AS_MESSAGE_FD
    echo "smartd warning script:  `eval eval eval echo $smartdscriptdir`/smartd_warning.sh" >&AS_MESSAGE_FD
    if test -n "$smartdplugindir"; then
      echo "smartd plugin path:     `eval eval eval echo $smartdplugindir`" >&AS_MESSAGE_FD
    else
      echo "smartd plugin path:     [[disabled]]" >&AS_MESSAGE_FD
    fi
    if test -n "$initddir"; then
      echo "smartd initd script:    `eval eval eval echo $initddir`/smartd${smartd_suffix}" >&AS_MESSAGE_FD
    elif test -z "$systemdsystemunitdir"; then
      echo "smartd initd script:    [[disabled]]" >&AS_MESSAGE_FD
    fi
    if test -n "$systemdsystemunitdir"; then
      echo "smartd systemd file:    `eval eval eval echo $systemdsystemunitdir`/smartd.service" >&AS_MESSAGE_FD
      if test -n "$systemdenvfile"; then
        echo "smartd environ file:    `eval eval eval echo $systemdenvfile`" >&AS_MESSAGE_FD
      else
        echo "smartd environ file:    [[disabled]]" >&AS_MESSAGE_FD
      fi
    fi
    if test -n "$savestates"; then
      echo "smartd save files:      `eval eval eval echo $savestates`MODEL-SERIAL.TYPE.state" >&AS_MESSAGE_FD
    else
      echo "smartd save files:      [[disabled]]" >&AS_MESSAGE_FD
    fi
    if test -n "$attributelog"; then
      echo "smartd attribute logs:  `eval eval eval echo $attributelog`MODEL-SERIAL.TYPE.csv" >&AS_MESSAGE_FD
    else
      echo "smartd attribute logs:  [[disabled]]" >&AS_MESSAGE_FD
    fi
    echo "libcap-ng support:      $use_libcap_ng" >&AS_MESSAGE_FD
    case "$host_os" in
      linux*) echo "SELinux support:        ${with_selinux-no}" >&AS_MESSAGE_FD ;;
    esac
    ;;
esac
echo "-----------------------------------------------------------------------------" >&AS_MESSAGE_FD

if test "$autoconf_25x" = "yes"; then
 echo "WARNING:" >&AS_MESSAGE_FD
 echo "Support for old autoconf 2.5x versions will be removed in a future" >&AS_MESSAGE_FD
 echo "release of smartmontools." >&AS_MESSAGE_FD
elif test "${with_docdir+set}" = "set"; then
 echo "WARNING:" >&AS_MESSAGE_FD
 echo "Option --with-docdir is deprecated and will be removed in a future" >&AS_MESSAGE_FD
 echo "release of smartmontools.  Use --docdir instead." >&AS_MESSAGE_FD
fi

if test "$used_deprecated_option" = "yes"; then
  echo "WARNING:" >&AS_MESSAGE_FD
  echo "Options --disable-drivedb, --enable-savestates, --enable-attributelog are" >&AS_MESSAGE_FD
  echo "deprecated and will be removed in a future release of smartmontools." >&AS_MESSAGE_FD
  echo "Use --without-drivedb, --with-savestates, --with-attributelog instead." >&AS_MESSAGE_FD
fi
