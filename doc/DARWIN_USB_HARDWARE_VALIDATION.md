# Darwin raw USB SMART validation

The default wire protocol is the one macOS was using immediately before capture.
After capture, the backend explicitly reselects the saved alternate before opening
its pipes, even if the interface still reports that alternate. There is no automatic
UASP/BOT fallback: errors are reported and the capture is released. Merely checking
the alternate number does not establish a working transport session.

Before opening the interface, it waits for the captured device's IOKit service
tree to become quiet (matching/termination complete, with a five-second deadline).
BOT sessions also perform standard Reset Recovery before their first command.
A STALL while reading a BOT status wrapper is cleared and that status read is
retried once, as specified by USB BOT section 5.3.3; CBW and data are not replayed.

Append a transport suffix to the existing bridge type to override that selection:

| Selection | Example |
| --- | --- |
| Follow macOS | `-d sntjmicron` |
| UASP only | `-d sntjmicron+usb,uasp` |
| BOT only | `-d sntjmicron+usb,bot` |
| Explicit namespace | `-d sntjmicron,0x1+usb,uasp` |

The suffix also works with `sat`, `sntasmedia`, `sntrealtek`, and `scsi`.
An unavailable protocol or invalid option fails; neither silently selects another
protocol. USB selection is independent of NVMe/SATA bridge command selection.
Known SATA adapters that reuse `152d:0583` remain distinguished in the drive database.
Unknown firmware may still require explicit `-d sntjmicron` to identify the bridge
safely; the JMS583 transport workarounds themselves are not firmware restricted.

## Build and single read

Build from the repository root, then run the checks separately:

```sh
./autogen.sh
./configure
make -j8
make check
./src/smartctl --scan -d usb
```

A passive scan does not capture disks. Confirm the intended external whole disk,
then run from a logged-in Terminal with administrator privileges:

```sh
sudo ./src/smartctl -a --json=o -r ioctl,1 -d sntjmicron /dev/diskN
sudo ./src/smartctl -a --json=o -r ioctl,1 -d sntjmicron+usb,uasp /dev/diskN
sudo ./src/smartctl -a --json=o -r ioctl,1 -d sntjmicron+usb,bot /dev/diskN
```

Replace `diskN` with the current verified device. Use `-j` instead of `--json=o`
when diagnostic text is unnecessary. The latter retains protocol selection and
command diagnostics in `smartctl.output` inside JSON.

ASM2362 (`174c:2362`) is already in the existing drive database. Its simplest
automatic command is `sudo ./src/smartctl -a /dev/diskN`. For an explicit override,
use `-d sntasmedia+usb,uasp` or `-d sntasmedia+usb,bot`. No database edit is required.
Known USB NVMe bridges use the existing database's SNT type during autodetection,
even if a native NVMe SMART capability is advertised. On the tested ASM2362,
that native API returned Identify but rejected SMART GetLogPage. Native ATA
drivers, other NVMe devices, and explicit `-d nvme` retain native access.
`usbraw:diskN` is still available to explicitly select raw USB with automatic
bridge detection. Default scans continue to exclude raw USB capture.

A root background service can have a different audit/session context and fail to
open the USB interface even after successful capture. This was reproduced on
macOS 26.6.2. Our administrator-launched hardware harness uses
`launchctl asuser <logged-in uid>` while retaining root credentials. It does not
change SIP, privacy settings, driver installation, or firmware.

The command temporarily detaches the device. The implementation unmounts existing
volumes without force and attempts to restore their UUIDs on the same USB device
after release. A registry ID is valid only for that connection: each capture's
release may create a new ID. Never reuse a previous run's ID or assume that a BSD
disk number still names the same device.

## Repeated acceptance with saved evidence

The opt-in script requires an unmounted external test disk, a unique bridge serial,
and the expected NVMe model, drive serial, and capacity. It is not invoked by CI
or `make check` and issues only `smartctl -a` reads (including the bridge wakeup).
It neither formats media nor starts a self-test.

```sh
sudo python3 lib/tests/darwin_usb_hardware.py \
  --smartctl ./src/smartctl \
  --output /tmp/darwin-usb-validation-new \
  --vendor 0x152d --product 0x0583 \
  --usb-serial '<USB_SERIAL>' \
  --model MTFDHBL256TDQ --drive-serial '<NVME_SERIAL>' \
  --capacity 256060514304 --rounds 5
```

Each round tests system selection, explicit UASP, then explicit BOT in separate
processes. Before each capture it resolves the unique USB bridge and verifies
external whole-disk status and capacity. After each read it requires the same
bridge at the same port, a new registry ID, and a usable macOS disk node. It
checks the actual protocol diagnostic and Identify/SMART JSON. Evidence includes
before/after plists, command lines, binary hash, raw JSON, stderr, and timings.
It also requires the requested error-log entries and, when supported, the self-test
log. Add `--autodetect` to run system selection as plain `/dev/diskN` without a
device type. For ASM2362 use `--autodetect --type sntasmedia --vendor 0x174c --product 0x2362` and the
attached enclosure's identity instead of the JMicron values above.

The release loop polls readiness with bounded backoff and a 30-second deadline;
it never treats elapsed time as success. There is no sleep between successful
readiness detection and the next operation. `smartctl` itself uses synchronous
START UNIT for JMS583, followed by the existing SNT Identify and SMART sequence,
with command completion timeouts rather than a fixed startup delay.

## Validated hardware and limits

On 2026-09-12, JEYI External (`152d:0583`, firmware `0x0209`, USB 10 Gb/s) with a
Micron `MTFDHBL256TDQ` / `MU05.1` NVMe drive completed reads through both BOT and
UASP. The user identified the device as a JZ215 Micron iSSD with SM2263XT and
reported normal CrystalDiskInfo operation on Windows. A Windows output dump was
not available for byte-for-byte comparison.

The relevant fixes are UASP pipe-usage descriptor traversal, BOT residue semantics,
and synchronous START UNIT to wake the JMS583 NVMe command path. TEST UNIT READY
can return GOOD while NVMe Identify still fails with sense `04/44/83`. BOT full
residue correction applies only to successful, complete JMicron DMA-IN transfers;
short transfers and errors remain errors/residual data. This family workaround
is enabled independently of firmware version; actual hardware validation so far
covers `0x0209` only.

On the same date, Ugreen Storage Device (`174c:2362`, bcdDevice `0x0100`, ASM2362)
with the same Micron model/firmware completed five rounds of system UASP, explicit
UASP, and explicit BOT: 15 Identify, SMART, error-log, self-test-log reads and
device releases. The ASM2362 session required alternate reselection even when
GET_INTERFACE and the descriptor both already reported alternate 1. BOT also
required initial Reset Recovery to avoid stale command/status data after capture.

The ASMedia SNT layer now checks actual residual lengths. On a successful short
Get Log Page response, it advances the existing byte offset and requests the
remaining data. It rejects incomplete Identify, zero progress, unaligned progress,
invalid residue, or offset overflow. This handling is independent of the host OS,
USB protocol, bridge firmware, and drive-database entries. The tested ASM2362
returned only 512 bytes for the initial 1024-byte error-log request on both BOT
and UASP; the remaining 512 bytes are now fetched instead of silently zero filled.

The disk had no mounted filesystem. Device re-enumeration was physically tested;
the identity matching used for mount restoration has unit coverage, but actual
unmount/remount behavior was not exercised by these hardware runs. USB 2 UASP
without streams, other bridge families, and other firmware versions need their
own hardware acceptance. Write-capable commands,
firmware updates, and arbitrary NVMe administration remain outside this transport's
read-oriented command policy.
