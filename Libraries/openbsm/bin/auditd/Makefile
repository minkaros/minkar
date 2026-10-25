# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# auditd(8): writes the kernel's audit trail to /var/audit. launchd starts
# it on demand through the host's audit control port (com.apple.auditd).

PROG =	auditd
MAN =	auditd.8
SRCS =	auditd.c auditd_darwin.c audit_warn.c \
	auditd_controlServer.c audit_triggersServer.c
LDADD =	-lbsm -lauditd
SECURITY = audit_class audit_control audit_event audit_user audit_warn

.include "../openbsm-prog.mk"

auditd_controlServer.c auditd_controlServer.h: ${.CURDIR}/auditd_control.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header /dev/null \
		-server auditd_controlServer.c -sheader auditd_controlServer.h \
		${.CURDIR}/auditd_control.defs
audit_triggersServer.c audit_triggersServer.h: ${.CURDIR}/audit_triggers.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header /dev/null \
		-server audit_triggersServer.c -sheader audit_triggersServer.h \
		${.CURDIR}/audit_triggers.defs

# Configuration in /etc/security and the trail in /var/audit, as on macOS.
all: security
security: .PHONY
	mkdir -p ${SYSROOT_DIR}/private/etc/security ${SYSROOT_DIR}/private/var/audit
	chmod 0700 ${SYSROOT_DIR}/private/var/audit
.for f in ${SECURITY}
	install -m 0444 ${OPENBSM}/etc/${f} ${SYSROOT_DIR}/private/etc/security/
.endfor
	chmod 0555 ${SYSROOT_DIR}/private/etc/security/audit_warn
	chmod 0400 ${SYSROOT_DIR}/private/etc/security/audit_control \
		${SYSROOT_DIR}/private/etc/security/audit_user

.include <bsd.prog.mk>

${OBJS}: ${.OBJDIR}/include/config/config.h auditd_controlServer.h audit_triggersServer.h
