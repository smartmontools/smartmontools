Release:  1
Summary:	smartmontools - for monitoring S.M.A.R.T. disks and devices
Summary(cs):	smartmontools - pro monitorování S.M.A.R.T. diskù a zaøízení
Summary(de):	smartmontools - zur Überwachung von S.M.A.R.T.-Platten und-Geräten
Summary(es):	smartmontools - para el seguimiento de discos y dispositivos S.M.A.R.T.
Summary(fr):	smartmontools - pour le suivi des disques et instruments S.M.A.R.T.
Summary(pt):	smartmontools - para monitorar discos e dispositivos S.M.A.R.T.
Summary(it):	smartmontools - per monitare dischi e dispositivi S.M.A.R.T.
Summary(pl):	Monitorowanie i kontrola dysków u¿ywaj±æ S.M.A.R.T.
Name:		smartmontools
Version:	5.24
License:	GPL
Group:		Applications/System
Group(de):	Applikationen/System
Group(es):	Aplicaciones/Sistema
Group(fr):	Applications/Système
Group(pt):	Aplicativos/Sistema
Group(it):      Applicazioni/Sistemi
Source0:	%{name}-%{version}.tar.gz
URL:            http://smartmontools.sourceforge.net/
Prereq:		/sbin/chkconfig
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Obsoletes:	smartctl
Obsoletes:      smartd
Obsoletes:	ucsc-smartsuite
Obsoletes:      smartsuite
Packager:       Bruce Allen <smartmontools-support@lists.sourceforge.net>

# Source code can be found at:
# http://ftp1.sourceforge.net/smartmontools/smartmontools-%{version}-%{release}.tar.gz

# CVS ID of this file is:
# $Id: smartmontools.spec,v 1.142 2003/11/19 21:34:07 ballen4705 Exp $

# Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
# Home page: http://smartmontools.sourceforge.net/
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
# 
# You should have received a copy of the GNU General Public License (for
# example COPYING); if not, write to the Free Software Foundation, Inc., 675
# Mass Ave, Cambridge, MA 02139, USA.
#
# This code was originally developed as a Senior Thesis by Michael Cornwell
# at the Concurrent Systems Laboratory (now part of the Storage Systems
# Research Center), Jack Baskin School of Engineering, University of
# California, Santa Cruz. http://ssrc.soe.ucsc.edu/


%description
smartmontools controls and monitors storage devices using the
Self-Monitoring, Analysis and Reporting Technology System (S.M.A.R.T.)
built into ATA and SCSI Hard Drives.  This is used to check the
reliability of the hard drive and to predict drive failures.  The suite
is derived from the smartsuite package, and contains two utilities.  The
first, smartctl, is a command line utility designed to perform simple
S.M.A.R.T. tasks.  The second, smartd, is a daemon that periodically
monitors smart status and reports errors to syslog.  The package is
compatible with the ATA/ATAPI-5 specification.  Future releases will be
compatible with the ATA/ATAPI-6 andATA/ATAPI-7 specifications.  The
package is intended to incorporate as much "vendor specific" and
"reserved" information as possible about disk drives.  man smartctl and
man smartd will provide more information.  This RPM file is compatible
with all RedHat releases back to at least 6.2 and should work OK on any
modern linux distribution.  The most recent versions of this package and
additional information can be found at the URL:
http://smartmontools.sourceforge.net/

%description -l cs
smartmontools øídí a monitorují zaøízení pro ukládání dat za pou¾ití
technologie automatického monitorování, analýzy a hlá¹ení
(Self-Monitoring, Analysis and Reporting Technology System -
S.M.A.R.T.) vestavìného do pevných diskù ATA a SCSI. Pou¾ívá se ke
kontrole pou¾itelnosti pevného disku a pøedvídání havárií diskù.
Nástroje jsou odvozeny od balíèku smartsuite a obsahují dva programy.
První, smartctl, je nástroj pro provádìní jednoduchých S.M.A.R.T. úloh
na pøíkazové øádce. Druhý, smartd, je démon, který periodicky
monitoruje stav a hlásí chyby do systémového protokolu. Balíèek je
kompatibilní se specifikací ATA/ATAPI-5. Dal¹í verze budou
kompatibilní se specifikacemi ATA/ATAPI-6 a ATA/ATAPI-7. Balíèek je
navr¾en tak, aby pokryl co nejvíce polo¾ek s informacemi "závislé na
výrobci" a "rezervováno". Více informací získáte pomocí man smartctl a
man smartd. Tento RPM balíèek je kompatibilní se v¹emi verzemi RedHatu
a mìl by fungovat na v¹ech moderních distribucích Linuxu. Aktuální
verzi najdete na URL http://smartmontools.sourceforge.net/

%description -l de
Die smartmontools steuern und überwachen Speichergeräte mittels des
S.M.A.R.T.-Systems (Self-Monitoring, Analysis and Reporting Technology,
Technologie zur Selbst-Überwachung, Analyse und Berichterstellung), das
in ATA- und SCSI-Festplatten eingesetzt wird.  Sie werden benutzt, um
die Zuverlässigkeit der Festplatte zu prüfen und Plattenfehler
vorherzusagen.  Die Suite wurde vom smartsuite-Paket abgeleitet und
enthält zwei Dienstprogramme.  Das erste, smartctl, ist ein
Kommandozeilentool, das einfache S.M.A.R.T. Aufgaben ausführt.  Das
zweite, smartd, ist ein Daemon, der periodisch den S.M.A.R.T.-Status
überwacht und Fehler ins Syslog protokolliert.  Das Paket ist zur
ATA/ATAPI-5 Spezifikation kompatibel. Zukünftige Versionen werden auch
die ATA/ATAPI-6 und ATA/ATAPI-7 Spezifikationen umsetzen.  Das Paket
versucht, so viele "herstellerspezifische" und "reservierte" Information
über Plattenlaufwerke wie möglich bereitzustellen.  man smartctl und man
smartd liefern mehr Informationen über den Einsatz.  Dieses RPM ist zu
allen RedHat-Versionen ab spätestens 6.2 kompatibel und sollte unter
jedem modernen Linux arbeiten.  Die aktuellsten Versionen dieses Pakets
und zusätzliche Informationen sind zu finden unter der URL:
http://smartmontools.sourceforge.net/

%description -l es
smartmontools controla y hace el seguimiento de dispositivos de
almacenamiento usando el Self-Monitoring, Analysis and Reporting
Technology System (S.M.A.R.T.) incorporado en discos duros ATA y SCSI. 
Es usado para asegurar la fiabilidad de discos duros y predecir averias. 
El conjunto de programas proviene del conjunto smartsuite y contiene dos
utilidades.  La primera, smartctl, es una utilidad command-line hecha
para hacer operaciones S.M.A.R.T. sencillas.  La segunda, smartd, es un
programa que periodicamente chequea el estatus smart e informa de
errores a syslog.  Estos programas son compatibles con el sistema
ATA/ATAPI-5.  Futuras versiones seran compatibles con los sistemas
ATA/ATAPI-6 y ATA/ATAPI-7.  Este conjunto de programas tiene el
proposito de incorporar la mayor cantidad posible de informacion
reservada y especifica de discos duros.  Los comandos 'man smartctl' y
'man smartd' contienen mas informacion.  Este fichero RPM es compatible
con todas las versiones de RedHat a partir de la 6.2 y posiblemente
funcionaran sin problemas en cualquier distribucion moderna de linux. 
La version mas reciente de estos programas ademas de informacion
adicional pueden encontrarse en: http://smartmontools.sourceforge.net/

%description -l fr
smartmontools contrôle et fait le suivi de périphériques de stockage
utilisant le système Self-Monitoring, Analysis and Reporting
Technology (S.M.A.R.T) intégré dans les disques durs ATA et SCSI.  Ce
système est utilisé pour vérifier la fiabilité du disque dur et prédire
les défaillances du lecteur.  La suite logicielle dérive du paquet
smartsuite et contient deux utilitaires.  Le premier, smartctl,
fonctionne en ligne de commande et permet de réaliser des tâches
S.M.A.R.T. simples.  Le second, smartd, est un démon qui fait
périodiquement le suivi du statut smart et transmet les erreurs au
syslog.  Ce paquet est compatible avec la spécification ATA/ATAPI-5. 
Les prochaines versions seront compatibles avec les spécifications
ATA/ATAPI-6 et ATA/ATAPI-7.  Ce paquet tente d'incorporer le plus
d'informations possible sur les disques durs qu'elles soient spécifiques
au constructeur ("vendor specific") ou réservées ("reserved").  man
smartctl et man smartd donnent plus de renseignements.  Ce fichier RPM
est compatible avec toutes les versions de RedHat v6.2 et ultérieures,
et devrait fonctionner sur toutes les distributions récentes de Linux. 
Les dernières versions de ce paquet et des informations supplémentaires
peuvent être trouvées à l'adresse URL:
http://smartmontools.sourceforge.net/

%description -l pt
smartmontools controla e monitora dispositivos de armazenamento
utilizando o recurso Self-Monitoring, Analysis and Reporting Technology
System (S.M.A.R.T.) integrado nos discos rígidos ATA e SCSI, cuja
finalidade é verificar a confiabilidade do disco rígido e prever falhas
da unidade.  A suite é derivada do pacote smartsuite, e contém dois
utilitários.  O primeiro, smartctl, é um utilitário de linha de comando
projetado para executar tarefas simples de S.M.A.R.T.  O segundo,
smartd, é um daemon que monitora periodicamente estados do smart e
reporta erros para o syslog.  O pacote é compatível com a especificação
ATA/ATAPI-5.  Futuras versões serão compatíveis com as especificações
ATA/ATAPI-6 e ATA/ATAPI-7.  O pacote pretende incorporar o maior número
possível de informações "específicas do fabricante" e "reservadas" sobre
unidades de disco.  man smartctl e man smartd contém mais informações. 
Este arquivo RPM é compatível com todas as versões do RedHat a partir da
6.2 e deverá funcionar perfeitamente em qualquer distribuição moderna do
Linux.  As mais recentes versões deste pacote e informações adicionais
podem ser encontradas em http://smartmontools.sourceforge.net/

%description -l it
smartmontools controlla e monitora dischi che usano il "Self-Monitoring,
Analysis and Reporting Technology System" (S.M.A.R.T.), in hard drive
ATA e SCSI. Esso è usato per controllare l'affidabilità dei drive e
predire i guasti. La suite è derivata dal package smartsuite e contiene
due utility. La prima, smartctl, è una utility a linea di comando
progettata per eseguire semplici task S.M.A.R.T.. La seconda, smartd, è
un daemon che periodicamente monitora lo stato di smart e riporta errori
al syslog. Il package è compatibile con le specifiche ATA/ATAPI-6 e
ATA/ATAPI-7. Il package vuole incorporare tutte le possibili
informazioni riservate e "vendor specific" sui dischi. man smartctl e
man smartd danno più informazioni. Questo file RPM è compatibile con
tutte le release di RedHat, almeno dalla 6.2 e dovrebbe funzionare bene
su ogni moderna distribuzione di linux. Le versioni più recenti di
questo package e informazioni addizionali possono essere trovate al sito
http://smartmontools.sourceforge.net/

%description -l pl
Pakiet zawiera dwa programy (smartctl oraz smartd) do kontroli i
monitorowania systemów przechowywania danych za pomoc± S.M.A.R.T -
systemu wbudowanego w wiêkszo¶æ nowych dysków ATA oraz SCSI. Pakiet
pochodzi od oprogramowania smartsuite i wspiera dyski ATA/ATAPI-5.

# The following sections are executed by the SRPM file
%prep

%setup -q

%build
  %configure
  make

%install
  rm -rf $RPM_BUILD_ROOT
  rm -rf %{_buildroot}
  %makeinstall
  rm -f examplescripts/Makefile*

%files
  %defattr(-,root,root)
  %attr(755,root,root) %{_sbindir}/smartd
  %attr(755,root,root) %{_sbindir}/smartctl
  %attr(755,root,root) /etc/rc.d/init.d/smartd
  %attr(644,root,root) %{_mandir}/man8/smartctl.8*
  %attr(644,root,root) %{_mandir}/man8/smartd.8*
  %attr(644,root,root) %{_mandir}/man5/smartd.conf.5*
  %doc AUTHORS CHANGELOG COPYING INSTALL NEWS README TODO WARNINGS smartd.conf examplescripts
  %config(noreplace) %{_sysconfdir}/smartd.conf

%clean
  rm -rf $RPM_BUILD_ROOT
  rm -rf %{_buildroot}
  rm -rf %{_builddir}/%{name}-%{version}

# The following are executed only by the binary RPM at install/uninstall

# since this installs the gzipped documentation files, remove
# non-gzipped ones of the same name.

# run before installation.  Passed "1" the first time package installed, else a larger number
%pre
if [ -f /usr/share/man/man8/smartctl.8 ] ; then
	echo "You MUST delete (by hand) the outdated file /usr/share/man/man8/smartctl.8 to read the new manual page for smartctl."	
fi
if [ -f /usr/share/man/man8/smartd.8 ] ; then
	echo "You MUST delete (by hand) the outdated file /usr/share/man/man8/smartd.8 to read the new manual page for smartd."	
fi
if [ -f /usr/share/man/man5/smartd.conf.5 ] ; then
        echo "You MUST delete (by hand) the outdated file /usr/share/man/man5/smartd.conf.5 to read the new manual page for smartd.conf"
fi

if [ ! -f /etc/smartd.conf ]; then
	echo "Note that you can use a configuration file /etc/smartd.conf to control the"
	echo "startup behavior of the smartd daemon.  See man 8 smartd for details."
fi

# run after installation.  Passed "1" the first time package installed, else a larger number
%post
# if smartd is already running, restart it with the new daemon
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd restart 1>&2
	echo "Restarted smartd services"
else
# else tell the user how to start it
        echo "Run \"/etc/rc.d/init.d/smartd start\" to start smartd service now."
fi

# Now see if we should tell user to set service to start on boot	
/sbin/chkconfig --list smartd > /dev/null 2> /dev/null
printmessage=$?

if [ $printmessage -ne 0 ] ; then
	echo "Run \"/sbin/chkconfig --add smartd\", to start smartd service on system boot"
else
	echo "smartd will continue to start up on system boot"
fi


# run before uninstallation.  Passed zero when the last version uninstalled, else larger
%preun

# if uninstalling the final copy, stop and remove any links	
if [ "$1" = "0" ]; then
  if [ -f /var/lock/subsys/smartd ]; then
    /etc/rc.d/init.d/smartd stop 1>&2
    echo "Stopping smartd services"
  fi

# see if any links remain, and kill them if they do
  /sbin/chkconfig --list smartd > /dev/null 2> /dev/null
  notlinked=$?
	
  if [ $notlinked -eq 0 ]; then
    /sbin/chkconfig --del smartd
    echo "Removing chkconfig links to smartd boot-time startup scripts"
  fi
fi

# run after uninstallation. Passed zero when the last version uninstalled, else larger
# %postun

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)

# Maintainers / Developers Key:
# [BA] Bruce Allen
# [EB] Erik Inge Bolsø
# [SB] Stanislav Brabec
# [PC] Peter Cassidy
# [CD] Capser Dik
# [DK] David Kirkby
# [DG] Douglas Gilbert
# [GG] Guido Guenther
# [KM] Kai Mäkisarai
# [FM] Frédéric L. W. Meunier
# [PW] Phil Williams

%changelog
* Wed Nov 19 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
  [DG] smartd/smartctl: changed scsiClearControlGLTSD() to
       scsiSetControlGLTSD() with an 'enabled' argument so '-S on'
       and '-S off' work for SCSI devices (if changing GLTSD supported).
  [BA] smartd/smartctl: wired in scsiClearControlGLTSD(). Could still
       use a corresponding Set function.  Left stubs for this purpose.
  [DG] scsicmds: added scsiClearControlGLTSD() [still to be wired in]
  [BA] smartctl: make SCSI -T options behave the same way as the
       ATA ones.
  [DG] smartctl: output scsi transport protocol if available
  [DG] scsi: stop device scan in smartd and smartctl if badly formed
       mode response [heuristic to filter out USB devices before we
       (potentially) lock them up].
  [BA] smartd: deviceclose()->CloseDevice(). Got rid of SCSIDEVELOPMENT
       macro-enabled code.  Added -W to list of gcc specific options to
       always enable. Made code clean for -W warnings.
  [PW] Added Maxtor DiamondMax VL 30 family to knowndrives table.
  [DG] scsi: add warning (when '-l error' active) if Control mode page
       GLTSD bit is set (global disable of saving log counters)
  [DG] scsi: remember mode sense cmd length. Output trip temperature
       from IE lpage (IBM extension) when unavailable from temp lpage.
  [BA] smartd: for both SCSI and ATA now warns user if either
       the number of self-test errors OR timestamp of most
       recent self-test error have increased.
  [DG] smartctl: output Seagate scsi Cache and Factory log pages (if
       available) when vendor attributes chosen
  [DG] smartd: add scsiCountFailedSelfTests() function.
  [DG] Do more sanity checking of scsi log page responses.
  [BA] smartd: now warns user if number of self-test errors has
       increased for SCSI devices.
  [BA] smartd: warn user if number of ATA self-test errors increases
       (as before) OR if hour time stamp of most recent self-test
       error changes.
  [DG] More checks for well formed mode page responses. This has the side
       effect of stopping scans on bad SCSI implementations (e.g. some
       USB disks) prior to sending commands (typically log sense) that
       locks them up.
  [PW] Added Western Digital Caviar family and Caviar SE family to
       knowndrives table.
  [BA] smartd: added -l daemon (which is the default value if -l
       is not used).
  [PW] Added Seagate Barracuda ATA V family to knowndrives table.
  [BA] smartd: added additional command line argument -l FACILITY
       or --logfacility FACILITY.  This can be used to redirect
       messages from smartd to a different file than the one used
       by other system daemons.
  [PW] Added Seagate Barracuda 7200.7, Western Digital Protege WD400EB,
       and Western Digital Caviar AC38400 to knowndrives table.
  [BA] smartd: scanning should now also work correctly for
       devfs WITHOUT traditional links /dev/hd[a-t] or /dev/sd[a-z].
  [PW] Added Maxtor 4W040H3, Seagate Barracuda 7200.7 Plus,
       IBM Deskstar 120GXP (40GB), Seagate U Series 20410,
       Fujitsu MHM2100AT, MHL2300AT, MHM2150AT, and IBM-DARA-212000
       to knowndrives table.
  [PW] Added remaining Maxtor DiamondMax Plus 9 models to knowndrives
       table.
  [EM] smartd: If no matches found, then return 0, rather than an error
       indication, as it just means no devices of the given type exist.
       Adjust FreeBSD scan code to mirror Linux version.
  [BA] smartd: made device scan code simpler and more robust. If
       too many devices detected, warn user but scan as many
       as possible.  If error in scanning, warn user but don't
       die right away.
  [EM] smartd: To keep as consistent as possible, migrate FreeBSD
       devicescan code to also use glob(3). Also verified clean 
       compile on a 4.7 FreeBSD system.
  [BA] smartd: Modified device scan code to use glob(3). Previously
       it appeared to have trouble when scanning devices on an XFS
       file system, and used non-public interface to directory
       entries. Problems were also reported when /dev/ was on an
       ext2/3 file system, but there was a JFS partition on the same
       disk.
  [BA] Clearer error messages when device scanning finds no suitable
       devices.
  [EM] FreeBSD:	Fixup code to allow for proper compilation under 
       -STABLE branch.

* Fri Oct 31 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartd: didn't close file descriptors of ATA packet devices
       that are scanned. Fixed.
- [BA] Added reload/report targets to the smartmontools init script.
       reload: reloads config file
       report: send SIGUSR1 to check devices now

* Mon Oct 27 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [EM] Fix compile issues for FreeBSD < 5-CURRENT.
- [PW] Added Fujitsu MHM2200AT to knowndrives table.
- [BA] To help catch bugs, clear ATA error structures before all
       ioctl calls.  Disable code that attempted to time-out on SCSI
       devices when they hung (doesn't work).
- [BA] Documented STATUS/ERROR flags added by [PW] below.
- [BA] Improved algorithm to recognize ATA packet devices. Should
       no longer generate SYSLOG kernel noise when user tries either
       smartd or smartctl on packet device (CD-ROM or DVD).  Clearer
       warning messages from smartd when scanning ATA packet device.
- [PW] Added TOSHIBA MK4025GAS to knowndrives table.
- [PW] Added a textual interpretation of the status and error registers
       in the SMART error log (ATA).  The interpretation is
       command-dependent and currently only eight commands are supported
       (those which produced errors in the error logs that I happen to
       have seen).
- [BA] added memory allocation tracking to solaris code.
       Fixed solaris signal handling (reset handler to default
       after first call to handler) by using sigset. Added
       HAVE_SIGSET to configure.in
- [CD] solaris port: added SCSI functionality to solaris
       stubs.
- [BA] smartd: attempt to address bug report about smartd
       hanging on USB devices when scanning:
       https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=107615
       Set a timeout of SCSITIMEOUT (nominally 7 seconds) before
       giving up.
- [EM] smartd: DEVICESCAN will follow links in a devfs filesystem and
       make sure the end point is a disc.  Update documentation, added
       note about FreeBSD scanning
- [BA] smartd: DEVICESCAN also looks for block devices in
       /dev.  Updated documentation.  Now scans for up to
       20 ATA devices /dev/hda-t rather than previous 12
       /dev/hda-l.
- [EM] smartd: mirror the FreeBSD DEVICESCAN logic for Linux,
       so that smartd now scans only devices found in /dev/. Also,
       make utility memory functions take a line number and file so
       that we report errors with the correct location.
- [GG] add a note about Debian bug #208964 to WARNINGS.
- [BA] smartctl: -T verypermissive option broken.  Use
       -T verpermissive until the next release, please.
- [BA] Syntax mods so that code also compiles on Solaris using
       Sun Workshop compiler.  Need -xmemalign 1i -xCC flags
       for cc.

* Wed Oct 15 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
  [DK] Changed configure.in so -Wall is only included if gcc
       is used (this is a gcc specific flag) and -fsignedchar
       is not used at all (this is a gcc specific compiler 
       flag).
  [BA] Modifications so that code now compiles under solaris. Now
       all that's needed (:-) is to fill in os_solaris.[hc].  Added
       os_generic.[hc] as guide to future ports.  Fixed -D option
       of smartd (no file name).  Modified -h opt of smartd/smartctl
       to work properly with solaris getopt().
  [EM] Update MAN pages with notes that 3ware drives are NOT supported
	under FreeBSD. Cleanup FreeBSD warning message handling.
  [EM] FreeBSD only: Fix first user found bug....I guess I was making
       the wrong assumption on how to convert ATA devnames to
       channel/unit numbers.
  [EM] Allow for option --enable-sample to append '.sample' to installed
	smartd.conf and rc script files. Also, let rc script shell setting
	be determined by configure
  [EM] Minor autoconf update to include -lcam for FreeBSD
  [EM] Add conditional logic to allow FreeBSD to compile pre-ATAng.
	-- note, not tested
	Add some documentation to INSTALL for FreeBSD.
  [EM] Implement SCSI CAM support for FreeBSD.  NOTE: I am not an expert
	in the use of CAM.  It seems to work for me, but I may be doing
	something horribly wrong, so please exercise caution.
  [EM] Switch over to using 'atexit' rather than 'on_exit' routine. This also
  	meant we needed to save the exit status elsewhere so our 'Goodbye'
	routine could examine it.
  [EM] Move the DEVICESCAN code to os specific files. Also moved some of the
	smartd Memory functions to utility.c to make available to smartctl.
  [EM] Code janitor work on os_freebsd.c.
  [EM] Added os_freebsd.[hc] code.  Additional code janitor
       work.
  [BA] Code janitor working, moving OS dependent code into
       os_linux.[hc].
  [GG] conditionally compile os_{freebsd,linux}.o depending on
       host architecture
  [PW] Print estimated completion time for tests
  [BA] Added -F samsung2 flag to correct firmware byte swap.
       All samsung drives with *-23 firmware revision string.

* Sun Oct 05 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [GG] Fixed broken Makefile.am (zero length smartd.conf.5
       was being created)
- [FM] Improved Slackware init script added to /etc/smartd.initd

* Fri Oct 03 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartctl: added '-T verypermissive' option which is
       equivalent to giving '-T permissive' many times.
- [BA] Try harder to identify from IDENTIFY DEVICE structure
       if SMART supported/enabled.  smartd now does a more
       thorough job of trying to assess this before sending
       a SMART status command to find out for sure.
- [BA] smartctl: it's now possible to override the program's
       guess of the device type (ATA or SCSI) with -d option.
- [BA] try hard to avoid sending IDENTIFY DEVICE to packet
       devices (CDROMS).  They can't do SMART, and this generates
       annoying syslog messages. At the same time, identify type
       of Packet device.
- [BA] smartctl: Can now use permissive option more
       than once, to control how far to go before giving up.
- [BA] smartd: if user asked to monitor either error or self-test
       logs (-l error or -l selftest) WITHOUT monitoring any of the
       Attribute values, code will SEGV.  For 5.1-18 and earlier,
       a good workaround is to enable Auto offline (-o on).
- [BA] smartctl: If enable auto offline command given, update auto
       offline status before printing capabilities.
- [GG] Make autotools build the default, remove autotools.diff
- [GG] Add auto{conf,make} support, not enabled by default. 
- [BA] Eliminated #include <linux/hdreg.h> from code. This
       should simplify porting to solaris, FreeBSD, etc. The
       only linux-specific code is now isolated to three routines,
       one for SCSI, one for Escalade, one for ATA.

* Fri Aug 22 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartd: fixed serious bug - Attributes not monitored unless
       user told smartd to ignore at least one of them!

* Tue Aug 19 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] Default runlevels for smartd changed from 3 and 5 to
       2, 3, 4, and 5.
- [BA] Removed as much dynamic memory allocation as possible from
       configuration file parsing. Reloading config file, even in
       presence of syntax errors etc. should not cause memory leaks.
- [PW] It is no longer permissible for the integer part (if any) of
       arguments to --report and --device to be followed by non-digits.
       For example, the "foo" in --report=ioctl,2foo was previously
       ignored, but now causes an error.
- [BA] smartd: added -q/--quit command line option to specify
       under what circumstances smartd should exit.  The old
       -c/--checkonce option is now obsoleted by this more
       general-purpose option.
- [BA] smartd now responds to a HUP signal by re-reading its
       configuration file /etc/smartd.conf.  If there are
       errors in this file, then the configuration file is
       ignored and smartd continues to monitor the devices that
       it was monitoring prior to receiving the HUP signal.
- [BA] Now correctly get SMART status from disks behind 3ware
       controllers, thanks to Adam Radford. Need 3w-xxxx driver
       version 1.02.00.037 or later. Previously the smartmontools
       SMART status always returned "OK" for 3ware controllers.
- [BA] Additional work on dynamic memory allocation/deallocation.
       This should have no effect on smartctl, but clears that way
       for smartd to dynamically add and remove entries.  It should
       also now be easier to modify smartd to re-read its config
       file on HUP (which is easy) without leaking memory (which is
       harder). The philosophy is that memory for data structures in
       smartd is now allocated only on demand, the first time it
       is needed.
- [BA] smartd: finished cleanup.  Now use create/rm functions for
       cfgentries and dynamic memory allocation almost everywhere.
       Philosophy: aggresively try and provoke SEGV to help find
       bad code.
- [BA] Added SAMSUNG SV0412H to knowndrives table.
- [BA] smartd: if DEVICESCAN used then knowndrives table might not set
       the -v attributes correctly -- may have been the same for all
       the drives.  Cleaned up some data structures and memory
       allocation to try and ensure segvs if such problems are
       introduced again.
- [BA] Now allow -S on and -o on for the 3ware device type.  For these
       commands to be passed through, the stock 3ware 3w-xxxx driver
       must be patched (8 lines).  I'll post a patch on the smartmontools
       home page after it's been tested by a few other people and 3ware
       have had a chance to look it over.

* Wed Aug 06 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartd - can now monitor ATA drives behind 3ware controllers.
- [BA] smartd - changed some FATAL out of memory error messages from
       syslog level LOG_INFO to LOG_CRIT.
- [BA] smartctl - added code to look at ATA drives behind 3ware RAID
       controllers using the 3w-xxxx driver.  Note that for technical
       reasons related to the 3w-xxxx driver, the "Enable Autosave",
       "Enable Automatic Offline" commands are not implemented.
       I will add this to smartd shortly.
- [BA] smartd - modified sleep loop, so that smartd no longer comes
       on the run queue every second.  Instead, unless interrupted,
       it sleeps until the next polling time, when it wakes up. Now
       smartd also tries to wake up at exactly the right
       intervals (nominally 30 min) even if the user has been sending
       signals to it.
- [GG] add Fujitsu MHN2300AT to vendoropts_9_seconds.
- [EB] Fujitsu change in knowndrives ... match the whole MPD and
       MPE series for vendoropts_9_seconds.
- [BA] smartd bug, might cause segv if a device can not be opened. Was
       due to missing comma in char* list.  Consequence is that email
       failure messages might have had the wrong Subject: heading for
       errorcount, FAILEDhealthcheck, FAILEDreadsmartdata, FAILEDreadsmarterrorlog,
       FAILEDreadsmartsefltestlog, FAILEDopendevice were all displaced by
       one.  And FAILEDopendevice might have caused a segv if -m was being
       used as a smartd Directive.

* Wed Jul 23 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] Cleaned up smartmontools.spec so that upgrading, removing
       and other such operations correctly preserve running behavior
       and booting behavior of smartd.
- [BA] Improved formatting of ATA Error Log printout, and added
       listing of names of commands that caused the error. Added
       obsolete ATA-4 SMART feature commands to table, along with
       obsolete SFF-8035i SMART feature command.
- [PW] Added atacmdnames.[hc], which turn command register &
       feature register pairs into ATA command names.
- [BA] Added conveyance self-test.  Some code added for selective
       self-tests, but #ifdefed out.
- [BA] Modified smartd exit status and log levels.  If smartd is
       "cleanly" terminated, for example with SIGTERM, then its
       exit messages are now logged at LOG_INFO not LOG_CRIT
- [BA] Added Attribute IDs  (Fujitsu) 0xCA - 0xCE.  This is decimal
       202-206. Added -v switches for interpretation of Attributes
       192, 198 and 201. 
- [BA] Made smartmontools work with any endian order machine for:
       - SMART selftest log
       - SMART ATA error log
       - SMART Attributes values
       - SMART Attributes thesholds
       - IDENTIFY DEVICE information
       - LOG DIRECTORY
       Smartmontools is now free of endian bias and works correctly
       on both little- and big-endian hardware.  This has been tested by
       three independent PPC users on a variety of ATA and SCSI hardware.
- [DG] Check that certain SCSI command responses are well formed. If
       IEC mode page response is not well formed exit smartctl. This
       is to protect aacraid. smartd should ignore a aacraid device.

* Mon Jun 16 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartctl: added column to -A output to show if Attributes are
       updated only during off-line testing or also during normal
       operation.

* Thu Jun 10 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartd: attempt to enable/disable automatic offline testing even
       if the disk appears not to support it.  Now the same logic
       as smartctl.
- [BA] Added definition of Attribute 201, soft read error rate.
- [BA] Added IBM/Hitachi IC35L120AVV207-1 (GXP-180) and corresponding
       8MB Cache GXP-120 to drive database.
- [BA] smartd: if DEVICESCAN Directive used in smartd.conf, and
       -I, -R or -r Directives used in conjunction with this, got
       segv errors.  Fixed by correcting memory allocation calls.
- [BA] smartd: enable automatic offline testing was broken due
       to cut-and-paste error that disabled it instead of
       enabling it.  Thanks to Maciej W. Rozycki for pointing
       out the problem and solution.
- [BA] Fixed "spelling" of some Attribute names to replace spaces
       in names by underscores. (Fixed field width easier for awk
       style parsing.)
- [BA] Added mods submitted by Guilhem Frezou to support Attribute 193
       being load/unload cycles. Add -v 193,loadunload option, useful
       for Hitachi drive DK23EA-30, and add this drive to knowndrive.c
       Add meaning of attribute 250 : Read error retry rate
- [BA] Added another entry for Samsung drives to knowndrive table.
- [DG] Refine SCSI log sense command to do a double fetch in most cases
       (but not for the TapeAlert log page). Fix TapeAlert and Self Test
       log pgae response truncation.
- [PW] Added 'removable' argument to -d Directive for smartd.  This indicates
       that smartd should continue (rather than exit) if the device does not 
       appear to be present.
- [BA] Modified smartmontools.spec [Man pages location] and
       smartd.initd [Extra space kills chkconfig!] for Redhat 6.x
       compatibility (thanks to Gerald Schnabel).

* Wed May 7 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [EB] Add another Fujitsu disk to knowndrives.c
- [GG] match for scsi/ and ide/ in case of devfs to exclude false postives
- [BA] If SCSI device listed in /etc/smartd.conf fails to open or do
       SMART stuff correctly, or not enough space
       to list all SCSI devices, fail with error unless
       -DSCSIDEVELOPMENT set during compile-time.
- [BA] Added automatic recognition of /dev/i* (example: /dev/ide/...)
       as an ATA device.
- [DG] Add "Device type: [disk | tape | medium changer | ...]" line to
       smartctl -i output for SCSI devices.
- [PW] Fixed bug in smartd where test email would be sent regularly (for
       example, daily if the user had specified -M daily) instead of just
       once on startup.
- [KM] More TapeAlert work. Added translations for media changer
       alerts. TapeAlert support reported according to the log page
       presence. ModeSense not attempted for non-ready tapes (all
       drives do not support this after all). Get peripheral type from
       Inquiry even if drive info is not printed. Add QUIETON()
       QUIETOFF() to TapeAlert log check.
- [BA] Stupid bug in atacmds.c minor_str[] affected ataVersionInfo().
       Two missing commas meant that minor_str[] had two few elements,
       leading to output like this:
       Device Model:     Maxtor 6Y120L0
       Serial Number:    Y40BF74E
       Firmware Version: YAR41VW0
       Device is:        Not in smartctl database [for details use: -P showall]
       ATA Version is:   7
       ATA Standard is:  9,minutes
                         ^^^^^^^^^
       Missing commas inserted.
- [BA] Fixed smartd bug.  On device registration, if ATA device did
       not support SMART error or self-test logs but user had asked to
       monitor them, an attempt would be made to read them anyway,
       possibly generating "Drive Seek" errors.  We now check that
       the self-test and error logs are supported before trying to
       access them the first time.
- [GG/BA] Fixed bug where if SMART ATA error log not supported,
       command was tried anyway. Changed some error printing to use
       print handlers.
- [GG] Makefile modifications to ease packaging
- [DG] Did work for TapeAlerts (SCSI). Now can detect /dev/nst0 as a
       SCSI device. Also open SCSI devices O_NONBLOCK so they don't
       hang on open awaiting media. The ATA side should worry about
       this also: during a DEVICESCAN a cd/dvd device without media
       will hang. Added some TapeAlert code suggested by Kai Makisara.

* Mon Apr 21 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [PW] Extended the -F option/Directive to potentially fix other firmware
       bugs in addition to the Samsung byte-order bug.  Long option name is
       now --firmwarebug and the option/Directive accepts an argument
       indicating the type of firmware bug to fix.
- [BA] Fixed a bug that prevented the enable automatic off-line
       test feature from enabling.  It also prevented the enable Attribute
       autosave from working.  See CVS entry for additional details.
- [PW] Modified the -r/--report option (smartctl and smartd) to allow the
       user to specify the debug level as a positive integer.
- [BA] Added --log directory option to smartctl.  If the disk
       supports the general-purpose logging feature set (ATA-6/7)
       then this option enables the Log Directory to be printed.
       This Log Directory shows which device logs are available, and
       their lengths in sectors.
- [PW] Added -P/--presets option to smartctl and -P Directive to smartd.
- [GG] Introduce different exit codes indicating the type of problem
       encountered for smartd.
- [DG] Add non-medium error count to '-l error' and extended self test
       duration to '-l selftest'. Get scsi IEs and temperature changes
       working in smartd. Step over various scsi disk problems rather
       than abort smartd startup.
- [DG] Support -l error for SCSI disks (and tapes). Output error counter
       log pages.
- [BA] Added -F/--fixbyteorder option to smartctl.  This allows us to read
       SMART data from some disks that have byte-reversed two- and four-
       byte quantities in their SMART data structures.
- [BA] Fixed serious bug: the -v options in smartd.conf were all put
       together and used together, not drive-by-drive.
- [PW] Added knowndrives.h and knowndrives.c.  The knowndrives array
       supersedes the drivewarnings array.
- [GG] add {-p,--pidfile} option to smartd to write a PID file on
       startup. Update the manpage accordingly.
- [DG] Fix scsi smartd problem detecting SMART support. More cleaning
       and fix (and rename) scsiTestUnitReady(). More scsi renaming.
- [BA] Fixed smartd so that if a disk that is explictily listed is not
       found, then smartd will exit with nonzero status BEFORE forking.
       If a disk can't be registered, this will also be detected before
       forking, so that init scripts can react correctly.
- [BA] Replaced all linux-specific ioctl() calls in atacmds.c with
       a generic handler smartcommandhandler().  Now the only routine
       that needs to be implemented for a given OS is os_specific_handler().
       Also implemented the --report ataioctl. This provides 
       two levels of reporting.  Using the option once gives a summary
       report of device IOCTL transactions.  Using the option twice give
       additional info (a printout of ALL device raw 512 byte SMART
       data structures).  This is useful for debugging.
- [DG] more scsi cleanup. Output scsi device serial number (VPD page
       0x80) if available as part of '-i'. Implement '-t offline' as
       default self test (only self test older disks support).
- [BA] Changed crit to info in loglevel of smartd complaint to syslog
       if DEVICESCAN enabled and device not found.
- [BA] Added -v 194,10xCelsius option/Directive. Raw Attribute number
       194 is ten times the disk temperature in Celsius.
- [DG] scsicmds.[hc] + scsiprint.c: clean up indentation, remove tabs.
       Introduce new intermediate interface based on "struct scsi_cmnd_io"
       to isolate SCSI generic commands + responses from Linux details;
       should help port to FreeBSD of SCSI part of smartmontools.
       Make SCSI command builders more parametric.

* Thu Mar 13 2003  Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartctl: if HDIO_DRIVE_TASK ioctl() is not implemented (no
       kernel support) then try to assess drive health by examining
       Attribute values/thresholds directly.
- [BA] smartd/smartctl: added -v 200,writeerrorcount option/Directive
       for Fujitsu disks.
- [BA] smartd: Now send email if any of the SMART commands fails,
       or if open()ing the device fails.  This is often noted
       as a common disk failure mode.
- [BA] smartd/smartctl: Added -v N,raw8 -v N,raw16 and -v N,raw48
       Directives/Options for printing Raw Attributes in different
       Formats.
- [BA] smartd: Added -r ID and -R ID for reporting/tracking Raw
       values of Attributes.
- [BA] smartd/smartctl: Changed printing of spin-up-time attribute
       raw value to reflect current/average as per IBM standard.
- [BA] smartd/smartctl: Added -v 9,seconds option for disks which
       use Attribute 9 for power-on lifetime in seconds.
- [BA] smartctl: Added a warning message so that users of some IBM
       disks are warned to update their firmware.  Note: we may want
       to add a command-line flag to disable the warning messages.
       I have done this in a general way, using regexp, so that we
       can add warnings about any type of disk that we wish..

* Wed Feb 12 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] smartd: Created a subdirectory examplescripts/ of source
       directory that contains executable scripts for the -M exec PATH
       Directive of smartd.
- [BA] smartd: DEVICESCAN in /etc/smartd.conf
       can now be followed by all the same Directives as a regular
       device name like /dev/hda takes.  This allows one to use
       (for example):
       DEVICESCAN -m root@example.com
       in the /etc/smartd.conf file.
- [BA] smartd: Added -c (--checkonce) command-line option. This checks
       all devices once, then exits.  The exit status can be
       used to learn if devices were detected, and if smartd is
       functioning correctly. This is primarily for Distribution
       scripters.
- [BA] smartd: Implemented -M exec Directive for
       smartd.conf.  This makes it possible to run an
       arbitrary script or mailing program with the
       -m option.
- [PW] smartd: Modified -M Directive so that it can be given
       multiple times.  Added -M exec Directive.

* Tue Jan 21 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] Fixed bug in smartctl pointed out by Pierre Gentile.
       -d scsi didn't work because tryata and tryscsi were 
       reversed -- now works on /devfs SCSI devices.
- [BA] Fixed bug in smartctl pointed out by Gregory Goddard
       <ggoddard@ufl.edu>.  Manual says that bit 6 of return
       value turned on if errors found in smart error log.  But
       this wasn't implemented.
- [BA] Modified printing format for 9,minutes to read
       Xh+Ym not X h + Y m, so that fields are fixed width.
- [BA] Added Attribute 240 "head flying hours"

* Sun Jan 12 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [BA] As requested, local time/date now printed by smartctl -i

* Thu Jan 9 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [PW] Added 'help' argument to -v for smartctl
- [PW] Added -D, --showdirectives option to smartd

* Sat Jan 4 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [DG] add '-l selftest' capability for SCSI devices (update smartctl.8)
- [BA] smartd,smartctl: added additional Attribute modification option
  -v 220,temp and -v 9,temp.
- [PW] Renamed smartd option -X to -d
- [PW] Changed smartd.conf Directives -- see man page
- [BA/DG] Fixed uncommented comment in smartd.conf
- [DG] Correct 'Recommended start stop count' for SCSI devices
- [PW] Replaced smartd.conf directive -C with smartd option -i
- [PW] Changed options for smartctl -- see man page.
- [BA] Use strerror() to generate system call error messages.
- [BA] smartd: fflush() all open streams before fork().
- [BA] smartctl, smartd simplified internal handling of checksums
  for simpler porting and less code.

* Sun Dec 8 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- [PW] smartd --debugmode changed to --debug
- [BA] smartd/smartctl added attribute 230 Head Amplitude from
  IBM DPTA-353750.
- [PW] Added list of proposed new options for smartctl to README.
- [PW] smartd: ParseOpts() now uses getopt_long() if HAVE_GETOPT_LONG is
  defined and uses getopt() otherwise.  This is controlled by CPPFLAGS in
  the Makefile.
- [BA] smartd: Fixed a couple of error messages done with perror()
  to redirect them as needed.
- [BA] smartctl: The -O option to enable an Immediate off-line test
  did not print out the correct time that the test would take to
  complete.  This is because the test timer is volatile and not
  fixed.  This has been fixed, and the smartctl.8 man page has been
  updated to explain how to track the Immediate offline test as it
  progresses, and to further emphasize the differences between the
  off-line immediate test and the self-tests.
- [BA] smartd/smartctl: Added new attribute (200) Multi_Zone_Error_Rate
- [BA] smartctl: modified so that arguments could have either a single -
  as in -ea or multiple ones as in -e -a.  Improved warning message for
  device not opened, and fixed error in redirection of error output of
  HD identity command.
- [PW] smartd: added support for long options.  All short options are still
  supported; see manpage for available long options.
- [BA] smartctl.  When raw Attribute value was 2^31 or larger, did
  not print correctly.

* Fri Nov 22 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- Allen: smartd: added smartd.conf Directives -T and -s.  The -T Directive
  enables/disables Automatic Offline Testing.  The -s Directive
  enables/disables Attribute Autosave. Documentation and
  example configuration file updated to agree.
- Allen: smartd: user can make smartd check the disks at any time
  (ie, interrupt sleep) by sending signal SIGUSR1 to smartd.  This
  can be done for example with:
  kill -USR1 <pid>
  where <pid> is the process ID number of smartd.
- Bolso: scsi: don't trust the data we receive from the drive too
  much. It very well might have errors (like zero response length).
  Seen on Megaraid logical drive, and verified in the driver source.
- Allen: smartd: added Directive -m for sending test email and
  for modifying email reminder behavior.  Updated manual, and sample
  configuration file to illustrate & explain this.
- Allen: smartd: increased size of a continued smartd.conf line to
  1023 characters.
- Allen: Simplified Directive parsers and improved warning/error
  messages.

* Sun Nov 17 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- Fixed bug in smartd where testunitready logic inverted
  prevented functioning on scsi devices.
- Added testunitnotready to smartctl for symmetry with smartd.
- Brabec: added Czech descriptions to .spec file
- Brabec: corrected comment in smartd.conf example
- Changed way that entries in the ATA error log are printed,
  to make it clearer which is the most recent error and
  which is the oldest one.
- Changed Temperature_Centigrade to Temperature_Celsius.
  The term "Centigrade" ceased to exist in 1948.  (c.f
  http://www.bartleby.com/64/C004/016.html).

* Wed Nov 13 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- smartd SCSI devices: can now send warning email message on failure
- Added a new smartd configuration file Directive: -M ADDRESS.
  This sends a single warning email to ADDRESS for failures or
  errors detected with the -c, -L, -l, or -f Directives.

* Mon Nov 11 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- Modified perror() statements in atacmds.c so that printout for SMART
  commands errors is properly suppressed or queued depending upon users
  choices for error reporting modes.
- Added Italian descriptions to smartmontools.spec file.
- Started impementing send-mail-on-error for smartd; not yet enabled.
 
* Sun Nov 10 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- Added -P (Permissive) Directive to smartd.conf file to allow SMART monitoring of
  pre-ATA-3 Rev 4 disks that have SMART but do not have a SMART capability bit.

* Thu Nov 7 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- Added a Man section 5 page for smartd.conf
- Changed Makefile so that the -V option does not reflect file state
  before commit!
- modified .spec file so that locale information now contains
  character set definition.   Changed pt_BR to pt since we do not use any
  aspect other than language.  See man setlocale.
- smartctl: added new options -W, -U, and -P to control if and how the
  smartctl exits if an error is detected in either a SMART data
  structure checksum, or a SMART command returns an error.
- modified manual page to break options into slightly more logical
  categories.
- reformatted 'usage' message order to agree with man page ordering

* Mon Nov 4 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- smartctl: added new options -n and -N to force device to be ATA or SCSI
- smartctl: no longer dies silently if device path does not start/dev/X
- smartctl: now handles arbitrary device paths
- Added additional macros for manual and sbin paths in this SPEC file.
- Modified Makefile to install /etc/smartd.conf, but without overwriting existing config file
- Modified this specfile to do the same, and to not remove any files that it did not install

* Thu Oct 30 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- Fixed typesetting error in man page smartd.8
- Removed redundant variable (harmless) from smartd.c

* Wed Oct 29 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- Added a new directive for the configuration file.  If the word
  DEVICESCAN appears before any non-commented material in the
  configuration file, then the confi file will be ignored and the
  devices wil be scanned.
- Note: it has now been confirmed that the code modifications between
  5.0.23 and 5.0.24 have eliminated the GCC 3.2 problems.  Note that
  there is a GCC bug howerver, see #848 at
  http://gcc.gnu.org/cgi-bin/gnatsweb.pl?database=gcc&cmd=query
- Added new Directive for Configuration file:
  -C <N> This sets the time in between disk checks to be <N>
  seconds apart.  Note that  although  you  can  give
  this Directive multiple times on different lines of
  the configuration file, only the final  value  that
  is  given  has  an  effect,  and applies to all the
  disks.  The default value of <N> is 1800  sec,  and
  the minimum allowed value is ten seconds.
- Problem wasn't the print format. F.L.W. Meunier <0@pervalidus.net>
  sent me a gcc 3.2 build and I ran it under a debugger.  The
  problem seems to be with passing the very large (2x512+4) byte
  data structures as arguments.  I never liked this anyway; it was
  inherited from smartsuite.  So I've changed all the heavyweight
  functions (ATA ones, anyone) to just passing pointers, not hideous
  kB size structures on the stack.  Hopefully this will now build OK
  under gcc 3.2 with any sensible compilation options.
- Because of reported problems with GCC 3.2 compile, I have gone
  thorough the code and explicitly changed all print format
  parameters to correspond EXACTLY to int unless they have to be
  promoted to long longs.  To quote from the glibc bible: [From
  GLIBC Manual: Since the prototype doesn't specify types for
  optional arguments, in a call to a variadic function the default
  argument promotions are performed on the optional argument
  values. This means the objects of type char or short int (whether
  signed or not) are promoted to either int or unsigned int, as
  required.
- smartd, smartctl now warn if they find an attribute whose ID
  number does not match between Data and Threshold structures.
- Fixed nasty bug which led to wrong number of arguments for a
  varargs statement, with attendent stack corruption.  Sheesh!
  Have added script to CVS attic to help find such nasties in the
  future.

* Tue Oct 29 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- Eliminated some global variables out of header files and other
  minor cleanup of smartd.
- Did some revision of the man page for smartd and made the usage
  messages for Directives consistent.
- smartd: prints warning message when it gets SIGHUP, saying that it is
  NOT re-reading the config file.
- smartctl: updated man page to say self-test commands -O,x,X,s,S,A
  appear to be supported in the code.  [I can't test these,  can anyone
  report?]
- smartctl: smartctl would previously print the LBA of a self-test
  if it completed, and the LBA was not 0 or 0xff...f However
  according to the specs this is not correct.  According to the
  specs, if the self-test completed without error then LBA is
  undefined.  This version fixes that.  LBA value only printed if
  self-test encountered an error.
- smartd has changed significantly. This is the first CVS checkin of
  code that extends the options available for smartd.  The following
  options can be placed into the /etc/smartd.conf file, and control the
  behavior of smartd.
- Configuration file Directives (following device name):
  -A     Device is an ATA device
  -S     Device is a SCSI device
  -c     Monitor SMART Health Status
  -l     Monitor SMART Error Log for changes
  -L     Monitor SMART Self-Test Log for new errors
  -f     Monitor for failure of any 'Usage' Attributes
  -p     Report changes in 'Prefailure' Attributes
  -u     Report changes in 'Usage' Attributes
  -t     Equivalent to -p and -u Directives
  -a     Equivalent to -c -l -L -f -t Directives
  -i ID  Ignore Attribute ID for -f Directive
  -I ID  Ignore Attribute ID for -p, -u or -t Directive
  #      Comment: text after a hash sign is ignored
  \      Line continuation character
- cleaned up functions used for printing CVS IDs.  Now use string
  library, as it should be.
- modified length of device name string in smartd internal structure
  to accomodate max length device name strings
- removed un-implemented (-e = Email notification) option from
  command line arg list.  We'll put it back on when implemeneted.
- smartd now logs serious (fatal) conditions in its operation at
  loglevel LOG_CRIT rather than LOG_INFO before exiting with error.
- smartd used to open a file descriptor for each SMART enabled
- device, and then keep it open the entire time smartd was running.
  This meant that some commands, like IOREADBLKPART did not work,
  since the fd to the device was open.  smartd now opens the device
  when it needs to read values, then closes it.  Also, if one time
  around it can't open the device, it simply prints a warning
  message but does not give up.  Have eliminated the .fd field from
  data structures -- no longer gets used.
- smartd now opens SCSI devices as well using O_RDONLY rather than
  O_RDWR.  If someone can no longer monitor a SCSI device that used
  to be readable, this may well be the reason why.
- smartd never checked if the number of ata or scsi devices detected
  was greater than the max number it could monitor.  Now it does.

* Fri Oct 25 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- changes to the Makefile and spec file so that if there are ungzipped manual
  pages in place these will be removed so that the new gzipped man pages are
  visible.
- smartd on startup now looks in the configuration file /etc/smartd.conf for
  a list of devices which to include in its monitoring list.  See man page
  (man smartd) for syntax. If not found, try all ata and ide devices.
- smartd: close file descriptors of SCSI device if not SMART capable
  Closes ALL file descriptors after forking to daemon.
- added new temperature attribute (231, temperature)
- smartd: now open ATA disks using O_RDONLY

* Thu Oct 24 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- smartd now prints the name of a failed or changed attribute into logfile,
  not just ID number
- Changed name of -p (print version) option to -V
- Minor change in philosophy: if a SMART command fails or the device
    appears incapable of a SMART command that the user has asked for,
    complain by printing an error message, but go ahead and try
    anyway.  Since unimplemented SMART commands should just return an
    error but not cause disk problems, this should't cause any
    difficulty.
- Added two new flags: q and Q.  q is quiet mode - only print: For
    the -l option, errors recorded in the SMART error log; For the -L
    option, errors recorded in the device self-test log; For the -c
    SMART "disk failing" status or device attributes (pre-failure or
    usage) which failed either now or in the past; For the -v option
    device attributes (pre-failure or usage) which failed either now
    or in the past.  Q is Very Quiet mode: Print no ouput.  The only
    way to learn about what was found is to use the exit status of
    smartctl.
- smartctl now returns sensible values (bitmask).  See smartctl.h
    for the values, and the man page for documentation.
- The SMART status check now uses the correct ATA call.  If failure
    is detected we search through attributes to list the failed ones.
    If the SMART status check shows GOOD, we then look to see if their
    are any usage attributes or prefail attributes have failed at any
    time.  If so we print them.
- Modified function that prints vendor attributes to say if the
    attribute has currently failed or has ever failed.
- -p option now prints out license info and CVS strings for all
    modules in the code, nicely formatted.
- Previous versions of this code (and Smartsuite) only generate
    SMART failure errors if the value of an attribute is below the
    threshold and the prefailure bit is set.  However the ATA Spec
    (ATA4 <=Rev 4) says that it is a SMART failure if the value of an
    attribute is LESS THAN OR EQUAL to the threshold and the
    prefailure bit is set.  This is now fixed in both smartctl and
    smartd.  Note that this is a troubled subject -- the original
    SFF 8035i specification defining SMART was inconsistent about
    this.  One section says that Attribute==Threshold is pass,
    and another section says it is fail.  However the ATA specs are
    consistent and say Attribute==Threshold is a fail.
- smartd did not print the correct value of any failing SMART attribute.  It
    printed the index in the attribute table, not the attribute
    ID. This is fixed.
- when starting self-tests in captive mode ioctl returns EIO because
    the drive has been busied out.  Detect this and don't return an eror
    in this case.  Check this this is correct (or how to fix it?)
 - fixed possible error in how to determine ATA standard support
    for devices with no ATA minor revision number.
- device opened only in read-only not read-write mode.  Don't need R/W 
    access to get smart data. Check this with Andre.
- smartctl now handles all possible choices of "multiple options"
    gracefully.  It goes through the following phases of operation,
    in order: INFORMATION, ENABLE/DISABLE, DISPLAY DATA, RUN/ABORT TESTS.
    Documentation has bee updated to explain the different phases of
    operation.  Control flow through ataPrintMain()
    simplified.
- If reading device identity information fails, try seeing if the info
    can be accessed using a "DEVICE PACKET" command.  This way we can
    at least get device info.
- Modified Makefile to automatically tag CVS archive on issuance of
    a release
- Modified drive detection so minor device ID code showing ATA-3 rev
    0 (no SMART) is known to not be SMART capable.
- Now verify the checksum of the device ID data structure, and of the
    attributes threshold structure.  Before neither of these
    structures had their checksums verified.
- New behavior vis-a-vis checksums.  If they are wrong, we log
    warning messages to stdout, stderr, and syslog, but carry on
    anyway.  All functions now call a checksumwarning routine if the
    checksum doesn't vanish as it should.
- Changed Read Hard Disk Identity function to get fresh info from
    the disk on each call rather than to use the values that were read
    upon boot-up into the BIOS.  This is the biggest change in this
    release.  The ioctl(device, HDIO_GET_IDENTITY, buf ) call should
    be avoided in such code.  Note that if people get garbled strings
    for the model, serial no and firmware versions of their drives,
    then blame goes here (the BIOS does the byte swapping for you,
    apparently!)
- Function ataSmartSupport now looks at correct bits in drive
    identity structure to verify first that these bits are valid,
    before using them.
- Function ataIsSmartEnabled() written which uses the Drive ID state
    information to tell if SMART is enabled or not.  We'll carry this
    along for the moment without using it.
- Function ataDoesSmartWork() guaranteed to work if the device
    supports SMART.
- Replace some numbers by #define MACROS
- Wrote Function TestTime to return test time associated with each
    different type of test.
- Thinking of the future, have added a new function called
    ataSmartStatus2().  Eventually when I understand how to use the
    TASKFILE API and am sure that this works correctly, it will
    replace ataSmartStatus().  This queries the drive directly to
    see if the SMART status is OK, rather than comparing thresholds to
    attribute values ourselves. But I need to get some drives that fail
    their SMART status to check it.

* Thu Oct 17 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
-   Removed extraneous space before some error message printing.
-   Fixed some character buffers that were too short for contents.
    Only used for unrecognized drives, so probably damage was minimal.

* Wed Oct 16 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
-   Initial release.  Code is derived from smartsuite, and is
    intended to be compatible with the ATA/ATAPI-5 specifications.
-   For IBM disks whose raw temp data includes three temps. print all
    three
-   print timestamps for error log to msec precision
-   added -m option for Hitachi disks that store power on life in
    minutes
-   added -L option for printing self-test error logs
-   in -l option, now print power on lifetime, so that one can see
    when the error took place
-   updated SMART structure definitions to ATA-5 spec
-   added -p option
-   added -f and -F options to enable/disable autosave threshold
    parameters

