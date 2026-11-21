#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate BC's runner (Docs/ROADMAP.md, lane BC), in the image as
# /usr/local/tests/bc/bc-run.sh; boottest runs it over ssh for each app:
#
#   bc-run.sh NAME ARCHIVE BUNDLE EXECUTABLE [SECONDS]
#
# ARCHIVE is the app's published download (a disk image, a zip or a tar),
# copied into the VM by boottest. The app is installed as a user would:
# a disk image attached with hdiutil and the app copied to /Applications
# with ditto, a zip or a tar expanded there. Then BUNDLE's EXECUTABLE is
# started with the probe (bcprobe.dylib) and watched for SECONDS (40 by
# default) until it shows a window or exits. One line says how far it
# got, then the app is quit and removed:
#
#   bc: NAME stage=N result=WORD menus=yes|no reason=TEXT
#
#   0 install      the archive could not be installed
#   1 signature    the kernel refused the signature (MinkarCodeIntegrity)
#   2 no-load      dyld could not load it (a library or symbol missing)
#   3 before-main  it ended before its libraries' initializers had run
#   4 at-start     it ended after them, before showing a window
#   5 no-window    it runs, but showed no window in SECONDS (a panel or
#                  an alert alone does not count)
#   6 window       it shows a window at the normal level
#
# menus=yes when it also shows its menu bar.

name=$1 archive=$2 bundle=$3 exe=$4 wait=${5:-40}
work=/private/tmp/bc/$name
probe=/usr/local/tests/bc/bcprobe.dylib
app=/Applications/$bundle

report() {
	# report STAGE WORD REASON
	reason=$(printf '%s%s%s' "${note:-}" "${user:+as $user; }" "$3" | tr '\t\n' '  ' | cut -c1-300)
	echo "bc: $name stage=$1 result=$2 menus=${menus:-no} reason=$reason"
	echo "bc: $name end"
	cleanup
	exit 0
}

# A child that ended stays a zombie until waited for, and kill -0 still
# finds it.
alive() {
	s=$(ps -o stat= -p "$pid" 2>/dev/null)
	[ -n "$s" ] && [ "${s#Z}" = "$s" ]
}

cleanup() {
	[ -n "$pid" ] && kill "$pid" 2>/dev/null
	[ -n "$pid" ] && sleep 2 && kill -9 "$pid" 2>/dev/null
	[ -d "$work/mnt" ] && hdiutil detach -force "$work/mnt" > /dev/null 2>&1
	rm -rf "$app" "$work" "$archive"
}

rm -rf "$work" "$app"
mkdir -p "$work"

# The desktop session is up when the Dock runs.
for i in $(seq 120); do pgrep -x Dock > /dev/null && break; sleep 1; done

# Install it.
magic=$(head -c 4 "$archive" | od -An -c | tr -d ' ')
if [ "$(tail -c 512 "$archive" | head -c 4)" = koly ]; then
	mkdir -p "$work/mnt"
	if ! hdiutil attach -nobrowse -readonly -mountpoint "$work/mnt" "$archive" > "$work/attach.txt" 2>&1; then
		# What hdiutil says to do with a format it reads but does not
		# attach yet (LZMA, ULMO): convert it, and say so.
		grep -q "hdiutil convert" "$work/attach.txt" ||
		    report 0 install "hdiutil attach: $(tail -1 "$work/attach.txt")"
		note="installed after hdiutil convert ($(sed -n 's/.*attach failed - //p' "$work/attach.txt" | cut -d';' -f1)); "
		hdiutil convert "$archive" -format UDZO -o "$work/converted.dmg" > "$work/convert.txt" 2>&1 ||
		    report 0 install "hdiutil convert: $(tail -1 "$work/convert.txt")"
		hdiutil attach -nobrowse -readonly -mountpoint "$work/mnt" "$work/converted.dmg" > "$work/attach.txt" 2>&1 ||
		    report 0 install "hdiutil attach after convert: $(tail -1 "$work/attach.txt")"
	fi
	src=$(find "$work/mnt" -maxdepth 2 -name "$bundle" -type d | head -1)
	[ -n "$src" ] || report 0 install "no $bundle in the disk image"
	ditto "$src" "$app" || report 0 install "ditto from the disk image failed"
	hdiutil detach "$work/mnt" > /dev/null 2>&1
elif [ "$magic" = 'PK\003\004' ]; then
	ditto -x -k "$archive" "$work/x" || report 0 install "ditto could not expand the zip"
	src=$(find "$work/x" -maxdepth 3 -name "$bundle" -type d | head -1)
	[ -n "$src" ] || report 0 install "no $bundle in the zip"
	mv "$src" "$app"
else
	mkdir -p "$work/x"
	tar -xf "$archive" -C "$work/x" || report 0 install "tar could not expand the archive"
	src=$(find "$work/x" -maxdepth 3 -name "$bundle" -type d | head -1)
	[ -n "$src" ] || report 0 install "no $bundle in the archive"
	mv "$src" "$app"
fi
rm -f "$archive"
[ -x "$app/Contents/MacOS/$exe" ] || report 0 install "no executable $exe in $bundle"

# Start it as the user who owns the desktop session, as a user would, with
# that user's login environment (su -l: its HOME, where apps keep their
# profiles)
# (some apps refuse to run as root); as root when root owns it.
dmesg > "$work/dmesg.before"
cd /
# The session's owner is the Dock's.
user=$(ps -o user= -p "$(pgrep -x Dock | head -1)" 2>/dev/null | tr -d ' ')
# A session owned by root (test images): the first local user, whose
# apps the window server shows in that session just as well.
[ "$user" = root ] && user=$(id -nu 501 2>/dev/null || echo root)
if [ -n "$user" ] && [ "$user" != root ] && id "$user" > /dev/null 2>&1; then
	chown -R "$user" "$work"
	su -l "$user" -c "DYLD_INSERT_LIBRARIES=$probe BCPROBE_OUT='$work/probe.txt' exec '$app/Contents/MacOS/$exe'" \
	    > "$work/out.txt" 2>&1 < /dev/null &
else
	DYLD_INSERT_LIBRARIES=$probe BCPROBE_OUT=$work/probe.txt \
	    "$app/Contents/MacOS/$exe" > "$work/out.txt" 2>&1 < /dev/null &
fi
launcher=$!
# The app's own process: su's child (its command line names the app
# too, so it is not looked up by name).
pid=$launcher
if [ -n "$user" ] && [ "$user" != root ]; then
	for i in $(seq 25); do
		p=$(pgrep -P "$launcher" | head -1)
		[ -n "$p" ] && { pid=$p; break; }
		sleep 0.2
	done
fi

window=no menus=no
for i in $(seq "$wait"); do
	sleep 1
	alive || break
	list=$(/usr/local/bin/wsctl windows 2>/dev/null)
	if printf '%s\n' "$list" | awk -v p="$pid" '$2 == p && $4 == "yes" && $3 == 24 { f = 1 } END { exit !f }'; then
		menus=yes
	fi
	if printf '%s\n' "$list" | awk -v p="$pid" '$2 == p && $4 == "yes" && $3 == 0 { f = 1 } END { exit !f }'; then
		window=yes
		sleep 2
		/usr/local/bin/wsctl windows 2>/dev/null | awk -v p="$pid" '$2 == p && $4 == "yes" && $3 == 24 { f = 1 } END { exit !f }' && menus=yes
		break
	fi
done

if alive; then
	if [ $window = yes ]; then
		report 6 window "$(/usr/local/bin/wsctl windows | awk -v p="$pid" '$2 == p && $4 == "yes" && $3 == 0 { print "a window at " $5 ", " $6; exit }')"
	fi
	panel=$(/usr/local/bin/wsctl windows | awk -v p="$pid" '$2 == p && $4 == "yes" && $3 > 0 && $3 < 24 { print "only a panel, " $6 ", at level " $3; exit }')
	report 5 no-window "running after ${wait}s without a document window${panel:+ ($panel)}; $(grep -v '^ *$' "$work/out.txt" | tail -1)"
fi

wait "$launcher"
rc=$?
dmesg > "$work/dmesg.after"
refused=$(diff "$work/dmesg.before" "$work/dmesg.after" | grep "MinkarCodeIntegrity: refused" | head -1)
[ -n "$refused" ] && report 1 signature "${refused#> }"
dyld=$(grep -m 3 -E "Library not loaded|Symbol not found|Referenced from|Reason: " "$work/out.txt" | sed 's/^ *//' | tr '\n' ' ')
[ -n "$dyld" ] && report 2 no-load "$dyld"
last=$(grep -v '^ *$' "$work/out.txt" | tail -1)
if ! grep -q '^init$' "$work/probe.txt" 2>/dev/null; then
	report 3 before-main "exit status $rc; $last"
fi
report 4 at-start "exit status $rc$(grep -q '^main-queue$' "$work/probe.txt" && echo ', after its run loop started'); $last"
