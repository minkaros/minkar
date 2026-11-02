#!/bin/bash
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# minkar-hwcheck: unattended hardware validation of Minkar on a physical
# machine (gate M1 and the hardware side of M2), run from Minkar's CLI.
#
#   sudo minkar-hwcheck            all checks, then a reboot test; the run
#                                  ends by itself after the reboot
#   sudo minkar-hwcheck --no-reboot   all checks, no reboot test
#   sudo minkar-hwcheck --display  the inventory and a display check only
#                                  (no load): the GPUs' PCI windows, where
#                                  the framebuffer is, whether PCI
#                                  configuration moved it; then a heartbeat
#                                  every 10 s on disk, so the next run says
#                                  how long this boot lasted (for a machine
#                                  that hangs). Boot with pci=0x1 to log
#                                  every BAR the configurator assigns.
#
# Results: /var/log/minkar-hwcheck/<stamp>/report.txt (summary first) and
# /var/log/minkar-hwcheck/minkar-hwcheck-<stamp>.tar.gz (report + raw
# dumps). Both are copied to every writable FAT volume under /Volumes (a
# USB stick), so they can be read on any other computer. Serial numbers
# are masked.
#
# Checks: inventory (CPU, memory, PCI with the driver of each device,
# USB, disks, display, firmware), SMP (every logical CPU online and
# running work in parallel), CPU load on every thread, memory, storage
# (write, read back, compare), network (link, DHCP, route, DNS, ping, TCP
# download, IPv6), services (launchd as PID 1, sshd, syslogd), power
# (ACPI, pmset), the kernel log, and a reboot (time to come back up).
# The power button and "shutdown -h" need a person: the report lists them.

set -u
export LC_ALL=C PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin

ROOT=/var/log/minkar-hwcheck
DAEMON=/Library/LaunchDaemons/org.minkar.hwcheck.after-reboot.plist
SELF=/usr/local/bin/minkar-hwcheck
MODE=full
case "${1:-}" in
--no-reboot) MODE=noreboot ;;
--display) MODE=display ;;
--after-reboot) MODE=after ;;
"" ) ;;
-h|--help) sed -n '29,56p' "$0"; exit 0 ;;
*) echo "usage: minkar-hwcheck [--no-reboot | --display]" >&2; exit 2 ;;
esac

if [ "$(id -u)" != 0 ]; then
	echo "minkar-hwcheck: run it as root (sudo minkar-hwcheck)" >&2
	exit 1
fi

if [ "$MODE" = after ]; then
	DIR=$(cat "$ROOT/current" 2>/dev/null)
	[ -n "$DIR" ] && [ -d "$DIR" ] || exit 1
else
	STAMP=$(date +%Y%m%d-%H%M%S)
	DIR=$ROOT/$STAMP
	mkdir -p "$DIR/raw"
	echo "$DIR" > "$ROOT/current"
fi
REPORT=$DIR/report.txt
DETAIL=$DIR/detail.txt
RESULTS=$DIR/results.txt
RAW=$DIR/raw
touch "$RESULTS" "$DETAIL"

# ---- helpers -------------------------------------------------------------

now() { echo "$EPOCHREALTIME"; }
elapsed() { echo "$1 $2" | awk '{ printf "%.2f", $2 - $1 }'; }
say() { echo "== $*"; echo "== $*" >> "$DETAIL"; }

# result PASS|FAIL|WARN|INFO name detail
result() {
	printf '%-4s  %-34s %s\n' "$1" "$2" "$3" >> "$RESULTS"
	printf '   %-4s %-34s %s\n' "$1" "$2" "$3"
}

# section title: commands run below go to detail.txt
section() { printf '\n## %s\n' "$1" >> "$DETAIL"; say "$1"; }

# run label command...: output to detail.txt, with a time limit
run() {
	local label=$1; shift
	printf '\n$ %s\n' "$*" >> "$DETAIL"
	timeout 120 "$@" >> "$DETAIL" 2>&1
	local rc=$?
	[ $rc -ne 0 ] && printf '(exit %d)\n' $rc >> "$DETAIL"
	return $rc
}

# raw name command...: full output to raw/name
raw() {
	local name=$1; shift
	timeout 300 "$@" > "$RAW/$name" 2>&1
}

mask_serials() {
	# Serial numbers, platform UUIDs: not needed to diagnose the hardware.
	sed -E -e 's/("(IOPlatformSerialNumber|serial-number|IOPlatformUUID|SerialNumber|USB Serial Number|kUSBSerialNumberString)" = )[<"][^>"]*[>"]/\1"(masked)"/g' \
	       -e 's/((Serial Number|Hardware UUID|Provisioning UDID)[^:]*: ).*/\1(masked)/' "$@"
}

sysctl_n() { sysctl -n "$1" 2>/dev/null; }

# awk does not read "0x..." strings as numbers: hex() does, for awk
# programs (prepend "$AWK_HEX").
AWK_HEX='function hex(h,   i, n) { h = tolower(h); sub(/^0x/, "", h); n = 0
	for (i = 1; i <= length(h); i++) n = n * 16 + index("0123456789abcdef", substr(h, i, 1)) - 1
	return n }
'

# ---- after the reboot ----------------------------------------------------

finish() {
	section "Package"
	for f in "$RAW"/*; do
		[ -f "$f" ] || continue
		mask_serials "$f" > "$f.m" && mv "$f.m" "$f"
	done
	mask_serials "$DETAIL" > "$DETAIL.m" && mv "$DETAIL.m" "$DETAIL"

	local pass fail warn
	pass=$(grep -c '^PASS' "$RESULTS")
	fail=$(grep -c '^FAIL' "$RESULTS")
	warn=$(grep -c '^WARN' "$RESULTS")
	{
		echo "Minkar hardware check"
		echo "date:    $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
		echo "system:  $(sw_vers 2>/dev/null | tr '\n' ' ')"
		echo "kernel:  $(uname -v)"
		echo "model:   $(sysctl_n hw.model)"
		echo "cpu:     $(sysctl_n machdep.cpu.brand_string)"
		echo "result:  PASS $pass  FAIL $fail  WARN $warn"
		echo
		grep -E '^(FAIL|WARN)' "$RESULTS"
		echo
		echo "All results:"
		cat "$RESULTS"
		echo
		echo "Still to check by hand (write the result next to each line):"
		echo "  [ ] Power button pressed once: the machine shuts down in order and powers off."
		echo "  [ ] 'sudo shutdown -h now': it powers off (fans stop)."
		echo "  [ ] Login on the console screen with the keyboard (minkar / root)."
		echo "  [ ] Picture on the screen at the end of boot (no garbled text)."
		echo
		echo "Details:"
		cat "$DETAIL"
	} > "$REPORT"

	local tarball=$ROOT/minkar-hwcheck-$(basename "$DIR").tar.gz
	tar -czf "$tarball" -C "$ROOT" "$(basename "$DIR")" 2>/dev/null

	# A copy on every writable FAT volume (a USB stick).
	local copied=""
	for v in /Volumes/*; do
		[ -d "$v" ] || continue
		mount | grep -q " on $v (msdos" || continue
		if cp "$REPORT" "$v/minkar-hwcheck-$(basename "$DIR").txt" 2>/dev/null &&
		   cp "$tarball" "$v/" 2>/dev/null; then
			sync
			copied="$copied $v"
		fi
	done

	rm -f "$ROOT/current"
	echo
	echo "Result:  PASS $pass  FAIL $fail  WARN $warn"
	echo "Report:  $REPORT"
	echo "Archive: $tarball"
	if [ -n "$copied" ]; then
		echo "Copied to:$copied"
	else
		echo "No FAT USB stick was mounted under /Volumes: plug one in and run"
		echo "  sudo cp $REPORT $tarball /Volumes/<pendrive>/"
	fi
}

if [ "$MODE" = after ]; then
	# Started by launchd at boot: wait for the network and services.
	sleep 45
	# Not unloaded (launchd would stop this very job): without its file
	# it does not run at the next boot.
	rm -f "$DAEMON"
	section "Reboot"
	before=$(cat "$DIR/reboot-requested" 2>/dev/null)
	boot=$(sysctl_n kern.boottime | sed -E 's/^[{] sec = ([0-9]+),.*/\1/')
	if [ -n "$before" ] && [ -n "$boot" ] && [ "$boot" -gt "${before%.*}" ]; then
		down=$((boot - ${before%.*}))
		result PASS "reboot" "the machine came back up (kernel started ${down}s after the request)"
	elif [ -n "$before" ] && [ -n "$boot" ]; then
		result FAIL "reboot" "this boot started before the reboot was requested: it did not reboot"
	else
		result FAIL "reboot" "no time of the request or boot"
	fi
	echo "uptime at check: $(uptime)" >> "$DETAIL"
	raw dmesg-after-reboot.txt dmesg
	if ps -p 1 -o comm= | grep -q launchd; then
		result PASS "launchd after reboot" "PID 1 is launchd"
	else
		result FAIL "launchd after reboot" "PID 1: $(ps -p 1 -o comm=)"
	fi
	if ipconfig getifaddr en0 >/dev/null 2>&1 || ipconfig getifaddr en1 >/dev/null 2>&1; then
		result PASS "network after reboot" "an address by DHCP again"
	else
		result WARN "network after reboot" "no IPv4 address on en0/en1"
	fi
	finish > /dev/console 2>&1
	exit 0
fi

echo "minkar-hwcheck: results go to $DIR"
if [ "$MODE" = display ]; then
	echo "Display check: about a minute, no load and no reboot."
else
	echo "Nothing to do meanwhile; it takes about 10 minutes and reboots at the end (unless --no-reboot)."
fi

# ---- inventory -----------------------------------------------------------

section "Inventory"
run system sw_vers
run kernel uname -a
run sysctl-hw sysctl hw
run cpu sysctl machdep.cpu
run bootargs sysctl kern.bootargs
run nvram nvram -p
raw sysctl.txt sysctl -a
raw ioreg-pci.txt ioreg -l -w0 -r -c IOPCIDevice
raw ioreg-pci-tree.txt ioreg -w0 -r -c IOPCIDevice
raw ioreg-usb.txt ioreg -l -w0 -p IOUSB
raw ioreg-acpi.txt ioreg -l -w0 -p IOACPIPlane
raw ioreg-service.txt ioreg -l -w0
raw ioreg-devicetree.txt ioreg -l -w0 -p IODeviceTree
raw kextstat.txt kextstat
raw system_profiler.txt system_profiler -detailLevel mini
raw dmesg.txt dmesg
raw mount.txt mount
raw ifconfig.txt ifconfig -a
raw diskutil.txt diskutil list
raw launchctl.txt launchctl list
raw ps.txt ps axww -o pid,ppid,user,%cpu,rss,command

model=$(sysctl_n hw.model)
result INFO "model" "${model:-unknown}"
result INFO "cpu" "$(sysctl_n machdep.cpu.brand_string)"
# The EFI node: firmware-vendor is UTF-16, firmware-revision a 32-bit number.
fw=$(ioreg -l -w0 -p IODeviceTree -n efi 2>/dev/null | awk "$AWK_HEX"'
	/"firmware-vendor" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v)
		for (i = 1; i + 1 <= length(v); i += 4) { c = substr(v, i, 2); if (c == "00") break
			vendor = vendor sprintf("%c", hex(c)) } }
	/"firmware-revision" = </ { r = $0; sub(/.*</, "", r); sub(/>.*/, "", r)
		rev = substr(r,7,2) substr(r,5,2) substr(r,3,2) substr(r,1,2) }
	END { printf "%s revision 0x%s", vendor, rev }')
result INFO "firmware" "$fw"

# ---- display (--display) ---------------------------------------------------

# Each display-class PCI device (class 03xxxx) and its assigned windows,
# decoded from "assigned-addresses": 5 little-endian words per window
# (phys.hi with the BAR in its low byte, phys.mid, phys.lo, size.hi,
# size.lo). One line per window: "vendor:device bar start size".
gpu_windows() {
	awk "$AWK_HEX"'
	function le(h, o) { return hex(substr(h,o+7,2) substr(h,o+5,2) substr(h,o+3,2) substr(h,o+1,2)) }
	function hx(n,   s, d) { s = ""; do { d = n % 16; s = substr("0123456789abcdef", d + 1, 1) s; n = (n - d) / 16 } while (n > 0); return "0x" s }
	function flush() {
		if (inpci && class ~ /^03/ && aa != "")
			for (o = 0; o + 40 <= length(aa); o += 40) {
				hi = le(aa, o); bar = hx(hi % 256)
				start = le(aa, o + 8) * 4294967296 + le(aa, o + 16)
				size = le(aa, o + 24) * 4294967296 + le(aa, o + 32)
				printf "%s:%s %s %s %s\n", vendor, device, bar, hx(start), hx(size)
			}
		aa = ""; class = ""
	}
	/\+-o / { flush(); inpci = ($0 ~ /<class IOPCIDevice/) }
	inpci && /"vendor-id" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); vendor = substr(v,3,2) substr(v,1,2) }
	inpci && /"device-id" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); device = substr(v,3,2) substr(v,1,2) }
	inpci && /"class-code" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); class = substr(v,5,2) substr(v,3,2) substr(v,1,2) }
	inpci && /"assigned-addresses" = </ { aa = $0; sub(/.*</, "", aa); sub(/>.*/, "", aa) }
	END { flush() }
	' "$RAW/ioreg-pci.txt"
}

display_check() {
	section "Display"
	local hb=$ROOT/heartbeat boot last
	boot=$(sysctl_n kern.boottime | sed -E 's/^[{] sec = ([0-9]+),.*/\1/')
	if [ -f "$hb" ]; then
		read -r hbboot hblast < "$hb"
		# A heartbeat from an older minkar-hwcheck has 0 for the boot.
		if [ "${hbboot:-0}" != 0 ] && [ "$hbboot" != "$boot" ]; then
			result INFO "previous boot" "lasted at least $((hblast - hbboot)) s (last heartbeat $(date -u -r "$hblast" '+%H:%M:%S UTC'))"
		fi
	fi
	run bootargs sysctl kern.bootargs
	grep -iE 'IOFB:|console relocated|lost console|console setting|console [0-9]+ x' "$RAW/dmesg.txt" >> "$DETAIL"
	# IOGOPFramebuffer publishes the geometry the firmware gave; in a VM,
	# MinkarVirtioGPU publishes its own (its scanout is in system memory).
	local fb fbw virtio=no
	ioreg -l -w0 -r -c IOGOPFramebuffer > "$RAW/ioreg-gop.txt" 2>&1
	if ! grep -q '<class IOGOPFramebuffer' "$RAW/ioreg-gop.txt" &&
	    ioreg -l -w0 -r -c MinkarVirtioGPU > "$RAW/ioreg-virtio-gpu.txt" 2>&1 &&
	    grep -q '<class MinkarVirtioGPU' "$RAW/ioreg-virtio-gpu.txt"; then
		virtio=yes
		cp "$RAW/ioreg-virtio-gpu.txt" "$RAW/ioreg-gop.txt"
	fi
	# ioreg prints numbers in decimal (or as 0x... when asked): as hex here.
	fb=$(awk -F'= ' "$AWK_HEX"'
	    function hx(n,   s, d) { s = ""; do { d = n % 16; s = substr("0123456789abcdef", d + 1, 1) s; n = (n - d) / 16 } while (n > 0); return "0x" s }
	    /"MinkarFramebufferAddress"/ { v = $2; print (v ~ /^0x/) ? v : hx(v + 0); exit }' "$RAW/ioreg-gop.txt")
	fbw=$(awk -F'= ' '/"MinkarFramebufferWidth"/ { w = $2 } /"MinkarFramebufferHeight"/ { h = $2 }
	    /"MinkarFramebufferRowBytes"/ { r = $2 } END { if (w != "") printf "%sx%s, %s bytes per row", w, h, r }' "$RAW/ioreg-gop.txt")
	result INFO "framebuffer" "${fbw:-?} at ${fb:-unknown}"
	if grep -q 'lost console' "$RAW/dmesg.txt"; then
		result INFO "pci: console" "left where the firmware put it (PCI configuration could not place its window)"
	elif grep -q 'console relocated' "$RAW/dmesg.txt"; then
		result WARN "pci: console" "PCI configuration moved the framebuffer: $(grep -oE 'console relocated to 0x[0-9a-f]+' "$RAW/dmesg.txt" | tail -1)"
	else
		result INFO "pci: console" "not moved by PCI configuration (or not logged: boot with pci=0x1)"
	fi
	gpu_windows > "$RAW/gpu-windows.txt"
	printf '\nGPU windows (vendor:device bar start size):\n' >> "$DETAIL"
	cat "$RAW/gpu-windows.txt" >> "$DETAIL"
	local n inside=0
	n=$(wc -l < "$RAW/gpu-windows.txt" | tr -d " ")
	result INFO "gpu windows" "$n assigned (raw/gpu-windows.txt)"
	# The window IOGOPFramebuffer matched on (it drives only the card
	# whose window holds the framebuffer); else the assigned windows.
	local wbase wsize
	wbase=$(awk -F'= ' '/"MinkarFramebufferWindowBase"/ { print $2; exit }' "$RAW/ioreg-gop.txt")
	wsize=$(awk -F'= ' '/"MinkarFramebufferWindowSize"/ { print $2; exit }' "$RAW/ioreg-gop.txt")
	if [ $virtio = yes ]; then
		result INFO "framebuffer drivers" "MinkarVirtioGPU (virtio-gpu, no IOGOPFramebuffer)"
		[ -n "$fb" ] && result PASS "framebuffer in system memory" \
		    "virtio-gpu: the host reads the scanout from RAM, no GPU window"
		fb=""
	else
		result INFO "framebuffer drivers" "$(grep -c '<class IOGOPFramebuffer' "$RAW/ioreg-gop.txt") IOGOPFramebuffer (one expected)"
	fi
	if [ -n "$wbase" ] && [ -n "$fb" ]; then
		result PASS "framebuffer in a gpu window" "$(awk -v b="$wbase" -v z="$wsize" '
		    function hx(n,   s, d) { s = ""; do { d = n % 16; s = substr("0123456789abcdef", d + 1, 1) s; n = (n - d) / 16 } while (n > 0); return "0x" s }
		    BEGIN { printf "window of %d MB at %s", z / 1048576, hx(b + 0) }')"
		fb=""
	fi
	if [ -n "$fb" ]; then
		inside=$(awk -v fb="$fb" "$AWK_HEX"'BEGIN { f = hex(fb) } { s = hex($3); z = hex($4); if (f >= s && f < s + z) { print $1 " " $2; exit } }' "$RAW/gpu-windows.txt")
		if [ -n "$inside" ]; then
			result PASS "framebuffer in a gpu window" "$inside"
		else
			result FAIL "framebuffer in a gpu window" "$fb is in no window a GPU decodes now"
		fi
	fi
	local low
	low=$(awk "$AWK_HEX"'{ if (hex($3) < 4294967296 && hex($4) >= 16777216) print $1 " " $2 " " $3 " " $4 }' "$RAW/gpu-windows.txt")
	result INFO "gpu windows below 4 GB" "${low:-none}"
	# MinkarAMDGPU (ADR 0002, G1): what it read of each AMD GPU, without
	# driving it: the VBIOS (and where from), and the VRAM size register.
	ioreg -l -w0 -r -c MinkarAMDGPU > "$RAW/ioreg-amdgpu.txt" 2>&1
	local amd prop
	amd=$(grep -c '<class MinkarAMDGPU' "$RAW/ioreg-amdgpu.txt")
	result INFO "amd gpus" "$amd (MinkarAMDGPU)"
	if [ "$amd" -gt 0 ]; then
		for prop in MinkarGPUFamily MinkarVBIOSSource MinkarVBIOSPartNumber \
		    MinkarVBIOSVersion MinkarVRAMMegabytes MinkarRegisterWindowSize; do
			result INFO "amd: ${prop#Minkar}" "$(awk -F'= ' -v k="\"$prop\"" \
			    '$1 ~ k { gsub(/"/, "", $2); v = v (v ? ", " : "") $2 } END { print (v ? v : "-") }' \
			    "$RAW/ioreg-amdgpu.txt")"
		done
		if grep -q '"MinkarVBIOSSource" = "none"' "$RAW/ioreg-amdgpu.txt"; then
			result WARN "amd: vbios" "no VBIOS from the firmware or the ROM window"
		elif grep -q '"MinkarVBIOSPartNumber"' "$RAW/ioreg-amdgpu.txt"; then
			result PASS "amd: vbios" "read and parsed by the AtomBIOS interpreter"
		else
			result FAIL "amd: vbios" "read, but the AtomBIOS interpreter refused it"
		fi
	fi
	# With pci=0x1, raw/dmesg.txt has every BAR the configurator wrote and
	# what it read back.
	grep -q 'PCI configuration begin' "$RAW/dmesg.txt" &&
	    result INFO "pci configuration log" "$(grep -c '  \[0x' "$RAW/dmesg.txt") BAR writes logged (raw/dmesg.txt)"
	# The heartbeat: this boot's start and the last time it was alive.
	nohup sh -c "while :; do echo \"$boot \$(date -u +%s)\" > $hb.new && mv $hb.new $hb && sync; sleep 10; done" \
	    > /dev/null 2>&1 < /dev/null &
	result INFO "heartbeat" "every 10 s in $hb until shutdown"
}

if [ "$MODE" = display ]; then
	display_check
	finish
	exit 0
fi

# Each PCI device, its IDs and the driver that matched it.
{
	echo
	echo "PCI devices (vendor:device class -> driver):"
	awk '
	/\+-o / {
		if (name != "") print_dev()
		name = $0; sub(/^[ |]*\+-o /, "", name); sub(/ *<class.*/, "", name)
		vendor = device = class = ""; drv = ""; depth = index($0, "+-o")
		inpci = ($0 ~ /<class IOPCIDevice/)
		next
	}
	inpci && /"vendor-id" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); vendor = substr(v,3,2) substr(v,1,2) }
	inpci && /"device-id" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); device = substr(v,3,2) substr(v,1,2) }
	inpci && /"class-code" = </ { v = $0; sub(/.*</, "", v); sub(/>.*/, "", v); class = substr(v,5,2) substr(v,3,2) substr(v,1,2) }
	function print_dev() { if (vendor != "") printf "  %-28s %s:%s %s\n", name, vendor, device, class }
	END { if (name != "") print_dev() }
	' "$RAW/ioreg-pci.txt"
	echo
	echo "PCI tree (a device with nothing under it has no driver):"
	cat "$RAW/ioreg-pci-tree.txt"
} >> "$DETAIL"

pci_total=$(grep -c '<class IOPCIDevice' "$RAW/ioreg-pci-tree.txt")
# A device without a driver: its next line is not deeper in the tree.
pci_nodriver=$(awk '
	/<class IOPCIDevice/ { if (pending) n++; pending = 1; d = index($0, "+-o"); name = $0; next }
	/\+-o / { if (pending && index($0, "+-o") <= d) n++; pending = 0 }
	END { if (pending) n++; print n + 0 }' "$RAW/ioreg-pci-tree.txt")
result INFO "pci devices" "$pci_total found, $pci_nodriver without a driver (see the PCI list)"

usb_count=$(grep -c '+-o ' "$RAW/ioreg-usb.txt")
result INFO "usb" "$usb_count entries in the USB plane"
fbmem=$(ioreg -l -w0 -r -c IOGOPFramebuffer 2>/dev/null | awk -F'= ' '/"IOFBMemorySize"/ { print $2; exit }')
vgpu=$(ioreg -l -w0 -r -c MinkarVirtioGPU 2>/dev/null | awk -F'= ' '/"MinkarFramebufferWidth"/ { w = $2 } /"MinkarFramebufferHeight"/ { h = $2 } END { if (w != "") print w "x" h }')
if [ -n "$vgpu" ]; then
	result PASS "display: virtio-gpu" "MinkarVirtioGPU at $vgpu (check the picture by hand)"
elif [ -n "$fbmem" ]; then
	result PASS "display: GOP framebuffer" "$((fbmem / 1048576)) MB of framebuffer (check the picture by hand)"
else
	result FAIL "display: GOP framebuffer" "no IOGOPFramebuffer: no picture from the firmware's GOP"
fi

# ---- CPU and SMP -----------------------------------------------------------

section "CPU"
ncpu=$(sysctl_n hw.ncpu)
logical=$(sysctl_n hw.logicalcpu)
physical=$(sysctl_n hw.physicalcpu)
threads=$(sysctl_n machdep.cpu.thread_count)
cores=$(sysctl_n machdep.cpu.core_count)
acpi_cpus=$(grep -cE '"(Processor|CPU)[0-9]*"|<class (IOACPIPlatformDevice|AppleACPICPU)' "$RAW/ioreg-acpi.txt" 2>/dev/null)
echo "ncpu=$ncpu logical=$logical physical=$physical thread_count=$threads core_count=$cores acpi=$acpi_cpus" >> "$DETAIL"
if [ "${ncpu:-0}" -gt 1 ] && [ "${ncpu:-0}" = "${threads:-x}" ]; then
	result PASS "smp: every cpu online" "$ncpu logical CPUs ($physical cores) of $threads in the package"
elif [ "${ncpu:-0}" -gt 1 ]; then
	result WARN "smp: every cpu online" "$ncpu online, the processor says $threads threads"
else
	result FAIL "smp: every cpu online" "hw.ncpu=$ncpu (the processor says $threads threads)"
fi

burn() { dd if=/dev/zero bs=1048576 count="$1" 2>/dev/null | md5 > /dev/null; }
# The same work on one CPU and on every CPU at once, sized to take about
# 4 seconds on one CPU (measured with a short run first).
t0=$(now); burn 100; t1=$(now)
unit=$(echo "$(elapsed "$t0" "$t1")" | awk '{ u = ($1 > 0) ? int(400 / $1) : 4000; if (u < 100) u = 100; if (u > 8000) u = 8000; print u }')
t0=$(now); burn "$unit"; t1=$(now)
one=$(elapsed "$t0" "$t1")
n=${ncpu:-1}
t0=$(now)
for i in $(seq 1 "$n"); do burn $unit & done
wait
t1=$(now)
all=$(elapsed "$t0" "$t1")
speedup=$(echo "$one $all $n" | awk '{ if ($2 > 0) printf "%.1f", $1 * $3 / $2; else print 0 }')
echo "one job: ${one}s; $n jobs at once: ${all}s; speedup $speedup" >> "$DETAIL"
need=$(echo "${physical:-1}" | awk '{ printf "%.1f", $1 * 0.7 }')
if echo "$speedup $need" | awk '{ exit !($1 >= $2) }'; then
	result PASS "smp: work runs in parallel" "speedup ${speedup}x with $n jobs (one job ${one}s)"
else
	result FAIL "smp: work runs in parallel" "speedup ${speedup}x with $n jobs; at least ${need}x expected"
fi

# Every thread busy for 30 seconds: the machine must stay up and answer.
t0=$(now)
for i in $(seq 1 "$n"); do timeout 30 sh -c 'while :; do :; done' & done
sleep 5
alive=$(ps axo command | grep -c "^sh -c while")
t_ps0=$(now); ps ax > /dev/null; t_ps1=$(now)
wait
t1=$(now)
if [ "$alive" -ge "$n" ]; then
	result PASS "cpu: every thread loaded 30 s" "$alive busy workers; ps answered in $(elapsed "$t_ps0" "$t_ps1")s under load"
else
	result WARN "cpu: every thread loaded 30 s" "only $alive of $n workers were running"
fi

# ---- memory -----------------------------------------------------------------

section "Memory"
mem=$(sysctl_n hw.memsize)
mem_gb=$(echo "${mem:-0}" | awk '{ printf "%.1f", $1 / 1073741824 }')
run vm_stat vm_stat
if [ "${mem:-0}" -gt 0 ]; then
	result PASS "memory size" "${mem_gb} GB"
else
	result FAIL "memory size" "hw.memsize missing"
fi
# A large block through memory: write it, read it back, compare.
t0=$(now)
a=$(openssl rand 268435456 2>/dev/null | tee /tmp/hwcheck-mem.bin | md5)
b=$(md5 -q /tmp/hwcheck-mem.bin)
rm -f /tmp/hwcheck-mem.bin
t1=$(now)
if [ "$a" = "$b" ]; then
	result PASS "memory: 256 MB round trip" "same checksum ($(elapsed "$t0" "$t1")s)"
else
	result FAIL "memory: 256 MB round trip" "checksums differ: $a / $b"
fi

# ---- storage ------------------------------------------------------------------

section "Storage"
root_dev=$(mount | awk '$3 == "/" { print $1 }')
run diskutil-root diskutil info /
proto=$(diskutil info / 2>/dev/null | awk -F': *' '/Protocol|Device Location|Media Name/ { printf "%s; ", $2 }')
result INFO "root volume" "$root_dev ${proto}"
case "$root_dev" in
/dev/disk*) result PASS "boot from internal storage" "root on $root_dev" ;;
*) result FAIL "boot from internal storage" "root is '$root_dev'" ;;
esac
f=/var/tmp/hwcheck-disk.bin
t0=$(now)
openssl rand -out /tmp/hwcheck-src.bin 67108864 2>/dev/null
src=$(md5 -q /tmp/hwcheck-src.bin)
: > "$f"
for i in $(seq 1 16); do cat /tmp/hwcheck-src.bin >> "$f"; done
sync
t1=$(now)
w=$(echo "1024 $(elapsed "$t0" "$t1")" | awk '{ if ($2 > 0) printf "%.0f", $1 / $2 }')
purge >/dev/null 2>&1
t0=$(now)
ok=1
for i in $(seq 0 15); do
	c=$(dd if="$f" bs=1048576 skip=$((i * 64)) count=64 2>/dev/null | md5)
	[ "$c" = "$src" ] || ok=0
done
t1=$(now)
r=$(echo "1024 $(elapsed "$t0" "$t1")" | awk '{ if ($2 > 0) printf "%.0f", $1 / $2 }')
rm -f "$f" /tmp/hwcheck-src.bin
if [ $ok = 1 ]; then
	result PASS "storage: 1 GB write and read" "same data; write ~${w} MB/s, read ~${r} MB/s (cache included)"
else
	result FAIL "storage: 1 GB write and read" "the data read back differs"
fi
run df df -h
if grep -qiE 'ahci.*(error|timeout)|nvme.*(error|timeout)|I/O error' "$RAW/dmesg.txt"; then
	result WARN "storage: kernel log" "disk errors in dmesg: $(grep -ciE 'ahci.*(error|timeout)|nvme.*(error|timeout)|I/O error' "$RAW/dmesg.txt") lines"
else
	result PASS "storage: kernel log" "no disk errors"
fi

# ---- network ---------------------------------------------------------------------

section "Network"
run ifconfig ifconfig -a
run netstat-routes netstat -rn
run resolv cat /etc/resolv.conf
ifaces=$(ifconfig -l 2>/dev/null | tr ' ' '\n' | grep -E '^en[0-9]+$')
result INFO "network interfaces" "$(echo $ifaces) ($(echo "$ifaces" | grep -c .) Ethernet)"
up=""
for i in $ifaces; do
	st=$(ifconfig "$i" 2>/dev/null | awk '/status:/ { print $2 }')
	addr=$(ipconfig getifaddr "$i" 2>/dev/null)
	media=$(ifconfig "$i" 2>/dev/null | awk -F': ' '/media:/ { print $2 }')
	echo "$i status=$st addr=$addr media=$media" >> "$DETAIL"
	if [ -n "$addr" ]; then
		result PASS "$i: link and DHCP" "$addr, $media"
		up="$up $i"
	elif [ "$st" = active ]; then
		result FAIL "$i: link and DHCP" "link up but no address by DHCP"
	else
		result WARN "$i: link and DHCP" "no link (${st:-no status}); cable?"
	fi
done
if [ -z "$ifaces" ]; then
	result FAIL "network: interfaces" "no Ethernet interface (no driver for the network card?)"
fi
gw=$(route -n get default 2>/dev/null | awk '/gateway:/ { print $2 }')
if [ -n "$up" ]; then
	if [ -n "$gw" ] && run ping-gw ping -c 3 -t 10 "$gw"; then
		result PASS "network: router" "ping $gw"
	else
		result FAIL "network: router" "no default route or no answer from '$gw'"
	fi
	if run dns host -W 5 example.org; then
		result PASS "network: DNS" "example.org resolves"
	else
		result FAIL "network: DNS" "example.org does not resolve"
	fi
	if run ping-internet ping -c 3 -t 10 1.1.1.1; then
		result PASS "network: internet" "ping 1.1.1.1"
	else
		result FAIL "network: internet" "no answer from 1.1.1.1"
	fi
	speed=$(curl -s -o /dev/null --max-time 60 -w '%{http_code} %{speed_download}' \
	    'https://speed.cloudflare.com/__down?bytes=50000000' 2>>"$DETAIL")
	code=${speed%% *}
	mbps=$(echo "${speed##* }" | awk '{ printf "%.1f", $1 * 8 / 1000000 }')
	if [ "$code" = 200 ]; then
		result PASS "network: 50 MB download (TLS)" "${mbps} Mbit/s"
	else
		result FAIL "network: 50 MB download (TLS)" "HTTP '${code}'"
	fi
	v6=$(ifconfig 2>/dev/null | awk '/inet6/ && $2 !~ /^fe80/ && $2 != "::1" { print $2; exit }')
	if [ -n "$v6" ]; then
		if run ping6 ping6 -c 3 2606:4700:4700::1111; then
			result PASS "network: IPv6" "$v6, ping6 answers"
		else
			result WARN "network: IPv6" "$v6, but ping6 to the internet does not answer"
		fi
	else
		result INFO "network: IPv6" "no global IPv6 address (the network may not offer one)"
	fi
else
	result FAIL "network: no interface with an address" "router, DNS, internet and download not tested"
fi

# ---- services ------------------------------------------------------------------------

section "Services"
if ps -p 1 -o comm= | grep -q launchd; then
	result PASS "launchd is PID 1" ""
else
	result FAIL "launchd is PID 1" "PID 1: $(ps -p 1 -o comm=)"
fi
# sshd starts on demand from launchd's socket: its port is checked below.
for d in syslogd configd diskarbitrationd notifyd powerd; do
	if pgrep -x "$d" >/dev/null 2>&1 || grep -q "[/ ]$d\b" "$RAW/ps.txt"; then
		result PASS "service: $d" "running"
	else
		result WARN "service: $d" "not running"
	fi
done
if netstat -an 2>/dev/null | grep -qE '\.22 .*LISTEN'; then
	result PASS "ssh listens on port 22" "launchd starts sshd on each connection"
else
	result FAIL "ssh listens on port 22" ""
fi

# ---- power --------------------------------------------------------------------------

section "Power"
run pmset pmset -g
run pmset-cap pmset -g cap
run pmset-batt pmset -g batt
if grep -q "ACPI: still starting" "$RAW/dmesg.txt"; then
	result FAIL "acpi interpreter" "ACPI did not finish starting (still starting in dmesg)"
elif grep -qiE 'ACPI' "$RAW/dmesg.txt"; then
	result PASS "acpi interpreter" "started"
else
	result WARN "acpi interpreter" "no ACPI messages in dmesg"
fi
if ioreg -w0 -r -c IOACPIPlatformDevice 2>/dev/null | grep -qiE 'PNP0C0C|PWRB'; then
	result PASS "power button device" "ACPI power button found (press test by hand)"
else
	result WARN "power button device" "no PNP0C0C/PWRB in ioreg (fixed button event may still work)"
fi

# ---- kernel log -------------------------------------------------------------------

section "Kernel log"
raw dmesg.txt dmesg
for pat in 'panic' 'Still waiting for root device' 'IOKitWaitQuiet' 'timed out|timeout' 'not responding'; do
	c=$(grep -ciE "$pat" "$RAW/dmesg.txt")
	if [ "$c" -gt 0 ]; then
		result WARN "dmesg: $pat" "$c lines (first: $(grep -m1 -iE "$pat" "$RAW/dmesg.txt" | cut -c1-80))"
	fi
done
grep -iE 'panic|error|fail|timeout|timed out|not responding' "$RAW/dmesg.txt" | head -80 >> "$DETAIL"
# Minkar's own drivers log each step ("MinkarBGE: ..."): where one stopped.
printf '\nMinkar driver messages:\n' >> "$DETAIL"
grep -E 'Minkar[A-Za-z0-9]+: ' "$RAW/dmesg.txt" | head -200 >> "$DETAIL"
for drv in $(grep -oE 'Minkar[A-Za-z0-9]+: ' "$RAW/dmesg.txt" | sort -u | tr -d ': '); do
	result INFO "driver $drv" "$(grep -E "$drv: " "$RAW/dmesg.txt" | tail -1 | sed "s/.*$drv: //" | cut -c1-90)"
done
result INFO "dmesg" "$(wc -l < "$RAW/dmesg.txt") lines (raw/dmesg.txt)"

# ---- reboot ---------------------------------------------------------------------------

if [ "$MODE" = noreboot ]; then
	result INFO "reboot" "not tested (--no-reboot)"
	finish
	exit 0
fi

section "Reboot"
mkdir -p "$(dirname "$SELF")"
[ "$(readlink -f "$0" 2>/dev/null || echo "$0")" = "$SELF" ] || cp "$0" "$SELF"
chmod 755 "$SELF"
cat > "$DAEMON" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>org.minkar.hwcheck.after-reboot</string>
<key>ProgramArguments</key><array><string>$SELF</string><string>--after-reboot</string></array>
<key>RunAtLoad</key><true/>
<key>StandardOutPath</key><string>$DIR/after-reboot.log</string>
<key>StandardErrorPath</key><string>$DIR/after-reboot.log</string>
</dict></plist>
PLIST
chmod 644 "$DAEMON"
now > "$DIR/reboot-requested"
echo
echo "Rebooting in 10 seconds to test it. After the reboot the check finishes by itself"
echo "(about 45 s after boot) and prints the summary on the screen."
sync
sleep 10
shutdown -r now
