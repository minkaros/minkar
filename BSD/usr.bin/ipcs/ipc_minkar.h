/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What ipcs and ipcrm need on Minkar.  XNU has no kvm(3) and no
 * kern.ipc.* tables; it hands out its System V IPC limits and entries
 * one at a time through the kern.sysv.ipcs.{shm,sem,msg} sysctls, which
 * take a command in and give it back with the cursor moved on.  kget()
 * in ipc.c turns that into the tables FreeBSD's code reads.
 */

#ifndef _IPC_MINKAR_H_
#define _IPC_MINKAR_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

/* The command the kern.sysv.ipcs.* sysctls take (a 64-bit process's). */
struct ipcs_command {
	int	ipcs_magic;
	int	ipcs_op;
	int	ipcs_cursor;
	int	ipcs_datalen;
	void	*ipcs_data;
};
#define	IPCS_MAGIC	0x00000001
#define	IPCS_SHM_CONF	0x00000001
#define	IPCS_SHM_ITER	0x00000002
#define	IPCS_SEM_CONF	0x00000010
#define	IPCS_SEM_ITER	0x00000020
#define	IPCS_MSG_CONF	0x00000100
#define	IPCS_MSG_ITER	0x00000200

/* XNU's limits, laid out as the kernel copies them out. */
struct shminfo {
	u_long	shmmax, shmmin, shmmni, shmseg, shmall;
};
struct seminfo {
	int	semmap, semmni, semmns, semmnu, semmsl, semopm, semume,
		semusz, semvmx, semaem;
};

/* One table entry: the descriptor the kernel gives out for each. */
struct shmid_kernel { struct shmid_ds u; };
struct semid_kernel { struct semid_ds u; };
struct msqid_kernel { struct msqid_ds u; };

/* An entry's flags in its mode, and how XNU makes an IPC ID. */
#define	SEM_ALLOC		01000
#define	IXSEQ_TO_IPCID(ix, perm) (((perm)._seq << 16L) | ((ix) & 0xffff))
#define	IPC_KEY(perm)		((perm)._key)

/*
 * XNU's IPCS_SEM_CONF gives how many identifiers, semaphores and undo
 * structures are allocated now; this puts the limits (kern.sysv.*) there.
 */
void	sem_limits(struct seminfo *);

#endif /* _IPC_MINKAR_H_ */
