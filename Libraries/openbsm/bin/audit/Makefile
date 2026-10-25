# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# audit(8): tells auditd to reread its configuration, rotate or stop.

PROG =	audit
MAN =	audit.8
SRCS =	audit.c auditd_controlUser.c

.include "../openbsm-prog.mk"

auditd_controlUser.c auditd_control.h: ${OPENBSM}/bin/auditd/auditd_control.defs
	${MIG} ${MIGFLAGS} -user auditd_controlUser.c -header auditd_control.h \
		-server /dev/null -sheader /dev/null \
		${OPENBSM}/bin/auditd/auditd_control.defs

.include <bsd.prog.mk>

${OBJS}: ${.OBJDIR}/include/config/config.h auditd_control.h
