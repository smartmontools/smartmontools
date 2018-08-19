## Process this file with automake to produce Makefile.in
#
# $Id$
#

@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4

# Make sure .cpp takes precedence to avoid compiling old .c file
SUFFIXES = .cpp .c .s .o

# BUILD_INFO can be provided by package maintainers (see INSTALL file)
BUILD_INFO= "(local build)"

AM_CPPFLAGS = \
        -DBUILD_INFO='$(BUILD_INFO)' \
        -DSMARTMONTOOLS_SYSCONFDIR='"$(sysconfdir)"' \
        -DSMARTMONTOOLS_SMARTDSCRIPTDIR='"$(smartdscriptdir)"'

if ENABLE_DRIVEDB
AM_CPPFLAGS += -DSMARTMONTOOLS_DRIVEDBDIR='"$(drivedbdir)"'
endif
if ENABLE_SAVESTATES
AM_CPPFLAGS += -DSMARTMONTOOLS_SAVESTATES='"$(savestates)"'
endif
if ENABLE_ATTRIBUTELOG
AM_CPPFLAGS += -DSMARTMONTOOLS_ATTRIBUTELOG='"$(attributelog)"'
endif

if OS_WIN32_MINGW
AM_CPPFLAGS += -I$(srcdir)/os_win32
endif
if NEED_GETOPT_LONG
AM_CPPFLAGS += -I$(srcdir)/getopt -DHAVE_GETOPT_LONG -D__GNU_LIBRARY__
endif
if NEED_REGEX
AM_CPPFLAGS += -I$(srcdir)/regex
endif

sbin_PROGRAMS = \
        smartctl \
        smartd

if ENABLE_UPDATE_SMART_DRIVEDB
if OS_WIN32_MINGW
else
sbin_SCRIPTS = update-smart-drivedb
endif
endif


smartctl_SOURCES = \
        smartctl.cpp \
        smartctl.h \
        atacmdnames.cpp \
        atacmdnames.h \
        atacmds.cpp \
        atacmds.h \
        ataidentify.cpp \
        ataidentify.h \
        ataprint.cpp \
        ataprint.h \
        dev_ata_cmd_set.cpp \
        dev_ata_cmd_set.h \
        dev_intelliprop.cpp \
        dev_intelliprop.h \
        dev_interface.cpp \
        dev_interface.h \
        dev_tunnelled.h \
        drivedb.h \
        json.cpp \
        json.h \
        knowndrives.cpp \
        knowndrives.h \
        nvmecmds.cpp \
        nvmecmds.h \
        nvmeprint.cpp \
        nvmeprint.h \
        scsicmds.cpp \
        scsicmds.h \
        scsiata.cpp \
        scsiprint.cpp \
        scsiprint.h \
        utility.cpp \
        utility.h \
        sg_unaligned.h

smartctl_LDADD = $(os_deps) $(os_libs)
smartctl_DEPENDENCIES = $(os_deps)

EXTRA_smartctl_SOURCES = \
        os_darwin.cpp \
        os_darwin.h \
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
        os_solaris.h \
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
        megaraid.h

if OS_WIN32_MINGW

smartctl_LDADD        += smartctl_res.o
smartctl_DEPENDENCIES += smartctl_res.o

endif


smartd_SOURCES = \
        smartd.cpp \
        atacmdnames.cpp \
        atacmdnames.h \
        atacmds.cpp \
        atacmds.h \
        dev_ata_cmd_set.cpp \
        dev_ata_cmd_set.h \
        dev_intelliprop.cpp \
        dev_intelliprop.h \
        dev_interface.cpp \
        dev_interface.h \
        dev_tunnelled.h \
        drivedb.h \
        knowndrives.cpp \
        knowndrives.h \
        nvmecmds.cpp \
        nvmecmds.h \
        scsicmds.cpp \
        scsicmds.h \
        scsiata.cpp \
        utility.cpp \
        utility.h \
        sg_unaligned.h

smartd_LDADD = $(os_deps) $(os_libs) $(CAPNG_LDADD)
smartd_DEPENDENCIES = $(os_deps)

EXTRA_smartd_SOURCES = \
        os_darwin.cpp \
        os_darwin.h \
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
        os_solaris.h \
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
        megaraid.h

if OS_WIN32_MINGW

smartd_SOURCES += \
        os_win32/daemon_win32.cpp \
        os_win32/daemon_win32.h \
        os_win32/syslog_win32.cpp \
        os_win32/syslog.h

smartd_LDADD        += smartd_res.o
smartd_DEPENDENCIES += smartd_res.o

endif

# Exclude from source tarball
nodist_EXTRA_smartctl_SOURCES = os_solaris_ata.s
nodist_EXTRA_smartd_SOURCES   = os_solaris_ata.s

if NEED_GETOPT_LONG

smartctl_SOURCES += \
        getopt/getopt.c \
        getopt/getopt.h \
        getopt/getopt1.c

smartd_SOURCES += \
        getopt/getopt.c \
        getopt/getopt.h \
        getopt/getopt1.c

endif

if NEED_REGEX

smartctl_SOURCES += \
        regex/regex.c \
        regex/regex.h \
        regex/regex_internal.h

smartd_SOURCES += \
        regex/regex.c \
        regex/regex.h \
        regex/regex_internal.h

# Included by regex.c:
EXTRA_smartctl_SOURCES += \
        regex/regcomp.c \
        regex/regexec.c \
        regex/regex_internal.c

EXTRA_smartd_SOURCES += \
        regex/regcomp.c \
        regex/regexec.c \
        regex/regex_internal.c

endif

if OS_WIN32

smartctl_SOURCES += \
        csmisas.h \
        os_win32/wmiquery.cpp \
        os_win32/wmiquery.h

smartd_SOURCES += \
        csmisas.h \
        os_win32/wmiquery.cpp \
        os_win32/wmiquery.h

smartctl_LDADD += -lole32 -loleaut32
smartd_LDADD   += -lole32 -loleaut32

endif

if OS_SOLARIS
# This block is required because Solaris uses manual page section 1m
# for administrative command (linux/freebsd use section 8) and Solaris
# uses manual page section 4 for file formats (linux/freebsd use
# section 5).  Automake can deal cleanly with man page sections 1-8
# and n, but NOT with sections of the form 1m.
extra_MANS =      smartd.conf.4 \
                  smartctl.1m   \
                  smartd.1m
if ENABLE_UPDATE_SMART_DRIVEDB
extra_MANS += update-smart-drivedb.1m
endif

all-local: $(extra_MANS)
install-man: $(extra_MANS)
	@$(NORMAL_INSTALL)
	$(MKDIR_P) '$(DESTDIR)$(mandir)/man4'
	$(MKDIR_P) '$(DESTDIR)$(mandir)/man1m'
	for i in $(extra_MANS); do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) '$$file' '$(DESTDIR)$(mandir)/man$$ext/$$inst'"; \
	  $(INSTALL_DATA) "$$file" "$(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	done
uninstall-man:
	@$(NORMAL_UNINSTALL)
	for i in $(extra_MANS); do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f '$(DESTDIR)$(mandir)/man$$ext/$$inst'"; \
	  rm -f "$(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	done
else
# For systems that adopts traditional manner

man_MANS =        smartd.conf.5 \
                  smartctl.8    \
                  smartd.8

if ENABLE_UPDATE_SMART_DRIVEDB
man_MANS += update-smart-drivedb.8
endif

endif

docsdir=$(docdir)
docs_DATA = \
        AUTHORS \
        ChangeLog \
        ChangeLog-5.0-6.0 \
        COPYING \
        INSTALL \
        NEWS \
        README \
        TODO \
        smartd.conf

examplesdir=$(exampledir)
examples_DATA = \
        examplescripts/README
examples_SCRIPTS = \
        examplescripts/Example1 \
        examplescripts/Example2 \
        examplescripts/Example3 \
        examplescripts/Example4 \
        examplescripts/Example5 \
        examplescripts/Example6

sysconf_DATA = smartd.conf

# If modified smartd.conf exists install smartd.conf.sample instead
install-sysconfDATA: $(sysconf_DATA)
	$(MKDIR_P) '$(DESTDIR)$(sysconfdir)'
	@s="$(srcdir)/smartd.conf"; \
	f="$(DESTDIR)$(sysconfdir)/smartd.conf$(smartd_suffix)"; \
	if test -z "$(smartd_suffix)" && test -f "$$f"; then \
	  if cmp "$$s" "$$f" >/dev/null 2>/dev/null; then :; else \
	    echo "************************************************************"; \
	    echo "*** $$f preserved"; \
	    echo "*** installing smartd.conf.sample instead"; \
	    echo "************************************************************"; \
	    f="$$f".sample; \
	  fi; \
	fi; \
	echo " $(INSTALL_DATA) '$$s' '$$f'"; \
	$(INSTALL_DATA) "$$s" "$$f"

# If smartd.conf.sample exists preserve smartd.conf
uninstall-sysconfDATA:
	@f="$(DESTDIR)$(sysconfdir)/smartd.conf$(smartd_suffix)"; \
	if test -z "$(smartd_suffix)" && test -f "$$f".sample; then \
	  echo "************************************************************"; \
	  echo "*** $$f preserved"; \
	  echo "*** removing smartd.conf.sample instead"; \
	  echo "************************************************************"; \
	  f="$$f".sample; \
	fi; \
	echo " rm -f '$$f'"; \
	rm -f "$$f"

smartdscript_SCRIPTS = smartd_warning.sh

EXTRA_DIST = \
        .editorconfig \
        autogen.sh \
        smartd.initd.in \
        smartd.cygwin.initd.in \
        smartd.freebsd.initd.in \
        smartd.8.in \
        smartctl.8.in \
        smartd.conf.5.in \
        smartd.conf \
        smartd.service.in \
        smartd_warning.sh.in \
        update-smart-drivedb.in \
        update-smart-drivedb.8.in \
        m4/pkg.m4 \
        os_darwin/com.smartmontools.smartd.plist.in \
        os_darwin/pkg/PackageInfo.in \
        os_darwin/pkg/Distribution.in \
        os_darwin/pkg/installer/README.html \
        os_darwin/pkg/root/usr/local/sbin/smart-pkg-uninstall \
        os_win32/default.manifest \
        os_win32/installer.nsi \
        os_win32/runcmd.c \
        os_win32/smartd_mailer.ps1 \
        os_win32/smartd_mailer.conf.sample.ps1 \
        os_win32/smartd_warning.cmd \
        os_win32/syslogevt.mc \
        os_win32/update-smart-drivedb.nsi \
        os_win32/versioninfo.rc.in \
        os_win32/wtssendmsg.c \
        $(docs_DATA) \
        $(examples_DATA) \
        $(examples_SCRIPTS)

CLEANFILES = \
        smartd.8 \
        smartd.1m \
        smartd.8.html \
        smartd.8.html.tmp \
        smartd.8.pdf \
        smartd.8.txt \
        smartctl.8 \
        smartctl.1m \
        smartctl.8.html \
        smartctl.8.html.tmp \
        smartctl.8.pdf \
        smartctl.8.txt \
        smartd.conf.5 \
        smartd.conf.4 \
        smartd.conf.5.html \
        smartd.conf.5.html.tmp \
        smartd.conf.5.pdf \
        smartd.conf.5.txt \
        smartd.initd \
        smartd.cygwin.initd \
        smartd.freebsd.initd \
        smartd.service \
        smartd_warning.sh \
        svnversion.h \
        update-smart-drivedb \
        update-smart-drivedb.8 \
        update-smart-drivedb.1m \
        update-smart-drivedb.8.html \
        update-smart-drivedb.8.html.tmp \
        update-smart-drivedb.8.pdf \
        update-smart-drivedb.8.txt \
        SMART

# 'make maintainer-clean' also removes files generated by './autogen.sh'
MAINTAINERCLEANFILES = \
        $(srcdir)/Makefile.in \
        $(srcdir)/aclocal.m4 \
        $(srcdir)/compile \
        $(srcdir)/configure \
        $(srcdir)/config.guess \
        $(srcdir)/config.h.in \
        $(srcdir)/config.h.in~ \
        $(srcdir)/config.sub \
        $(srcdir)/depcomp \
        $(srcdir)/install-sh \
        $(srcdir)/missing \
        $(srcdir)/m4/pkg.m4

smartctl.o utility.o: svnversion.h

if IS_SVN_BUILD
# Get version info from SVN
svnversion.h: ChangeLog Makefile $(svn_deps)
	@echo ' svn info | $$(VERSION_FROM_SVN_INFO) > $@'
	@echo '/* svnversion.h.  Generated by Makefile from svn info.  */' > $@
	@(cd $(srcdir) \
	 && svnversion 2>/dev/null | sed -n 's,^\([0-9].*\),REV  "\1",p' \
	 && TZ= LC_ALL=C svn info 2>/dev/null \
	 | sed -n 'h;s,^.* Date: *\([^ ]*\) .*$$,DATE "\1",p;g;s,^.* Date: *[^ ]* *\([^ ]*\) .*$$,TIME "\1",p') \
	| sed 's,^,#define SMARTMONTOOLS_SVN_,' >> $@
else

# SVN not available, guess version info from Id strings
svnversion.h: ChangeLog Makefile NEWS
	@echo ' cat ChangeLog NEWS $$(SOURCES) | $$(VERSION_FROM_SVN_IDS) > $@'
	@echo '/* svnversion.h.  Generated by Makefile from Id strings.  */' > $@
	@(cd $(srcdir) && cat ChangeLog NEWS Makefile.am configure.ac smart*.in *.cpp *.h) \
	| sed -n 's,^.*\$$[I][d]: [^ ]* \([0-9][0-9]* [0-9][-0-9]* [0-9][:0-9]*\)[^:0-9][^$$]*\$$.*$$,\1,p' \
	| sort -n -r \
	| sed -n 'h;s,^\([^ ]*\) .*$$,REV  "\1",p;g;s,^[^ ]* \([^ ]*\) .*$$,DATE "\1",p;g;s,^[^ ]* [^ ]* \([^ ]*\)$$,TIME "\1",p;q' \
	| sed 's,^,#define SMARTMONTOOLS_SVN_,' >> $@
endif


if ENABLE_DRIVEDB
drivedb_DATA = drivedb.h
endif

update-smart-drivedb: update-smart-drivedb.in config.status
	$(SHELL) ./config.status --file=$@
	chmod +x $@

smartd_warning.sh: smartd_warning.sh.in config.status
	$(SHELL) ./config.status --file=$@
	chmod +x $@

if INSTALL_INITSCRIPT
if OS_DARWIN
initd_DATA = com.smartmontools.smartd.plist

initd_DATA_install = install-initdDATA-darwin
initd_DATA_uninstall = uninstall-initdDATA-darwin

com.smartmontools.smartd.plist : os_darwin/com.smartmontools.smartd.plist.in
	sed "s|/usr/sbin/|$(sbindir)/|" $< > $@

install-initdDATA-darwin: $(initd_DATA)
	$(MKDIR_P) '$(DESTDIR)$(initddir)'
	$(INSTALL_DATA) $(top_builddir)/$(initd_DATA) $(DESTDIR)$(initddir)/$(initd_DATA)

uninstall-initdDATA-darwin:
	rm -f $(DESTDIR)$(initddir)/$(initd_DATA)

else

initd_DATA = $(initdfile)

$(initdfile): $(srcdir)/$(initdfile).in Makefile
	sed 's|/usr/local/sbin/|$(sbindir)/|g' $(srcdir)/$(initdfile).in > $@

initd_install_name = smartd$(smartd_suffix)

initd_DATA_install = install-initdDATA-generic
initd_DATA_uninstall = uninstall-initdDATA-generic

install-initdDATA-generic: $(initd_DATA)
	$(MKDIR_P) '$(DESTDIR)$(initddir)'
	$(INSTALL_SCRIPT) '$(top_builddir)/$(initdfile)' '$(DESTDIR)$(initddir)/smartd$(smartd_suffix)'

uninstall-initdDATA-generic:
	rm -f '$(DESTDIR)$(initddir)/$(initd_install_name)'
endif
else

initd_DATA_install = install-initdDATA-null
initd_DATA_uninstall = uninstall-initdDATA-null

install-initdDATA-null:
uninstall-initdDATA-null:
endif

install-initdDATA : $(initd_DATA_install)

uninstall-initdDATA: $(initd_DATA_uninstall)

if INSTALL_SYSTEMDUNIT
systemdsystemunit_DATA = smartd.service
endif

smartd.service: smartd.service.in Makefile
	@echo ' $$(SMARTD_SERVICE_FILTER) < $(srcdir)/smartd.service.in > $@'
	@{ \
	sed 's|/usr/local/sbin/smartd|$(sbindir)/smartd|' | \
	if test -n '$(systemdenvfile)'; then \
	  sed 's|/usr/local/etc/sysconfig/smartmontools|$(systemdenvfile)|'; \
	else \
	  sed -e '/^EnvironmentFile=/d' -e 's| *\$$smartd[_a-z]* *||g'; \
	fi; } < $(srcdir)/smartd.service.in > $@


# Create empty directories if configured.
# Default install rules no longer create empty directories since automake 1.11.
installdirs-local:
	@for d in '$(smartdplugindir)' '$(savestatesdir)' '$(attributelogdir)'; do \
	  test -n "$$d" || continue; \
	  echo " $(MKDIR_P) '$(DESTDIR)$$d'"; \
	  $(MKDIR_P) "$(DESTDIR)$$d" || exit 1; \
	done

install-data-local: installdirs-local

#
# Build man pages
#
MAN_FILTER = { \
    sed -e 's|CURRENT_SVN_VERSION|$(releaseversion)|g' \
        -e "s|CURRENT_SVN_DATE|`sed -n 's,^.*DATE[^"]*"\([^"]*\)".*$$,\1,p' svnversion.h`|g" \
        -e "s|CURRENT_SVN_REV|`sed -n 's,^.*REV[^"]*"\([^"]*\)".*$$,r\1,p' svnversion.h`|g" \
        -e 's|/usr/local/share/man/|$(mandir)/|g' \
        -e 's|/usr/local/sbin/|$(sbindir)/|g' \
        -e 's|/usr/local/share/doc/smartmontools/examplescripts/|!exampledir!|g' \
        -e 's|/usr/local/share/doc/smartmontools/|$(docsdir)/|g' \
        -e 's|!exampledir!|$(exampledir)/|g' \
        -e 's|/usr/local/etc/smartd\.conf|$(sysconfdir)/smartd.conf|g' \
        -e 's|/usr/local/etc/smart_drivedb\.h|$(sysconfdir)/smart_drivedb.h|g' \
        -e 's|/usr/local/etc/smartd_warning\.sh|$(smartdscriptdir)/smartd_warning.sh|g' \
        -e 's|\\fBmail\\fP|\\fB$(os_mailer)\\fP|g' \
        -e 's|\\'\''mail\\'\''|\\'\''$(os_mailer)\\'\''|g' \
        -e 's|/usr/bin/mail|/usr/bin/$(os_mailer)|g' \
        -e 's|RELEASE_6_0_DRIVEDB|$(DRIVEDB_BRANCH)|g' | \
    if test -n '$(drivedbdir)'; then \
      sed 's|/usr/local/share/smartmontools/drivedb\.h|$(drivedbdir)/drivedb.h|g' ; \
    else \
      sed '/^\.\\" %IF ENABLE_DRIVEDB/,/^\.\\" %ENDIF ENABLE_DRIVEDB/ s,^,.\\"\# ,' ; \
    fi | \
    if test '$(with_update_smart_drivedb)' = 'yes'; then \
      cat; \
    else \
      sed '/^\.\\" %IF ENABLE_UPDATE_SMART_DRIVEDB/,/^\.\\" %ENDIF ENABLE_UPDATE_SMART_DRIVEDB/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(initddir)'; then \
      sed 's|/usr/local/etc/rc\.d/init\.d/|$(initddir)/|g' ; \
    else \
      sed '/^\.\\" %IF ENABLE_INITSCRIPT/,/^\.\\" %ENDIF ENABLE_INITSCRIPT/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(savestates)'; then \
      sed 's|/usr/local/var/lib/smartmontools/smartd\.|$(savestates)|g' ; \
    else \
      sed '/^\.\\" %IF ENABLE_SAVESTATES/,/^\.\\" %ENDIF ENABLE_SAVESTATES/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(attributelog)'; then \
      sed 's|/usr/local/var/lib/smartmontools/attrlog\.|$(attributelog)|g' ; \
    else \
      sed '/^\.\\" %IF ENABLE_ATTRIBUTELOG/,/^\.\\" %ENDIF ENABLE_ATTRIBUTELOG/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(smartdplugindir)'; then \
      sed 's|/usr/local/etc/smartd_warning\.d|$(smartdplugindir)|g' ; \
    else \
      sed '/^\.\\" %IF ENABLE_SMARTDPLUGINDIR/,/^\.\\" %ENDIF ENABLE_SMARTDPLUGINDIR/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(CAPNG_LDADD)'; then \
      cat; \
    else \
      sed '/^\.\\" %IF ENABLE_CAPABILITIES/,/^\.\\" %ENDIF ENABLE_CAPABILITIES/ s,^,.\\"\# ,' ; \
    fi | \
    if test '$(with_nvme_devicescan)' = 'yes'; then \
      cat; \
    else \
      sed '/^\.\\" %IF ENABLE_NVME_DEVICESCAN/,/^\.\\" %ENDIF ENABLE_NVME_DEVICESCAN/ s,^,.\\"\# ,' ; \
    fi | \
    if test -n '$(os_man_filter)'; then \
      sed -e 's,OS_MAN_FILTER,$(os_man_filter),g' \
          -e '/^\.\\" %IF NOT OS .*$(os_man_filter)/,/^.\\" %ENDIF NOT OS .*$(os_man_filter)/ s,^,.\\"\# ,' \
          -e '/^\.\\" %IF OS .*$(os_man_filter)/,/^\.\\" %ENDIF OS .*$(os_man_filter)/ s,^,!!,' \
          -e '/^\.\\" %IF OS ./,/^\.\\" %ENDIF OS ./ s,^,.\\"\# ,' \
          -e '/^!*\.\\" %IF NOT OS ./,/^!*\.\\" %ENDIF NOT OS ./ s,^,!!,' \
          -e 's,^!!!*\.\\"! \(.*\)$$,\1 \\"\#,' \
          -e 's,^!!!*,,' ; \
    else \
      cat; \
    fi; }

# Implicit rule 'smart%: smart%.in ...' does not work with BSD make
smartctl.8: smartctl.8.in Makefile svnversion.h
	@echo ' $$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@

smartd.8: smartd.8.in Makefile svnversion.h
	@echo ' $$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@

smartd.conf.5: smartd.conf.5.in Makefile svnversion.h
	@echo ' $$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@

update-smart-drivedb.8: update-smart-drivedb.8.in Makefile svnversion.h
	@echo ' $$(MAN_FILTER) < $(srcdir)/update-smart-drivedb.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/update-smart-drivedb.8.in > $@

# Build Solaris specific man pages
SOLARIS_MAN_FILTER = \
    sed -e '/^\.TH/s, \([58]\) , !!\1!! ,' \
        -e '/^\.BR/s, (\([578]\)), (!!\1!!),' \
        -e 's,\\fP(\([578]\)),\\fP(!!\1!!),g' \
        -e 's,!!5!!,4,g' -e 's,!!7!!,5,g' -e 's,!!8!!,1m,g' \
        -e 's,/var/log/messages,/var/adm/messages,g'

smartctl.1m: smartctl.8
	@echo ' $$(SOLARIS_MAN_FILTER) < smartctl.8 > $@'
	@$(SOLARIS_MAN_FILTER) < smartctl.8 > $@

smartd.1m: smartd.8
	@echo ' $$(SOLARIS_MAN_FILTER) < smartd.8 > $@'
	@$(SOLARIS_MAN_FILTER) < smartd.8 > $@

smartd.conf.4: smartd.conf.5
	@echo ' $$(SOLARIS_MAN_FILTER) < smartd.conf.5 > $@'
	@$(SOLARIS_MAN_FILTER) < smartd.conf.5 > $@

update-smart-drivedb.1m: update-smart-drivedb.8
	@echo ' $$(SOLARIS_MAN_FILTER) < update-smart-drivedb.8 > $@'
	@$(SOLARIS_MAN_FILTER) < update-smart-drivedb.8 > $@


# Convert man pages into .html, .pdf and .txt
# TODO: configure
MAN2HTML = man2html
#MAN2HTML = groff -man -Thtml
MAN2PDF = man -Tpdf -l
#MAN2PDF = groff -man -Tpdf
MAN2TXT = groff -man -Tascii -P'-bcou'

# Remove HTTP header and fix links in man2html output
FIXHTML = sed -e '1s,^Content-type.*,,' \
              -e 's,<A HREF="[^"]*/man2html?\([1-8]\)+\(smart[cd][.a-z]*\)">,<A HREF="\2.\1.html">,g' \
              -e 's,<A HREF="[^"]*/man2html">,<A HREF=".">,g' \
              -e 's,<A HREF="[^"]*/man2html?[^"]*">\([^<]*\)</A>,\1,g' \
              -e 's,<A HREF="mailto:[^s][^m][^a][^"]*">\([^<]*\)</A>,\1,g'

htmlman: smartctl.8.html smartd.8.html smartd.conf.5.html update-smart-drivedb.8.html

pdfman:  smartctl.8.pdf smartd.8.pdf smartd.conf.5.pdf update-smart-drivedb.8.pdf

txtman:  smartctl.8.txt smartd.8.txt smartd.conf.5.txt update-smart-drivedb.8.txt

%.5.html: %.5
	$(MAN2HTML) $< > $@.tmp
	@echo ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

%.8.html: %.8
	$(MAN2HTML) $< > $@.tmp
	@echo ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

%.5.pdf: %.5
	$(MAN2PDF) $< > $@

%.8.pdf: %.8
	$(MAN2PDF) $< > $@

%.5.txt: %.5
	$(MAN2TXT) $< > $@

%.8.txt: %.8
	$(MAN2TXT) $< > $@


# Check drive database syntax
check:
	@if ./smartctl -B $(srcdir)/drivedb.h -P showall >/dev/null; then \
	  echo "$(srcdir)/drivedb.h: OK"; \
	else \
	  echo "$(srcdir)/drivedb.h: Syntax check failed"; exit 1; \
	fi


if OS_WIN32_MINGW
# Windows resources

smartctl_res.o: smartctl_res.rc $(os_win32_manifest)
	$(WINDRES) $< $@

smartd_res.o: smartd_res.rc syslogevt.rc $(os_win32_manifest)
	$(WINDRES) $< $@

runcmda_res.o: runcmda_res.rc defadmin.manifest
	$(WINDRES) $< $@

runcmdu_res.o: runcmdu_res.rc $(os_win32_manifest)
	$(WINDRES) $< $@

wtssendmsg_res.o: wtssendmsg_res.rc $(os_win32_manifest)
	$(WINDRES) $< $@

# Convert version for VERSIONINFO resource: 6.6 r4519 -> 6.6.0.4519,
# set description, name, version and year
WIN_RC_FILTER = { \
    ver=`echo '$(PACKAGE_VERSION).0' | sed -n 's,^\([0-9]*\.[0-9]*\.[0-9]*\).*$$,\1,p'` && \
    rev=`sed -n 's,^.*REV[^"]*"\([0-9]*\).*$$,\1,p' svnversion.h` && \
    txtver="$${ver:-0.0.0}.$${rev:-0}" && binver=`echo "$$txtver" | sed 's|\.|,|g'` && \
    yy=`sed -n 's,^.*DATE[^"]*"20\([0-9][0-9]\).*$$,\1,p' svnversion.h` && yy="$${yy:-XX}" && \
    sed -e "s|@DESC@|$$d|" -e "s|@NAME@|$$n|" -e "s|@BINARY_VERSION@|$$binver|g" \
        -e "s|@TEXT_VERSION@|$$txtver|g" -e "s|@YY@|$$yy|g"; }

WIN_MAKE_RES = \
    echo "n=$$n d=\"$$d"'"; $$(WIN_RC_FILTER) < $< > $@'; \
    $(WIN_RC_FILTER) < $< > $@

WIN_APP_MANIFEST = \
    if test -n '$(os_win32_manifest)'; then \
      echo "echo '1 24 \"$(srcdir)/$(os_win32_manifest)\"' >> $@"; \
      echo '1 24 "$(srcdir)/$(os_win32_manifest)"' >> $@; \
    fi

smartctl_res.rc: os_win32/versioninfo.rc.in Makefile svnversion.h
	@n=smartctl d="Control and Monitor Utility for SMART Disks"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

smartd_res.rc: os_win32/versioninfo.rc.in Makefile svnversion.h
	@n=smartd d="SMART Disk Monitoring Daemon"; $(WIN_MAKE_RES)
	echo '#include "./syslogevt.rc"' >> $@
	@$(WIN_APP_MANIFEST)

runcmdu_res.rc: os_win32/versioninfo.rc.in Makefile svnversion.h
	@n=runcmdu d="Run console command"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

runcmda_res.rc: os_win32/versioninfo.rc.in Makefile svnversion.h
	@n=runcmda d="Run console command as admin"; $(WIN_MAKE_RES)
	echo '1 24 "./defadmin.manifest"' >> $@

wtssendmsg_res.rc: os_win32/versioninfo.rc.in Makefile svnversion.h
	@n=wtssendmsg d="Send console messages"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

syslogevt.rc: os_win32/syslogevt.mc
	$(WINDMC) -b $<

defadmin.manifest: os_win32/default.manifest
	sed 's,"asInvoker","requireAdministrator",' $< > $@

# Definitions for Windows distribution

if OS_WIN64
win_bits = 64
else
win_bits = 32
endif

distdir_win32  = $(PACKAGE)-$(VERSION).win$(win_bits)
distzip_win32  = $(PACKAGE)-$(VERSION).win$(win_bits).zip
distinst_win32 = $(PACKAGE)-$(VERSION).win$(win_bits)-setup.exe

exedir_win32 = $(distdir_win32)/bin
docdir_win32 = $(distdir_win32)/doc

EXEFILES_WIN32 = \
        $(exedir_win32)/smartctl.exe \
        $(exedir_win32)/smartctl-nc.exe \
        $(exedir_win32)/smartd.exe \
        $(exedir_win32)/smartd_mailer.ps1 \
        $(exedir_win32)/smartd_mailer.conf.sample.ps1 \
        $(exedir_win32)/smartd_warning.cmd \
        $(exedir_win32)/runcmda.exe \
        $(exedir_win32)/runcmdu.exe \
        $(exedir_win32)/wtssendmsg.exe

if ENABLE_DRIVEDB
if OS_WIN32_NSIS
EXEFILES_WIN32 += \
        $(exedir_win32)/update-smart-drivedb.exe
endif
endif

FILES_WIN32 = \
        $(EXEFILES_WIN32) \
        $(docdir_win32)/AUTHORS.txt \
        $(docdir_win32)/ChangeLog.txt \
        $(docdir_win32)/ChangeLog-5.0-6.0.txt \
        $(docdir_win32)/COPYING.txt \
        $(docdir_win32)/INSTALL.txt \
        $(docdir_win32)/NEWS.txt \
        $(docdir_win32)/README.txt \
        $(docdir_win32)/TODO.txt \
        $(docdir_win32)/checksums$(win_bits).txt \
        $(docdir_win32)/smartd.conf \
        $(docdir_win32)/smartctl.8.html \
        $(docdir_win32)/smartctl.8.pdf \
        $(docdir_win32)/smartd.8.html \
        $(docdir_win32)/smartd.8.pdf \
        $(docdir_win32)/smartd.conf.5.html \
        $(docdir_win32)/smartd.conf.5.pdf

if ENABLE_DRIVEDB
FILES_WIN32 += \
        $(exedir_win32)/drivedb.h
endif

CLEANFILES += \
        $(FILES_WIN32) \
        defadmin.manifest \
        distdir.mkdir \
        runcmda.exe runcmda_res.rc \
        runcmdu.exe runcmdu_res.rc \
        smartctl-nc.exe smartctl-nc.exe.tmp \
        smartctl_res.rc smartd_res.rc \
        syslogevt.h \
        syslogevt.rc syslogevt_*.bin \
        update-smart-drivedb.exe \
        wtssendmsg.exe wtssendmsg_res.rc

# Note: Only use without options to be compatible with all variants
UNIX2DOS = unix2dos

# Build Windows distribution

dist-win32: $(distzip_win32)

install-win32: $(distinst_win32)
	./$(distinst_win32)

installer-win32: $(distinst_win32)

distdir-win32: distdir.mkdir $(FILES_WIN32)

$(distzip_win32): distdir.mkdir $(FILES_WIN32)
	@rm -fv $(distzip_win32)
	cd $(distdir_win32) && zip -9 ../$(distzip_win32) bin/* doc/*
	md5sum $@ > $@.md5
	sha1sum $@ > $@.sha1
	sha256sum $@ > $@.sha256

if OS_WIN32_NSIS
# Build NSIS installer
# Note: Only option character '-' is also compatible with Linux version of makensis
$(distinst_win32): os_win32/installer.nsi smartctl_res.rc distdir.mkdir $(FILES_WIN32)
	test -z '$(builddir_win64)' || ( cd $(builddir_win64) && make distdir-win32 )
	@date=`sed -n 's,^.*DATE[^"]*"\([^"]*\)".*$$,\1,p' svnversion.h` && \
	rev=`sed -n 's,^.*REV[^"]*"\([^"]*\)".*$$,r\1,p' svnversion.h` && \
	version=`sed -n 's|^ *VALUE "FileVersion", "\([0-9.]*\)".*$$|\1|p' smartctl_res.rc` && \
	yy=`echo "$$date" | sed -n 's,^20\([0-9][0-9]\).*$$,\1,p'`; yy="$${yy:-XX}" && \
	verstr="$(PACKAGE_VERSION) $$date $$rev "$(BUILD_INFO) && \
	d64= && if [ -n '$(builddir_win64)' ]; then d64='-DINPDIR64=$(builddir_win64)/$(PACKAGE)-$(VERSION).win64'; fi && \
	echo "'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR='$$verstr' $<" && \
	'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR="$$verstr" $<
	md5sum $@ > $@.md5
	sha1sum $@ > $@.sha1
	sha256sum $@ > $@.sha256

# Build drivedb.h update tool
update-smart-drivedb.exe: os_win32/update-smart-drivedb.nsi
	"$(MAKENSIS)" -V2 -NOCD -DBRANCH=$(DRIVEDB_BRANCH) $<

else
$(distinst_win32):
	@echo "makensis: command not found. Please install NSIS from http://nsis.sourceforge.net/" 1>&2
	@exit 1
endif

cleandist-win32:
	rm -rf $(distdir_win32) distdir.mkdir

distdir.mkdir:
	@test -d $(exedir_win32) || mkdir -pv $(exedir_win32)
	@test -d $(docdir_win32) || mkdir -pv $(docdir_win32)
	touch $@

$(exedir_win32)/%.exe: %.exe
	cp -p $< $@
	if test -n '$(STRIP)'; then $(STRIP) -s $@; else strip -s $@; fi
	touch -r $< $@

# strip would break NSIS integrity check
$(exedir_win32)/update-smart-drivedb.exe: update-smart-drivedb.exe
	cp -p $< $@

$(exedir_win32)/%.h: $(srcdir)/%.h
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(exedir_win32)/%.cmd: $(srcdir)/os_win32/%.cmd
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(exedir_win32)/%.ps1: $(srcdir)/os_win32/%.ps1
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.html: %.html
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.pdf: %.pdf
	cp -p $< $@

$(docdir_win32)/%.txt: $(srcdir)/%
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.conf: $(srcdir)/%.conf
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/checksums$(win_bits).txt: $(EXEFILES_WIN32)
	(cd $(exedir_win32) && md5sum *.exe && sha1sum *.exe && sha256sum *.exe) \
	| $(UNIX2DOS) > $@

# Build non-console version of smartctl for GSmartControl.
# The script below changes the word at offset 220 (Subsystem) from 3
# (Console) to 2 (GUI) in a copy of smartctl.exe.
# This will be changed when a tool (like 'editbin') is available in
# the Cygwin distribution
smartctl-nc.exe: smartctl.exe
	@rm -f $@
	cp -p smartctl.exe $@.tmp
	@if test `od -A n -j 220 -N 2 -d $@.tmp` -eq 3; then :; \
	  else echo "invalid EXE header"; exit 1; fi
	@echo "editbin /subsystem:windows $@.tmp"
	@echo -ne '\002' | dd bs=1 seek=220 count=1 conv=notrunc of=$@.tmp 2>/dev/null
	@if test `od -A n -j 220 -N 2 -d $@.tmp` -eq 2; then :; \
	  else echo "EXE patch failed"; exit 1; fi
	mv -f $@.tmp $@

# Build runcmd?.exe and wtssendmsg.exe
runcmd.o: os_win32/runcmd.c
	$(CC) -c -Os $<

runcmdu.exe: runcmd.o runcmdu_res.o
	$(CC) -o $@ $^

runcmda.exe: runcmd.o runcmda_res.o
	$(CC) -o $@ $^

wtssendmsg.exe: os_win32/wtssendmsg.c wtssendmsg_res.o
	$(CC) -Os -o $@ $^ -lwtsapi32

# Build os_win32/vc14/{config.h,smart*.rc,svnversion.h} for MSVC14 from MinGW files

vcver = 14

CONFIG_VC_FILES = \
        $(srcdir)/os_win32/vc$(vcver)/config.h \
        $(srcdir)/os_win32/vc$(vcver)/smartctl_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/smartd_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/svnversion.h

config-vc$(vcver): $(CONFIG_VC_FILES)

$(srcdir)/os_win32/vc$(vcver)/config.h: config.h Makefile
	sed -e '1i/* os_win32/vc$(vcver)/config.h.  Generated from config.h by Makefile.  */' \
	    -e 's,^#define HAVE_\(ATTR_PACKED\|GETTIMEOFDAY\|[DK_]*NTDDDISK_H\|LONG_DOUBLE_WIDER\|STRINGS_H\|UNISTD_H\|WORKING_SNPRINTF\) 1$$,/* #undef HAVE_\1 */ /* VC$(vcver) */,' \
	    -e 's,^\(#define SMARTMONTOOLS_BUILD_HOST "[^-]*\)[^"]*,\1-pc-w32vc$(vcver),' $< > $@

$(srcdir)/os_win32/vc$(vcver)/svnversion.h: svnversion.h
	cp $< $@

$(srcdir)/os_win32/vc$(vcver)/smartctl_res.rc: smartctl_res.rc
	sed '/^1 24 /d' $< > $@

$(srcdir)/os_win32/vc$(vcver)/smartd_res.rc: smartd_res.rc
	sed '/^1 24 /d' $< > $@

clean-vc$(vcver):
	rm -f $(srcdir)/os_win32/vc$(vcver)/smartmontools.VC.VC.opendb
	rm -f $(srcdir)/os_win32/vc$(vcver)/smartmontools.VC.db
	rm -f $(srcdir)/os_win32/vc$(vcver)/syslogevt.h
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Debug
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Release
	rm -rf $(srcdir)/os_win32/vc$(vcver)/x64

distclean-vc$(vcver): clean-vc$(vcver)
	rm -f $(CONFIG_VC_FILES)

maintainer-clean-vc$(vcver): distclean-vc$(vcver)
	rm -rf $(srcdir)/os_win32/vc$(vcver)/.vs

endif


if OS_DARWIN
# Definitions for OSX distribution
distdir_darwin  = $(PACKAGE)-$(VERSION).darwin
dmg_darwin =  $(PACKAGE)-$(VERSION).dmg
pkg_darwin = $(PACKAGE)-$(VERSION).pkg

# build darwin installer 
$(pkg_darwin):
	${MAKE} install DESTDIR=$(distdir_darwin)/root
	@cp $(srcdir)/os_darwin/pkg/root/usr/local/sbin/smart-pkg-uninstall $(distdir_darwin)/root$(sbindir)
	@mkdir -p $(distdir_darwin)/pkg
	@( cd $(distdir_darwin)/root && find . | cpio -o --format odc --owner 0:80 | gzip -c ) > $(distdir_darwin)/pkg/Payload
	PAYLOAD_FILES=`find $(distdir_darwin)/root | wc -l` &&\
	PAYLOAD_SIZEKB=`du -BK  -s $(distdir_darwin)/root|${AWK} '{print $$1}'|tr -d 'K'` &&\
	sed -e "s|@version@|$(VERSION)|" -e "s|@files@|$${PAYLOAD_FILES}|" \
		-e "s|@size@|$${PAYLOAD_SIZEKB}|" $(srcdir)/os_darwin/pkg/PackageInfo.in \
		> $(distdir_darwin)/pkg/PackageInfo &&\
	sed -e "s|@version@|$(VERSION)|" -e "s|@files@|$${PAYLOAD_FILES}|" -e "s|@size@|$${PAYLOAD_SIZEKB}|" \
		-e "s|@pkgname@|$(pkg_darwin)|" \
		$(srcdir)/os_darwin/pkg/Distribution.in > $(distdir_darwin)/pkg/Distribution
	@mkdir -p $(distdir_darwin)/pkg/Resources/English.lproj
	@cp $(srcdir)/COPYING $(distdir_darwin)/pkg/Resources/English.lproj/license.txt
	@mkbom -u 0 -g 80 $(distdir_darwin)/root $(distdir_darwin)/pkg/Bom
	@mkdir -p $(distdir_darwin)/dmg
	@( cd $(distdir_darwin)/pkg && xar --compression none -cf "../dmg/$(pkg_darwin)" * )

# build darwon dmg image
$(dmg_darwin):
	@cp $(srcdir)/os_darwin/pkg/installer/README.html $(distdir_darwin)/dmg
	@mkisofs -V 'smartmontools' -no-pad -r -apple -o $(distdir_darwin)/smartmontools-$(VERSION).iso \
		-hfs-bless "$(distdir_darwin)/dmg/" "$(distdir_darwin)/dmg/"
	@dmg dmg $(distdir_darwin)/smartmontools-$(VERSION).iso $(dmg_darwin)
	md5sum $@ > $@.md5
	sha1sum $@ > $@.sha1
	sha256sum $@ > $@.sha256

install-darwin: install-darwin-cleanup $(pkg_darwin) $(dmg_darwin)

install-darwin-cleanup: 
	@rm -rf $(distdir_darwin)
endif
