#!/bin/sh
# HarfBuzz's shaping tests (test/shape/run-tests.py, in sh: Minkar has no
# Python). Each line of a .tests file: font;options;unicodes;expected. The
# text is shaped with the OpenType font functions and, unless the line
# asks for those alone, with FreeType's too; both must give the expected
# glyphs. Fonts named by an absolute path (a system's own) are skipped.
# Usage: shape.sh hb-shape file.tests...
hb_shape=$1; shift
export LC_ALL=C
set -f
passes=0 fails=0 skips=0
for file in "$@"; do
	cwd=${file%/*}
	while IFS= read -r line || [ -n "$line" ]; do
		case $line in '#'*|'') continue;; esac
		IFS=';' read -r font options unicodes expected <<-END
		$line
		END
		case $font in /*|'"/'*) skips=$((skips + 1)); continue;; esac
		font=$cwd/$font
		extra=--shaper=ot
		[ "$expected" = '*' ] || extra="$extra --verify --unsafe-to-concat"
		case " $options " in
		*' --shaper=coretext '*|*' --shaper=directwrite '*|*' --shaper=uniscribe '*)
			skips=$((skips + 1)); continue;;
		esac
		case " $options " in
		*' --font-funcs=ot '*)
			g1=$("$hb_shape" "$font" --font-funcs=ot $extra --unicodes "$unicodes" $options);;
		*)
			g1=$("$hb_shape" "$font" --font-funcs=ft $extra --unicodes "$unicodes" $options)
			g2=$("$hb_shape" "$font" --font-funcs=ot $extra --unicodes "$unicodes" $options)
			if [ "$g1" != "$g2" ] && [ "$expected" != '*' ]; then
				echo "FAIL $file: $unicodes: FT funcs $g1, OT funcs $g2"; fails=$((fails + 1))
			else
				passes=$((passes + 1))
			fi;;
		esac
		if [ "$expected" != '*' ] && [ "$g1" != "$expected" ]; then
			echo "FAIL $file: $font --unicodes $unicodes $options"
			echo "    actual:   $g1"
			echo "    expected: $expected"
			fails=$((fails + 1))
		else
			passes=$((passes + 1))
		fi
	done < "$file"
done
echo "$passes passed; $fails failed; $skips skipped"
[ $fails -eq 0 ]
