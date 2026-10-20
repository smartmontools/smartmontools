# smartmontools Installation Instructions

## Supported platforms

Builds have been successfully tested for:
- Linux: i686, x86_64, arm64, riscv64, s390x
- FreeBSD: x86_64, arm64, powerpc64
- NetBSD: x86_64, sparc64
- OpenBSD: x86_64
- macOS: i686, x86_64, arm64
- OpenIndiana: x86_64
- Solaris: x86_64
- Windows: i686, x86_64, arm64
- Cygwin: x86_64
- OS/2(ArcaOS): i686

The QNX port is still present in the source code, but currently unmaintained.

## Installation

### Install precompiled package

Many distributions provide precompiled `smartmontools` packages.
For available versions, see for example
[repology.org](https://repology.org/project/smartmontools/versions)
([Badge](https://repology.org/badge/vertical-allrepos/smartmontools.svg)).

DMG files for macOS are also available from the
[releases page](https://github.com/smartmontools/smartmontools/releases).

**Experimental** binaries for more platforms are available from the
[CI builds](https://github.com/smartmontools/smartmontools-builds/releases) and from other
[workflows](https://github.com/smartmontools/smartmontools/actions).

#### Install the Windows package

[NSIS](https://nsis.sourceforge.io/Main_Page) installers for the Windows version of smartmontools
are available from the [releases page](https://github.com/smartmontools/smartmontools/releases).

The default installation type, "Full", creates start menu shortcuts, including an uninstaller, and adds
the installation directory to the User PATH variable.
The type "Extract files only" is useful to unpack a "portable" version without affecting the Windows
registry.
The files could also be unpacked with [7-Zip](https://www.7-zip.org/).

The (32-bit) installer allows to select 32-bit (x86) or 64-bit (x86_64) executables.
Both versions provide identical functionality.
In very rare cases, the 32-bit version fails on 64-bit Windows due to bugs in the 32-bit part
of some 64-bit device driver.

The minimum Windows version supported is usually Vista.
It depends on the version of the used [MinGW-w64](https://www.mingw-w64.org/) toolchain.

See also section [Windows: installed files and related installer options](#windows-files)
below.

### Install from source tarball

This requires a C++11 compatible toolchain (`g++`, `clang++`, ...) and a suitable version of
`make` (GNU make, BSD make, ...).

Download the latest source tarball from the
[releases page](https://github.com/smartmontools/smartmontools/releases).
Older releases are still available at
[SourceForge](https://sourceforge.net/projects/smartmontools/files/smartmontools/).

```
curl -L -O https://github.com/smartmontools/smartmontools/releases/download/RELEASE_TAG/smartmontools-VERSION.tar.gz
tar -xzf smartmontools-VERSION.tar.gz
cd smartmontools-VERSION
./configure
make
sudo make install
```

### Install from git repository

This additionally requires `git` and GNU autotools (`autoconf`, `automake`, `libtool`).

```
git clone https://github.com/smartmontools/smartmontools.git
cd smartmontools
./autogen.sh
./configure
make
sudo make install
```

### Install from ZIP file

If `git` is unavailable, the current source could also be downloaded as a ZIP file.
Then `SMARTMONTOOLS_TEST_BUILD` needs to be set in the environment because the
version information cannot retrieved from the `git log`.

```
curl -L -O https://github.com/smartmontools/smartmontools/archive/refs/heads/main.zip
unzip main.zip
cd smartmontools-main
./autogen.sh
export SMARTMONTOOLS_TEST_BUILD=1
./configure
make
sudo make install
```

### Run without `make install` directly from build directory

```
./configure
make
src/smartctl -V
src/smartctl --scan
sudo src/smartctl -B lib/drivedb.h -x /dev/sda
echo /dev/sda | sudo src/smartd -B lib/drivedb.h -c - -q onecheck
```

Note: if shared libraries are enabled (see `--enable-shared` below), the executables in
`src` are wrappers.
These run the real executables from `src/.libs` and ensure that the correct shared
`libsmartmon` library from `lib/.libs` is loaded.
The wrappers provide two extra options:

```
src/smartctl --lt-debug # print debug info before running smartctl
src/smartctl --lt-dump-script > src/smartctl.sh # create a wrapper shell script
```

## Build examples

### Show `./configure` options and environment variables

```
./configure --help
```

### Reduce verbosity of current build

```
make V=0
```

### Reduce verbosity of all builds

```
./configure --enable-silent-rules
make # undo with 'V=1'
```

### Run build in a separate directory

```
mkdir build-config1
cd build-config1
../configure [OPTION...]
make
```

Or with absolute path names:

```
mkdir /tmp/build-config1
cd /tmp/build-config1
/path/to/smartmontools-VERSION/configure [OPTION...]
make
```

This allows to handle multiple configurations simultaneously.

### Install in another destination for testing

```
make DESTDIR=/tmp/test-install install
```

Note: `DESTDIR` must be an absolute path.

### Uninstall

```
make uninstall
```

```
make DESTDIR=/tmp/test-install uninstall
```

Note: this does not remove the empty directories.

### Clean up

```
make clean            # Remove items generated by 'make' ...
make distclean        # ... and items generated by './configure' ...
make maintainer-clean # ... and items generated by './autogen.sh'.
./autogen.sh --clean  # Remove only items generated by './autogen.sh'.
```

### Run `smartctl -V` and check `drivedb.h` syntax

```
make check
```

Note: shows only the `VERSION` string of the `drivedb.h` file if cross-compiling.

### Create a source tarball

```
make dist
```

### Create a binaries tarball

```
make bin-dist
```

### Exclude development files from installation

```
./configure --without-devel
```

### Create a shared instead of a static `libsmartmon` library

```
./configure --enable-shared --disable-static
```

### Link binaries statically

```
./configure --enable-static-link
```

Note: this is not compatible with `--enable-shared`.

### Use `clang++` instead of `c++/g++`

```
./configure CC=clang CXX=clang++
```

### Create a *CLang Static Analyzer* report

```
util/clang-scan-build.sh make
```

Open `clang-report/DATE-TIME/index.html` in browser.
The directory does not exist if nothing has been reported.

### Use `gmake` instead of `make`

```
./configure MAKE=gmake
gmake
sudo gmake install
```

### Create a reproducible build

```
./configure SOURCE_DATE_EPOCH=$(date +%s)
make dist
make
make bin-dist
```

If `make` does not support `export` (GNU make) or `.export` (BSD make):

```
export SOURCE_DATE_EPOCH=$(date +%s)
./configure
make dist
make
make bin-dist
```

Note: `make dist` and `make bin-dist` require the GNU version of `tar` (`gnutar`, `gtar`,
`tar`).

### Keep the installed version of `drivedb.h` unchanged

```
./configure --with-drivedbinstdir
make
sudo make install
sudo /usr/local/sbin/update-smart-drivedb --install
```

This keeps `/usr/local/share/smartmontools/drivedb.h` unchanged.
The file `/usr/local/var/lib/smartmontools/drivedb.h` is used and updated then.

### Prevent `font name 'CW' is deprecated` warning from `groff(1)`

```
./configure --with-troff-cw-font=CR
```

Note: this does not affect the formatting of man pages for a terminal window.

### Linux: add `--capabilities` and systemd `Type=notify` support to `smartd`

```
sudo apt install libcap-ng libsystemd-dev # Debian/Ubuntu
./configure --with-libcap-ng --with-libsystemd
```

Note: the libraries are always detected if installed unless `--without-lib*` are specified.
The extra `--with-lib*` options let `./configure` fail if the libraries could not be detected.

### Windows: build a NSIS installer containing 32-bit and 64-bit smartmontools

```
mkdir build64
cd build64
../configure --build=$(../config.guess) --host=x86_64-w64-mingw32 --enable-static-link
make
make distdir-win32
cd ..
mkdir build32
cd build32
../configure --build=$(../config.guess) --host=i686-w64-mingw32 --enable-static-link
make
make distdir-win32
make builddir_win64=../../build64/src installer-win32
```

Tested on Linux and Cygwin.
This requires the packages dos2unix, groff, [nsis](https://nsis.sourceforge.io/Main_Page) and
man2html.

### Windows: build with MSVC17

Run this on [MSYS2](https://www.msys2.org/) or with the cross compile options on
[Cygwin](https://www.cygwin.com/):

```
mkdir build-vc
cd build-vc
../configure # --build=$(../config.guess) --host=x86_64-w64-mingw32
make config-vc
cd ..
MSBuild -m -p:Platform=x64 -p:Configuration=Debug src/os_win32/vc17/smartmontools.sln
MSBuild -m -p:Platform=Win32 -p:Configuration=Release-static src/os_win32/vc17/smartmontools.sln
MSBuild -m -p:Platform=ARM64 -p:Configuration=Release src/os_win32/vc17/smartmontools.sln
```

The compiled files appear in `src/os_win32/vc17/${Platform}/${Configuration}`.
These require at least Windows Vista.

Clean up later:

```
cd build-vc
make clean-vc # distclean-vc # maintainer-clean-vc
```

### Build some new `libsmartmon` example

```
mkdir build
cd build
../configure
make
cd src/examples
editor some-new-example.cpp
./make.sh some-new-example # Windows/Cygwin: some-new-example.exe
./some-new-example --some-option /dev/sda
```

This would also work on a copy of the `build/src/examples` directory because the `make.sh`
script uses absolute path names to adjust the `make` variables.

## Installed files and related `./configure` options

The following list shows the directories and files which are usually installed by default and
the related `./configure` options which modify (or disable) the install locations:

```
/usr/local/                                             # --prefix
/usr/local/etc/                                         # --sysconfdir, --with-smartdscriptdir
/usr/local/etc/smartd.conf
/usr/local/etc/smartd_warning.d/                        # --with-smartdplugindir (--without-smartdplugindir)
/usr/local/etc/smartd_warning.sh
/usr/local/include/                                     # --includedir (--without-devel)
/usr/local/include/smartmon/*.h
/usr/local/lib/                                         # --libdir
/usr/local/lib/libsmartmon.a                            # (--without-devel, --disable-static)
/usr/local/lib/libsmartmon.la                           # (--without-devel)
/usr/local/libexec/                                     # --libexecdir
/usr/local/libexec/smartmontools/examples/              # --with-devel-bin (--without-devel[-bin])
/usr/local/libexec/smartmontools/examples/*
/usr/local/sbin/                                        # --sbindir
/usr/local/sbin/smartctl
/usr/local/sbin/smartd
/usr/local/sbin/update-smart-drivedb                    # (--without-update-smart-drivedb)
/usr/local/share/                                       # --data[root]dir
/usr/local/share/doc/smartmontools/                     # --docdir
/usr/local/share/doc/smartmontools/*
/usr/local/share/doc/smartmontools/examplescripts/      # --with-smartd-scripts
/usr/local/share/doc/smartmontools/examplescripts/*
/usr/local/share/doc/smartmontools/examplesrc/          # --with-devel-src (--without-devel[-src])
/usr/local/share/doc/smartmontools/examplesrc/Makefile
/usr/local/share/doc/smartmontools/examplesrc/*.cpp
/usr/local/share/man/                                   # --mandir
/usr/local/share/man/man5/smartd.conf.5
/usr/local/share/man/man8/smartctl.8
/usr/local/share/man/man8/smartd.8
/usr/local/share/man/man8/update-smart-drivedb.8        # (--without-update-smart-drivedb)
/usr/local/share/smartmontools/                         # --with-drivedb[inst]dir (--without-update-smart-drivedb)
/usr/local/share/smartmontools/drivedb.h
```

The following optional directories and files may be installed or configured if explicitly
enabled or autodetected by `./configure`:

```
/usr/local/etc/default/smartmontools                    # --with-systemdenvfile (--without-systemdenvfile)
/usr/local/etc/init.d/                                  # --with-initscriptdir
/usr/local/etc/init.d/smartd
/usr/local/lib/libsmartmon*.so*                         # --enable-shared
/usr/local/lib/systemd/system/smartd.service            # --with-smartd-service-file (--without-smartd-service-file)
/usr/local/var/                                         # --localstatedir
/usr/local/var/lib/smartmontools/                       # --with-[drivedbdir|attributelog|savestates|jsonstate]
```

## <a id="windows-files"></a>Windows: installed files and related installer options

The following list shows the directories and files installed by the smartmontools installer for windows
and the related options:

```
C:\Program Files\smartmontools\                                       # Destination Folder [...]
C:\Program Files\smartmontools\bin\drivedb.h                          # [X] Program files / drivedb.h (Drive Database)
C:\Program Files\smartmontools\bin\runcmdu.exe                        # [X] Start Menu Shortcuts
C:\Program Files\smartmontools\bin\runcmda.exe                        # [X] Add smartctl to drive menu / ...
C:\Program Files\smartmontools\bin\smartctl.exe                       # [X] Program files / smartctl
C:\Program Files\smartmontools\bin\smartctl-nc.exe                    # [X] Program files / smartctl-nc (GSmartControl)
C:\Program Files\smartmontools\bin\smartd.conf                        # [X] Program files / smartd
C:\Program Files\smartmontools\bin\smartd.exe                         # [X] Program files / smartd
C:\Program Files\smartmontools\bin\smartd_mailer.conf.sample.ps1      # [X] Program files / smartd
C:\Program Files\smartmontools\bin\smartd_mailer.ps1                  # [X] Program files / smartd
C:\Program Files\smartmontools\bin\smartd_warning.cmd                 # [X] Program files / smartd
C:\Program Files\smartmontools\bin\update-smart-drivedb.ps1           # [X] Program files / drivedb.h (Drive Database)
C:\Program Files\smartmontools\bin\wtssendmsg.exe                     # [X] Program files / smartd
C:\Program Files\smartmontools\doc\*                                  # [X] Documentation
C:\Program Files\smartmontools\uninst-smartmontools.exe               # [X] Uninstaller
C:\Program Files\smartmontools\uninst-user-path.txt                   # [X] Add install dir to User PATH
C:\Program Files\smartmontools\uninst-system-path.txt                 # [X] Add install dir to System PATH
C:\ProgramData\Microsoft\Windows\Start Menu\Programs\smartmontools\*  # [X] Start Menu Shortcuts
```

Affected registry entries:

```
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\smartmontools\*  # [X] Uninstaller
HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment\Path    # [X] Add install dir to System PATH
HKCU\Environment\Path                                                     # [X] Add install dir to User PATH
HKCR\Drive\shell\smartctl*                                                # [X] Add smartctl to drive menu / ...
```

The following additional programs are provided:

- `smartctl-nc.exe`: a copy of `smartctl.exe` with the `SUBSYSTEM` type in the EXE header
changed from `CONSOLE` to `GUI`.
This prevents that an empty console window appears when the command is run in background
with output redirected.
Used by [GSmartControl](https://gsmartcontrol.shaduri.dev/).
When `smartctl-nc.exe` is run without redirection from a console window, its output is not
visible because Windows detaches the program from the console.
- `smartd_warning.cmd`: called by `smartd.exe` if the `-m` directive is used and a warning
occurs (similar to `smartd_warning.sh` from the other platforms).
It could be run with a restricted access token, see the `smartd` option `--warn-as-user`.
It supports a `--dryrun` option for testing
- `smartd_mailer.ps1`: called by `smartd_warning.cmd` if the configuration file
`smartd_mailer.conf.ps1` is also present.
It sends emails via SMTP.
See `smartd_mailer.conf.sample.ps1` for details.
- `wtssendmsg.exe`: called by `smartd_warning.cmd` to display warning messages on client
desktops if the first "email" address is `console`, `active` or `connected`.
- `runcmdu.exe`: runs a command line, prints its exit status and then waits for `<return>`.
Used by the start menu shortcuts.
- `runcmda.exe`: like `runcmdu.exe`, but also requests administrator privileges in its
application manifest.
Used by the drive menu entries.

## Usage examples

If `sudo` is unavailable as for example on Windows, run the commands with root/administrator
privileges.

Some platforms provide an API for R/O access to diagnostic information which work without
root/administrator privileges.
This is the case for NVMe devices behind the `stornvme.sys` driver on Windows.

### Scan for devices

```
/usr/local/sbin/smartctl --scan
sudo /usr/local/sbin/smartctl --scan-open
```

Note: it is platform specific whether `--scan` works without `sudo`.

### Print diagnostic information

```
sudo /usr/local/sbin/smartctl -x /dev/sda
```

### Print diagnostic information in JSON format

```
sudo /usr/local/sbin/smartctl -j -x /dev/sda
```

### Print diagnostic information in JSON format with embedded original format

```
sudo /usr/local/sbin/smartctl --json=o -x /dev/sda
```

The lines of the original non-JSON output appear in the JSON array
`{"smartctl": {"output": ["..."]}}`.

### Print diagnostic information in YAML format

```
sudo /usr/local/sbin/smartctl --json=y -x /dev/sda
```

### Print diagnostic information in a *grep*able format

```
sudo /usr/local/sbin/smartctl --json=g -x /dev/sda | grep -E '(power_on_time|temperature).*[0-9]'
```

### Start a short device self-test

```
sudo /usr/local/sbin/smartctl -t short /dev/sda
```

### Use SAT (SCSI/ATA Translation) to access a SATA device behind an USB bridge

```
sudo /usr/local/sbin/smartctl -d sat -x /dev/sda
```

### Test `smartd` with one device

```
echo /dev/sda | sudo /usr/local/sbin/smartd -c - -q onecheck

```

### Test `smartd` email warnings

```
echo /dev/sda -m admin@domain.local -M test | sudo /usr/local/sbin/smartd -c - -q onecheck

```
