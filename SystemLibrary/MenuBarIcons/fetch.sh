#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# Downloads the icons pack.txt lists from Google's material-design-icons at
# the commit below, checks them against SHA256SUMS and renders the PNGs
# (36 px: 18 points at 2x). Run by hand when pack.txt changes; the build
# only installs svg/ and png/. Needs curl and rsvg-convert.
set -e
commit=10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7
here=$(cd "$(dirname "$0")" && pwd)
base=https://raw.githubusercontent.com/google/material-design-icons/$commit/symbols/web
size=36
mkdir -p "$here/svg" "$here/png"
grep -v '^#' "$here/pack.txt" | while read -r name icon; do
	[ -n "$name" ] || continue
	curl -sf "$base/$icon/materialsymbolsoutlined/${icon}_20px.svg" -o "$here/svg/$name.svg"
done
(cd "$here/svg" && sha256sum -c --quiet ../SHA256SUMS)
for f in "$here"/svg/*.svg; do
	n=$(basename "$f" .svg)
	rsvg-convert -w $size -h $size "$f" -o "$here/png/$n.png"
done
