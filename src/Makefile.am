## Process this file with automake to produce Makefile.in
#
# src/Makefile.am
#

# BUILD_INFO can be provided by package maintainers (see INSTALL file)
BUILD_INFO= "(local build)"

if REPRODUCIBLE_BUILD
set_source_date_epoch = SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)
def_source_date_epoch = -D$(set_source_date_epoch)
if OS_DARWIN
export ZERO_AR_DATE := 1
endif
endif

AM_CPPFLAGS = \
        $(def_source_date_epoch) \
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
AM_CPPFLAGS += -I$(srcdir)/getopt -D_GETOPT_STANDALONE
endif
if NEED_REGEX
AM_CPPFLAGS += -I$(srcdir)/regex -D_REGEX_STANDALONE
endif

# Suppress 'echo' if './configure --enable-silent-rules' or 'make V=0' is used.
# Variables AM_V_* are provided by automake >= 1.13.
ECHO_V = ! $(AM_V_P) || echo
ECHO_V_GEN = @$(AM_V_P) || echo '  GEN      $@'; $(ECHO_V)

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
        dev_interface.cpp \
        dev_interface.h \
        dev_jmb39x_raid.cpp \
        dev_tunnelled.h \
        drivedb.h \
        farmcmds.cpp \
        farmcmds.h \
        farmprint.cpp \
        farmprint.h \
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
        scsinvme.cpp \
        scsiprint.cpp \
        scsiprint.h \
        static_assert.h \
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
        openbsd_nvme_ioctl.h \
        megaraid.h \
        sssraid.h

if OS_WIN32_MINGW

smartctl_SOURCES += \
        os_win32/popen_win32.cpp \
        os_win32/popen.h

smartctl_LDADD        += os_win32/smartctl_res.o
smartctl_DEPENDENCIES += os_win32/smartctl_res.o

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
        dev_interface.cpp \
        dev_interface.h \
        dev_jmb39x_raid.cpp \
        dev_tunnelled.h \
        drivedb.h \
        knowndrives.cpp \
        knowndrives.h \
        nvmecmds.cpp \
        nvmecmds.h \
        scsicmds.cpp \
        scsicmds.h \
        scsiata.cpp \
        scsinvme.cpp \
        static_assert.h \
        utility.cpp \
        utility.h \
        sg_unaligned.h

smartd_LDADD = $(os_deps) $(os_libs) $(CAPNG_LDADD) $(SYSTEMD_LDADD)
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

if OS_POSIX

smartd_SOURCES += \
        popen_as_ugid.cpp \
        popen_as_ugid.h

endif

if OS_WIN32_MINGW

smartd_SOURCES += \
        os_win32/daemon_win32.cpp \
        os_win32/daemon_win32.h \
        os_win32/popen_win32.cpp \
        os_win32/popen.h \
        os_win32/syslog_win32.cpp \
        os_win32/syslog.h

smartd_LDADD        += os_win32/smartd_res.o
smartd_DEPENDENCIES += os_win32/smartd_res.o

endif

if NEED_GETOPT_LONG

smartctl_SOURCES += \
        getopt/getopt.c \
        getopt/getopt.h \
        getopt/getopt1.c \
        getopt/getopt_int.h \
        getopt/bits/getopt_core.h \
        getopt/bits/getopt_ext.h

smartd_SOURCES += \
        getopt/getopt.c \
        getopt/getopt.h \
        getopt/getopt1.c \
        getopt/getopt_int.h \
        getopt/bits/getopt_core.h \
        getopt/bits/getopt_ext.h

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

# Some versions of the MinGW-w64 import lib for kernel32.dll include some symbols
# duplicated from advapi32.dll.  Older versions of windows provide these symbols
# only in advapi32.dll.  Ensure that the advapi32 lib is linked first.
smartctl_LDADD += -ladvapi32
smartd_LDADD   += -ladvapi32

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
        smartd.conf

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
        cppcheck.sh \
        getversion.sh \
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
        os_darwin/com.smartmontools.smartd.plist.in \
        os_darwin/pkg/PackageInfo.in \
        os_darwin/pkg/Distribution.in \
        os_darwin/pkg/installer/README.html \
        os_darwin/pkg/root/usr/local/sbin/smart-pkg-uninstall \
        os_win32/default.manifest \
        os_win32/installer.nsi \
        os_win32/pe32edit.sh \
        os_win32/runcmd.c \
        os_win32/smartd_mailer.ps1 \
        os_win32/smartd_mailer.conf.sample.ps1 \
        os_win32/smartd_warning.cmd \
        os_win32/syslogevt.mc \
        os_win32/update-smart-drivedb.ps1.in \
        os_win32/versioninfo.rc.in \
        os_win32/wtssendmsg.c \
        $(docs_DATA)

CLEANFILES = \
        cppcheck.txt \
        config.log \
        shellcheck.txt \
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
        version.h \
        version.h.tmp \
        version.sh \
        version.sh.tmp \
        update-smart-drivedb \
        update-smart-drivedb.8 \
        update-smart-drivedb.1m \
        update-smart-drivedb.8.html \
        update-smart-drivedb.8.html.tmp \
        update-smart-drivedb.8.pdf \
        update-smart-drivedb.8.txt

# 'make maintainer-clean' also removes files generated by './autogen.sh'
MAINTAINERCLEANFILES = \
        $(srcdir)/Makefile.in \
        $(srcdir)/config.h.in \
        $(srcdir)/config.h.in~

smartctl.o utility.o: version.h

# Get version info
version.h: version.sh getversion.sh
	$(AM_V_GEN)$(srcdir)/getversion.sh -i < version.sh > $@.tmp
	$(AM_V_at)mv -f $@.tmp $@

version.sh: getversion.sh ../configure.ac ../Makefile Makefile config.h
	$(AM_V_GEN)$(srcdir)/getversion.sh -s > $@.tmp
	$(AM_V_at)mv -f $@.tmp $@
	@! $(AM_V_P) || cat $@

if ENABLE_DRIVEDB
drivedbinst_DATA = drivedb.h
endif

update-smart-drivedb: update-smart-drivedb.in ../config.status
	$(AM_V_GEN)$(SHELL) ../config.status -q --file=$@:$(srcdir)/update-smart-drivedb.in
	$(AM_V_at)chmod +x $@

smartd_warning.sh: smartd_warning.sh.in ../config.status
	$(AM_V_GEN)$(SHELL) ../config.status -q --file=$@:$(srcdir)/smartd_warning.sh.in
	$(AM_V_at)chmod +x $@

if INSTALL_INITSCRIPT
if OS_DARWIN
initd_DATA = com.smartmontools.smartd.plist

initd_DATA_install = install-initdDATA-darwin
initd_DATA_uninstall = uninstall-initdDATA-darwin

com.smartmontools.smartd.plist : os_darwin/com.smartmontools.smartd.plist.in
	$(AM_V_GEN)sed "s|/usr/sbin/|$(sbindir)/|" $(srcdir)/os_darwin/com.smartmontools.smartd.plist.in > $@

install-initdDATA-darwin: $(initd_DATA)
	$(MKDIR_P) '$(DESTDIR)$(initddir)'
	$(INSTALL_DATA) $(top_builddir)/$(initd_DATA) $(DESTDIR)$(initddir)/$(initd_DATA)

uninstall-initdDATA-darwin:
	rm -f $(DESTDIR)$(initddir)/$(initd_DATA)

else

initd_DATA = $(initdfile)

$(initdfile): $(srcdir)/$(initdfile).in Makefile
	$(AM_V_GEN)sed 's|/usr/local/sbin/|$(sbindir)/|g' $(srcdir)/$(initdfile).in > $@

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
	$(ECHO_V_GEN) ' $$(SMARTD_SERVICE_FILTER) < $(srcdir)/smartd.service.in > $@'
	@{ \
	sed 's|/usr/local/sbin/smartd|$(sbindir)/smartd|' | \
	if test -n '$(SYSTEMD_LDADD)'; then \
	  cat; \
	else \
	  sed '/^Type=notify/d'; \
	fi | \
	if test -n '$(systemdenvfile)'; then \
	  sed 's|/usr/local/etc/sysconfig/smartmontools|$(systemdenvfile)|'; \
	else \
	  sed -e '/^EnvironmentFile=/d' -e 's| *\$$smartd[_a-z]* *||g'; \
	fi; } < $(srcdir)/smartd.service.in > $@


# Create empty directories if configured.
# Default install rules no longer create empty directories since automake 1.11.
installdirs-local:
	@for d in '$(smartdplugindir)' '$(drivedbdir)' '$(savestatesdir)' '$(attributelogdir)'; do \
	  test -n "$$d" || continue; \
	  echo " $(MKDIR_P) '$(DESTDIR)$$d'"; \
	  $(MKDIR_P) "$(DESTDIR)$$d" || exit 1; \
	done

install-data-local: installdirs-local

#
# Build man pages
#
MAN_FILTER = { \
    . ./version.sh && \
    sed -e "s|@SMARTMONTOOLS_GIT_VER_DESC@|$$SMARTMONTOOLS_GIT_VER_DESC|g" \
        -e "s|@SMARTMONTOOLS_GIT_REV_DATE@|$${SMARTMONTOOLS_GIT_REV_DATE:--}|g" \
        -e "s|@SMARTMONTOOLS_GIT_REV@|$${SMARTMONTOOLS_GIT_REV:--}|g" \
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
        -e 's|drivedb/7\.5|drivedb/$(DRIVEDB_BRANCH)|g' | \
    if test -n '$(drivedbdir)'; then \
      sed -e 's|/usr/local/share/smartmontools/drivedb\.h|$(drivedbinstdir)/drivedb.h|g' \
          -e 's|/usr/local/var/lib/smartmontools/drivedb\.h|$(drivedbdir)/drivedb.h|g'  ; \
    else \
      sed '/^\.\\" %IF ENABLE_DRIVEDB/,/^\.\\" %ENDIF ENABLE_DRIVEDB/ s,^,.\\"\# ,' ; \
    fi | \
    if test '$(drivedbinstdir)' != '$(drivedbdir)'; then \
      cat; \
    else \
      sed '/^\.\\" %IF ENABLE_DB_INSTALL/,/^\.\\" %ENDIF ENABLE_DB_INSTALL/ s,^,.\\"\# ,' ; \
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
    if test -n '$(SYSTEMD_LDADD)'; then \
      cat; \
    else \
      sed '/^\.\\" %IF ENABLE_SYSTEMD_NOTIFY/,/^\.\\" %ENDIF ENABLE_SYSTEMD_NOTIFY/ s,^,.\\"\# ,' ; \
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

smartctl.8: smartctl.8.in Makefile version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@

smartd.8: smartd.8.in Makefile version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@

smartd.conf.5: smartd.conf.5.in Makefile version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@

update-smart-drivedb.8: update-smart-drivedb.8.in Makefile version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/update-smart-drivedb.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/update-smart-drivedb.8.in > $@

# Build Solaris specific man pages
SOLARIS_MAN_FILTER = \
    sed -e '/^\.TH/s, \([58]\) , !!\1!! ,' \
        -e '/^\.BR/s, (\([578]\)), (!!\1!!),' \
        -e 's,\\fP(\([578]\)),\\fP(!!\1!!),g' \
        -e 's,!!5!!,4,g' -e 's,!!7!!,5,g' -e 's,!!8!!,1m,g' \
        -e 's,/var/log/messages,/var/adm/messages,g'

smartctl.1m: smartctl.8
	$(ECHO_V_GEN) ' $$(SOLARIS_MAN_FILTER) < smartctl.8 > $@'
	@$(SOLARIS_MAN_FILTER) < smartctl.8 > $@

smartd.1m: smartd.8
	$(ECHO_V_GEN) ' $$(SOLARIS_MAN_FILTER) < smartd.8 > $@'
	@$(SOLARIS_MAN_FILTER) < smartd.8 > $@

smartd.conf.4: smartd.conf.5
	$(ECHO_V_GEN) ' $$(SOLARIS_MAN_FILTER) < smartd.conf.5 > $@'
	@$(SOLARIS_MAN_FILTER) < smartd.conf.5 > $@

update-smart-drivedb.1m: update-smart-drivedb.8
	$(ECHO_V_GEN) ' $$(SOLARIS_MAN_FILTER) < update-smart-drivedb.8 > $@'
	@$(SOLARIS_MAN_FILTER) < update-smart-drivedb.8 > $@


# Convert man pages into .html, .pdf and .txt
MAN2HTML = man2html
#MAN2HTML = $(set_source_date_epoch) groff -man -Thtml
MAN2PDF = $(set_source_date_epoch) man -Tpdf -l
#MAN2PDF = $(set_source_date_epoch) groff -man -Tpdf

# Remove HTTP header and timestamp and fix links in man2html output
FIXHTML = sed -e '1s,^Content-type.*,,' \
              -e 's,<A HREF="[^"]*/man2html?\([1-8]\)+\(smart[cd][.a-z]*\)">,<A HREF="\2.\1.html">,g' \
              -e 's,<A HREF="[^"]*/man2html">,<A HREF=".">,g' \
              -e 's,<A HREF="[^"]*/man2html?[^"]*">\([^<]*\)</A>,\1,g' \
              -e 's,<A HREF="mailto:[^s][^m][^a][^"]*">\([^<]*\)</A>,\1,g' \
              -e '/^Time: [012][0-9]:[0-5][0-9]:[0-5][0-9] [^<]*$$/d'

# Avoid automake warning: '.PHONY was already defined in condition ...'
phony = cppcheck htmlman pdfman shellcheck
.PHONY: $(phony)

htmlman: smartctl.8.html smartd.8.html smartd.conf.5.html update-smart-drivedb.8.html

pdfman:  smartctl.8.pdf smartd.8.pdf smartd.conf.5.pdf update-smart-drivedb.8.pdf

smartctl.8.html: smartctl.8
	$(AM_V_GEN)$(MAN2HTML) smartctl.8 > $@.tmp
	@$(ECHO_V) ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

smartd.8.html: smartd.8
	$(AM_V_GEN)$(MAN2HTML) smartd.8 > $@.tmp
	@$(ECHO_V) ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

smartd.conf.5.html: smartd.conf.5
	$(AM_V_GEN)$(MAN2HTML) smartd.conf.5 > $@.tmp
	@$(ECHO_V) ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

update-smart-drivedb.8.html: update-smart-drivedb.8
	$(AM_V_GEN)$(MAN2HTML) update-smart-drivedb.8 > $@.tmp
	@$(ECHO_V) ' $$(FIXHTML) < $@.tmp > $@'
	@$(FIXHTML) < $@.tmp > $@

smartctl.8.pdf: smartctl.8
	$(AM_V_GEN)$(MAN2PDF) smartctl.8 > $@

smartd.8.pdf: smartd.8
	$(AM_V_GEN)$(MAN2PDF) smartd.8 > $@

smartd.conf.5.pdf: smartd.conf.5
	$(AM_V_GEN)$(MAN2PDF) smartd.conf.5 > $@

update-smart-drivedb.8.pdf: update-smart-drivedb.8
	$(AM_V_GEN)$(MAN2PDF) update-smart-drivedb.8 > $@

# Check drive database syntax
check:
	@if ./smartctl -P showall >/dev/null && \
	    ./smartctl -B $(srcdir)/drivedb.h -P showall >/dev/null; then \
	  echo "$(srcdir)/drivedb.h: OK"; \
	else \
	  echo "$(srcdir)/drivedb.h: Syntax check failed"; exit 1; \
	fi

# Create cppcheck report
cppcheck: cppcheck.txt

CPPCHECKFLAGS=
cppcheck.txt: $(smartctl_SOURCES) $(EXTRA_smartctl_SOURCES) \
              $(smartd_SOURCES) $(EXTRA_smartd_SOURCES)
	$(srcdir)/cppcheck.sh $(CPPCHECKFLAGS) > $@

# Create shellcheck report
shellcheck: shellcheck.txt

SHELLCHECK=shellcheck

# Exclude "SC3043 (warning): In POSIX sh, 'local' is undefined."
# Virtually all relevant /bin/sh shells support 'local'
SHELLCHECKFLAGS=-S warning -e SC3043

SHELLSCRIPTS= \
    $(top_srcdir)/autogen.sh \
    $(srcdir)/cppcheck.sh \
    $(srcdir)/do_release \
    $(srcdir)/getversion.sh \
    $(srcdir)/os_win32/pe32edit.sh \
    smartd_warning.sh \
    update-smart-drivedb

shellcheck.txt: $(SHELLSCRIPTS)
	$(ECHO_V_GEN) '  $(SHELLCHECK) $(SHELLCHECKFLAGS) $(SHELLSCRIPTS) > $@'
	@$(SHELLCHECK) $(SHELLCHECKFLAGS) $(SHELLSCRIPTS) > $@ || \
	if [ $$? = 1 ]; then echo "$@: issues found"; else rm -f $@; exit 1; fi


if OS_WIN32_MINGW
# Windows resources

os_win32/smartctl_res.o: os_win32/smartctl_res.rc $(os_win32_manifest)
	$(AM_V_GEN)$(WINDRES) os_win32/smartctl_res.rc $@

os_win32/smartd_res.o: os_win32/smartd_res.rc os_win32/syslogevt.rc $(os_win32_manifest)
	$(AM_V_GEN)$(WINDRES) os_win32/smartd_res.rc $@

os_win32/runcmda_res.o: os_win32/runcmda_res.rc os_win32/defadmin.manifest
	$(AM_V_GEN)$(WINDRES) os_win32/runcmda_res.rc $@

os_win32/runcmdu_res.o: os_win32/runcmdu_res.rc $(os_win32_manifest)
	$(AM_V_GEN)$(WINDRES) os_win32/runcmdu_res.rc $@

os_win32/wtssendmsg_res.o: os_win32/wtssendmsg_res.rc $(os_win32_manifest)
	$(AM_V_GEN)$(WINDRES) os_win32/wtssendmsg_res.rc $@

# Set description, name, version and year
WIN_RC_FILTER = { \
    . ./version.sh && \
    txtver=$$SMARTMONTOOLS_GIT_VER_WIN && binver=`echo "$$txtver" | sed 's|\.|,|g'` && \
    yy=`echo "$$SMARTMONTOOLS_GIT_REV_DATE" | sed -n 's,^20\([0-9][0-9]\).*$$,\1,p'` && yy="$${yy:-XX}" && \
    sed -e "s|@DESC@|$$d|" -e "s|@NAME@|$$n|" -e "s|@BINARY_VERSION@|$$binver|g" \
        -e "s|@TEXT_VERSION@|$$txtver|g" -e "s|@YY@|$$yy|g"; }

WIN_MAKE_RES = \
    $(ECHO_V) "n=$$n d=\"$$d"'"; $$(WIN_RC_FILTER) < $(srcdir)/os_win32/versioninfo.rc.in > $@'; \
    $(WIN_RC_FILTER) < $(srcdir)/os_win32/versioninfo.rc.in > $@

WIN_APP_MANIFEST = \
    if test -n '$(os_win32_manifest)'; then \
      $(ECHO_V) "echo '1 24 \"$(srcdir)/$(os_win32_manifest)\"' >> $@"; \
      echo '1 24 "$(srcdir)/$(os_win32_manifest)"' >> $@; \
    fi

os_win32/smartctl_res.rc: os_win32/versioninfo.rc.in Makefile version.sh
	$(AM_V_GEN)
	@n=smartctl d="Control and Monitor Utility for SMART Disks"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

os_win32/smartd_res.rc: os_win32/versioninfo.rc.in Makefile version.sh
	$(AM_V_GEN)
	@n=smartd d="SMART Disk Monitoring Daemon"; $(WIN_MAKE_RES)
	$(AM_V_at)echo '#include "./syslogevt.rc"' >> $@
	@$(WIN_APP_MANIFEST)

os_win32/runcmdu_res.rc: os_win32/versioninfo.rc.in Makefile version.sh
	$(AM_V_GEN)
	@n=runcmdu d="Run console command"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

os_win32/runcmda_res.rc: os_win32/versioninfo.rc.in Makefile version.sh
	$(AM_V_GEN)
	@n=runcmda d="Run console command as admin"; $(WIN_MAKE_RES)
	$(AM_V_at)echo '1 24 "os_win32/defadmin.manifest"' >> $@

os_win32/wtssendmsg_res.rc: os_win32/versioninfo.rc.in Makefile version.sh
	$(AM_V_GEN)
	@n=wtssendmsg d="Send console messages"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

os_win32/syslogevt.rc: os_win32/syslogevt.mc
	$(AM_V_GEN)
	@if test -n '$(WINDMC)'; then \
	  $(ECHO_V) '$(WINDMC) -b -h os_win32 -r os_win32 $(srcdir)/os_win32/syslogevt.mc'; \
	  $(WINDMC) -b -h os_win32 -r os_win32 $(srcdir)/os_win32/syslogevt.mc; \
	else \
	  $(ECHO_V) "echo '// MESSAGETABLE not available' > $@"; \
	  echo '// MESSAGETABLE not available' > $@; \
	fi

os_win32/defadmin.manifest: os_win32/default.manifest
	$(AM_V_GEN)sed 's,"asInvoker","requireAdministrator",' $(srcdir)/os_win32/default.manifest > $@

# Build Windows distribution

if OS_WIN64
win_bits = 64
else
win_bits = 32
endif

distdir_win32  = $(PACKAGE)-$(VERSION).win$(win_bits)
distzip_win32  = $(PACKAGE)-$(VERSION).win$(win_bits).zip
distinst_win32 = $(PACKAGE)-$(VERSION).win$(win_bits)-setup.exe

BINFILES_WIN32 = \
        smartctl.exe smartctl-nc.exe smartd.exe \
        os_win32/runcmda.exe os_win32/runcmdu.exe os_win32/wtssendmsg.exe \
        $(srcdir)/os_win32/smartd_mailer.ps1 \
        $(srcdir)/os_win32/smartd_mailer.conf.sample.ps1 \
        $(srcdir)/os_win32/smartd_warning.cmd

if ENABLE_DRIVEDB
BINFILES_WIN32 += \
        os_win32/update-smart-drivedb.ps1 \
        $(srcdir)/drivedb.h
endif

DOCFILES_WIN32 = \
        smartctl.8.html smartd.8.html smartd.conf.5.html \
        smartctl.8.pdf smartd.8.pdf smartd.conf.5.pdf \
        $(top_srcdir)/doc/AUTHORS \
        $(top_srcdir)/doc/CHANGELOG.md \
        $(top_srcdir)/COPYING \
        $(top_srcdir)/doc/INSTALL \
        $(top_srcdir)/doc/old/NEWS-5.0-7.5.txt \
        $(top_srcdir)/README.md \
        $(srcdir)/smartd.conf

CLEANFILES += \
        smartctl-nc.exe smartctl-nc.exe.tmp \
        os_win32/defadmin.manifest \
        os_win32/runcmda.exe os_win32/runcmda_res.rc \
        os_win32/runcmdu.exe os_win32/runcmdu_res.rc \
        os_win32/smartctl_res.rc os_win32/smartd_res.rc \
        os_win32/syslogevt.h \
        os_win32/syslogevt.rc os_win32/syslogevt_*.bin \
        os_win32/update-smart-drivedb.ps1 \
        os_win32/wtssendmsg.exe os_win32/wtssendmsg_res.rc

# Note: Only use without options to be compatible with all variants
UNIX2DOS = unix2dos

phony += cleandist-win32 dist-win32 distdir-win32 install-win32 installer-win32

dist-win32: $(distzip_win32)

install-win32: $(distinst_win32)
	./$(distinst_win32)

installer-win32: $(distinst_win32)

distdir-win32: $(distdir_win32).stamp

$(distzip_win32): $(distdir_win32).stamp
	$(AM_V_GEN)
	$(AM_V_at)rm -f $(distzip_win32)
	$(AM_V_at)cd $(distdir_win32) && zip -9 -q ../$(distzip_win32) bin/* doc/*
	$(AM_V_at)md5sum $@ > $@.md5
	$(AM_V_at)sha1sum $@ > $@.sha1
	$(AM_V_at)sha256sum $@ > $@.sha256
	@test -z '$(SOURCE_DATE_EPOCH)' \
	|| echo "Warning: '$(distzip_win32)' is not reproducible if SOURCE_DATE_EPOCH is used." >&2

# Build NSIS installer
# Note: Only option character '-' is also compatible with Linux version of makensis
$(distinst_win32): os_win32/installer.nsi Makefile os_win32/smartctl_res.rc $(distdir_win32).stamp
	$(AM_V_GEN)
	@test -n '$(MAKENSIS)' || ! echo 'makensis: command not found (https://nsis.sourceforge.io)'
	$(AM_V_at)test -z '$(builddir_win64)' || ( cd $(builddir_win64) && $(MAKE) distdir-win32 )
	@. ./version.sh && \
	version=$$SMARTMONTOOLS_GIT_VER_WIN && \
	yy=`echo "$$SMARTMONTOOLS_GIT_REV_DATE" | sed -n 's,^20\([0-9][0-9]\).*$$,\1,p'` && yy="$${yy:-XX}" && \
	verstr=$$SMARTMONTOOLS_GIT_VER_WIN && \
	d64= && if [ -n '$(builddir_win64)' ]; then d64='-DINPDIR64=$(builddir_win64)/$(PACKAGE)-$(VERSION).win64'; fi && \
	$(ECHO_V) "'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR='$$verstr' $(srcdir)/os_win32/installer.nsi" && \
	'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR="$$verstr" $(srcdir)/os_win32/installer.nsi
	$(AM_V_at)md5sum $@ > $@.md5
	$(AM_V_at)sha1sum $@ > $@.sha1
	$(AM_V_at)sha256sum $@ > $@.sha256

cleandist-win32:
	rm -f $(distdir_win32).stamp
	rm -rf $(distdir_win32)
	rm -f $(distinst_win32) $(distinst_win32).md5 $(distinst_win32).sha1 $(distinst_win32).sha256
	rm -f $(distzip_win32) $(distzip_win32).md5 $(distzip_win32).sha1 $(distzip_win32).sha256

# Build distribution directory.
# Strip all executables.  Convert all text files to CR/LF.
# Build checksums*.txt file.
# Fix header and file timestamps if reproducible build.
# The latter uses 'stat' and 'touch' enhancements from GNU coreutils.
$(distdir_win32).stamp: Makefile $(BINFILES_WIN32) $(DOCFILES_WIN32)
	@$(AM_V_P) || echo '  GEN      $(distdir_win32)/*/*'
	@rm -f $@
	$(AM_V_at)rm -rf $(distdir_win32)
	$(AM_V_at)$(MKDIR_P) $(distdir_win32)/bin
	$(AM_V_at)$(MKDIR_P) $(distdir_win32)/doc
	@vrun() { $(ECHO_V) "$$@" && eval "$$@"; } && \
	strip='$(STRIP)' && strip=$${strip:-strip} && \
	for src in $(BINFILES_WIN32); do \
	  dst=$(distdir_win32)/bin/$${src##*/} && \
	  case $${src##*/} in \
	    *.exe) vrun $$strip -o $$dst $$src ;; \
	    *) vrun $(UNIX2DOS) \< $$src \> $$dst && vrun touch -r $$src $$dst ;; \
	  esac || exit 1; \
	done && \
	for src in $(DOCFILES_WIN32); do \
	  dst=$(distdir_win32)/doc/$${src##*/} && \
	  case $${src##*/} in [A-Z]*[A-Z]) dst=$$dst.txt ;; esac && \
	  case $${src##*/} in \
	    *.pdf) vrun cp -p $$src $$dst ;; \
	    *) vrun $(UNIX2DOS) \< $$src \> $$dst && vrun touch -r $$src $$dst ;; \
	   esac || exit 1; \
	done && \
	if [ -n '$(SOURCE_DATE_EPOCH)' ]; then \
	  for exe in $(distdir_win32)/bin/*.exe; do \
	    vrun $(srcdir)/os_win32/pe32edit.sh -t $(SOURCE_DATE_EPOCH) $$exe || exit 1; \
	  done; \
	fi && \
	$(ECHO_V) ' $$(MD5_SHA1_SHA256_SUM) *.exe *.cmd *.ps1 > $(distdir_win32)/doc/checksums$(win_bits).txt' && \
	(cd $(distdir_win32)/bin && for algo in md5 sha1 sha256; do $${algo}sum *.exe *.cmd *.ps1; done) \
	| $(UNIX2DOS) > $(distdir_win32)/doc/checksums$(win_bits).txt && \
	if [ -n '$(SOURCE_DATE_EPOCH)' ]; then \
	  touch -d @$(SOURCE_DATE_EPOCH) source_date.stamp && \
	  find $(distdir_win32) -newer source_date.stamp | while read f; do \
	    vrun touch -r source_date.stamp $$f || exit 1; \
	  done && \
	  rm -f source_date.stamp; \
	fi
	@touch $@

# Build non-console version of smartctl for GSmartControl.
smartctl-nc.exe: smartctl.exe
	$(AM_V_GEN)
	@rm -f $@
	$(AM_V_at)cp -p smartctl.exe $@.tmp
	$(AM_V_at)$(srcdir)/os_win32/pe32edit.sh -s 2 $@.tmp
	$(AM_V_at)mv -f $@.tmp $@

# Build runcmd?.exe and wtssendmsg.exe
os_win32/runcmd.o: os_win32/runcmd.c
	$(AM_V_CC)$(CC) -c -o $@ -Os $(srcdir)/os_win32/runcmd.c

os_win32/runcmdu.exe: os_win32/runcmd.o os_win32/runcmdu_res.o
	$(AM_V_CCLD)$(CC) -o $@ os_win32/runcmd.o os_win32/runcmdu_res.o

os_win32/runcmda.exe: os_win32/runcmd.o os_win32/runcmda_res.o
	$(AM_V_CCLD)$(CC) -o $@ os_win32/runcmd.o os_win32/runcmda_res.o

os_win32/wtssendmsg.exe: os_win32/wtssendmsg.c os_win32/wtssendmsg_res.o
	$(AM_V_CCLD)$(CC) -Os -o $@ $(srcdir)/os_win32/wtssendmsg.c os_win32/wtssendmsg_res.o -lwtsapi32

# Build drivedb.h update script
os_win32/update-smart-drivedb.ps1: os_win32/update-smart-drivedb.ps1.in ../config.status
	$(AM_V_GEN)$(SHELL) ../config.status -q --file=$@:$(srcdir)/os_win32/update-smart-drivedb.ps1.in

# MSVC Version to change in make command line
vc = 17
vcver = $(vc)

phony += check-vc-version clean-vc config-vc distclean-vc maintainer-clean-vc

check-vc-version:
	@case '$(vcver)' in \
	  17) test -d '$(srcdir)/os_win32/vc$(vcver)' || \
	    { echo '$(srcdir)/os_win32/vc$(vcver): Not found (not included in src tarball)' >&2; exit 1; } ;; \
	  *) echo 'Usage: $(MAKE) vc=17 config-vc clean-vc distclean-vc maintainer-clean-vc'; exit 1 ;; \
	esac

# Build os_win32/vcNN/{config.h,smart*.rc,version.h} for MSVCNN from MinGW files

CONFIG_VC_FILES = \
        $(srcdir)/os_win32/vc$(vcver)/config.h \
        $(srcdir)/os_win32/vc$(vcver)/smartctl_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/smartd_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/version.h

config-vc: check-vc-version $(CONFIG_VC_FILES)

$(srcdir)/os_win32/vc$(vcver)/config.h: config.h Makefile
	@! test -f $(srcdir)/config.h || ! echo '$(MAKE) config-vc: does not work if run from configured source directory'
	$(AM_V_GEN)sed \
	  -e '1i/* os_win32/vc$(vcver)/config.h.  Generated from config.h by Makefile.  */' \
	  -e 's,^#define HAVE_\(ATTR_PACKED\|CLOCK_GETTIME\|GETTIMEOFDAY\|__INT128\|LONG_DOUBLE_WIDER\|STRINGS_H\|UNISTD_H\) 1.*$$,/* #undef HAVE_\1 */ /* VC$(vcver) */,' \
	  -e 's,^\(#define SMARTMONTOOLS_BUILD_HOST "[^-]*\)[^"]*,\1-pc-w32vc$(vcver),' config.h > $@

$(srcdir)/os_win32/vc$(vcver)/version.h: version.h
	$(AM_V_GEN)cp version.h $@

$(srcdir)/os_win32/vc$(vcver)/smartctl_res.rc: os_win32/smartctl_res.rc
	$(AM_V_GEN)sed '/^1 24 /d' os_win32/smartctl_res.rc > $@

$(srcdir)/os_win32/vc$(vcver)/smartd_res.rc: os_win32/smartd_res.rc
	$(AM_V_GEN)sed '/^1 24 /d' os_win32/smartd_res.rc > $@

clean-vc: check-vc-version
	rm -f $(srcdir)/os_win32/vc$(vcver)/.vs/smartmontools/v$(vcver)/Browse.VC.opendb
	rm -f $(srcdir)/os_win32/vc$(vcver)/.vs/smartmontools/v$(vcver)/*.VC.db*
	rm -f $(srcdir)/os_win32/vc$(vcver)/.vs/smartmontools/v$(vcver)/DocumentLayout*.json
	rm -rf $(srcdir)/os_win32/vc$(vcver)/.vs/smartmontools/v$(vcver)/ipch
	rm -rf $(srcdir)/os_win32/vc$(vcver)/.vs/smartmontools/FileContentIndex
	rm -f $(srcdir)/os_win32/vc$(vcver)/syslogevt.h
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Debug
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Debug-static
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Release
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Release-static
	rm -rf $(srcdir)/os_win32/vc$(vcver)/x64

distclean-vc: clean-vc
	rm -f $(CONFIG_VC_FILES)

maintainer-clean-vc: distclean-vc
	rm -f $(srcdir)/os_win32/vc$(vcver)/*.vcxproj.user
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
	@( cd $(distdir_darwin)/root && find .|xargs touch -d "@${SOURCE_DATE_EPOCH}" && find . | cpio --reproducible -o --format odc --owner 0:80 | gzip -n -c ) > $(distdir_darwin)/pkg/Payload
	PAYLOAD_FILES=`find $(distdir_darwin)/root | wc -l` &&\
	PAYLOAD_SIZEKB=`du -BK  -s $(distdir_darwin)/root|${AWK} '{print $$1}'|tr -d 'K'` &&\
	sed -e "s|@version@|$(VERSION)|" -e "s|@files@|$${PAYLOAD_FILES}|" \
		-e "s|@size@|$${PAYLOAD_SIZEKB}|" $(srcdir)/os_darwin/pkg/PackageInfo.in \
		> $(distdir_darwin)/pkg/PackageInfo &&\
	sed -e "s|@version@|$(VERSION)|" -e "s|@files@|$${PAYLOAD_FILES}|" -e "s|@size@|$${PAYLOAD_SIZEKB}|" \
		-e "s|@pkgname@|$(pkg_darwin)|" \
		$(srcdir)/os_darwin/pkg/Distribution.in > $(distdir_darwin)/pkg/Distribution
	@mkdir -p $(distdir_darwin)/pkg/Resources/English.lproj
	@cp $(top_srcdir)/COPYING $(distdir_darwin)/pkg/Resources/English.lproj/license.txt
	@mkbom -u 0 -g 80 $(distdir_darwin)/root $(distdir_darwin)/pkg/Bom
	@mkdir -p $(distdir_darwin)/dmg
	@( cd $(distdir_darwin)/pkg && xar --distribution --compression none -cf "../dmg/$(pkg_darwin)" * )

# build darwon dmg image
$(dmg_darwin):
	@cp $(srcdir)/os_darwin/pkg/installer/README.html $(distdir_darwin)/dmg
	@find $(distdir_darwin)/dmg | xargs touch -d "@${SOURCE_DATE_EPOCH}"
	@xorriso -as mkisofs -V 'smartmontools' -no-pad -r -o $(distdir_darwin)/smartmontools-$(VERSION).iso \
		-hfs-bless "$(distdir_darwin)/dmg/" "$(distdir_darwin)/dmg/"
	@dmg dmg $(distdir_darwin)/smartmontools-$(VERSION).iso $(dmg_darwin)
	md5sum $@ > $@.md5
	sha1sum $@ > $@.sha1
	sha256sum $@ > $@.sha256

install-darwin: install-darwin-cleanup $(pkg_darwin) $(dmg_darwin)

install-darwin-cleanup: 
	@rm -rf $(distdir_darwin)
endif
