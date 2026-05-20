## Process this file with automake to produce Makefile.in
#
# src/snmp-agentxd/Makefile.am
#

AM_CPPFLAGS = \
        -DAGENTXD_SYSCONFDIR='"$(sysconfdir)/smartmontools"' \
        -I $(top_srcdir)/include \
        -I $(top_srcdir)/src/snmp-agentxd

sbin_PROGRAMS = smartmon-snmp-agentxd

smartmon_snmp_agentxd_SOURCES = \
        agentxd_main.cpp    \
        agentxd_config.cpp  \
        agentxd_config.h    \
        agentxd_datasrc.cpp \
        agentxd_datasrc.h   \
        agentxd_json.cpp    \
        agentxd_json.h      \
        agentxd_loop.cpp    \
        agentxd_loop.h      \
        agentxd_cache.cpp   \
        agentxd_cache.h     \
        agentxd_notify.cpp  \
        agentxd_notify.h    \
        snmp_oids.h         \
        snmp_common_mib.cpp \
        snmp_common_mib.h   \
        snmp_nvme_mib.cpp   \
        snmp_nvme_mib.h     \
        snmp_sata_mib.cpp   \
        snmp_sata_mib.h     \
        snmp_sas_mib.cpp    \
        snmp_sas_mib.h      \
        snmp_sensor_mib.cpp \
        snmp_sensor_mib.h

smartmon_snmp_agentxd_LDADD = \
        $(top_builddir)/lib/libsmartmon.la \
        $(SNMP_LIBS)

smartmon_snmp_agentxd_DEPENDENCIES = \
        $(top_builddir)/lib/libsmartmon.la

# Sample config file
smartmonconfdir = $(sysconfdir)/smartmontools
dist_smartmonconf_DATA = smartmon-snmp-agentxd.conf

# Man page
man8_MANS = smartmon-snmp-agentxd.8

EXTRA_DIST = \
        smartmon-snmp-agentxd.8.in \
        smartmon-snmp-agentxd.service.in

# Generate man page from .in template
smartmon-snmp-agentxd.8: smartmon-snmp-agentxd.8.in
	$(AM_V_GEN)sed \
	    -e 's|@sysconfdir@|$(sysconfdir)|g' \
	    -e 's|@sbindir@|$(sbindir)|g' \
	    -e 's|@PACKAGE_VERSION@|$(PACKAGE_VERSION)|g' \
	    $< > $@

# systemd unit (optional — only installed when systemd support is present)
if INSTALL_SYSTEMDUNIT
systemdsystemunit_DATA = smartmon-snmp-agentxd.service

smartmon-snmp-agentxd.service: smartmon-snmp-agentxd.service.in
	$(AM_V_GEN)sed \
	    -e 's|@sbindir@|$(sbindir)|g' \
	    -e 's|@sysconfdir@|$(sysconfdir)|g' \
	    $< > $@

CLEANFILES = smartmon-snmp-agentxd.8 smartmon-snmp-agentxd.service
else
CLEANFILES = smartmon-snmp-agentxd.8
endif
