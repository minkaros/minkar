#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fetch.sh MANIFEST CACHE: the files MANIFEST lists, from linux-firmware at
# its tag, into CACHE/linux-firmware-TAG (kept there: fetched once), each
# checked against its SHA-256. Exit status 1 when one is missing or differs.
set -eu
manifest=$1
cache=$2
url=https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain
tag=$(awk '$1 == "tag" { print $2 }' "$manifest")
dir=$cache/linux-firmware-$tag
mkdir -p "$dir"
awk '!/^#/ && $1 != "tag" && NF == 2' "$manifest" | while read -r path sum; do
	file=$dir/$path
	if [ ! -f "$file" ]; then
		mkdir -p "$(dirname "$file")"
		curl -sfL -o "$file.part" "$url/$path?h=$tag" || {
			rm -f "$file.part"
			echo "fetch.sh: cannot fetch $path at $tag" >&2
			exit 1
		}
		mv "$file.part" "$file"
	fi
	got=$(sha256sum "$file" | awk '{ print $1 }')
	if [ "$got" != "$sum" ]; then
		echo "fetch.sh: $path: SHA-256 $got, MANIFEST wants $sum" >&2
		exit 1
	fi
done
echo "$dir"
