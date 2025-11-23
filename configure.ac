#
# configure.ac
#
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([smartmontools],[8.0],[smartmontools-support@listi.jpberlin.de],[],[https://www.smartmontools.org/])
AM_INIT_AUTOMAKE([1.13 foreign subdir-objects tar-ustar])
LT_INIT([disable_shared])

# Version of drive database branch
smartmontools_drivedb_version=7.5

# Set by 'do_release' script, commented out when release number is bumped.
smartmontools_release_date= # 2025-04-30
smartmontools_release_time= # "07:42:55 UTC"

if test -n "$smartmontools_release_date"; then
  AC_DEFINE_UNQUOTED(SMARTMONTOOLS_RELEASE_DATE, "$smartmontools_release_date",    [smartmontools Release Date])
  AC_DEFINE_UNQUOTED(SMARTMONTOOLS_RELEASE_TIME, "$smartmontools_release_time",    [smartmontools Release Time])
fi

AC_DEFINE_UNQUOTED(SMARTMONTOOLS_CONFIGURE_ARGS, "$ac_configure_args",             [smartmontools Configure Arguments])

AC_CONFIG_SRCDIR([src/smartctl.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_MAINTAINER_MODE

AC_LANG([C++])
dnl Checks for programs.
AC_PROG_CXX
AM_PROG_AS
AC_PROG_INSTALL

m4_pattern_forbid([^PKG_PROG_])
if test "$cross_compiling" = "no"; then
  # PKG_PROG_PKG_CONFIG from 'pkgconf' >= 2.3.0 aborts if 'pkg-config' is
  # missing and the new 2nd parameter is not set
  m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG([], [PKG_CONFIG=])],
    [AC_MSG_WARN([m4/pkg.m4 missing, systemd detection disabled])])
fi

AC_ARG_VAR(SOURCE_DATE_EPOCH, [Reproducible build timestamp])
AC_ARG_VAR(OBJCOPY, [Object file copy command which supports '-D'])
AC_ARG_VAR(WINDMC, [Windows message compiler command])
AC_ARG_VAR(WINDRES, [Windows resource compiler command])
AC_ARG_VAR(MAKENSIS, [NSIS compiler command])

AC_CANONICAL_HOST

# Some make targets are incompatible with cross-compiling.
# Note: Don't use 'test "$cross_compiling" = "yes"' because it may be "maybe".
res=yes
if test "$cross_compiling" != "no"; then
  AC_MSG_CHECKING([whether executables could be run despite cross-compiling])
  case "$build:$host" in
    x86_64-pc-cygwin:i686-w64-mingw32)   ;;
    x86_64-pc-cygwin:x86_64-w64-mingw32) ;;
    *) res=no ;;
  esac
  AC_MSG_RESULT([$res])
fi
AM_CONDITIONAL(REALLY_CROSS_COMPILING, [test "$res" = "no"])

case "$host_os" in
  mingw*)
    AC_CHECK_TOOL(WINDMC, [windmc])
    AC_CHECK_TOOL(WINDRES, [windres])
    test -n "$WINDRES" || AC_MSG_ERROR([Missing Windows resource compiler (windres)])
    AC_CHECK_PROG(MAKENSIS, [makensis], [makensis])
    if test -z "$MAKENSIS" && test -n "$PROGRAMFILES"; then
      AC_MSG_CHECKING([for makensis at default install location])
      for x in '' ' (x86)'; do if test -f "${PROGRAMFILES}${x}/NSIS/makensis.exe"; then
        MAKENSIS="${PROGRAMFILES}${x}/NSIS/makensis"; break
      fi; done
      AC_MSG_RESULT([${MAKENSIS:-no}])
    fi
    ;;
esac

source_date_iso=
if test -n "$SOURCE_DATE_EPOCH"; then
  # SOURCE_DATE_EPOCH must be either unset or numeric.
  test -z "`echo "$SOURCE_DATE_EPOCH" | sed -e 's,^$,X,' -e 's,[[0-9]],,g'`" \
  || AC_MSG_ERROR([Malformed SOURCE_DATE_EPOCH: '$SOURCE_DATE_EPOCH'])
  # There are no portable 'date' or 'touch' options accepting SOURCE_DATE_EPOCH
  # (GNU: '-d @$SOURCE_DATE_EPOCH', BSD: '-r $SOURCE_DATE_EPOCH', POSIX: unsupported).
  # Try conversion to ISO format with GNU or BSD 'date'.  Use portable
  # 'touch -d $source_date_iso' in Makefile if successful.
  AC_MSG_CHECKING([how to convert SOURCE_DATE_EPOCH to ISO format])
  for cmd in 'date -d @' 'date -r ' 'gdate -d @' ; do
    source_date_iso=`${cmd}${SOURCE_DATE_EPOCH} -u +'%Y-%m-%dT%H:%M:%SZ' 2>/dev/null` && break
    cmd="unknown"
  done
  AC_MSG_RESULT([$cmd])
fi
AC_SUBST([SOURCE_DATE_EPOCH])
AC_SUBST([source_date_iso])
AM_CONDITIONAL(REPRODUCIBLE_BUILD, [test -n "$SOURCE_DATE_EPOCH"])

# Checks for header files.
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([byteswap.h], [], [], [])

case "$host" in
  *-*-freebsd*|*-*-dragonfly*|*-*-kfreebsd*-gnu*|*-*-midnightbsd*)
    # Check for FreeBSD twe and twa include files
    AC_CHECK_HEADERS([sys/tweio.h sys/twereg.h sys/tw_osl_ioctl.h])
    # Check for the FreeBSD CCISS system header and use internal one if not found
    AC_CHECK_HEADERS([dev/ciss/cissio.h],
      [AC_DEFINE([CISS_LOCATION],[<dev/ciss/cissio.h>],[freebsd ciss header location])],
      [AC_DEFINE([CISS_LOCATION],["cissio_freebsd.h"],[freebsd ciss header location])])
    ;;
  *-*-linux*)
    # <linux/compiler.h> is needed for cciss_ioctl.h at least on SuSE LINUX
    AC_CHECK_HEADERS([sys/sysmacros.h linux/compiler.h])
    # Check for Linux CCISS include file
    AC_CHECK_HEADERS([linux/cciss_ioctl.h], [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
])
    ;;
  *-*-netbsd*|*-*-openbsd*)
    AC_CHECK_HEADERS([dev/ata/atavar.h])
    ;;
esac

# Checks for typedefs, and compiler characteristics.
AC_CHECK_TYPES([__int128])
AC_TYPE_LONG_DOUBLE_WIDER

# Checks for library functions.
AC_CHECK_FUNCS([getopt_long], [need_getopt_long=no], [need_getopt_long=yes])
AM_CONDITIONAL(NEED_GETOPT_LONG, [test "$need_getopt_long" = "yes"])

case "$host_os" in
  mingw*)
    # Older MinGW-w64 (5.0.3) require -lwinpthread
    AC_SEARCH_LIBS([clock_gettime], [winpthread])
    ;;
  *)
    AC_CHECK_FUNCS([close_range])
    ;;
esac

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

AC_ARG_ENABLE(static-link,
  [AS_HELP_STRING([--enable-static-link], [Do not link against shared libraries [no]])],
  [ case "$enableval:$enable_shared: $LDFLAGS " in
      *\ --static\ *|*\ -static\ *|*:*\ -Bstatic\ *)
        AC_MSG_ERROR(['--enable-static-link=$enableval' is not compatible with LDFLAGS='$LDFLAGS']) ;;
      yes:yes:*)
        AC_MSG_ERROR(['--enable-static-link' is not compatible with '--enable-shared']) ;;
      yes:*)
        # Use '--static' because '-static' is a libtool option with a different meaning
        # TODO: This requires GCC or Clang
        LDFLAGS="${LDFLAGS}${LDFLAGS:+ }--static" ;;
    esac
  ], [enable_static_link=no])
AC_SUBST([enable_static_link])

AC_ARG_WITH(build-info,
  [AS_HELP_STRING([--with-build-info='(TEXT)'],
    [Build information printed the in first output line [(local build)]])],
  [ case $withval in
      *[[\"\'\\]]*) AC_MSG_ERROR([Invalid quoting character(s) in '--with-build-info' argument]) ;;
      *) BUILD_INFO='"'"$withval"'"' ;;
    esac
  ], [BUILD_INFO='"(local build)"'])
AC_SUBST(BUILD_INFO)

AC_ARG_WITH(devel,
  [AS_HELP_STRING([--with-devel@<:@=yes|no@:>@], [Install development headers and library [yes]])],
  [], [with_devel=yes])
AM_CONDITIONAL(INSTALL_DEVEL_INC, [test "$with_devel" = "yes"])
AM_CONDITIONAL(INSTALL_DEVEL_LIB, [test "$with_devel" = "yes" || test "$enable_shared" = "yes"])

develsrcdir='${docdir}/examplesrc'
AC_ARG_WITH(devel-src,
  [AS_HELP_STRING([--with-devel-src@<:@=DIR|yes|no@:>@],
    [Location of library example source codes [DOCDIR/examplesrc]])],
  [ case $withval in
      yes) ;; no) develsrcdir= ;; *) develsrcdir=$withval ;;
    esac
  ], [test "$with_devel" = "yes" || develsrcdir=])
AC_SUBST([develsrcdir])
AM_CONDITIONAL(INSTALL_DEVEL_SRC, [test -n "$develsrcdir"])

develbindir='${libexecdir}/${PACKAGE}/examples'
AC_ARG_WITH(devel-bin,
  [AS_HELP_STRING([--with-devel-bin@<:@=DIR|yes|no@:>@],
    [Location of library example binaries [LIBEXECDIR/smartmontools/examples]])],
  [ case $withval in
      yes) ;; no) develbindir= ;; *) develbindir=$withval ;;
    esac
  ], [test "$with_devel" = "yes" || develbindir=])
AC_SUBST([develbindir])
AM_CONDITIONAL(INSTALL_DEVEL_BIN, [test -n "$develbindir"])

objcopy_deterministic=false
case "$with_devel:$enable_static:$SOURCE_DATE_EPOCH" in
  yes:yes:?*)
    # Use 'objcopy -D' to remove the timestamps from the static library
    # because libtool offers no way to pass '-D' to 'ranlib'.
    AC_CHECK_TOOL(OBJCOPY, [objcopy], [false])
    AC_MSG_CHECKING([whether $OBJCOPY accepts -D])
    objcopy_deterministic="$OBJCOPY -D"
    res=yes
    if ! (rm -f conftest.a && ${AR:-false} cr conftest.a \
          && $objcopy_deterministic conftest.a) >/dev/null 2>&1; then
      # Accept missing reproducibility if unsupported and warn below
      objcopy_deterministic=": unsupported objcopy -D"
      res=no
    fi
    rm -f conftest.a
    AC_MSG_RESULT([$res])
    ;;
esac
AC_SUBST([objcopy_deterministic])
AM_CONDITIONAL(REPRODUCIBLE_BUILD_STATIC_LIB, [test "$objcopy_deterministic" != "false"])

initddir=
AC_ARG_WITH(initscriptdir,
  [AS_HELP_STRING([--with-initscriptdir=@<:@DIR|no@:>@], [Location of init scripts [no]])],
  [ case "$withval" in
      auto|yes) AC_MSG_ERROR(['--with-initscriptdir=$withval' is no longer supported]) ;;
      no) ;;
      *) initddir="$withval" ;;
    esac
  ])
AC_SUBST(initddir)
AM_CONDITIONAL(INSTALL_INITSCRIPT, [test -n "$initddir"])

# use different init script templates for different OS
case "${host}" in
  *-*-freebsd*|*-*-midnightbsd*)
    initdfile="smartd.freebsd.initd"
    ;;
  *-apple-darwin*)
    initdfile="com.smartmontools.smartd.plist"
    ;;
  *-*-cygwin*)
    initdfile="smartd.cygwin.initd"
    ;;
  *)
    initdfile="smartd.initd"
    ;;
esac
AC_SUBST(initdfile)

examplescriptsdir='${docdir}/examplescripts'
# TODO: Remove after smartmontools 8.1
AC_ARG_WITH(exampledir, [AS_HELP_STRING([--with-exampledir=DIR], [(deprecated)])],
  [examplescriptsdir=$withval])

AC_ARG_WITH(smartd-scripts,
  [AS_HELP_STRING([--with-smartd-scripts=DIR],
    [Location of smartd example scripts [DOCDIR/examplescripts]])],
  [examplescriptsdir=$withval])
AC_SUBST([examplescriptsdir])

drivedbdir='${datadir}/${PACKAGE}'
drivedbinstdir=
AC_ARG_WITH(drivedbinstdir,
  [AS_HELP_STRING([--with-drivedbinstdir@<:@=DIR|yes|no@:>@],
    [Optional separate install location of drive database file [no] (yes=DATADIR/smartmontools)])],
  [ case "$withval" in
      no) ;;
      yes) drivedbinstdir=$drivedbdir; drivedbdir='${localstatedir}/lib/${PACKAGE}' ;;
      *)   drivedbinstdir=$withval;    drivedbdir='${localstatedir}/lib/${PACKAGE}' ;;
    esac
  ])

AC_ARG_WITH(drivedbdir,
  [AS_HELP_STRING([--with-drivedbdir@<:@=DIR|yes|no@:>@],
    [Location of drive database file [DATADIR/smartmontools or LOCALSTATEDIR/lib/smartmontools if separate INSTDIR]])],
  [case "$withval" in yes) ;; no) drivedbdir= ;; *) drivedbdir="$withval" ;; esac])
test -n "$drivedbinstdir" || drivedbinstdir=$drivedbdir
AC_SUBST(drivedbinstdir)
AC_SUBST(drivedbdir)
AM_CONDITIONAL(ENABLE_DRIVEDB, [test -n "$drivedbdir"])

AC_ARG_WITH(update-smart_drivedb,
  [AS_HELP_STRING([--with-update-smart-drivedb@<:@=yes|no@:>@],
    [Install update-smart-drivedb script [yes]])],
  [ case "$withval" in
      yes|no) ;;
      *) AC_MSG_ERROR([--with-update-smart-drivedb=$withval is no longer supported]) ;;
    esac
  ],
  [with_update_smart_drivedb=yes])
test -n "$drivedbdir" || with_update_smart_drivedb=no
AC_SUBST(with_update_smart_drivedb)
AM_CONDITIONAL(ENABLE_UPDATE_SMART_DRIVEDB, [test "$with_update_smart_drivedb" = "yes"])

gnupg="gpg"
# Also check for '--with-gnupg[=yes]' because 'yes' is a valid command with infinite output
AC_ARG_WITH(gnupg,
  [AS_HELP_STRING([--with-gnupg@<:@=FILE|yes|no@:>@], [GnuPG used to verify drivedb.h [gpg]])],
  [case "$withval" in yes) ;; no) gnupg= ;; *) gnupg="$withval" ;; esac], [])
AC_SUBST(gnupg)

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

AC_ARG_WITH(scriptpath,
  [AS_HELP_STRING([--with-scriptpath=@<:@PATH|no@:>@],
    [PATH variable set within scripts [/usr/local/bin:/usr/bin:/bin]])],
  [scriptpath=; test "$withval" != "no" && scriptpath="$withval"],
  [scriptpath="/usr/local/bin:/usr/bin:/bin"])
AC_SUBST(scriptpath)
AM_CONDITIONAL(ENABLE_SCRIPTPATH, [test -n "$scriptpath"])

savestates=
AC_ARG_WITH(savestates,
  [AS_HELP_STRING([--with-savestates@<:@=PREFIX|yes|no@:>@],
    [Enable default smartd state files [no] (yes=LOCALSTATEDIR/lib/smartmontools/smartd.)])],
  [case "$withval" in yes) savestates='${localstatedir}/lib/${PACKAGE}/smartd.' ;;
                      no) ;; *) savestates="$withval" ;; esac])
savestatesdir="${savestates%/*}"
AC_SUBST(savestates)
AC_SUBST(savestatesdir)
AM_CONDITIONAL(ENABLE_SAVESTATES, [test -n "$savestates"])

attributelog=
AC_ARG_WITH(attributelog,
  [AS_HELP_STRING([--with-attributelog@<:@=PREFIX|yes|no@:>@],
    [Enable default smartd attribute log files [no] (yes=LOCALSTATEDIR/lib/smartmontools/attrlog.)])],
  [case "$withval" in yes) attributelog='${localstatedir}/lib/${PACKAGE}/attrlog.' ;;
                      no) ;; *) attributelog="$withval" ;; esac])
attributelogdir="${attributelog%/*}"
AC_SUBST(attributelog)
AC_SUBST(attributelogdir)
AM_CONDITIONAL(ENABLE_ATTRIBUTELOG, [test -n "$attributelog"])

AC_ARG_ENABLE(sample,
  [AS_HELP_STRING([--enable-sample], [Enables appending .sample to the installed smartd rc script and configuration file])],
  [smartd_suffix=; test "$enableval" = "yes" && smartd_suffix=".sample"],
  [smartd_suffix=;])
AC_SUBST(smartd_suffix)

AC_ARG_ENABLE([scsi-cdb-check],
  [AS_HELP_STRING([--enable-scsi-cdb-check], [do sanity check on each SCSI cdb])],
  [ if test "$enableval" = "yes"; then
      AC_DEFINE(SCSI_CDB_CHECK, 1, [Define to 1 to enable check on each SCSI cdb])
    fi
  ],[])

AC_ARG_ENABLE([fast-lebe],
  [AS_HELP_STRING([--disable-fast-lebe], [use generic little-endian/big-endian code instead])],
  [ if test "$enableval" = "no"; then
      AC_DEFINE(IGNORE_FAST_LEBE, 1, [Define to 1 to use generic LE/BE code instead])
    fi
  ],[])

AC_ARG_WITH(os-deps,
  [AS_HELP_STRING([--with-os-deps='os_module.lo ...'], [Specify OS dependent module(s) [guessed]])],
  [ for x in $with_os_deps; do
      case $x in
        *.lo) ;;
        *) AC_MSG_ERROR(['--with-os-deps' requires '*.lo' arguments]) ;;
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

AC_ARG_WITH(libcap-ng,
  [AS_HELP_STRING([--with-libcap-ng@<:@=auto|yes|no@:>@], [Add Libcap-ng support to smartd [auto]])],
  [], [with_libcap_ng=auto])

use_libcap_ng=no
case "$with_libcap_ng:$host_os" in
  auto:linux*|yes:*)
    AC_CHECK_HEADERS([cap-ng.h], [AC_CHECK_LIB([cap-ng], [capng_clear],
      [AC_DEFINE(HAVE_LIBCAP_NG, 1,
        [Define to 1 if you have the `cap-ng' library (-lcap-ng).]) dnl `vim syntax
       CAPNG_LDADD="-lcap-ng"; use_libcap_ng=yes],
      [test "$with_libcap_ng" != "yes" || AC_MSG_ERROR([libcap-ng headers found but library is missing])])],
      [test "$with_libcap_ng" != "yes" || AC_MSG_ERROR([Missing libcap-ng header files])])
    ;;
esac
AC_SUBST(CAPNG_LDADD)

AC_ARG_WITH(libsystemd,
  [AS_HELP_STRING([--with-libsystemd@<:@=auto|yes|no@:>@],
    [Add systemd 'Type=notify' support to smartd [auto]])],
  [], [with_libsystemd=auto])

use_libsystemd=no
case "$with_libsystemd:$host_os" in
  auto:linux*|yes:*)
    AC_CHECK_HEADERS([systemd/sd-daemon.h], [AC_CHECK_LIB([systemd], [sd_notify],
      [AC_DEFINE(HAVE_LIBSYSTEMD, 1,
        [Define to 1 if you have the `systemd' library (-lsystemd).]) dnl `vim syntax
       SYSTEMD_LDADD="-lsystemd"; use_libsystemd=yes],
      [test "$with_libsystemd" != "yes" || AC_MSG_ERROR([libsystemd headers found but library is missing])])],
      [test "$with_libsystemd" != "yes" || AC_MSG_ERROR([Missing libsystemd header files])])
    ;;
esac
AC_SUBST(SYSTEMD_LDADD)

AC_ARG_WITH(systemdsystemunitdir,
  [AS_HELP_STRING([--with-systemdsystemunitdir@<:@=DIR|auto|yes|no@:>@], [Location of systemd service files [auto]])],
  [], [with_systemdsystemunitdir=auto])

systemdsystemunitdir=
case "$with_systemdsystemunitdir:$use_libsystemd" in
  auto:yes|yes:yes)
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
  auto:*|no:*) ;;
  *:yes) systemdsystemunitdir="$with_systemdsystemunitdir" ;;
  *) AC_MSG_ERROR(['--with-systemdsystemunitdir=$with_systemdsystemunitdir' now requires '--with-libsystemd']) ;;
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

# TODO: Remove when NVMe support is no longer EXPERIMENTAL
AC_ARG_WITH(nvme-devicescan,
  [AS_HELP_STRING([--with-nvme-devicescan@<:@=yes|no@:>@],
    [Include NVMe devices in smartctl --scan and smartd DEVICESCAN [yes]])])

# TODO: Remove after smartmontools 7.5
AC_ARG_WITH(signal-func,
  [AS_HELP_STRING([--with-signal-func], [(removed)])],
  [AC_MSG_ERROR([
The option '--with-signal-func' is no longer supported.  If this option is
still needed, please inform $PACKAGE_BUGREPORT.
])])

AC_ARG_WITH(cxx11-option,
  [AS_HELP_STRING([--with-cxx11-option=@<:@OPTION|auto|no@:>@],
    [Compiler option to enable C++11 support, 'no' to skip check [auto]])],
  [], [with_cxx11_option=auto])

# check_cxx_options "MESSAGE" MINVER MAXVER "OPTION..."
check_cxx_options()
{
  AC_MSG_CHECKING([for $CXX option to accept $1])
  res=unknown
  save_CXXFLAGS=$CXXFLAGS
  for option in "" $4; do
    CXXFLAGS="$save_CXXFLAGS $option"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #if __cplusplus < $2 || $3 <= __cplusplus
        #error false
      #endif]])], [res=$option])
    test "$res" = "unknown" || break
  done
  CXXFLAGS=$save_CXXFLAGS
  AC_MSG_RESULT([${res:-none needed}])
}

cxx_missing=
case "$with_cxx11_option" in
  no) ;;
  auto)
    check_cxx_options "C++17" 201703 999999 "-std=gnu++17 -std=gnu++1z -std=c++17 -std=c++1z"
    cxx17_option=$res;
    check_cxx_options "C++14 but not C++17" 201402 201703 "-std=gnu++14 -std=gnu++1y -std=c++14 -std=c++1y"
    cxx14_option=$res;
    case "$cxx17_option:$cxx14_option" in
      unknown:unknown) cxx_missing=14 ;;
      unknown:*)       cxx_missing=17 ;;
    esac
    check_cxx_options "C++11 but not C++14" 201103 201402 "-std=gnu++11 -std=gnu++0x -std=c++11 -std=c++0x"
    test "$res" != "unknown" || res=$cxx14_option
    test "$res" != "unknown" || res=$cxx17_option
    test "$res" != "unknown" || AC_MSG_ERROR([
This script was unable to determine a compiler option to accept C++11.
Use option '--with-cxx11-option=OPTION' to specify a compiler option.
Use option '--without-cxx11-option' to try anyway without this check.
In both cases, please send info about compiler and platform to
$PACKAGE_BUGREPORT - Thanks!])
    CXXFLAGS="$CXXFLAGS${res:+ }$res"
    ;;
  *)
    AC_MSG_CHECKING([whether $CXX $with_cxx11_option accepts C++11])
    CXXFLAGS="$CXXFLAGS${with_cxx11_option:+ }$with_cxx11_option"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #if __cplusplus < 201103
        #error false
      #endif]])], [res=yes], [res=no])
    AC_MSG_RESULT([$res])
    test "$res" = "yes" || AC_MSG_ERROR([$CXX $with_cxx11_option does not accept C++11])
    ;;
esac

case "$host_os: $CPPFLAGS $CXXFLAGS" in
  mingw*:*\ -U__USE_MINGW_ANSI_STDIO\ )
    ;;
  mingw*:*)
    # MinGW 4.6.* did not properly define PRI?64 if __USE_MINGW_ANSI_STDIO is set.
    # MinGW 4.9.* set __USE_MINGW_ANSI_STDIO in first C++ include which may be too late.
    # MinGW 8.0.0 and later may automatically set __USE_MINGW_ANSI_STDIO in certain
    # configurations.  __USE_MINGW_ANSI_STDIO is not needed if the UCRT is used.
    # Make sure __USE_MINGW_ANSI_STDIO or _UCRT is set and fail if format check fails.
    AC_MSG_CHECKING([whether __USE_MINGW_ANSI_STDIO=1 is predefined])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #include <stdio.h>
        #if __USE_MINGW_ANSI_STDIO != 1
          #error fail
        #endif]])],
      [res=yes], [res=no])
    AC_MSG_RESULT([$res])
    AC_MSG_CHECKING([whether _UCRT is predefined])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #include <stdio.h>
        #ifndef _UCRT
          #error fail
        #endif]])],
      [res2=yes], [res2=no])
    AC_MSG_RESULT([$res2])
    if test "$res:$res2" = "no:no"; then
      CPPFLAGS="${CPPFLAGS}${CPPFLAGS:+ }-D__USE_MINGW_ANSI_STDIO"
    fi
    AC_MSG_CHECKING([whether $CXX supports __USE_MINGW_ANSI_STDIO or UCRT])
    save_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS -Wformat -Werror"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #define __STDC_FORMAT_MACROS 1
        #include <inttypes.h>
        #include <stdio.h>
        void f(char * p, size_t s) {
          snprintf(p, s, "%lld %" PRId64, (long long)s, (int64_t)s);
        }]])],
      [res=yes], [res=no])
    AC_MSG_RESULT([$res])
    if test "$res" != "yes"; then
      AC_MSG_ERROR([
This version of $CXX does not support __USE_MINGW_ANSI_STDIO or UCRT.
Use option 'CPPFLAGS=-U__USE_MINGW_ANSI_STDIO' to skip this check.
Please send info about your system to $PACKAGE_BUGREPORT.
])
    fi
    CXXFLAGS=$save_CXXFLAGS
    ;;
esac

# TODO: Remove after smartmontools 8.1
AC_ARG_WITH(mingw-aslr,
  [AS_HELP_STRING([--with-mingw-aslr], [(removed)])],
  [AC_MSG_ERROR([
The option '--with-mingw-aslr' is no longer supported.  Recent versions
of the MinGW-w64 toolchain usually enable ASLR by default.
])])

os_win32_manifest=
case "$host" in
  *-*-mingw*)
    # Newer MinGW may add a default manifest
    AC_MSG_CHECKING([whether $CXX adds an application manifest])
    cc_adds_manifest=no
    AC_LINK_IFELSE([AC_LANG_PROGRAM()], [
        if "$WINDRES" -O rc conftest.exe 2>/dev/null | grep '^1.*RT_MANIFEST' >/dev/null 2>&1; then
          cc_adds_manifest=incomplete
          # Manifest must provide a Win 10 compatibility ID
          if "$WINDRES" -O rc conftest.exe 2>/dev/null | grep '{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}' >/dev/null 2>&1; then
            cc_adds_manifest=yes
          fi
        fi],
      [AC_MSG_ERROR([test compile failed])])
    AC_MSG_RESULT([$cc_adds_manifest])
    test "$cc_adds_manifest" = "yes" || os_win32_manifest='os_win32/default.manifest'
    ;;
esac

AC_ARG_WITH(cxx11-regex,
  [AS_HELP_STRING([--with-cxx11-regex@<:@=yes|no@:>@],
    [Use C++11 std::regex instead of POSIX regex(3) [no]])])

need_regex=no
if test "$with_cxx11_regex" = "yes"; then
  AC_DEFINE(WITH_CXX11_REGEX, 1, [Define to 1 to use C++11 std::regex instead of POSIX regex(3)])
else
  AC_CHECK_FUNCS([regcomp], [], [need_regex=yes])
fi
AM_CONDITIONAL(NEED_REGEX, [test "$need_regex" = "yes"])

# Set platform-specific modules and symbols
os_libs=
os_dltools='curl wget lynx'
os_mailer=mail
os_hostname="'hostname' 'uname -n'"
os_dnsdomainname=
os_nisdomainname="'domainname'"
os_posix=yes
os_darwin=no
os_solaris=no
os_win32=no
os_win32_mingw=no
os_win64=no
os_man_filter=
os_nvme_devicescan=
os_nvme_devicescan_changed=no
case "${host}" in
  *-*-linux*)
    os_deps='os_linux.lo cciss.lo dev_areca.lo'
    os_dnsdomainname="'dnsdomainname' 'hostname -d'"
    os_nisdomainname="'nisdomainname' 'hostname -y' 'domainname'"
    os_man_filter=Linux
    os_nvme_devicescan=yes
    ;;
  *-*-freebsd*|*-*-dragonfly*|*-*-kfreebsd*-gnu*|*-*-midnightbsd*)
    os_deps='os_freebsd.lo cciss.lo dev_areca.lo'
    os_libs='-lcam -lsbuf'
    os_dltools='fetch curl wget lynx'
    AC_CHECK_LIB(usb, libusb20_dev_get_device_desc)
    os_man_filter=FreeBSD
    os_nvme_devicescan=yes
    ;;
  *-*-solaris*)
    os_deps='os_solaris.lo dev_legacy.lo' # TODO: migrate to new interface
    os_mailer='mailx'
    os_solaris=yes
    os_man_filter=Solaris
    ;;
  *-*-netbsd*)
    os_deps='os_netbsd.lo'
    os_libs='-lutil'
    os_man_filter=NetBSD
    os_nvme_devicescan=yes
    # TODO: Remove after smartmontools 7.5
    os_nvme_devicescan_changed=yes
    ;;
  *-*-openbsd*)
    os_deps='os_openbsd.lo'
    os_libs='-lutil'
    os_dltools='curl wget lynx ftp'
    os_man_filter=OpenBSD
    ;;
  *-*-cygwin*)
    os_deps='os_win32.lo dev_areca.lo'
    os_libs='-lole32 -loleaut32'
    os_mailer='email'
    os_hostname="'hostname' 'uname -n' 'echo "'"${HOSTNAME?unset}"'"'"
    os_dnsdomainname="'dnsdomainname' 'hostname -d' 'echo "'"${USERDNSDOMAIN?unset}"'"'"
    os_nisdomainname=
    os_win32=yes
    os_man_filter=Cygwin
    os_nvme_devicescan=yes
    ;;
  x86_64-*-mingw*)
    os_deps='os_win32.lo dev_areca.lo'
    # Some versions of the MinGW-w64 import lib for kernel32.dll include some symbols
    # duplicated from advapi32.dll.  Older versions of windows provide these symbols
    # only in advapi32.dll.  Ensure that the advapi32 lib is linked first.
    os_libs='-lole32 -loleaut32 -ladvapi32'
    os_posix=no
    os_win32=yes
    os_win32_mingw=yes
    os_win64=yes
    os_man_filter=Windows
    os_nvme_devicescan=yes
    ;;
  *-*-mingw*)
    os_deps='os_win32.lo dev_areca.lo'
    os_libs='-lole32 -loleaut32 -ladvapi32'
    os_posix=no
    os_win32=yes
    os_win32_mingw=yes
    os_man_filter=Windows
    os_nvme_devicescan=yes
    ;;
  *-*-darwin*)
    os_deps='os_darwin.lo'
    os_libs='-framework CoreFoundation -framework IOKit'
    os_darwin=yes
    os_man_filter=Darwin
    os_nvme_devicescan=yes
    ;;
  *-*-nto-qnx*)
    os_deps='os_qnxnto.lo dev_legacy.lo' # TODO: migrate to new interface
    ;;
  *-*-os2-*)
    os_deps='os_os2.lo dev_legacy.lo' # TODO: migrate to new interface
    os_posix=no
    ;;
  *)
    os_deps='os_generic.lo'
    ;;
esac

# Replace if '--with-os-deps' was specified
test -z "$with_os_deps" || os_deps="$with_os_deps"

# Define 'OLD_INTERFACE' if 'dev_legacy.cpp' is still required
case " $os_deps " in *\ dev_legacy.lo\ *)
  AC_DEFINE(OLD_INTERFACE, 1, [Define to 1 if os_*.cpp still uses the old interface]) ;;
esac

AC_MSG_CHECKING([whether the OS provides a POSIX API])
if test "$os_posix" = "yes"; then
  AC_DEFINE(HAVE_POSIX_API, 1, [Define to 1 if the OS provides a POSIX API])
fi
AC_MSG_RESULT([$os_posix])

# TODO: Remove when NVMe support is no longer EXPERIMENTAL
case "$os_nvme_devicescan:${with_nvme_devicescan+set}" in
  no:|yes:)
    AC_MSG_CHECKING([whether NVMe device scanning could be safely enabled])
    with_nvme_devicescan=$os_nvme_devicescan
    AC_MSG_RESULT([$os_nvme_devicescan])
    os_nvme_devicescan=used ;;
esac
AC_SUBST(with_nvme_devicescan)
if test "$with_nvme_devicescan" = "yes"; then
  AC_DEFINE(WITH_NVME_DEVICESCAN, 1, [Define to 1 to include NVMe devices in smartd DEVICESCAN.])
fi

AC_SUBST([os_deps])
AC_SUBST([os_libs])
AC_SUBST([os_dltools])
AC_SUBST([os_mailer])
AC_SUBST([os_hostname])
AC_SUBST([os_dnsdomainname])
AC_SUBST([os_nisdomainname])
AC_SUBST([os_man_filter])
AC_SUBST([os_win32_manifest])

# drivedb.h update branch
DRIVEDB_BRANCH=$smartmontools_drivedb_version
AC_SUBST([DRIVEDB_BRANCH])

# Enable platform-specific makefile sections
AM_CONDITIONAL(OS_POSIX, [test "$os_posix" = "yes"])
AM_CONDITIONAL(OS_DARWIN, [test "$os_darwin" = "yes"])
AM_CONDITIONAL(OS_SOLARIS, [test "$os_solaris" = "yes"])
AM_CONDITIONAL(OS_WIN32, [test "$os_win32" = "yes"])
AM_CONDITIONAL(OS_WIN32_MINGW, [test "$os_win32_mingw" = "yes"])
AM_CONDITIONAL(OS_WIN64, [test "$os_win64" = "yes"])

if test "$GXX" = "yes"; then
  orig_CXXFLAGS=$CXXFLAGS
  # Add -Wall and -W[extra] if its not already specified
  case " $CXXFLAGS " in
    *\ -Wall\ *) ;;
    *) CXXFLAGS="$CXXFLAGS -Wall" ;;
  esac
  case " $CXXFLAGS " in
    *\ -W\ *|*\ -Wextra\ *) ;;
    *) CXXFLAGS="$CXXFLAGS -W" ;;
  esac
  # Add -Wformat=2, -Wsuggest-override, -Werror=return-type (G++ >= 8.0
  # assumes that control never reaches the end of a non-void function),
  # if supported and no -W option was set in configure cmdline
  # (TODO: -Wformat-signedness).
  case " $orig_CXXFLAGS" in
    *\ -W[!l]*) ;;
    *)
      for option in "-Wformat=2" "-Wsuggest-override" "-Werror=return-type"; do
        AC_MSG_CHECKING([whether $CXX supports $option])
        save_CXXFLAGS=$CXXFLAGS
        CXXFLAGS="$CXXFLAGS $option"
        AC_COMPILE_IFELSE([AC_LANG_SOURCE()],
          [res=yes], [res=no; CXXFLAGS=$save_CXXFLAGS])
        AC_MSG_RESULT([$res])
      done ;;
  esac
  # Add -fstack-protector-strong (GCC 4.9) if supported and no -f option was
  # set in configure cmdline
  case " $orig_CXXFLAGS" in
    *\ -f*) ;;
    *)
      option="-fstack-protector-strong"
      AC_MSG_CHECKING([whether $CXX supports $option])
      save_CXXFLAGS=$CXXFLAGS
      CXXFLAGS="$CXXFLAGS $option"
      # Link to check that '__stack_chk_*()' functions are available.
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[int i;]],
        [[volatile char buf[10]; buf[i] = 0;]])],
        [res=yes], [res=no; CXXFLAGS=$save_CXXFLAGS])
      AC_MSG_RESULT([$res]) ;;
  esac
else
  # We are NOT using gcc, so enable host-specific compiler flags
  case "${host}" in
    sparc*-*-solaris*)
      # Tell the Solaris/SPARC C++ compiler about packed ATA structures
      case " $CXXFLAGS" in
        *\ -xmemalign*) ;;
        *) CXXFLAGS="-xmemalign=1i $CXXFLAGS" ;;
      esac ;;
  esac
  case "${host}" in
    *-*-solaris*)
      # Turn on optimization if user has not explicitly set its value
      case " $CXXFLAGS" in
        *\ -xO*) ;;
        *) CXXFLAGS="-xO2 $CXXFLAGS" ;;
      esac
      # Suppress trivial warnings
      case " $CXXFLAGS" in
        *\ -erroff*) ;;
        *) CXXFLAGS="-erroff=%none,wbadinitl,wbadasgl,badargtypel2w,badargtype2w $CXXFLAGS" ;;
      esac ;;
  esac
fi

AC_MSG_CHECKING([whether _FORTIFY_SOURCE is predefined])
# Use COMPILE check to detect compiler presets and C*FLAGS.
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
  #ifndef _FORTIFY_SOURCE
  #error false
  #endif]])], [res=yes], [res=no])
AC_MSG_RESULT([$res])
if test "$res" = "no"; then
  # Heuristic to check for _FORTIFY_SOURCE=[23] support.
  # Done after check for -fstack-protector because this may add an additional lib.
  # Note: Even if _FORTIFY_SOURCE is present in the include files, it may not be
  # detected here for some reason (disabled for C++, compiler optimization disabled,
  # LDFLAGS include '-s', ...)
  save_CPPFLAGS=$CPPFLAGS
  for level in 3 2; do
    AC_MSG_CHECKING([whether $CXX supports _FORTIFY_SOURCE=$level])
    CPPFLAGS="${CPPFLAGS}${CPPFLAGS:+ }-D_FORTIFY_SOURCE=$level"
    # Use LINK instead of COMPILE to make sure that the required lib is also present.
    # Check for NM is in 'libtool.m4'.
    AC_LINK_IFELSE([AC_LANG_SOURCE([[
      #include <stdlib.h>
      #include <string.h>
      char buf[42], *p;
      int main(int argc, char **argv) {
      #if _FORTIFY_SOURCE > 2
        p = (char *)malloc(argc * 24);
      #else
        p = buf;
      #endif
        if (argc >= 2) strcpy(p, argv[1]);
        return p[0];
      }]])],
      [res=yes; ${NM:-false} conftest$EXEEXT 2>/dev/null | grep '_strcpy_chk' >/dev/null 2>&1 || res=no],
      [res=unknown])
    AC_MSG_RESULT([$res])
    test "$res" != "yes" || break
    CPPFLAGS=$save_CPPFLAGS
  done
fi

msg="for tar"
opts=
if test -n "$SOURCE_DATE_EPOCH"; then
  msg="for GNU tar supporting reproducible tarballs"
  opts=" --sort=name --mtime=@$SOURCE_DATE_EPOCH --clamp-mtime --owner=root:0 --group=root:0"
fi
AC_MSG_CHECKING([$msg])
dist_tar=false
res=no
rm -rf conftest.dir
mkdir conftest.dir
# Don't use 'am__tar' from automake because it checks 'tar' before 'gtar'
for t in "gnutar --format=ustar" "gtar --format=ustar" "tar --format=ustar" "tar"; do
  cmd="${t}${opts} -chf -"
  if $cmd conftest.dir >/dev/null 2>&1; then
    dist_tar=$cmd
    res=${dist_tar%% *}
    break
  fi
done
rmdir conftest.dir
AC_MSG_RESULT([$res])
AC_SUBST([dist_tar])

AC_DEFINE_UNQUOTED(SMARTMONTOOLS_BUILD_HOST,     "${host}",                       [smartmontools Build Host])

AC_SUBST(CXXFLAGS)

# Pass some build flags also to examples/Makefile

examples_cppflags=$(echo " $CPPFLAGS " | sed -n 's/^.* \(-D_FORTIFY_SOURCE=[[0-3]]\) .*$/\1/p')
AC_SUBST([examples_cppflags])

examples_cxxflags=
if test "$GXX" = "yes"; then
  examples_cxxflags="-O2 -Wall -W"
fi
case " $CXXFLAGS " in *\ -fstack-protector-strong\ *)
  # This option may silently add '-lssp' which is then required to link the lib
  examples_cxxflags="${examples_cxxflags}${examples_cxxflags:+ }-fstack-protector-strong" ;;
esac
AC_SUBST([examples_cxxflags])

examples_ldflags=
case " $CXXFLAGS $LDFLAGS " in *\ --static\ *)
  examples_ldflags="-static" ;;
esac
AC_SUBST([examples_ldflags])

examples_libs="${os_libs}${os_libs:+ }${LIBS}"
AC_SUBST([examples_libs])

AC_CONFIG_FILES([
  Makefile
  include/Makefile
  lib/Makefile
  lib/examples/Makefile
  src/Makefile
])
AC_OUTPUT
AC_PROG_MAKE_SET

# Note: Use `...` here as some shells do not properly parse '$(... case $x in X) ...)'
info=`
  echo "-----------------------------------------------------------------------------"
  echo "${PACKAGE}-${VERSION} configuration:"
  echo "host operating system:  $host"
  echo "C++ compiler:           $CXX"
  echo "C compiler:             $CC"
  echo "preprocessor flags:     $CPPFLAGS"
  echo "C++ compiler flags:     $CXXFLAGS"
  echo "C compiler flags:       $CFLAGS"
  echo "linker flags:           $LDFLAGS"
  echo "build shared library:   $enable_shared"
  echo "build static library:   $enable_static"
  echo "OS specific modules:    $os_deps"
  echo "OS specific libraries:  $os_libs $LIBS"
  echo "lib examples flags:     $examples_cppflags $examples_cxxflags $examples_ldflags"
  echo "Build information:      $BUILD_INFO"
  if test -n "$SOURCE_DATE_EPOCH"; then
    echo "Reproducible build:     SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH [[$source_date_iso]]"
  else
    echo "Reproducible build:     [[disabled]]"
  fi

  case "$host_os" in
    mingw*)
      echo "application manifest:   ${os_win32_manifest:-[[built-in]]}"
      echo "resource compiler:      $WINDRES"
      echo "message compiler:       ${WINDMC:-[[missing]]}"
      echo "NSIS compiler:          ${MAKENSIS:-[[missing]]}"
      if test -n "$drivedbdir"; then
        echo "drive database file:    EXEDIR/drivedb.h"
        if test -n "$MAKENSIS"; then
          echo "database update script: EXEDIR/update-smart-drivedb.ps1"
          echo "database update branch: drivedb/$DRIVEDB_BRANCH"
        fi
      else
        echo "drive database file:    [[disabled]]"
      fi
      if test -n "$savestates"; then
        echo "smartd save files:      \`eval eval eval echo $savestates\`MODEL-SERIAL.TYPE.state"
      fi
      if test -n "$attributelog"; then
        echo "smartd attribute logs:  \`eval eval eval echo $attributelog\`MODEL-SERIAL.TYPE.csv"
      fi
      echo "NVMe DEVICESCAN:        ${with_nvme_devicescan-no}"
      ;;

    *)
      echo "binary dir:             \`eval eval eval echo $sbindir\`"
      echo "man page dir:           \`eval eval eval echo $mandir\`"
      echo "doc file dir:           \`eval eval eval echo $docdir\`"
      if test "$with_devel" = "yes"; then
        echo "header dir:             \`eval eval eval echo $includedir/smartmon\`"
      else
        echo "header dir:             [[disabled]]"
      fi
      if test "$with_devel" = "yes" || test "$enable_shared" = "yes"; then
        echo "library dir:            \`eval eval eval echo $libdir\`"
      else
        echo "library dir:            [[disabled]]"
      fi
      echo "lib examples src dir:   \`eval eval eval echo ${develsrcdir:-[[disabled]]}\`"
      echo "lib examples bin dir:   \`eval eval eval echo ${develbindir:-[[disabled]]}\`"
      echo "smartd examples dir:    \`eval eval eval echo $examplescriptsdir\`"
      if test -n "$drivedbdir"; then
        if test "$drivedbinstdir" != "$drivedbdir"; then
          echo "installed drivedb file: \`eval eval eval echo $drivedbinstdir\`/drivedb.h"
        fi
        echo "drive database file:    \`eval eval eval echo $drivedbdir\`/drivedb.h"
        if test "$with_update_smart_drivedb" = "yes"; then
          echo "database update script: \`eval eval eval echo $sbindir\`/update-smart-drivedb"
          echo "database update branch: drivedb/$DRIVEDB_BRANCH"
          echo "download tools:         \`eval eval eval echo $os_dltools\`"
          if test -n "$gnupg"; then
            echo "GnuPG for verification: \`eval eval eval echo $gnupg\`"
          else
            echo "GnuPG for verification: [[disabled]]"
          fi
        else
          echo "database update script: [[disabled]]"
        fi
      else
        echo "drive database file:    [[disabled]]"
      fi
      echo "local drive database:   \`eval eval eval echo $sysconfdir\`/smart_drivedb.h"
      echo "smartd config file:     \`eval eval eval echo $sysconfdir\`/smartd.conf${smartd_suffix}"
      echo "smartd warning script:  \`eval eval eval echo $smartdscriptdir\`/smartd_warning.sh"
      echo "smartd plugin dir:      \`eval eval eval echo ${smartdplugindir:-[[disabled]]}\`"
      if test -n "$scriptpath"; then
        echo "PATH within scripts:    \`eval eval eval echo $scriptpath\`"
      else
        echo "PATH within scripts:    [[inherited]]"
      fi
      if test -n "$initddir"; then
        echo "smartd initd script:    \`eval eval eval echo $initddir\`/smartd"
      elif test -z "$systemdsystemunitdir"; then
        echo "smartd initd script:    [[disabled]]"
      fi
      if test -n "$systemdsystemunitdir"; then
        echo "smartd service file:    \`eval eval eval echo $systemdsystemunitdir\`/smartd.service"
        if test -n "$systemdenvfile"; then
          echo "smartd environ file:    \`eval eval eval echo $systemdenvfile\`"
        else
          echo "smartd environ file:    [[disabled]]"
        fi
      fi
      if test -n "$savestates"; then
        echo "smartd save files:      \`eval eval eval echo $savestates\`MODEL-SERIAL.TYPE.state"
      else
        echo "smartd save files:      [[disabled]]"
      fi
      if test -n "$attributelog"; then
        echo "smartd attribute logs:  \`eval eval eval echo $attributelog\`MODEL-SERIAL.TYPE.csv"
      else
        echo "smartd attribute logs:  [[disabled]]"
      fi
      case "$host_os" in
        linux*)
          echo "SELinux support:        ${with_selinux-no}"
          echo "libcap-ng support:      $use_libcap_ng"
          echo "systemd notify support: $use_libsystemd" ;;
      esac
      echo "NVMe DEVICESCAN:        ${with_nvme_devicescan-[[not implemented]]}"
      ;;
  esac
  echo "-----------------------------------------------------------------------------"
`

AC_MSG_NOTICE([
$info
])

# TODO: Remove when NVMe support is no longer EXPERIMENTAL
case "$os_nvme_devicescan:$with_nvme_devicescan:$os_nvme_devicescan_changed" in
  used:yes:yes) AC_MSG_WARN([
The default for the inclusion of NVME devices in smartd.conf
'DEVICESCAN' and 'smartctl --scan' has been changed to 'yes' on
this platform.
Use option '--with-nvme-devicescan' to suppress this warning.
])  ;;
  used:no:*) AC_MSG_WARN([
This version of smartmontools provides NVMe support which is still
EXPERIMENTAL.  NVMe devices are not yet included in smartd.conf
'DEVICESCAN' and 'smartctl --scan' unless '-d nvme' is specified.
Use option '--with-nvme-devicescan' to include NVMe devices.
Use option '--without-nvme-devicescan' to suppress this warning.
])  ;;
  yes:no:*) AC_MSG_WARN([
The default for the inclusion of NVME devices in smartd.conf
'DEVICESCAN' and 'smartctl --scan' is 'yes' on this platform.
If option '--without-nvme-devicescan' is still needed, please inform
$PACKAGE_BUGREPORT - Thanks!
])  ;;
esac

case "$host_os:$with_libsystemd:$use_libsystemd:$PKG_CONFIG" in
  linux*:auto:no:?*)
    if $PKG_CONFIG systemd >/dev/null 2>&1; then
      AC_MSG_WARN([
systemd(1) is used on this system but smartd systemd notify support will
not be available because a required library is missing.
Use option '--without-libsystemd' to suppress this warning.
])
    fi ;;
esac

case "$host_os:$WINDMC" in
  mingw*:) AC_MSG_WARN([
The Windows message compiler (windmc) is missing.  The build will work,
but smartd will not contain the MESSAGETABLE required for proper display
of smartd log messages in the Windows event log.
]) ;;
esac

test -z "$cxx_missing" || AC_MSG_WARN([
This version of smartmontools does not use C++14 or C++17 enhancements,
but future versions possibly will.
This script was unable to determine a compiler option to enable C++$cxx_missing.
Please send info about compiler and platform to
$PACKAGE_BUGREPORT - Thanks!
])

case $objcopy_deterministic in
  :*) AC_MSG_WARN([
The installed static library will not be reproducible because 'objcopy -D'
is not available.
]) ;;
esac

case "$dist_tar:$SOURCE_DATE_EPOCH" in
  false:?*) AC_MSG_WARN([
Reproducible tarballs require GNU tar which is not available.
'make dist' and 'make bin-dist' will fail.
]) ;;
  false:) AC_MSG_WARN([
No compatible tar found.  'make bin-dist' will fail.
]) ;;
esac

# TODO: Remove after smartmontools 8.1
test -z "$with_exampledir" || AC_MSG_WARN([
Option '--with-exampledir' is deprecated, use '--with-smartd-scripts' instead.
])
