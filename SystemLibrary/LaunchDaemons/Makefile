# System launchd jobs: /System/Library/LaunchDaemons (architecture plan,
# Phase 2). launchctl reads these XML property lists at boot.
# org.minkar.WindowServer.plist is not installed yet: WindowServer comes up
# with Phase 4.

SUBDIR =

SYSDAEMONS =	com.apple.configd.plist com.apple.mDNSResponder.plist com.apple.notifyd.plist \
		com.apple.syslogd.plist com.apple.aslmanager.plist com.apple.auditd.plist \
		com.openssh.sshd.plist \
		org.minkar.dhcpclient.plist org.minkar.kextstart.plist \
		org.minkar.getty.console.plist com.vix.cron.plist org.minkar.atrun.plist \
		org.minkar.newsyslog.plist
DIR =		${SYSROOT_DIR}/System/Library/LaunchDaemons

all: install-daemons

install-daemons: .PHONY
	mkdir -p ${DIR}
.for d in ${SYSDAEMONS}
	python3 -c 'import plistlib, sys; plistlib.load(open(sys.argv[1], "rb"))' ${.CURDIR}/${d}
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${d} ${DIR}/
.endfor

.include <bsd.prog.mk>
