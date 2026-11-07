# The GNU forms fontconfig's run-test.sh uses, in the BSD tools Minkar has
# (sourced by it in the W0 copy): touch -d @N, date -d @N, stat -c FORMAT.
touch() {
	if [ "$1" = -d ]; then
		case $2 in @*) command touch -t "$(command date -r "${2#@}" +%Y%m%d%H%M.%S)" "$3"; return;; esac
	fi
	command touch "$@"
}
date() {
	if [ "$1" = -d ]; then
		case $2 in @*) n=${2#@}; shift 2; command date -r "$n" "$@"; return;; esac
	fi
	command date "$@"
}
stat() {
	if [ "$1" = -c ]; then
		fmt=$(printf '%s' "$2" | sed -e 's/%n/@N/g' -e 's/%s/@z/g' -e 's/%y/@Sm/g' \
		    -e 's/%z/@Sc/g' -e 's/%Y/@m/g' -e 's/@/%/g')
		shift 2
		command stat -t '%Y-%m-%d %H:%M:%S' -f "$fmt" "$@"
		return
	fi
	command stat "$@"
}
