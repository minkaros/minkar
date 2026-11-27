#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# check-release.sh IMAGE VERSION FAMILY: checks that a release image
# ('bmake release-image') has nothing of the development images: no test
# accounts, no password or ssh key for root, no ssh login for root, no
# automatic login, nothing of the tests (/usr/local); and that launchd
# itself is PID 1, the boot is graphical, and the version and family are
# the release's (PROD_VERSION, PROD_FAMILY). Mounts its volumes read-only
# (needs root). One line per check; exits 1 when one fails.

image=$1 version=$2 family=$3
[ -f "$image" ] && [ -n "$version" ] && [ -n "$family" ] ||
    { echo "usage: check-release.sh IMAGE VERSION FAMILY" >&2; exit 2; }
[ "$(id -u)" -eq 0 ] || { echo "check-release: needs root (loop devices, mounts)" >&2; exit 2; }

failed=0
check() {
	if [ "$2" = "$3" ]; then
		echo "ok    $1"
	else
		echo "FAIL  $1 (expected '$2', got '$3')"
		failed=1
	fi
}

dir=$(mktemp -d)
loop=$(losetup --find --show --partscan --read-only "$image") || exit 2
mkdir -p "$dir/esp" "$dir/root"
cleanup() {
	umount "$dir/esp" "$dir/root" 2>/dev/null
	losetup -d "$loop"
	rmdir "$dir/esp" "$dir/root" "$dir"
}
trap cleanup EXIT
mount -t vfat -o ro "${loop}p1" "$dir/esp" || exit 2
mount -t hfsplus -o ro "${loop}p2" "$dir/root" || exit 2
r=$dir/root

check "root has no password (locked)" "*" \
    "$(awk -F: '$1 == "root" { print $2 }' "$r/private/etc/master.passwd")"
check "no login accounts (none from uid 501)" "" \
    "$(awk -F: '$3 >= 501 && $3 < 60000 { print $1 }' "$r/private/etc/master.passwd")"
check "no home folders in /Users" "" \
    "$(ls "$r/Users" 2>/dev/null | grep -v -x -e Shared -e .localized)"
check "root has no ssh keys" "no" \
    "$([ -e "$r/private/var/root/.ssh/authorized_keys" ] && echo yes || echo no)"
check "sshd keeps its default for root (no PermitRootLogin yes)" "0" \
    "$(grep -c '^PermitRootLogin yes' "$r/private/etc/ssh/sshd_config")"
check "no automatic login" "0" \
    "$(grep -c autoLoginUser "$r/Library/Preferences/com.apple.loginwindow.plist" 2>/dev/null || echo 0)"
check "the medium starts the Installer" "yes" \
    "$([ -e "$r/private/var/db/.MinkarInstallMedium" ] && echo yes || echo no)"
check "nothing of the tests (/usr/local)" "no" \
    "$([ -e "$r/usr/local" ] && echo yes || echo no)"
check "launchd itself is PID 1 (not minkarinit)" "no" \
    "$([ -L "$r/sbin/launchd" ] && echo yes || echo no)"
plist=$r/System/Library/CoreServices/SystemVersion.plist
value() { sed -n "/<key>$1<\/key>/{n;s/.*<string>\(.*\)<\/string>.*/\1/p;}" "$plist"; }
check "ProductFamily" "$family" "$(value ProductFamily)"
check "ProductUserVisibleVersion" "$version" "$(value ProductUserVisibleVersion)"
# ProductVersion is the compatibility value, never Minkar's version.
check "ProductVersion is not the release's version" "no" \
    "$([ "$(value ProductVersion)" = "$version" ] || [ -z "$(value ProductVersion)" ] && echo yes || echo no)"
args=$(sed -n '/<key>boot-args<\/key>/{n;s/.*<string>\(.*\)<\/string>.*/\1/p;}' "$dir/esp/Minkar/org.minkar.boot.plist")
check "a graphical boot (no -v in the boot arguments)" "0" \
    "$(printf ' %s ' "$args" | grep -c -e ' -v ' -e 'serial=' -e 'debug=')"
check "the boot screen's logo" "yes" "$([ -f "$dir/esp/Minkar/Boot/logo.raw" ] && echo yes || echo no)"

[ $failed -eq 0 ] && echo "check-release: PASS" || echo "check-release: FAIL"
exit $failed
