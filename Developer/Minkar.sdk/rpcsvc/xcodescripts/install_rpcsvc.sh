#!/bin/sh
set -e

destination="$DSTROOT"/usr/include/rpcsvc

printf "Creating installation directory\n"
printf " install -m 0755 -d %s\n" "$destination"
install -m 0755 -d "$destination"

for xfile in "$SRCROOT"/*.x; do
	hfile=`basename "$xfile" .x`.h

	printf "Installing %s\n" `basename "$xfile"`

	printf " rpcgen -h -o %s %s\n" "$OBJROOT"/"$hfile" "$xfile"
	${RPCGEN} -h -o "$OBJROOT"/"$hfile" "$xfile"

	printf " install -C -m 0444 %s %s\n" "$OBJROOT"/"$hfile" "$destination"
	install -C -m 0444 "$OBJROOT"/"$hfile" "$destination"

	printf " install -C -m 0444 %s %s\n" "$xfile" "$destination"
	install -C -m 0444 "$xfile" "$destination"
done
