==========================================================
smartmontools - S.M.A.R.T. utility toolset for Darwin/Mac
OSX, FreeBSD, Linux, NetBSD, OpenBSD, Solaris, and Windows.
==========================================================

$Id: README 4120 2015-08-27 16:12:21Z samm2 $

== HOME ==
The home for smartmontools is located at:
    
    http://www.smartmontools.org/

Please see this web site for updates, documentation, and for submitting
patches and bug reports.

You will find a mailing list for support and other questions at:

    http://lists.sourceforge.net/lists/listinfo/smartmontools-support


== COPYING ==
Copyright (C) 2002-9 Bruce Allen
Copyright (C) 2004-15 Christian Franke

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

You should have received a copy of the GNU General Public License (for
example COPYING).  If not, see <http://www.gnu.org/licenses/>.


== CREDITS ==
See AUTHORS file.


== OVERVIEW ==
smartmontools contains utilities that control and monitor storage
devices using the Self-Monitoring, Analysis and Reporting Technology
(SMART) system build into ATA/SATA and SCSI/SAS hard drives and
solid-state drives.  This is used to check the reliability of the
drive and to predict drive failures.


== CONTENTS ==
The suite contains two utilities:

smartctl is a command line utility designed to perform S.M.A.R.T. tasks
	 such as disk self-checks, and to report the S.M.A.R.T. status of
	 the disk.

smartd   is a daemon that periodically monitors S.M.A.R.T. status and
         reports errors and changes in S.M.A.R.T. attributes to syslog.


== OBTAINING SMARTMONTOOLS ==

Source tarballs
---------------

http://sourceforge.net/projects/smartmontools/files/

SVN
---

svn co http://svn.code.sf.net/p/smartmontools/code/trunk/smartmontools smartmontools

This will create a subdirectory called smartmontools containing the code.

To instead get the 5.38 release:

svn co http://svn.code.sf.net/p/smartmontools/code/tags/RELEASE_5_38/sm5 smartmontools

You can see what the different tags are by looking at
http://sourceforge.net/p/smartmontools/code/HEAD/tree/tags/

== BUILDING/INSTALLING SMARTMONTOOLS ==

Refer to the "INSTALL" file for detailed installation instructions.

== GETTING STARTED ==

To examine SMART data from a disk, try:
  smartctl -a /dev/sda
See the manual page 'man smartctl' for more information.

To start automatic monitoring of your disks with the smartd daemon,
try:
  smartd -d
to start the daemon in foreground (debug) mode, or
  smartd
to start the daemon in background mode.  This will log messages to
SYSLOG.  If you would like to get email warning messages, please set
up the configuration file smartd.conf with the '-m' mail warning
Directive.  See the manual page 'man smartd' for more information.
