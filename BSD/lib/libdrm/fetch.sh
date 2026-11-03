#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fetch.sh MANIFEST CACHE: the tarball MANIFEST names, into CACHE (kept:
# fetched once), checked against its SHA-256 and unpacked next to it;
# prints the unpacked directory. Exit status 1 if it is missing or differs.
# MANIFEST's "file" line, if any, names the cached file (for a URL whose own
# name does not say what it is).
set -eu
manifest=$1
cache=$2
url=$(awk '$1 == "url" { print $2 }' "$manifest")
sum=$(awk '$1 == "sha256" { print $2 }' "$manifest")
name=$(awk '$1 == "file" { print $2 }' "$manifest")
file=$cache/${name:-$(basename "$url")}
dir=${file%.tar.*}
mkdir -p "$cache"
if [ ! -f "$file" ]; then
	curl -sfL -o "$file.part" "$url" || { rm -f "$file.part"; echo "fetch.sh: cannot fetch $url" >&2; exit 1; }
	mv "$file.part" "$file"
fi
got=$(sha256sum "$file" | awk '{ print $1 }')
[ "$got" = "$sum" ] || { echo "fetch.sh: $file: SHA-256 $got, MANIFEST wants $sum" >&2; exit 1; }
if [ ! -d "$dir" ]; then
	mkdir -p "$dir.part"
	tar -C "$dir.part" --strip-components=1 -xf "$file"
	mv "$dir.part" "$dir"
fi
echo "$dir"
