#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Host preparation steps for the CI workflow (.github/workflows), on an
# Ubuntu 22.04 runner. Run from the top of the source tree.
#
#   host-setup.sh free-disk           remove preinstalled SDKs we never use
#   host-setup.sh packages [image]    build packages and Open CF-lite;
#                                     "image" adds QEMU etc.
#   host-setup.sh opencflite          build Open CF-lite (OPENCFLITE_PREFIX)
#   host-setup.sh touch-toolchain     mark the restored toolchain up to date
#   host-setup.sh annotate-log FILE.. end of each log as an error annotation

set -eu

# Docs/BUILDING.md, "Host requirements". "time" is /usr/bin/time, for
# build-stages.sh.
BUILD_PACKAGES="clang lld cmake ninja-build python3 git curl \
libxml2-dev libpng-dev uuid-dev libssl-dev zlib1g-dev libbsd-dev \
libacl1-dev libc++-dev libc++abi-dev llvm-14-dev libedit-dev libffi-dev \
libdispatch-dev libblocksruntime-dev libkqueue-dev libpthread-workqueue-dev \
libicu-dev autoconf automake libtool pkg-config \
bmake make csh bison flex byacc time"
# Open CF-lite (APSL, host only): plktool and xcbuild link it.
OPENCFLITE_REPO=https://github.com/gerickson/opencflite.git
OPENCFLITE_TAG=opencflite-635.21.8
IMAGE_PACKAGES="nasm gdisk dosfstools hfsprogs qemu-system-x86 ovmf \
openssh-client openssl"

free_disk() {
	df -h /
	sudo rm -rf /usr/share/dotnet /usr/local/lib/android /opt/ghc \
	    /usr/local/.ghcup /opt/hostedtoolcache/CodeQL /usr/share/swift
	df -h /
}

packages() {
	pkgs=$BUILD_PACKAGES
	[ "${1:-}" = image ] && pkgs="$pkgs $IMAGE_PACKAGES"
	sudo apt-get update -q
	# shellcheck disable=SC2086
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends $pkgs
	if [ "${1:-}" = image ]; then
		# mkdiskimage.py mounts the HFS+ root; the cloud kernel keeps
		# hfsplus in the "extra" modules.
		sudo modprobe hfsplus 2>/dev/null || {
			sudo apt-get install -y -q "linux-modules-extra-$(uname -r)"
			sudo modprobe hfsplus
		}
		# Gate M2 resolves the host's own name from the guest, through
		# QEMU's DNS forwarding to the host's resolver.
		host=$(hostname -s)
		getent hosts "$host" >/dev/null ||
		    echo "127.0.1.1 $host" | sudo tee -a /etc/hosts
	fi
	opencflite
	clang --version | head -1
	bmake -V MAKE_VERSION -f /dev/null 2>/dev/null || true
}

# Open CF-lite in OPENCFLITE_PREFIX (default /usr/local), unless it is
# already there.
opencflite() {
	prefix=${OPENCFLITE_PREFIX:-/usr/local}
	if [ -e "$prefix/lib/libCoreFoundation.so" ]; then
		echo "opencflite: already in $prefix"
		return
	fi
	tmp=${RUNNER_TEMP:-${TMPDIR:-/tmp}}
	src=$tmp/opencflite
	rm -rf "$src"
	git clone -q --depth 1 --branch "$OPENCFLITE_TAG" "$OPENCFLITE_REPO" "$src"
	log=$tmp/opencflite.log
	# Ubuntu has no tzcode tzfile.h; Open CF-lite carries one in include/.
	# Ubuntu's ICU has no "icu" pkg-config package, and without it
	# configure links no ICU at all: name the directories, or programs
	# that link CoreFoundation (plktool, xcbuild) miss its ICU symbols.
	icu_libs=/usr/lib/$(gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
	if ! (cd "$src" && ./configure -q --prefix="$prefix" --with-tz-includes="$src" \
	    --with-icu-includes=/usr/include --with-icu-libs="$icu_libs" &&
	    make -s -j"$(nproc)" && sudo make -s install) > "$log" 2>&1; then
		tail -n 40 "$log"
		[ -n "${GITHUB_ACTIONS:-}" ] && annotate_log "$log"
		exit 1
	fi
	if ! readelf -d "$prefix/lib/libCoreFoundation.so" | grep -q 'NEEDED.*libicuuc'; then
		echo "opencflite: libCoreFoundation.so does not link ICU"
		[ -n "${GITHUB_ACTIONS:-}" ] &&
		    echo "::error title=Open CF-lite::libCoreFoundation.so does not link ICU"
		exit 1
	fi
	sudo ldconfig
}

touch_toolchain() {
	eval "$(tools/ci/cache-paths.sh | grep -E '^(toolchain|objdir)=')"
	now=$(date +%s)
	# shellcheck disable=SC2154
	for d in "$toolchain" "$objdir"; do
		test -d "$d" || { echo "missing: $d" >&2; exit 1; }
		find "$d" -exec touch -h -d "@$now" {} +
	done
}

annotate_log() {
	for f in "$@"; do
		test -f "$f" || continue
		printf '::error title=%s::' "$(basename "$f")"
		tail -n 60 "$f" | sed -e 's/\x1b\[[0-9;?]*[A-Za-z]//g' -e 's/\r//g' \
		    -e 's/%/%25/g' | awk '{ printf "%s%%0A", $0 }'
		echo
	done
}

cmd=${1:-}
[ $# -gt 0 ] && shift
case $cmd in
free-disk)		free_disk ;;
packages)		packages "$@" ;;
touch-toolchain)	touch_toolchain ;;
opencflite)		opencflite ;;
annotate-log)		annotate_log "$@" ;;
*)	echo "usage: host-setup.sh free-disk|packages [image]|opencflite|touch-toolchain|annotate-log file..." >&2
	exit 1 ;;
esac
