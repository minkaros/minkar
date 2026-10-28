#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Builds Citrus's conversion tables (FreeBSD's share/i18n) and installs
# them under DESTDIR/usr/share/i18n, as FreeBSD's install would.
#
#   build-i18n.sh SRCDIR WORKDIR DESTDIR...
#
# SRCDIR is Libraries/libiconv. mkcsmapper and mkesdb are built for the
# build host with HOST_CC (default cc) in WORKDIR, the i18n tree is copied
# there and its own Makefiles run with BSD/share/mk; the files each one
# lists in FILES go to its FILESDIR in every DESTDIR.

set -e
src=$1 work=$2
shift 2
mk=$(cd "$src/../../BSD/share/mk" && pwd)
cc=${HOST_CC:-cc}
yacc=${YACC:-byacc}
lex=${LEX:-flex}

mkdir -p "$work/tools"
for t in mkcsmapper mkesdb; do
	d=$work/tools/$t
	mkdir -p "$d"
	(cd "$d" && $yacc -d -o yacc.c "$src/tools/$t/yacc.y" && $lex -o lex.c "$src/tools/$t/lex.l")
	$cc -O2 -w -include stdint.h -include "$src/compat/minkar_iconv.h" \
	    -I"$src/compat" -I"$src/tools/$t" -I"$src/tools/mkesdb" -I"$src/citrus" \
	    -I"$src/modules/mapper_std" -I"$d" -o "$d/$t" "$d/yacc.c" "$d/lex.c" \
	    "$src/tools/hostcompat.c" "$src/citrus/citrus_bcs.c" \
	    "$src/citrus/citrus_db_factory.c" "$src/citrus/citrus_db_hash.c" \
	    "$src/citrus/citrus_lookup_factory.c" "$src/citrus/citrus_pivot_factory.c"
done

# A clean environment: the outer build's SUBDIR, MAKEFLAGS and object
# directories must not reach these Makefiles.
rm -rf "$work/i18n"
cp -R "$src/i18n" "$work/i18n"
run() { (cd "$1" && shift && env -i PATH="$PATH" HOME=/ bmake -m "$mk" "$@"); }
run "$work/i18n" -j"${JOBS:-4}" MKCSMAPPER="$work/tools/mkcsmapper/mkcsmapper" \
    MKESDB="$work/tools/mkesdb/mkesdb" all

# Every directory with FILES: the top two and their subdirectories.
for dir in "$work/i18n/csmapper" "$work/i18n/esdb" "$work"/i18n/csmapper/*/ "$work"/i18n/esdb/*/; do
	[ -f "$dir/Makefile" ] || continue
	filesdir=$(run "$dir" -V FILESDIR)
	files=$(run "$dir" -V FILES)
	[ -n "$files" ] || continue
	for dest in "$@"; do
		mkdir -p "$dest$filesdir"
		(cd "$dir" && install -m 0444 $files "$dest$filesdir/")
	done
done
