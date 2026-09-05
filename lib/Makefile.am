## Process this file with automake to produce Makefile.in
#
# lib/Makefile.am
#

if REPRODUCIBLE_BUILD
@dot_export@ SOURCE_DATE_EPOCH
if OS_DARWIN
ZERO_AR_DATE = 1
@dot_export@ ZERO_AR_DATE
endif
endif

# TODO: Remove SMARTMONTOOLS_SYSCONFDIR and SMARTMONTOOLS_DRIVEDBDIR dependency
AM_CPPFLAGS = \
        -DBUILD_INFO='$(BUILD_INFO)' \
        -DSMARTMONTOOLS_SYSCONFDIR='"$(sysconfdir)"' \
         -I ../include -I$(top_srcdir)/include

# Keep Objective-C++ Darwin code under the same language and warning policy as
# the rest of the C++ backend.  The .mm source is not built on other systems.
AM_OBJCXXFLAGS = $(CXXFLAGS) -fblocks

# Automake does not add a libtool tag to its Objective-C++ compile rule.
# Select a supported tag explicitly; compiler-name inference is unreliable
# for cross compilers and some libtool releases do not support OBJCXX.
LTOBJCXXCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=$(LIBTOOL_OBJCXX_TAG) \
	$(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=compile $(OBJCXX) \
	$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
	$(AM_OBJCXXFLAGS) $(OBJCXXFLAGS)

if ENABLE_DRIVEDB
AM_CPPFLAGS += -DSMARTMONTOOLS_DRIVEDBDIR='"$(drivedbdir)"'
endif

if OS_WIN32
AM_CPPFLAGS += -I$(top_srcdir)/include/smartmon/os_win32
endif
if NEED_REGEX
AM_CPPFLAGS += -I$(top_srcdir)/include/smartmon/regex -D_REGEX_STANDALONE
endif

# Suppress 'echo' if './configure --enable-silent-rules' or 'make V=0' is used.
# Variables AM_V_* are provided by automake >= 1.13.
ECHO_V = ! $(AM_V_P) || echo
ECHO_V_GEN = @$(AM_V_P) || echo '  GEN      $@'; $(ECHO_V)

if INSTALL_DEVEL_LIB

lib_LTLIBRARIES = libsmartmon.la

if REPRODUCIBLE_BUILD
# Remove timestamps from possible static and import libraries because libtool
# offers no way to pass '-D' to 'ranlib'.
install-exec-hook:
	@for f in $(DESTDIR)$(libdir)/*.a; do if [ -f "$$f" ]; then \
	  echo " $(objcopy_deterministic) '$$f'"; \
	  $(objcopy_deterministic) "$$f" || exit $?; \
	fi; done
endif

else
noinst_LTLIBRARIES = libsmartmon.la
endif

libsmartmon_la_SOURCES = \
        atacmdnames.cpp \
        atacmds.cpp \
        dev_ata_cmd_set.cpp \
        dev_ata_cmd_set.h \
        dev_intelliprop.cpp \
        dev_interface.cpp \
        dev_jmb39x_raid.cpp \
        dev_parse_debug.cpp \
        dev_tunnelled.h \
        farmcmds.cpp \
        hexdump.cpp \
        knowndrives.cpp \
        nvmecmds.cpp \
        json.cpp \
        scsicmds.cpp \
        scsiata.cpp \
        scsinvme.cpp \
        utility.cpp

libsmartmon_la_LIBADD = $(os_deps)
libsmartmon_la_DEPENDENCIES = $(os_deps)

# Listing the Darwin-only .mm file in EXTRA_*_SOURCES makes Automake select
# its Objective-C++ link rule on every platform.  Keep this mixed-source
# library on the configured C++ link driver and its explicit libtool tag.
libsmartmon_la_LINK = $(CXXLINK) $(libsmartmon_la_LDFLAGS)

if OS_DARWIN
# Also record framework dependencies in the shared library.  In particular,
# IOUSBHost must remain weakly linked when running on older macOS versions.
libsmartmon_la_LIBADD += $(os_libs)

if !REALLY_CROSS_COMPILING
check_PROGRAMS = os_darwin_scan_test os_darwin_usb_test
TESTS = $(check_PROGRAMS)
os_darwin_scan_test_SOURCES = tests/os_darwin_scan_test.cpp os_darwin.cpp
os_darwin_scan_test_CPPFLAGS = $(AM_CPPFLAGS)
os_darwin_scan_test_LDADD = libsmartmon.la $(os_libs)
os_darwin_usb_test_SOURCES = tests/os_darwin_usb_test.mm
os_darwin_usb_test_LDADD = libsmartmon.la $(os_libs)
os_darwin_usb_test_LINK = $(CXXLINK) $(os_darwin_usb_test_LDFLAGS)
endif
endif

EXTRA_libsmartmon_la_SOURCES = \
        os_darwin.cpp \
        os_darwin.h \
        os_darwin_usb.h \
        os_darwin_usb.mm \
        os_linux.cpp \
        os_linux.h \
        os_freebsd.cpp \
        os_freebsd.h \
        os_netbsd.cpp \
        os_netbsd.h \
        os_openbsd.cpp \
        os_openbsd.h \
        os_os2.cpp \
        os_os2.h \
        os_qnxnto.cpp \
        os_qnxnto.h \
        os_solaris.cpp \
        os_win32.cpp \
        os_generic.cpp \
        os_generic.h \
        aacraid.h \
        cciss.cpp \
        cciss.h \
        cissio_freebsd.h \
        dev_areca.cpp \
        dev_areca.h \
        dev_legacy.cpp \
        linux_nvme_ioctl.h \
        freebsd_nvme_ioctl.h \
        netbsd_nvme_ioctl.h \
        openbsd_nvme_ioctl.h \
        megaraid.h \
        sssraid.h

if OS_WIN32
libsmartmon_la_SOURCES += \
        csmisas.h \
        os_win32/wmiquery.cpp

# Windows DLLs do not support undefined symbols
libsmartmon_la_LDFLAGS = -no-undefined -lole32 -loleaut32
endif

if OS_WIN32_MINGW
libsmartmon_la_SOURCES += \
        os_win32/popen_win32.cpp
endif

if OS_OS2
# OS/2 DLLs do not support undefined symbols
libsmartmon_la_LDFLAGS = -no-undefined
endif

if NEED_REGEX

libsmartmon_la_SOURCES += \
        regex/regex.c \
        regex/regex_internal.h

# Included by regex.c:
EXTRA_libsmartmon_la_SOURCES += \
        regex/regcomp.c \
        regex/regexec.c \
        regex/regex_internal.c

endif

if ENABLE_DRIVEDB
drivedbinst_DATA = drivedb.h
endif

knowndrives.lo: drivedb.h

# Create drive database file with updated "VERSION: ..." string using same
# syntax as on drivedb branches.
drivedb.h: ../drivedb/drivedb.h Makefile ../include/smartmon/version.sh
	$(ECHO_V_GEN) ' sed $$(ADD_VERSION) $(top_srcdir)/drivedb/drivedb.h > $@'
	@. ../include/smartmon/version.sh && \
	r=$$SMARTMONTOOLS_GIT_REV && d=$$SMARTMONTOOLS_GIT_REV_DATE && \
	t=$$SMARTMONTOOLS_GIT_REV_TIME && s=$$SMARTMONTOOLS_GIT_SVN_REV && \
	x="/$$s $$d $$t +0000 $$r" && \
	if [ "$${r:+y}$${d:+y}$${t:+y}$${s:+y}" != "yyyy" ]; then \
	  if [ "$$SMARTMONTOOLS_TEST_BUILD" != "1" ]; then \
	    echo "../include/smartmon/version.sh: missing version information" >&2; \
	    echo "Use this to override:" >&2; \
	    echo "export SMARTMONTOOLS_TEST_BUILD=1" >&2; exit 1; \
	  fi; \
	  x=" (unknown)"; \
	fi && \
	sed 's,^\(  { "VERSION: [1-9][0-9]*\.[0-9][0-9]*\)",\1'"$$x"'",' \
	  $(top_srcdir)/drivedb/drivedb.h > $@.tmp && \
	mv -f $@.tmp $@

CLEANFILES = \
        drivedb.h \
        drivedb.h.tmp

# 'make maintainer-clean' also removes files generated by './autogen.sh'
MAINTAINERCLEANFILES = \
        $(srcdir)/Makefile.in
