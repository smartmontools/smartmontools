## Process this file with automake to produce Makefile.in
#
# src/Makefile.am
#

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
        -DSMARTMONTOOLS_SMARTDSCRIPTDIR='"$(smartdscriptdir)"' \
         -I ../include -I$(top_srcdir)/include

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
        ataidentify.cpp \
        ataidentify.h \
        ataprint.cpp \
        ataprint.h \
        farmprint.cpp \
        farmprint.h \
        nvmeprint.cpp \
        nvmeprint.h \
        scsiprint.cpp \
        scsiprint.h

smartctl_LDADD = ../lib/libsmartmon.la $(os_libs)
smartctl_DEPENDENCIES = ../lib/libsmartmon.la

EXTRA_smartctl_SOURCES =

if OS_WIN32_MINGW

smartctl_LDADD        += os_win32/smartctl_res.o
smartctl_DEPENDENCIES += os_win32/smartctl_res.o

endif


smartd_SOURCES = \
        smartd.cpp

smartd_LDADD = ../lib/libsmartmon.la $(os_libs) $(CAPNG_LDADD) $(SYSTEMD_LDADD)
smartd_DEPENDENCIES = ../lib/libsmartmon.la

EXTRA_smartd_SOURCES =

if OS_POSIX

smartd_SOURCES += \
        popen_as_ugid.cpp \
        popen_as_ugid.h

endif

if OS_WIN32_MINGW

smartd_SOURCES += \
        os_win32/daemon_win32.cpp \
        os_win32/daemon_win32.h \
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

all_local =

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

all_local += $(extra_MANS)

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

doc_DATA = \
        smartd.conf

# Avoid automake warning: '.PHONY was already defined in condition ...'
phony = install-smartd_conf uninstall-smartd_conf
.PHONY: $(phony)

install_data_local = install-smartd_conf
uninstall_local = uninstall-smartd_conf

# If modified smartd.conf exists install smartd.conf.sample instead
install-smartd_conf:
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
uninstall-smartd_conf:
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
        clang-scan-build.sh \
        cppcheck.sh \
        getversion.sh \
        smartd.initd.in \
        smartd.cygwin.initd.in \
        smartd.freebsd.initd.in \
        smartd.8.in \
        smartctl.8.in \
        smartd.conf.5.in \
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
        $(doc_DATA)

CLEANFILES = \
        cppcheck.txt \
        com.smartmontools.smartd.plist \
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
        update-smart-drivedb \
        update-smart-drivedb.8 \
        update-smart-drivedb.1m \
        update-smart-drivedb.8.html \
        update-smart-drivedb.8.html.tmp \
        update-smart-drivedb.8.pdf \
        update-smart-drivedb.8.txt

# 'make maintainer-clean' also removes files generated by './autogen.sh'
MAINTAINERCLEANFILES = \
        $(srcdir)/Makefile.in

update-smart-drivedb: update-smart-drivedb.in ../config.status
	$(AM_V_GEN)$(SHELL) ../config.status -q --file=$@:$(srcdir)/update-smart-drivedb.in
	$(AM_V_at)chmod +x $@

smartd_warning.sh: smartd_warning.sh.in ../config.status
	$(AM_V_GEN)$(SHELL) ../config.status -q --file=$@:$(srcdir)/smartd_warning.sh.in
	$(AM_V_at)chmod +x $@

com.smartmontools.smartd.plist: os_darwin/com.smartmontools.smartd.plist.in Makefile
	$(AM_V_GEN)sed 's|/usr/sbin/|$(sbindir)/|' $(srcdir)/os_darwin/com.smartmontools.smartd.plist.in > $@

$(initdfile): $(initdfile).in Makefile
	$(AM_V_GEN)sed 's|/usr/local/sbin/|$(sbindir)/|g' $(srcdir)/$(initdfile).in > $@

if INSTALL_INITSCRIPT
if OS_DARWIN

initd_DATA = com.smartmontools.smartd.plist

else

phony += install-initd uninstall-initd
all_local += $(initdfile)
install_data_local += install-initd
uninstall_local += uninstall-initd

install-initd: $(initdfile)
	$(MKDIR_P) $(DESTDIR)$(initddir)
	$(INSTALL_SCRIPT) $(initdfile) $(DESTDIR)$(initddir)/smartd$(smartd_suffix)

uninstall-initd:
	rm -f $(DESTDIR)$(initddir)/smartd$(smartd_suffix)

endif
endif

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

all-local: $(all_local)
install-data-local: $(install_data_local) installdirs-local
uninstall-local: $(uninstall_local)

#
# Build man pages
#
MAN_FILTER = { \
    . ../include/smartmon/version.sh && \
    sed -e "s|@SMARTMONTOOLS_GIT_VER_DESC@|$$SMARTMONTOOLS_GIT_VER_DESC|g" \
        -e "s|@SMARTMONTOOLS_GIT_REV_DATE@|$${SMARTMONTOOLS_GIT_REV_DATE:--}|g" \
        -e "s|@SMARTMONTOOLS_GIT_REV@|$${SMARTMONTOOLS_GIT_REV:--}|g" \
        -e 's|/usr/local/share/man/|$(mandir)/|g' \
        -e 's|/usr/local/sbin/|$(sbindir)/|g' \
        -e 's|/usr/local/share/doc/smartmontools/examplescripts/|!exampledir!|g' \
        -e 's|/usr/local/share/doc/smartmontools/|$(docdir)/|g' \
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

smartctl.8: smartctl.8.in Makefile ../include/smartmon/version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartctl.8.in > $@

smartd.8: smartd.8.in Makefile ../include/smartmon/version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.8.in > $@

smartd.conf.5: smartd.conf.5.in Makefile ../include/smartmon/version.sh
	$(ECHO_V_GEN) ' $$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@'
	@$(MAN_FILTER) < $(srcdir)/smartd.conf.5.in > $@

update-smart-drivedb.8: update-smart-drivedb.8.in Makefile ../include/smartmon/version.sh
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

phony += cppcheck htmlman pdfman shellcheck

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

# Default check target
if REALLY_CROSS_COMPILING
check:
	@echo "make check: unavailable if cross-compiling"
else
# Show '-V' output and check drive database syntax
check:
	./smartctl -V
	@echo "./smartctl -B $(top_srcdir)/lib/drivedb.h -P showall >/dev/null"
	@if ./smartctl -P showall >/dev/null && \
	    ./smartctl -B $(top_srcdir)/lib/drivedb.h -P showall >/dev/null; then \
	  echo "$(top_srcdir)/lib/drivedb.h: OK"; \
	else \
	  echo "$(top_srcdir)/lib/drivedb.h: Syntax check failed"; exit 1; \
	fi
endif

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
    $(srcdir)/clang-scan-build.sh \
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
    . ../include/smartmon/version.sh && \
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

os_win32/smartctl_res.rc: os_win32/versioninfo.rc.in Makefile ../include/smartmon/version.sh
	$(AM_V_GEN)
	@n=smartctl d="Control and Monitor Utility for SMART Disks"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

os_win32/smartd_res.rc: os_win32/versioninfo.rc.in Makefile ../include/smartmon/version.sh
	$(AM_V_GEN)
	@n=smartd d="SMART Disk Monitoring Daemon"; $(WIN_MAKE_RES)
	$(AM_V_at)echo '#include "./syslogevt.rc"' >> $@
	@$(WIN_APP_MANIFEST)

os_win32/runcmdu_res.rc: os_win32/versioninfo.rc.in Makefile ../include/smartmon/version.sh
	$(AM_V_GEN)
	@n=runcmdu d="Run console command"; $(WIN_MAKE_RES)
	@$(WIN_APP_MANIFEST)

os_win32/runcmda_res.rc: os_win32/versioninfo.rc.in Makefile ../include/smartmon/version.sh
	$(AM_V_GEN)
	@n=runcmda d="Run console command as admin"; $(WIN_MAKE_RES)
	$(AM_V_at)echo '1 24 "os_win32/defadmin.manifest"' >> $@

os_win32/wtssendmsg_res.rc: os_win32/versioninfo.rc.in Makefile ../include/smartmon/version.sh
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
        $(top_srcdir)/lib/drivedb.h
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

# Variable to pass '-DEXPORT_UNINST' and then '-DIMPORT_UNINST' to 'makensis' for code signing
MAKENSISFLAGS =

# Build NSIS installer
# Note: Only option character '-' is also compatible with Linux version of makensis
$(distinst_win32): os_win32/installer.nsi Makefile os_win32/smartctl_res.rc $(distdir_win32).stamp
	$(AM_V_GEN)
	@test '$(enable_static_link)' = 'yes' || ! echo "$(distinst_win32): requires '--enable-static-link'"
	@test -n '$(MAKENSIS)' || ! echo 'makensis: command not found (https://nsis.sourceforge.io)'
	$(AM_V_at)test -z '$(builddir_win64)' || ( cd $(builddir_win64) && $(MAKE) distdir-win32 )
	@. ../include/smartmon/version.sh && \
	version=$$SMARTMONTOOLS_GIT_VER_WIN && \
	yy=`echo "$$SMARTMONTOOLS_GIT_REV_DATE" | sed -n 's,^20\([0-9][0-9]\).*$$,\1,p'` && yy="$${yy:-XX}" && \
	verstr=$$SMARTMONTOOLS_GIT_VER_WIN && \
	d64= && if [ -n '$(builddir_win64)' ]; then d64='-DINPDIR64=$(builddir_win64)/$(PACKAGE)-$(VERSION).win64'; fi && \
	$(ECHO_V) "'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR='$$verstr' $(MAKENSISFLAGS) $(srcdir)/os_win32/installer.nsi" && \
	'$(MAKENSIS)' -V2 -NOCD -DINPDIR=$(distdir_win32) $$d64 -DOUTFILE=$@ -DVERSION=$$version -DYY=$$yy -DVERSTR="$$verstr" $(MAKENSISFLAGS) $(srcdir)/os_win32/installer.nsi
	$(AM_V_at)test -z '$(SOURCE_DATE_EPOCH)' || touch -d @$(SOURCE_DATE_EPOCH) $@
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

# Build os_win32/vcNN/{config.h,smart*.rc,smarmon/*.h} for MSVCNN from MinGW files

CONFIG_VC_FILES = \
        $(srcdir)/os_win32/vc$(vcver)/config.h \
        $(srcdir)/os_win32/vc$(vcver)/smartctl_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/smartd_res.rc \
        $(srcdir)/os_win32/vc$(vcver)/smartmon/smartmon_config.h \
        $(srcdir)/os_win32/vc$(vcver)/smartmon/version.h

config-vc: check-vc-version $(CONFIG_VC_FILES)

# Adjust MinGW-w64 [smartmon_]config.h for MSVC
CONFIG_VC_FILTER = { \
    if test -f $(top_srcdir)/config.h; then \
      echo '$(MAKE) config-vc: does not work if run from configured source directory' >&2; exit 1; \
    fi && \
    echo '/* os_win32/vc$(vcver)/[smartmon_]config.h.  Generated from [smartmon_]config.h by Makefile.  */' && \
    sed -e 's/^\#define \([SMARTON_]*HAVE_\(ATTR_PACKED\|__INT128\|LONG_DOUBLE_WIDER\|STRINGS_H\|UNISTD_H\)\) 1.*$$/\/* \#undef \1 *\/ \/* VC$(vcver) *\//' \
        -e 's/^\(\#define SMARTMONTOOLS_BUILD_HOST "[^-]*\)[^"].*$$/\1-pc-w32vc$(vcver)"/'; }

$(srcdir)/os_win32/vc$(vcver)/config.h: ../config.h Makefile
	$(ECHO_V_GEN) ' $$(CONFIG_VC_FILTER) < ../config.h > $@.tmp'
	@$(CONFIG_VC_FILTER) < ../config.h > $@.tmp
	$(AM_V_at)mv -f $@.tmp $@

$(srcdir)/os_win32/vc$(vcver)/smartmon/smartmon_config.h: ../include/smartmon/smartmon_config.h Makefile
	$(ECHO_V_GEN) ' $$(CONFIG_VC_FILTER) < ../include/smartmon/smartmon_config.h > $@.tmp'
	$(AM_V_at)$(MKDIR_P) $(srcdir)/os_win32/vc$(vcver)/smartmon
	@$(CONFIG_VC_FILTER) < ../include/smartmon/smartmon_config.h > $@.tmp
	$(AM_V_at)mv -f $@.tmp $@

$(srcdir)/os_win32/vc$(vcver)/smartmon/version.h: ../include/smartmon/version.h
	$(AM_V_at)$(MKDIR_P) $(srcdir)/os_win32/vc$(vcver)/smartmon
	$(AM_V_GEN)cp ../include/smartmon/version.h $@

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
	rm -f $(srcdir)/os_win32/vc$(vcver)/config.h.tmp
	rm -f $(srcdir)/os_win32/vc$(vcver)/smartmon/smartmon_config.h.tmp
	rm -f $(srcdir)/os_win32/vc$(vcver)/syslogevt.h
	rm -rf $(srcdir)/os_win32/vc$(vcver)/ARM64
	rm -rf $(srcdir)/os_win32/vc$(vcver)/Win32
	rm -rf $(srcdir)/os_win32/vc$(vcver)/x64

distclean-vc: clean-vc
	rm -f $(CONFIG_VC_FILES)
	rm -rf $(srcdir)/os_win32/vc$(vcver)/smartmon

maintainer-clean-vc: distclean-vc
	rm -f $(srcdir)/os_win32/vc$(vcver)/*.vcxproj.user
	rm -f $(srcdir)/os_win32/vc$(vcver)/examples/*.vcxproj.user
	rm -rf $(srcdir)/os_win32/vc$(vcver)/.vs

endif


if OS_DARWIN
# Definitions for OSX distribution
distdir_darwin  = $(PACKAGE)-$(VERSION).darwin
dmg_darwin =  $(PACKAGE)-$(VERSION).dmg
pkg_darwin = $(PACKAGE)-$(VERSION).pkg

# build darwin installer 
$(pkg_darwin):
	${MAKE} install DESTDIR="$$(pwd)/$(distdir_darwin)/root"
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
	-test -z '$(SOURCE_DATE_EPOCH)' || touch -d @$(SOURCE_DATE_EPOCH) $@
	md5sum $@ > $@.md5
	sha1sum $@ > $@.sha1
	sha256sum $@ > $@.sha256

install-darwin: install-darwin-cleanup $(pkg_darwin) $(dmg_darwin)

install-darwin-cleanup: 
	@rm -rf $(distdir_darwin)
endif
