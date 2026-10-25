#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The ravynOS contributors
#
# Boot a ravynOS disk image (bmake image) in QEMU on a macOS host, with
# QEMU from MacPorts (/opt/local), Homebrew or anywhere in PATH: the EDK2
# firmware is looked for in the share/qemu next to the qemu binary (or
# set QEMU_SHARE). The console is the serial port, in this terminal: log
# in there,
# or over ssh: ssh -p 2222 root@127.0.0.1. "poweroff" ends QEMU.
#
#   sh run-macos.sh [ravynOS.img]
#
# Intel Macs use the Hypervisor framework (hvf); Apple silicon emulates
# x86_64 (tcg), which works but is much slower. Westmere is the CPU model
# the boot tests use; on Intel, CPU=host is faster. Ctrl-A X quits QEMU.

set -e

IMG=${1:-ravynOS.img}
QEMU=${QEMU:-qemu-system-x86_64}
QEMU_BIN=$(command -v "$QEMU") || { echo "$QEMU not found in PATH" >&2; exit 1; }
if [ -z "$QEMU_SHARE" ]; then
	# The binary's own prefix first (MacPorts: /opt/local/bin -> /opt/local/
	# share/qemu), then the usual MacPorts and Homebrew places.
	for d in "$(dirname "$QEMU_BIN")/../share/qemu" /opt/local/share/qemu \
	    /opt/homebrew/share/qemu /usr/local/share/qemu; do
		if [ -f "$d/edk2-x86_64-code.fd" ]; then
			QEMU_SHARE=$d
			break
		fi
	done
fi
SHARE=${QEMU_SHARE:?"EDK2 firmware (edk2-x86_64-code.fd) not found; set QEMU_SHARE"}
CODE=$SHARE/edk2-x86_64-code.fd
VARS_TEMPLATE=$SHARE/edk2-i386-vars.fd
VARS=${VARS:-$(dirname "$IMG")/ravynOS-efivars.fd}
SSH_PORT=${SSH_PORT:-2222}

for f in "$IMG" "$CODE" "$VARS_TEMPLATE"; do
	[ -f "$f" ] || { echo "not found: $f" >&2; exit 1; }
done
# The firmware's variable store, writable, kept next to the image.
[ -f "$VARS" ] || cp "$VARS_TEMPLATE" "$VARS"

if [ "$(uname -m)" = x86_64 ] && sysctl -n kern.hv_support 2>/dev/null | grep -q 1; then
	ACCEL=hvf
	CPU=${CPU:-Westmere}
else
	ACCEL=tcg,thread=multi
	CPU=${CPU:-Westmere}
fi

exec "$QEMU_BIN" \
	-machine q35 -accel "$ACCEL" -cpu "$CPU" -smp 2 -m 4096 \
	-drive if=pflash,format=raw,readonly=on,file="$CODE" \
	-drive if=pflash,format=raw,file="$VARS" \
	-drive if=none,id=disk,format=raw,file="$IMG" \
	-device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 \
	-device virtio-rng-pci \
	-netdev user,id=net0,hostfwd=tcp:127.0.0.1:"$SSH_PORT"-:22 \
	-device e1000e,netdev=net0 \
	-serial mon:stdio -display none -no-reboot
