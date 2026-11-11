## About Smartmontools
The smartmontools package contains two utility programs (`smartctl` and `smartd`) 
to control and monitor storage systems using the **Self-Monitoring, Analysis and 
Reporting Technology System** (SMART) built into most modern ATA/SATA, SCSI/SAS and NVMe disks. 
In many cases, these utilities will provide advanced warning of disk degradation and failure.

Smartmontools was originally derived from the Linux [smartsuite package](https://sourceforge.net/projects/smartsuite/) and supports ATA/SATA, SCSI/SAS, and NVMe disks and also SCSI/SAS tape devices.
It should run on any modern Linux, FreeBSD, NetBSD, OpenBSD, Darwin (macOS), Solaris, Windows, Cygwin, OS/2, eComStation, or QNX system.
Smartmontools can also be run from one of many different Live CDs/DVDs.

## Important links
- [Project homepage](https://www.smartmontools.org/)
- [GitHub repository](https://github.com/smartmontools/smartmontools)
- [CI builds](https://github.com/smartmontools/smartmontools-builds/releases)
- [Project Releases](https://github.com/smartmontools/smartmontools/releases)


## Build from source on macOS

The following workflow was verified on Apple Silicon using Apple Clang and
Homebrew GNU Autotools. See [the installation guide](doc/INSTALL.md) for
other platforms and configuration options.

### Prerequisites

Install Apple's Command Line Tools if they are not already available:

```sh
xcode-select --install
```

With [Homebrew](https://brew.sh/) installed, install the build tools:

```sh
brew install autoconf automake libtool
```

### Configure and compile

Use a full Git clone so the build can derive version information from
history. If you have a shallow checkout, run this from the repository root
before building:

```sh
git fetch --unshallow
```

The current version calculation requires base revision
`943adaeda55c2d534c722fe66c6b4613a782caa1`. If it is missing, `make` fails
while generating `drivedb.h`. For a test-only build without the required
history, `export SMARTMONTOOLS_TEST_BUILD=1` bypasses that check, but the
generated drive database version is marked unknown.

From the root of the cloned smartmontools repository:

```sh
AUTOMAKE=automake ACLOCAL=aclocal ./autogen.sh
mkdir -p build
cd build
../configure CC=clang CXX=clang++
make -j"$(sysctl -n hw.ncpu)"
```

The explicit `AUTOMAKE` and `ACLOCAL` settings select Homebrew's unversioned
commands. This is needed with Automake 1.20 because `autogen.sh` currently
searches automatically for versioned commands only through 1.19.

Build products remain in `build/`, separate from the source files. Autotools
also generates configuration scripts and support files in the source tree.
No `sudo` or system-wide installation is needed to build or run the checks.

### Verify the build

From `build/`:

```sh
make check
./src/smartctl -V
./src/smartd -V
```

`make check` verifies that `smartctl` runs and validates the drive database.
The version commands do not access disks or start the monitoring daemon.
The executables are `build/src/smartctl` and `build/src/smartd`, relative to
the repository root.

The verified build reported version `pre-8.0-612` at Git revision
`90c012b1eb2e`. It completed with non-fatal warnings about the deprecated
macOS `kIOMasterPortDefault` API, linker flags, and duplicate recipes for
`com.smartmontools.smartd.plist`.

### Clean up and rebuild

From the repository root:

```sh
make -C build clean
```

This removes compiled executables, libraries, object files, and other build
products while preserving configuration and generated Makefiles. Rebuild
and verify with:

```sh
make -C build -j"$(sysctl -n hw.ncpu)"
make -C build check
```

For a more thorough reset, `make -C build distclean` also removes the build
configuration; run `../configure CC=clang CXX=clang++` from `build/` before
building again. Neither cleanup command removes the source checkout or
the Autotools support files generated in the source tree.

## Code Signing
This program uses free code signing provided by [SignPath.io](https://signpath.io) and a free code signing certificate by the [SignPath Foundation](https://signpath.org)

## License
Smartmontools uses [GNU GPL Version 2](https://www.gnu.org/licenses/gpl-2.0.html#SEC1) license. 
