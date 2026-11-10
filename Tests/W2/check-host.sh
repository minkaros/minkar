#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W2 (WebCore and WebKit's processes): a machine running Minkar draws
# page.html with WPE WebKit on its headless display (render.c, built here
# with the toolchain against the SDK), and the image is compared with the
# page's reference (compare.py).
#
#   check-host.sh TOOLCHAIN SDK HOST KEY OUT
#
# TOOLCHAIN is the toolchain's usr/bin; HOST user@address[:port]; KEY the
# private key to log in with (may be empty); OUT where render, the
# rendering, the reference and their difference go. The page is drawn in
# /var/tmp/w2 on the host, emptied before and after. Ends with
# "w2-check: PASS" or "w2-check: FAIL".
tc=$1 sdk=$2 host=$3 key=$4 out=$5
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)

user=${host%%@*} rest=${host#*@}
addr=${rest%%:*} port=22
case $rest in *:*) port=${rest##*:} ;; esac
remote=/var/tmp/w2
opts="-o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$out/.known_hosts"
rm -f "$out/.known_hosts"
on_host() { ssh $opts ${key:+-i "$key"} -p "$port" "$user@$addr" "$@"; }
copy() { scp -q $opts ${key:+-i "$key"} -P "$port" "$@"; }

# render, against the SDK's WPE WebKit (private: usr/local/webkit).
pc="env -u MACOSX_DEPLOYMENT_TARGET PKG_CONFIG_LIBDIR=$sdk/usr/lib/pkgconfig:$sdk/usr/local/lib/pkgconfig:$sdk/usr/local/webkit/lib/pkgconfig PKG_CONFIG_PATH= PKG_CONFIG_SYSROOT_DIR=$sdk PKG_CONFIG_FDO_SYSROOT_RULES=1 pkg-config"
flags=$($pc --cflags --libs wpe-webkit-2.0 wpe-platform-headless-2.0) ||
    { echo "w2-check: WPE WebKit is not in $sdk"; echo "w2-check: FAIL"; exit 1; }
env -u MACOSX_DEPLOYMENT_TARGET "$tc/clang" -target x86_64-apple-macosx10.15 --sysroot="$sdk" \
    -mmacos-version-min=10.15 -fuse-ld="$tc/ld64" -O2 -Wall -Wno-nullability-completeness \
    -o "$out/render" "$here/render.c" $flags ||
    { echo "w2-check: render does not build"; echo "w2-check: FAIL"; exit 1; }

on_host "rm -rf $remote && mkdir -p $remote" || { echo "w2-check: cannot reach $host"; echo "w2-check: FAIL"; exit 1; }
copy "$out/render" "$here/page.html" "$user@$addr:$remote/" || { echo "w2-check: cannot copy to $host"; echo "w2-check: FAIL"; exit 1; }
rm -f "$out/rendered.ppm"
start=$(date +%s)
on_host "cd $remote && ./render file://$remote/page.html rendered.ppm 800 600" > "$out/render.log" 2>&1
rc=$?
echo "w2-check: render exited $rc in $(( $(date +%s) - start )) s"
[ $rc -eq 0 ] && copy "$user@$addr:$remote/rendered.ppm" "$out/"
on_host "rm -rf $remote"
if [ ! -f "$out/rendered.ppm" ]; then
	tail -20 "$out/render.log"
	echo "w2-check: FAIL"
	exit 1
fi
python3 "$here/compare.py" "$out/rendered.ppm" "$out" > "$out/compare.log"
rc=$?
sed 's/^w2-compare/w2-check/' "$out/compare.log"
exit $rc
