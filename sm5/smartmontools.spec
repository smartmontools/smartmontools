Release:  48
Summary:	SMARTmontools - for monitoring S.M.A.R.T. disks and devices
Summary(cs):	SMARTmontools - pro monitorování S.M.A.R.T. diskù a zaøízení
Summary(de):	SMARTmontools - zur Überwachung von S.M.A.R.T.-Platten und-Geräten
Summary(es):	SMARTmontools - para el seguimiento de discos y dispositivos S.M.A.R.T.
Summary(fr):	SMARTmontools - pour le suivi des disques et instruments S.M.A.R.T.
Summary(pt):	SMARTmontools - para monitorar discos e dispositivos S.M.A.R.T.
Summary(it):	SMARTmontools - per monitare dischi e dispositivi S.M.A.R.T.
Summary(pl):	Monitorowanie i kontrola dysków u¿ywaj±æ S.M.A.R.T.
Name:		smartmontools
Version:	5.0
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
BuildRoot:	%{_builddir}/%{name}-%{version}-root
Obsoletes:	smartctl
Obsoletes:      smartd
Obsoletes:	ucsc-smartsuite
Obsoletes:      smartsuite
Packager:       Bruce Allen <smartmontools-support@lists.sourceforge.net>

# Source code can be found at:
# http://ftp1.sourceforge.net/smartmontools/smartmontools-%{version}-%{release}.tar.gz

# CVS ID of this file is:
# $Id: smartmontools.spec,v 1.73 2002/11/29 21:57:17 ballen4705 Exp $

# Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
SMARTmontools controls and monitors storage devices using the
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
SMARTmontools øídí a monitorují zaøízení pro ukládání dat za pou¾ití
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
Die SMARTmontools steuern und überwachen Speichergeräte mittels des
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
SMARTmontools controla y hace el seguimiento de dispositivos de
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
SMARTmontools contrôle et fait le suivi de périphériques de stockage
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
SMARTmontools controla e monitora dispositivos de armazenamento
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
SMARTmontools controlla e monitora dischi che usano il "Self-Monitoring,
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
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
%attr(755,root,root) %{_sbindir}/smartd
%attr(755,root,root) %{_sbindir}/smartctl
%attr(755,root,root) /etc/rc.d/init.d/smartd
%attr(644,root,root) %{_mandir}/man8/smartctl.8*
%attr(644,root,root) %{_mandir}/man8/smartd.8*
%attr(644,root,root) %{_mandir}/man5/smartd.conf.5*
%doc WARNINGS CHANGELOG COPYING TODO README VERSION smartd.conf
%config(noreplace) %{_sysconfdir}/smartd.conf
%config %{_sysconfdir}/smartd.conf.example

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{_builddir}/%{name}-%{version}

# The following are executed only by the binary RPM at install/uninstall

# since this installs the gzipped documentation files, remove
# non-gzipped ones of the same name.
%pre
if [ -f /usr/share/man/man8/smartctl.8 ] ; then
	echo "You MUST delete (by hand) the outdated file /usr/share/man/man8/smartctl.8 to read the new manual page for smartctl."	
fi
if [ -f /usr/share/man/man8/smartd.8 ] ; then
	echo "You MUST delete (by hand) the outdated file /usr/share/man/man8/smartd.8 to read the new manual page for smartd."	
fi

%post
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd restart 1>&2
	echo "Restarted smartd services"
else
        echo "Run \"/etc/rc.d/init.d/smartd start\" to start smartd service now."
	echo "Run \"/sbin/chkconfig --add smartd\", to start smartd service on system boot"
fi
echo "Note that you can now use a configuration file /etc/smartd.conf to control the"
echo "startup behavior of the smartd daemon.  See man 8 smartd for details."

%preun
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd stop 1>&2
	echo "Stopping smartd services"
fi
/sbin/chkconfig --del smartd

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
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

