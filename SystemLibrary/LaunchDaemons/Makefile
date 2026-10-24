# System launchd jobs: /System/Library/LaunchDaemons (architecture plan,
# Phase 2). launchctl reads these XML property lists at boot. The *.json
# files are the FreeBSD-era jobs, to be converted as their programs are
# ported.

SUBDIR =

SYSDAEMONS =	com.apple.configd.plist com.apple.mDNSResponder.plist com.apple.notifyd.plist \
		com.apple.syslogd.plist com.apple.aslmanager.plist \
		com.openssh.sshd.plist \
		com.ravynos.dhcpclient.plist com.ravynos.kextstart.plist \
		com.ravynos.getty.console.plist
DIR =		${SYSROOT_DIR}/System/Library/LaunchDaemons

all: install-daemons

install-daemons: .PHONY
	mkdir -p ${DIR}
.for d in ${SYSDAEMONS}
	python3 -c 'import plistlib, sys; plistlib.load(open(sys.argv[1], "rb"))' ${.CURDIR}/${d}
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${d} ${DIR}/
.endfor

.include <bsd.prog.mk>
