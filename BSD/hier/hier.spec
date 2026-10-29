# Base file system hierarchy of Minkar, after macOS (hier(7)).
# Installed into the sysroot before anything else is built.
#
#   d MODE PATH      directory, owned by root:wheel
#   l TARGET PATH    symbolic link
#
# /etc, /var and /tmp live in /private, as on macOS. launchctl's
# fix_bogus_file_metadata() checks the launchd and /var/db entries at boot.

d 0755 private
d 0755 private/etc
d 0755 private/var
d 1777 private/tmp
l private/etc etc
l private/var var
l private/tmp tmp

d 0755 private/var/account
d 0755 private/var/at
d 0755 private/var/db
d 0755 private/var/db/launchd
d 0755 private/var/db/launchd/com.apple.launchd
d 0755 private/var/db/mds
d 0555 private/var/empty
d 0755 private/var/folders
d 0755 private/var/log
d 0755 private/var/log/com.apple.launchd
d 0750 private/var/root
d 0755 private/var/run
d 1777 private/var/tmp
d 0755 private/var/vm

d 0755 Applications
d 0755 Applications/Utilities
d 1775 cores
d 0555 dev
d 0755 Library
d 0755 Library/LaunchAgents
d 0755 Library/LaunchDaemons
d 0755 Library/Preferences
d 0755 Library/StartupItems
d 0755 System
d 0755 System/Library
d 0755 System/Library/LaunchAgents
d 0755 System/Library/LaunchDaemons
d 0755 Users
d 1777 Users/Shared
d 1777 Volumes
d 0755 bin
d 0755 sbin
d 0755 usr
d 0755 usr/bin
d 0755 usr/lib
d 0755 usr/libexec
d 0755 usr/local
d 0755 usr/sbin
d 0755 usr/share
