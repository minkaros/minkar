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
# Or a git source: "git URL", "commit SHA" (the commit, which git checks
# every object it fetches against) and "path P" lines, the only parts of
# the tree fetched (a partial, sparse clone at that commit); the checkout
# goes to CACHE/<name>-<first 12 of SHA>, name from the "name" line.
set -eu
manifest=$1
cache=$2
git=$(awk '$1 == "git" { print $2 }' "$manifest")
if [ -n "$git" ]; then
	commit=$(awk '$1 == "commit" { print $2 }' "$manifest")
	name=$(awk '$1 == "name" { print $2 }' "$manifest")
	paths=$(awk '$1 == "path" { print $2 }' "$manifest")
	dir=$cache/$name-$(printf %.12s "$commit")
	if [ ! -d "$dir" ] || [ "$(cat "$dir/.minkar-paths" 2>/dev/null)" != "$paths" ]; then
		rm -rf "$dir.part" && mkdir -p "$dir.part"
		(cd "$dir.part" && git init -q && git remote add origin "$git" &&
		    git sparse-checkout init --no-cone > /dev/null &&
		    printf '%s\n' $paths | sed 's|^|/|' > .git/info/sparse-checkout &&
		    git fetch -q --depth 1 --filter=blob:none origin "$commit" &&
		    git checkout -q FETCH_HEAD &&
		    [ "$(git rev-parse HEAD)" = "$commit" ]) ||
		    { echo "fetch.sh: cannot fetch $commit from $git" >&2; exit 1; }
		printf '%s\n' "$paths" > "$dir.part/.minkar-paths"
		rm -rf "$dir" && mv "$dir.part" "$dir"
	fi
	echo "$dir"
	exit 0
fi
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
