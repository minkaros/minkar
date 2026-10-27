#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# install-pages.sh SRCDIR SYSROOT
#
# Installs the manual page each command's source carries, for the commands
# the system has, when its own Makefile did not: SRCDIR/{bin,sbin,
# usr.bin,usr.sbin,libexec}/*/ (and their subdirectories) hold NAME.1 or
# NAME.8; it goes to SYSROOT/usr/share/man/manN/ when SYSROOT has the
# command NAME (in /bin, /sbin, /usr/bin, /usr/sbin or /usr/libexec) and no
# page NAME.N yet. A command's own directory comes first. Pages that still
# carry build-time @VARIABLES@ are left out.
# Then the other names of a command (a link to it, hard or symbolic, such
# as egrep for grep) get a link to its page, as FreeBSD's MLINKS do, and
# so does a command a page names in its NAME section (.Nm basename ,
# dirname: dirname(1) is basename(1)).

set -eu
src=$1
root=$2

has_command() {
	for d in bin sbin usr/bin usr/sbin usr/libexec; do
		[ -f "$root/$d/$1" ] && return 0
	done
	return 1
}

for top in bin sbin usr.bin usr.sbin libexec; do
	[ -d "$src/$top" ] || continue
	# NAME/NAME.N first, then the other pages, so a command's own
	# directory wins over another one's copy.
	{
		find "$src/$top" -mindepth 2 -maxdepth 2 -name '*.[18]' |
		    awk -F/ '{ p = $NF; sub(/\.[18]$/, "", p);
			if (p == $(NF - 1)) print }'
		find "$src/$top" -mindepth 2 -maxdepth 4 -name '*.[18]' | sort
	} | while read -r page; do
		file=${page##*/}
		name=${file%.?}
		sec=${file##*.}
		dest=$root/usr/share/man/man$sec
		[ -e "$dest/$file" ] || [ -e "$dest/$file.gz" ] && continue
		has_command "$name" || continue
		grep -q '@[A-Z_][A-Z_]*@' "$page" && continue
		mkdir -p "$dest"
		install -m 0444 "$page" "$dest/$file"
		echo "  man$sec/$file"
	done
done | tee /dev/stderr | wc -l | awk '{ print "install-pages: " $1 " pages" }'

# The other names: NAME is a symbolic link to a command, or a hard link
# with another name in the same directory, and that command has a page.
page_of() {
	for sec in 1 8; do
		for f in "$root/usr/share/man/man$sec/$1.$sec" \
		    "$root/usr/share/man/man$sec/$1.$sec.gz"; do
			[ -e "$f" ] && { echo "$f"; return 0; }
		done
	done
	return 1
}

for d in bin sbin usr/bin usr/sbin; do
	[ -d "$root/$d" ] || continue
	for cmd in "$root/$d"/*; do
		name=${cmd##*/}
		page_of "$name" >/dev/null && continue
		target=
		if [ -L "$cmd" ]; then
			target=$(readlink "$cmd")
			target=${target##*/}
		elif [ -f "$cmd" ]; then
			target=$(find "$root/$d" -maxdepth 1 -samefile "$cmd" \
			    ! -name "$name" | while read -r t; do
				page_of "${t##*/}" >/dev/null && { echo "${t##*/}"; break; }
			done || true)
		fi
		[ -n "$target" ] || continue
		page=$(page_of "$target") || continue
		file=${page##*/}
		ext=${file#"$target"}
		ln -f "$page" "${page%/*}/$name$ext"
		echo "  ${page%/*}/$name$ext -> $file" | sed "s|$root/usr/share/man/||"
	done
done | tee /dev/stderr | wc -l | awk '{ print "install-pages: " $1 " links" }'

# The names a page gives in its NAME section (mdoc .Nm lines before .Nd).
page_names() {
	awk '/^\.Sh NAME/ { on = 1; next }
	    on && /^\.(Nd|Sh)/ { exit }
	    on && /^\.Nm/ { for (i = 2; i <= NF; i++) if ($i != ",") {
		sub(/,$/, "", $i); print $i } }' "$1"
}

for d in bin sbin usr/bin usr/sbin; do
	[ -d "$root/$d" ] || continue
	for cmd in "$root/$d"/*; do
		name=${cmd##*/}
		page_of "$name" >/dev/null && continue
		for page in "$root"/usr/share/man/man1/*.1 "$root"/usr/share/man/man8/*.8; do
			[ -f "$page" ] || continue
			page_names "$page" | grep -qxF -- "$name" || continue
			file=${page##*/}
			ln -f "$page" "${page%/*}/$name.${file##*.}"
			echo "  ${page%/*}/$name.${file##*.} -> $file" | sed "s|$root/usr/share/man/||"
			break
		done
	done
done | tee /dev/stderr | wc -l | awk '{ print "install-pages: " $1 " named in a page" }'
