## Process this file with automake to produce Makefile.in
#
# $Id: Makefile.am,v 1.95 2009/03/22 17:17:39 chrfranke Exp $
#

@SET_MAKE@

# Make sure .cpp takes precedence to avoid compiling old .c file
SUFFIXES = .cpp .c .s .o

# BUILD_INFO can be provided by package maintainers (see INSTALL file)
BUILD_INFO= "(local build)"

AM_CPPFLAGS = -DBUILD_INFO='$(BUILD_INFO)' -DSMARTMONTOOLS_SYSCONFDIR='"$(sysconfdir)"'
if ENABLE_DRIVEDB
AM_CPPFLAGS += -DSMARTMONTOOLS_DRIVEDBDIR='"$(drivedbdir)"'
endif
if ENABLE_SAVESTATES
AM_CPPFLAGS += -DSMARTMONTOOLS_SAVESTATES='"$(savestates)"'
endif

sbin_PROGRAMS = smartd 	\
		smartctl

smartd_SOURCES =  smartd.cpp      \
                  atacmdnames.cpp \
                  atacmdnames.h   \
                  atacmds.cpp     \
                  atacmds.h       \
                  dev_ata_cmd_set.cpp \
                  dev_ata_cmd_set.h   \
                  dev_interface.cpp   \
                  dev_interface.h     \
                  dev_tunnelled.h     \
                  extern.h        \
                  int64.h         \
                  knowndrives.cpp \
                  knowndrives.h   \
                  scsicmds.cpp    \
                  scsicmds.h      \
                  scsiata.cpp     \
                  scsiata.h       \
                  utility.cpp     \
                  utility.h

smartd_LDADD = @os_deps@ @os_libs@
smartd_DEPENDENCIES = @os_deps@

EXTRA_smartd_SOURCES = os_darwin.cpp    \
                       os_darwin.h      \
                       os_linux.cpp     \
                       os_linux.h       \
                       os_freebsd.cpp   \
                       os_freebsd.h     \
                       os_netbsd.cpp    \
                       os_netbsd.h      \
                       os_openbsd.cpp   \
                       os_openbsd.h     \
                       os_solaris.cpp   \
                       os_solaris.h     \
                       os_solaris_ata.s \
                       os_win32.cpp     \
                       os_generic.cpp   \
                       os_generic.h     \
                       cciss.cpp        \
                       cciss.h          \
                       dev_legacy.cpp


if OS_WIN32_MINGW

smartd_SOURCES +=                           \
                posix/regex.h               \
                posix/regex.c               \
                os_win32/daemon_win32.h     \
                os_win32/daemon_win32.cpp   \
                os_win32/hostname_win32.h   \
                os_win32/hostname_win32.cpp \
                os_win32/syslog.h           \
                os_win32/syslog_win32.cpp

# Included by regex.c:
EXTRA_smartd_SOURCES +=                   \
                posix/regcomp.c           \
                posix/regexec.c           \
                posix/regex_internal.c    \
                posix/regex_internal.h

endif

smartctl_SOURCES= smartctl.cpp    \
                  smartctl.h      \
                  atacmdnames.cpp \
                  atacmdnames.h   \
                  atacmds.cpp     \
                  atacmds.h       \
                  ataprint.cpp    \
                  ataprint.h      \
                  dev_ata_cmd_set.cpp \
                  dev_ata_cmd_set.h   \
                  dev_interface.cpp   \
                  dev_interface.h     \
                  dev_tunnelled.h     \
                  extern.h        \
                  int64.h         \
                  knowndrives.cpp \
                  knowndrives.h   \
                  scsicmds.cpp    \
                  scsicmds.h      \
                  scsiata.cpp     \
                  scsiata.h       \
                  scsiprint.cpp   \
                  scsiprint.h     \
                  utility.cpp     \
                  utility.h


smartctl_LDADD = @os_deps@ @os_libs@
smartctl_DEPENDENCIES = @os_deps@

EXTRA_smartctl_SOURCES = os_linux.cpp \
                       os_linux.h     \
                       os_freebsd.cpp \
                       os_freebsd.h   \
                       os_netbsd.cpp  \
                       os_netbsd.h    \
                       os_openbsd.cpp \
                       os_openbsd.h   \
                       os_solaris.cpp \
                       os_solaris.h   \
                       os_win32.cpp   \
                       os_generic.cpp \
                       os_generic.h   \
                       cciss.cpp      \
                       cciss.h        \
                       dev_legacy.cpp

if OS_WIN32_MINGW

smartctl_SOURCES +=                    \
                posix/regex.h          \
                posix/regex.c          \
                os_win32/syslog.h

# Included by regex.c:
EXTRA_smartctl_SOURCES +=              \
                posix/regcomp.c        \
                posix/regexec.c        \
                posix/regex_internal.c \
                posix/regex_internal.h

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
install-man: $(extra_MANS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(mandir)/man4
	$(mkinstalldirs) $(DESTDIR)$(mandir)/man1m
	for i in $(extra_MANS); do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(mandir)/man$$ext/$$inst; \
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
	  echo " rm -f $(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	  rm -f $(DESTDIR)$(mandir)/man$$ext/$$inst; \
	done
%.1m: %.8
	awk '/^.TH/ {$$3="1m"} {print}' < $< | \
	sed -e 's/smartd\.conf\(.*\)(5)/smartd.conf\1(4)/g' \
            -e 's/syslog\.conf\(.*\)(5)/syslog.conf\1(4)/g' \
	    -e 's/smartctl\(.*\)(8)/smartctl\1(1m)/g' \
	    -e 's/syslogd\(.*\)(8)/syslogd\1(1m)/g' \
            -e 's|/var/log/messages|/var/adm/messages|g' \
	    -e 's/smartd\(.*\)(8)/smartd\1(1m)/g' > $@
%.4: %.5
	awk '/^.TH/ {$$3="4"}  {print}' < $< | \
	sed -e 's/smartd\.conf\(.*\)(5)/smartd.conf\1(4)/g' \
            -e 's/syslog\.conf\(.*\)(5)/syslog.conf\1(4)/g' \
	    -e 's/smartctl\(.*\)(8)/smartdctl\1(1m)/g' \
	    -e 's/syslogd\(.*\)(8)/syslogd\1(1m)/g' \
            -e 's|/var/log/messages|/var/adm/messages|g' \
	    -e 's/smartd\(.*\)(8)/smartd\1(1m)/g' > $@
else
# For systems that adopts traditional manner
man_MANS =        smartd.conf.5 \
                  smartctl.8    \
                  smartd.8
endif

docsdir=$(docdir)
docs_DATA = AUTHORS     \
            CHANGELOG   \
            COPYING     \
            INSTALL     \
            NEWS        \
            README      \
            TODO        \
            WARNINGS    \
            smartd.conf

sysconf_DATA = smartd.conf$(smartd_suffix)

if SMARTD_SUFFIX
smartd.conf$(smartd_suffix): smartd.conf
	cp ${srcdir}/smartd.conf smartd.conf$(smartd_suffix)
endif

EXTRA_DIST = smartd.initd.in                       \
             smartd.8.in                           \
             smartctl.8.in                         \
             smartd.conf.5.in                      \
             smartd.conf                           \
             autogen.sh                            \
             os_darwin/SMART.in                    \
             os_darwin/StartupParameters.plist     \
             os_darwin/English_Localizable.strings \
             os_win32/installer.nsi                \
             $(docs_DATA)

CLEANFILES = cvsversion.h       \
             drivedb.h          \
             smartd.conf.5      \
             smartd.conf.4      \
             smartd.8           \
             smartd.1m          \
             smartd.8.html      \
             smartd.8.txt       \
             smartctl.8         \
             smartctl.1m        \
             smartctl.8.html    \
             smartctl.8.txt     \
             smartd.conf.5.html \
             smartd.conf.5.txt  \
             smartd.initd 	\
             SMART

if SMARTD_SUFFIX
CLEANFILES += smartd.conf$(smartd_suffix)
endif

CVSVERSION_DEPS = CHANGELOG Makefile
if IS_CVS_BUILD
# Rebuild cvsversion.h after cvs update or commit
CVSVERSION_DEPS += $(srcdir)/CVS/Entries
endif

utility.o: cvsversion.h

cvsversion.h: $(CVSVERSION_DEPS)
	echo '/* cvsversion.h.  Generated by Makefile.  */' > $@
	(cd $(srcdir) && cat Makefile.am configure.in smart*.in *.cpp *.h *.s) \
	| sed -n 's,^.*\$$[I][d]: [^ ]* [^ ]* \(20[01][0-9]/[01][0-9]/[0-3][0-9] [0-9][0-9]:[0-9][0-9]\):[0-9][0-9] [^$$]*\$$.*$$,\1,p' \
	| sort -r \
	| sed 's,/,-,g;s,^\(.*\)$$,#define SMARTMONTOOLS_CVS_DATE_TIME "\1",;1q' \
	>> $@


# Drive Database
drivedb.h: knowndrives.cpp Makefile
	echo '/* drivedb.h.  Generated from knowndrives.cpp by Makefile.  */' > $@
	sed '1,/^\/\/ BEGIN drivedb.h/d;/^\/\/ END drivedb.h/,$$d;s/^  //' $(srcdir)/knowndrives.cpp >> $@

if ENABLE_DRIVEDB
drivedb_DATA = drivedb.h
endif

if ENABLE_SAVESTATES
# Create $(savestatesdir) only
savestates_DATA =
endif

smartd.conf.5.in: smartd.8.in
	sed '1,/STARTINCLUDE/ D;/ENDINCLUDE/,$$D' < $(srcdir)/smartd.8.in > $(top_builddir)/tmp.directives
	sed '/STARTINCLUDE/,$$D'  < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.head
	sed '1,/ENDINCLUDE/D'   < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.tail
	cat $(top_builddir)/tmp.head > $(srcdir)/smartd.conf.5.in
	echo '.\" STARTINCLUDE' >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.directives >> $(srcdir)/smartd.conf.5.in
	echo '.\" ENDINCLUDE'   >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.tail >> $(srcdir)/smartd.conf.5.in
	rm -f $(top_builddir)/tmp.head $(top_builddir)/tmp.tail $(top_builddir)/tmp.directives

if INSTALL_INITSCRIPT
if OS_DARWIN
initd_DATA = SMART                            \
	os_darwin/StartupParameters.plist     \
	os_darwin/English_Localizable.strings

initd_install_name = SMART

initd_DATA_install = install-initdDATA-darwin
initd_DATA_uninstall = uninstall-initdDATA-darwin

SMART : os_darwin/SMART.in
	sed "s|/usr/sbin/|$(sbindir)/|" $< > $@

install-initdDATA-darwin: $(initd_DATA)
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(mkinstalldirs) $(DESTDIR)$(initddir)/SMART
	$(mkinstalldirs) $(DESTDIR)$(initddir)/SMART/Resources
	$(INSTALL_SCRIPT) $(top_builddir)/SMART $(DESTDIR)$(initddir)/SMART
	$(INSTALL_DATA) $(srcdir)/os_darwin/StartupParameters.plist \
	    $(DESTDIR)$(initddir)/SMART/StartupParameters.plist
	for i in English ; do \
	  RDIR=$(DESTDIR)$(initddir)/SMART/Resources/$${i}.lproj ; \
	  $(mkinstalldirs) $$RDIR ;\
	  $(INSTALL_DATA) $(srcdir)/os_darwin/$${i}_Localizable.strings \
	    $$RDIR/Localizable.strings ; \
	done
	@echo -e "\n\n####################################################################\n#"
	@echo -e "#                       PLEASE READ THIS BOX!\n#"
	@echo -e "#   To manually start the smartd daemon, run:\n#   ${initddir}/SMART/SMART start\n#"
	@echo -e "#   To automatically start smartd on bootup, add the line:\n#   SMARTd=-YES-\n#   to /etc/hostconfig\n#"
	@echo -e "#   smartd can now use a configuration file ${sysconfdir}/smartd.conf. Do:\n#   man smartd"
	@echo -e "#   to learn about it. A sample configuration file can be found in:\n#   ${docdir}\n#"
	@echo -e "####################################################################\n\n"

uninstall-initdDATA-darwin:
	rm -rf $(DESTDIR)$(initddir)/$(initd_install_name)

else

initd_DATA = smartd.initd

smartd.initd: $(srcdir)/smartd.initd.in Makefile
	sed "s|/usr/local/sbin/|$(sbindir)/|g" $(srcdir)/smartd.initd.in > $@

initd_install_name = smartd$(smartd_suffix)

initd_DATA_install = install-initdDATA-generic
initd_DATA_uninstall = uninstall-initdDATA-generic

install-initdDATA-generic: $(initd_DATA)
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(INSTALL_SCRIPT) $(top_builddir)/smartd.initd $(DESTDIR)$(initddir)/smartd$(smartd_suffix)
	@echo -e "\n\n####################################################################\n#"
	@echo -e "#                       PLEASE READ THIS BOX!\n#"
	@echo -e "#   To manually start the smartd daemon, run:\n#   ${initddir}/smartd start\n#"
	@echo -e "#   To automatically start smartd on bootup, run:\n#   /sbin/chkconfig --add smartd\n#"
	@echo -e "#   smartd can now use a configuration file ${sysconfdir}/smartd.conf. Do:\n#   man smartd"
	@echo -e "#   to learn about it. A sample configuration file can be found in:\n#   ${docdir}\n#"
	@echo -e "####################################################################\n\n"


uninstall-initdDATA-generic:
	rm -rf $(DESTDIR)$(initddir)/$(initd_install_name)
endif
else

initd_DATA_install = install-initdDATA-null
initd_DATA_uninstall = uninstall-initdDATA-null

install-initdDATA-null:
uninstall-initdDATA-null:
endif

install-initdDATA : $(initd_DATA_install)

uninstall-initdDATA: $(initd_DATA_uninstall)

uninstall-docsDATA:
	rm -rf $(DESTDIR)$(docsdir)

if ENABLE_DRIVEDB
MAN_DRIVEDB = sed "s|/usr/local/share/smartmontools/drivedb\\.h|$(drivedbdir)/drivedb.h|g"
else
MAN_DRIVEDB = sed '/BEGIN ENABLE_DRIVEDB/,/END ENABLE_DRIVEDB/d'
endif

if ENABLE_SAVESTATES
MAN_SAVESTATES = sed "s|/usr/local/var/lib/smartmontools/smartd\\.|$(savestates)|g"
else
MAN_SAVESTATES = sed '/BEGIN ENABLE_SAVESTATES/,/END ENABLE_SAVESTATES/d'
endif

if OS_FREEBSD
.for file in $(man_MANS)
${file}: $(srcdir)/${file}.in Makefile cvsversion.h
	sed "s|CURRENT_CVS_VERSION|$(releaseversion)|g; \
	     s|CURRENT_CVS_DATE|`sed -n 's,^[^"]*"\([^ ]*\) [^"]*".*$$,\1,p' cvsversion.h`|g; \
	     s|CURRENT_CVS_TIME|`sed -n 's,^[^"]*"[^ ]* \([^"]*\)".*$$,\1,p' cvsversion.h`|g; \
	     s|/usr/local/share/man/|$(mandir)/|g; \
	     s|/usr/local/sbin/|$(sbindir)/|g; \
	     s|/usr/local/etc/rc\\.d/init.d/|$(initddir)/|g; \
	     s|/usr/local/share/doc/smartmontools-5.1/|$(docsdir)/|g;  \
	     s|/usr/local/etc/smartd\\.conf|$(sysconfdir)/smartd.conf|g; \
	     s|/usr/local/etc/smart_drivedb\\.h|$(sysconfdir)/smart_drivedb\\.h|g" ${.ALLSRC:M*.in} | \
	$(MAN_DRIVEDB) | \
	$(MAN_SAVESTATES) > $@
.endfor
else
smart%: $(srcdir)/smart%.in Makefile cvsversion.h
	sed "s|CURRENT_CVS_VERSION|$(releaseversion)|g" $< | \
	sed "s|CURRENT_CVS_DATE|`sed -n 's,^[^"]*"\([^ ]*\) [^"]*".*$$,\1,p' cvsversion.h`|g" | \
	sed "s|CURRENT_CVS_TIME|`sed -n 's,^[^"]*"[^ ]* \([^"]*\)".*$$,\1,p' cvsversion.h`|g" | \
	sed "s|/usr/local/share/man/|$(mandir)/|g" | \
	sed "s|/usr/local/sbin/|$(sbindir)/|g" | \
	sed "s|/usr/local/etc/rc\\.d/init.d/|$(initddir)/|g" | \
	sed "s|/usr/local/share/doc/smartmontools-5.1/|$(docsdir)/|g" | \
	sed "s|/usr/local/etc/smartd\\.conf|$(sysconfdir)/smartd.conf|g" | \
	sed "s|/usr/local/etc/smart_drivedb\\.h|$(sysconfdir)/smart_drivedb\\.h|g" | \
	$(MAN_DRIVEDB) | \
	$(MAN_SAVESTATES) > $@
endif

# Commands to convert man pages into .html and .txt
# TODO: configure
MAN2HTML = man2html
#MAN2HTML = groff -man -Thtml
MAN2TXT = groff -man -Tascii -P'-bcou'

# Remove HTTP header and fix links in man2html output
FIXHTML = sed '1s,^Content-type.*,,' \
        | sed 's,<A HREF="http://[-a-z/]*/man2html?\([1-8]\)+\(smart[cd][.a-z]*\)">,<A HREF="\2.\1.html">,g' \
        | sed 's,<A HREF="http://[-a-z/]*/man2html">,<A HREF=".">,g' \
        | sed 's,<A HREF="http://[-a-z/]*/man2html?[^"]*">\([^<]*\)</A>,\1,g' \
        | sed 's,<A HREF="mailto:[^s][^m][^a][^"]*">\([^<]*\)</A>,\1,g'

# Convert man pages into .html and .txt

htmlman: smartctl.8.html smartd.8.html smartd.conf.5.html

txtman:  smartctl.8.txt smartd.8.txt smartd.conf.5.txt

if OS_WIN32_MINGW

%.5.html: %.5
	$(DOS2UNIX) < $< | $(MAN2HTML) | $(FIXHTML) > $@

%.8.html: %.8
	$(DOS2UNIX) < $< | $(MAN2HTML) | $(FIXHTML) > $@
else

%.5.html: %.5
	$(MAN2HTML) $< | $(FIXHTML) > $@

%.8.html: %.8
	$(MAN2HTML) $< | $(FIXHTML) > $@
endif

%.5.txt: %.5
	$(MAN2TXT) $< > $@

%.8.txt: %.8
	$(MAN2TXT) $< > $@



if OS_WIN32_MINGW
# Definitions for Windows distribution

distdir_win32 = $(PACKAGE)-$(VERSION).win32
distzip_win32 = $(PACKAGE)-$(VERSION).win32.zip
distinst_win32= $(PACKAGE)-$(VERSION).win32-setup.exe

exedir_win32 = $(distdir_win32)/bin
docdir_win32 = $(distdir_win32)/doc

FILES_WIN32 = $(exedir_win32)/smartctl.exe \
              $(exedir_win32)/smartctl-nc.exe \
              $(exedir_win32)/smartd.exe \
              $(docdir_win32)/AUTHORS.txt \
              $(docdir_win32)/CHANGELOG.txt \
              $(docdir_win32)/COPYING.txt \
              $(docdir_win32)/INSTALL.txt \
              $(docdir_win32)/NEWS.txt \
              $(docdir_win32)/README.txt \
              $(docdir_win32)/TODO.txt \
              $(docdir_win32)/WARNINGS.txt \
              $(docdir_win32)/smartd.conf \
              $(docdir_win32)/smartctl.8.html \
              $(docdir_win32)/smartctl.8.txt \
              $(docdir_win32)/smartd.8.html \
              $(docdir_win32)/smartd.8.txt \
              $(docdir_win32)/smartd.conf.5.html \
              $(docdir_win32)/smartd.conf.5.txt

CLEANFILES += $(FILES_WIN32) $(exedir_win32)/syslogevt.exe \
              smartctl-nc.exe smartctl-nc.exe.tmp \
              distdir.mkdir syslogevt.check

# Textfile converter from cygutils
UNIX2DOS = unix2dos -D
DOS2UNIX = dos2unix -U

# Build Windows distribution

dist-win32: $(distzip_win32)

install-win32: $(distinst_win32)
	./$(distinst_win32)

installer-win32: $(distinst_win32)

distdir-win32: distdir.mkdir $(FILES_WIN32) syslogevt.check

$(distzip_win32): distdir.mkdir $(FILES_WIN32) syslogevt.check
	@rm -fv $(distzip_win32)
	cd $(distdir_win32) && zip -9Dr ../$(distzip_win32) .

# Build NSIS installer, try to locate makensis in default location first
$(distinst_win32): $(srcdir)/os_win32/installer.nsi distdir.mkdir $(FILES_WIN32) syslogevt.check
	@makensis="$(MAKENSIS)"; if [ -z "$$makensis" ]; then \
	  if [ ! -z "$$PROGRAMFILES" ] && "$$PROGRAMFILES/NSIS/makensis" /VERSION >/dev/null 2>&1; then \
	    makensis="$$PROGRAMFILES/NSIS/makensis"; \
	  elif makensis /VERSION >/dev/null 2>&1; then \
	    makensis=makensis; \
	  else \
	    echo 'makensis: command not found. Please download and install NSIS' 1>&2; \
	    echo 'from http://nsis.sourceforge.net/Download' 1>&2; exit 1; \
	  fi; \
	fi; \
	echo "$$makensis /V2 /NOCD /DINPDIR=$(distdir_win32) /DOUTFILE=$(distinst_win32) $(srcdir)/os_win32/installer.nsi"; \
	"$$makensis" /V2 /NOCD /DINPDIR="$(distdir_win32)" /DOUTFILE="$(distinst_win32)" "$(srcdir)/os_win32/installer.nsi"

cleandist-win32:
	rm -rf $(distdir_win32) distdir.mkdir syslogevt.check

distdir.mkdir:
	@test -d $(exedir_win32) || mkdir -pv $(exedir_win32)
	@test -d $(docdir_win32) || mkdir -pv $(docdir_win32)
	touch $@

syslogevt.check:
	@if [ -f $(srcdir)/os_win32/syslogevt.exe ]; then \
	  cp -pv $(srcdir)/os_win32/syslogevt.exe $(exedir_win32)/syslogevt.exe; \
	 else echo "Warning: $(srcdir)/os_win32/syslogevt.exe missing."; fi
	touch $@

$(exedir_win32)/%.exe: %.exe
	cp -p $< $@
	strip -s $@
	touch -r $< $@

$(docdir_win32)/%.html: %.html
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.txt: %.txt
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.txt: $(srcdir)/%
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

$(docdir_win32)/%.conf: $(srcdir)/%.conf
	$(UNIX2DOS) < $< > $@
	touch -r $< $@

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


# Build {config,cvsversion}_vc8.h for MSVC8 from MinGW {config,cvsversion}.h

config-vc8: $(srcdir)/os_win32/config_vc8.h  $(srcdir)/os_win32/cvsversion_vc8.h

$(srcdir)/os_win32/config_vc8.h: config.h
	sed '1i/* config_vc8.h.  Generated from config.h by Makefile.  */' $< | \
	sed 's,^#define HAVE_\(ATTR_PACKED\|INTTYPES_H\|STDINT_H\|STRINGS_H\|STRTOULL\|U*INT64_T\|UNISTD_H\|WORKING_SNPRINTF\) 1$$,/* #undef HAVE_\1 */,' | \
	sed 's,i.86-pc-mingw32,i686-pc-win32vc8,' > $@

$(srcdir)/os_win32/cvsversion_vc8.h: cvsversion.h
	cp cvsversion.h $@

endif

SUBDIRS= . examplescripts
