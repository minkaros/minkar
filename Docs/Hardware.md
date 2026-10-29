# Reference hardware

Minkar is tested on a short list of machines: one QEMU virtual machine and
two documented physical PCs. Bugs on these are fixed first; other hardware
is best effort. This page records what works on each machine, what does
not yet, and how each fix was found. The [roadmap](ROADMAP.md) (Phase 1)
tracks the list as a whole.

| Machine | Role | State |
|---|---|---|
| QEMU VM | Every change: `boottest`, the local test suite, the CI | Boots to login under TCG and KVM |
| PC 1: Gigabyte H270N-WIFI | First physical PC | Boots to login from SATA; display, keyboard and network work |
| PC 2 | Second physical PC | Not chosen yet |

## QEMU virtual machine

`q35` machine, OVMF firmware, AHCI disk, `qemu-xhci` with a USB keyboard and
tablet, e1000e or e1000 network card; `-cpu Westmere` (with `-cpu host`
QEMU fails on an MSR the kernel reads).

- **TCG and KVM** (2026-10-27): the kernel read two MSRs KVM does not have
  (core/thread count, flex ratio) before it could handle a fault; under a
  hypervisor it now uses CPUID and the PIT. The test suite's three VMs run
  under KVM (the xHCI driver now sleeps until its interrupt instead of
  polling every millisecond): the full suite takes about 9 minutes
  instead of 32. *Partial:* on the CI runners the kernel does not boot
  under KVM yet, so `boottest` uses TCG unless KVM is asked for.
- **Console test** (`run.sh console`): boots an image with a PC's boot
  arguments and no serial console, types on the USB keyboard through QMP
  and reads the screen back through the console's font.
- **Known bug:** without `pci_keep_console=1` the boot stops when PCI
  configuration turns the console back on. The PC images and the console
  test pass that argument.

## PC 1: Gigabyte H270N-WIFI

| Part | Device | State |
|---|---|---|
| Board | Gigabyte H270N-WIFI (Intel H270), AMI UEFI firmware | Boots |
| CPU | Intel Core i7-7700 (4 cores, 8 threads) | ACPI lists all eight; only the boot CPU runs (no SMP yet, as in QEMU) |
| Disk | SATA through AHCI (`MinkarAHCIPort`) | Root file system, fsck |
| Disk | NVMe (M.2) | No driver yet |
| USB | xHCI (`MinkarXHCIPort`), USB 2 hub of a KVM switch | Keyboard and mouse, directly and through the switch |
| Display | GOP framebuffer, Xiaomi 34" ultrawide | Native 3440x1440 text console |
| Network | Intel I219-V (PCH LAN) | ping on the LAN (`MinkarE1000`) |
| Network | Intel I211 | DHCP and ping on the LAN (`MinkarE1000`) |
| Wi-Fi | Intel (M.2) | No driver |

Images for it are built with `GRAPHICS_MODE=native` and the boot arguments
`-v debug=0x14e keepsyms=1 kprintf_console=1 pci_keep_console=1
ahci_debug=1`, and written to a SATA disk. `LOADER_PAUSE=10` keeps the
loader's output on screen for photos.

### Still open

- NVMe.
- The ACPI interpreter (ACPICA) does not finish starting; the kernel now
  says every 10 seconds which step and AML method it is in.
- IOKit does not get quiet within 30 seconds, so some drivers (the
  display's) start after the login prompt; `kextstart` now names the
  services still busy.
- Only the boot CPU runs (the other CPUs are found but not started; also
  in QEMU).
- Booting without `pci_keep_console=1` (see the QEMU known bug).

### How it got there

**Loader** (2026-10-26). From the SATA disk, the loader read the kernel
from another disk's EFI partition, then overwrote firmware memory: the
kernel was copied to 1 MB while boot services ran (QEMU's firmware happens
to leave that range alone). Then the kernel reset the machine on its first
no-execute page: the AMI firmware leaves `EFER.NXE` off, OVMF turns it on.
With the loader fixed, the kernel boots through `bsd_init`, the APIC, the
eight CPUs and the root UUID, and stopped in PCI configuration, where the
console is off.

**PCI** (2026-10-26). With `kprintf_console=1 pci_keep_console=1` to watch
it: PCI configuration froze rewriting the x16 bridge's memory window;
IOPCIFamily now keeps the firmware's windows at boot when they hold what
was allocated. AHCI and xHCI then waited forever for their interrupt: the
I/O APIC was named after its APIC ID (2) while every device asks for
`io-apic@0`; it is now named by its place in the MADT. An intermittent
panic in QEMU (2026-10-27) came from IOPCIFamily reading MSI-X's
pending-bit array for a plain MSI device while a driver disabled its last
vector; plain MSI now calls its first vector instead.

**SATA** (2026-10-27). The disks were found but IDENTIFY never ran: the
port was reset while its command engine ran and `PxSERR` was not cleared
after, so Intel's HBA kept BSY set; the port now follows the AHCI
specification's order. **PC 1 boots to the `login:` prompt**: root on the
SATA disk, launchd, fsck, sshd and a shell. The AHCI and xHCI drivers are
named `MinkarAHCIPort` and `MinkarXHCIPort` (`org.minkar.driver.*`),
keeping their authors' copyright.

**USB** (2026-10-27). Each interrupt-IN endpoint has its own ring and
every one of a device is configured (the keyboard's media keys, the
mouse's extra interfaces); keyboards in report protocol and interfaces
with a keyboard and a mouse are driven (`MinkarUSBHIDInterface`); a device
that cannot be read no longer floods the console. The keyboard behind the
KVM switch (a USB 2 hub): the driver cleared a SuperSpeed-only port
feature, the hub answered STALL and its control endpoint stayed halted; it
now sends that only to SuperSpeed hubs and resets a halted control
endpoint after any STALL. A device with one bulk direction no longer
panics the kernel.

**Display** (2026-10-27 and 28). The loader sets the display mode from
`graphics-mode` in the boot plist (`GRAPHICS_MODE=`); `native` reads the
display's EDID, and the 34" ultrawide runs at 3440x1440 (3456 pixels per
line). The text was scrambled because the loader cleared the screen
through the firmware's console after reading the mode, and the firmware
put its own mode back; the loader now clears the framebuffer itself and
reads the mode last.

**Network** (2026-10-28). The I219-V answers ping on the LAN since
MinkarE1000 runs Intel's shared e1000 code (FreeBSD's, BSD-3) for the PCH
LAN and the I210/I211.

**Console terminal** (2026-10-28). Backspace erased the whole line and the
history drew commands in the wrong place. The console now does what
xterm's terminfo asks (CHA, insert and delete characters and lines,
scrolling, the alternate screen, 256 colors as its 8) and decodes UTF-8;
line editing and the history redraw in place, and btop draws its boxes in
ASCII (`km.c` had cut every byte to 7 bits).

**After the login** (2026-10-28, hw-test28). The display's driver started
after the login prompt and set the console up again: the cursor went back
to the top and the prompt was drawn over the boot messages. The console now
keeps its text and cursor when a driver hands it the mode it already has,
and once the login is up the kernel's messages stay off the screen
(`kern.console_quiet`, set by `kextstart`; they still reach `dmesg` and the
log, and a panic or a shutdown shows them again). The console's getty
clears the screen for every login but the first.

**Two network cards** (2026-10-28). Moving the cable from the I219-V to the
I211 gave the I211 an address, but ping went out through the I219-V, which
kept its address and routes with no link. As on macOS, `dhcpclient` now
gives up an interface's addresses (IPv4 and IPv6) and routes when its link
goes down, and asks for it again when the link comes back.

**Powering off** (2026-10-28, hw-test30). `shutdown -h` stopped with the
machine on: the ACPI interpreter had never finished starting. With no
interpreter the kernel now takes SLP_TYP from the DSDT's `_S5_` and writes
SLP_EN itself, and PC 1 turns off. `arp -a` answers at once there; it was
slow only while the network was misconfigured.

**Full-screen programs** (2026-10-28). The console draws box lines, block
elements and Braille dots, so btop shows its boxes and graphs as on a
terminal emulator instead of `+ - | #`.
