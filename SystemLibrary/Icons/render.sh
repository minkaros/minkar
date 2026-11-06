#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Copies the SVG sources that pack.txt names from the icon themes and
# renders them to the PNGs the system installs. Run by hand when pack.txt
# changes; both results are kept in the repository (the build needs no SVG
# renderer).
#
#   render.sh THEMES_DIR
#
# THEMES_DIR holds Faience, Faience-Azur and Faience-Ocre (from the
# Faience 0.5 archive) and Faenza (from the pinned faenza-icon-theme
# commit in README.md). Needs rsvg-convert.

set -eu
themes=$1
here=$(cd "$(dirname "$0")" && pwd)
size=256

grep -v '^#' "$here/pack.txt" | while read -r group name source; do
	[ -n "$group" ] || continue
	mkdir -p "$here/svg/$group" "$here/png/$group"
	cp -L "$themes/$source" "$here/svg/$group/$name.svg"
	rsvg-convert -w $size -h $size "$here/svg/$group/$name.svg" -o "$here/png/$group/$name.png"
done
