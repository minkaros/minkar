# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# at's settings, shared with atrun (BSD/libexec/atrun): FreeBSD main's,
# with macOS's places (/var/at, atrun in /usr/libexec). daemon is uid and
# gid 1 on Minkar, as on macOS.

LOCKFILE =	.lockfile
ATSPOOL_DIR =	/var/at/spool
ATJOB_DIR =	/var/at/jobs
ATLIB_DIR =	/usr/libexec
LOADAVG_MX =	1.5
DAEMON_GID =	1
DAEMON_UID =	1
DEFAULT_BATCH_QUEUE = E
DEFAULT_AT_QUEUE = c
PERM_PATH =	/var/at

AT_CFLAGS = -DATJOB_DIR=\"${ATJOB_DIR}/\" \
	-DLFILE=\"${ATJOB_DIR}/${LOCKFILE}\" \
	-DLOADAVG_MX=${LOADAVG_MX} -DATSPOOL_DIR=\"${ATSPOOL_DIR}\" \
	-DDAEMON_UID=${DAEMON_UID} -DDAEMON_GID=${DAEMON_GID} \
	-DDEFAULT_BATCH_QUEUE=\'${DEFAULT_BATCH_QUEUE}\' \
	-DDEFAULT_AT_QUEUE=\'${DEFAULT_AT_QUEUE}\' -DPERM_PATH=\"${PERM_PATH}/\"

AT_SED = sed -e "s@_ATSPOOL_DIR@${ATSPOOL_DIR}@g; \
	s@_ATJOB_DIR@${ATJOB_DIR}@g; s@_ATLIB_DIR@${ATLIB_DIR}@g; \
	s@_DEFAULT_BATCH_QUEUE@${DEFAULT_BATCH_QUEUE}@g; \
	s@_DEFAULT_AT_QUEUE@${DEFAULT_AT_QUEUE}@g; \
	s@_LOADAVG_MX@${LOADAVG_MX}@g; s@_PERM_PATH@${PERM_PATH}@g; \
	s@_LOCKFILE@${LOCKFILE}@g"
