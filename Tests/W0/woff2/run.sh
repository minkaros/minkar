#!/bin/sh
# Minkar's own tests of woff2 (W0) on every OFL font the system ships
# (Inter and Agave): one line a check, the totals last.
here=$(dirname "$0")
exec "$here/woff2-test" /System/Library/Fonts/Inter-*.ttf /System/Library/Fonts/Agave-*.ttf
