#!/bin/sh
# Minkar's own tests of freetype (W0) on every OFL font the system ships
# (Inter and Agave): one line a check, the totals last.
here=$(dirname "$0")
exec "$here/ft-test" /System/Library/Fonts/Inter-*.ttf /System/Library/Fonts/Agave-*.ttf
