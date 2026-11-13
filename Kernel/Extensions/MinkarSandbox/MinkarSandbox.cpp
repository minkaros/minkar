/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The sandbox policy (decided 13 Nov 2026: Minkar's own, in phases).
 *
 * A process gives itself a compiled profile (libsystem_sandbox's
 * sandbox_init, through __mac_syscall); it is kept in a label slot of
 * the process's credential, so the processes it starts keep it, across
 * exec too, and it cannot be taken away. The operations checked here are
 * evaluated against it with the same code as userland's (sbeval.c,
 * sbregex.c): files (open, create, unlink, rename, metadata, attributes,
 * extended attributes, links, exec), the network (connect, bind, listen,
 * socket creation), sysctls and signals. Mach service lookups are checked
 * by launchd, which asks here (sandbox_check). A process without a
 * profile is never checked.
 */

#include <IOKit/IOLib.h>
#include <mach/kmod.h>

extern "C" {
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ucred.h>
#include <sys/systm.h>
#include <netinet/in.h>
#include <security/mac.h>
#include <security/mac_policy.h>
#include <security/_label.h>
#include <libkern/OSAtomic.h>
}

#include "sbblob.h"

#define LOG(fmt, ...) printf("MinkarSandbox: " fmt "\n", ##__VA_ARGS__)

/* A profile: shared by the credentials that carry it. */
struct Profile {
	volatile SInt32 refs;
	uint32_t size;
	uint8_t blob[1];
};

static mac_policy_handle_t sHandle;
static int sSlot = -1;

static Profile *profile_retain(Profile *p) {
	if (p != NULL)
		OSIncrementAtomic(&p->refs);
	return p;
}

static void profile_release(Profile *p) {
	if (p != NULL && OSDecrementAtomic(&p->refs) == 1)
		IOFree(p, offsetof(Profile, blob) + p->size);
}

static Profile *label_profile(struct label *l) {
	return l != NULL ? (Profile *)mac_label_get(l, sSlot) : NULL;
}

static Profile *cred_profile(kauth_cred_t cred) {
	return cred != NULL ? label_profile(cred->cr_label) : NULL;
}

/* ---- Labels: the profile goes with the credential ---- */

static void cred_label_init(struct label *label) {
	mac_label_set(label, sSlot, 0);
}

static void cred_label_destroy(struct label *label) {
	profile_release(label_profile(label));
	mac_label_set(label, sSlot, 0);
}

static void cred_label_associate(kauth_cred_t parent, kauth_cred_t child) {
	Profile *p = profile_retain(cred_profile(parent));
	mac_label_set(child->cr_label, sSlot, (intptr_t)p);
}

/* kauth_proc_label_update passes a label holding only the new profile. */
static void cred_label_update(kauth_cred_t cred, struct label *newlabel) {
	Profile *p = label_profile(newlabel);
	if (p == NULL)
		return;
	Profile *old = cred_profile(cred);
	mac_label_set(cred->cr_label, sSlot, (intptr_t)profile_retain(p));
	profile_release(old);
}

/* ---- Evaluating ---- */

struct Check {
	char path[MAXPATHLEN];
	char addr[MAXPATHLEN + 8];
	struct sb_query query;
	uint8_t workspace[1];	/* sb_regex_workspace_size() bytes */
};

static Check *check_alloc(void) {
	size_t size = offsetof(Check, workspace) + sb_regex_workspace_size();
	Check *c = (Check *)IOMalloc(size);
	if (c != NULL) {
		bzero(c, offsetof(Check, workspace));
		c->query.regex_workspace = c->workspace;
	}
	return c;
}

static void check_free(Check *c) {
	IOFree(c, offsetof(Check, workspace) + sb_regex_workspace_size());
}

/* The action; a denial is logged unless the rule says not to. */
static int decide(Profile *p, const char *operation, Check *c) {
	int flags = 0;
	int action = sb_evaluate(p->blob, operation, &c->query, &flags);
	if (action == SB_ALLOW)
		return 0;
	if (!(flags & SB_FLAG_NO_LOG)) {
		char name[32];
		proc_selfname(name, sizeof(name));
		const char *what = c->query.path;
		for (int k = 1; what == NULL && k < SBN_KINDS; k++)
			what = c->query.names[k];
		if (what == NULL)
			what = c->query.remote_addr != NULL ? c->query.remote_addr : c->query.local_addr;
		LOG("%s(%d) deny %s %s", name, proc_selfpid(), operation, what != NULL ? what : "");
	}
	return EPERM;
}

static int vnode_kind(vnode_t vp) {
	switch (vnode_vtype(vp)) {
	case VREG: return SBV_REGULAR_FILE;
	case VDIR: return SBV_DIRECTORY;
	case VLNK: return SBV_SYMLINK;
	case VCHR: return SBV_CHARACTER_DEVICE;
	case VBLK: return SBV_BLOCK_DEVICE;
	case VFIFO: return SBV_FIFO;
	case VSOCK: return SBV_SOCKET;
	default: return 0;
	}
}

/* A file operation on a vnode (or on a name in a directory). */
/*
 * /etc, /tmp and /var are the system's links into /private: every path
 * through them follows one, and the kernel asks to read the link (as
 * readlink) each time. Their metadata is always allowed, as on the
 * reference platform; what they lead to is checked as itself.
 */
static bool is_system_link(const char *operation, const char *path, vnode_t vp) {
	if (strcmp(operation, "file-read-metadata") != 0 || vnode_vtype(vp) != VLNK)
		return false;
	return strcmp(path, "/etc") == 0 || strcmp(path, "/tmp") == 0 || strcmp(path, "/var") == 0;
}

static int check_vnode(kauth_cred_t cred, const char *operation, vnode_t vp, struct componentname *cnp) {
	Profile *p = cred_profile(cred);
	if (p == NULL || vp == NULL)
		return 0;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	int len = MAXPATHLEN;
	int error = 0;
	if (vn_getpath(vp, c->path, &len) != 0)
		c->path[0] = '\0';
	if (cnp != NULL) {
		size_t n = strlen(c->path);
		if (n + 1 + (size_t)cnp->cn_namelen + 1 > sizeof(c->path))
			error = ENAMETOOLONG;
		else {
			if (n == 0 || c->path[n - 1] != '/')
				c->path[n++] = '/';
			memcpy(c->path + n, cnp->cn_nameptr, (size_t)cnp->cn_namelen);
			c->path[n + cnp->cn_namelen] = '\0';
		}
	} else
		c->query.vnode_type = vnode_kind(vp);
	if (error == 0 && is_system_link(operation, c->path, vp))
		goto out;
	if (error == 0) {
		c->query.path = c->path;
		error = decide(p, operation, c);
	}
out:
	check_free(c);
	return error;
}

static int vnode_check_open(kauth_cred_t cred, vnode_t vp, struct label *, int acc_mode) {
	int error = 0;
	if (acc_mode & FREAD)
		error = check_vnode(cred, "file-read-data", vp, NULL);
	if (error == 0 && (acc_mode & (FWRITE | O_APPEND | O_TRUNC)))
		error = check_vnode(cred, "file-write-data", vp, NULL);
	return error;
}

static int vnode_check_create(kauth_cred_t cred, vnode_t dvp, struct label *, struct componentname *cnp,
    struct vnode_attr *) {
	return check_vnode(cred, "file-write-create", dvp, cnp);
}

static int vnode_check_unlink(kauth_cred_t cred, vnode_t, struct label *, vnode_t vp, struct label *,
    struct componentname *) {
	return check_vnode(cred, "file-write-unlink", vp, NULL);
}

static int vnode_check_rename(kauth_cred_t cred, vnode_t, struct label *, vnode_t vp, struct label *,
    struct componentname *, vnode_t tdvp, struct label *, vnode_t, struct label *, struct componentname *tcnp) {
	int error = check_vnode(cred, "file-write-unlink", vp, NULL);
	if (error == 0)
		error = check_vnode(cred, "file-write-create", tdvp, tcnp);
	return error;
}

static int vnode_check_link(kauth_cred_t cred, vnode_t dvp, struct label *, vnode_t vp, struct label *,
    struct componentname *cnp) {
	int error = check_vnode(cred, "file-link", vp, NULL);
	if (error == 0)
		error = check_vnode(cred, "file-write-create", dvp, cnp);
	return error;
}

static int vnode_check_stat(struct ucred *active, struct ucred *, vnode_t vp, struct label *) {
	return check_vnode(active, "file-read-metadata", vp, NULL);
}

static int vnode_check_getattrlist(kauth_cred_t cred, vnode_t vp, struct label *, struct attrlist *) {
	return check_vnode(cred, "file-read-metadata", vp, NULL);
}

static int vnode_check_readlink(kauth_cred_t cred, vnode_t vp, struct label *) {
	return check_vnode(cred, "file-read-metadata", vp, NULL);
}

static int vnode_check_readdir(kauth_cred_t cred, vnode_t dvp, struct label *) {
	return check_vnode(cred, "file-read-data", dvp, NULL);
}

static int vnode_check_getextattr(kauth_cred_t cred, vnode_t vp, struct label *, const char *, struct uio *) {
	return check_vnode(cred, "file-read-xattr", vp, NULL);
}

static int vnode_check_setextattr(kauth_cred_t cred, vnode_t vp, struct label *, const char *, struct uio *) {
	return check_vnode(cred, "file-write-xattr", vp, NULL);
}

static int vnode_check_setmode(kauth_cred_t cred, vnode_t vp, struct label *, mode_t) {
	return check_vnode(cred, "file-write-mode", vp, NULL);
}

static int vnode_check_truncate(kauth_cred_t active, kauth_cred_t, vnode_t vp, struct label *) {
	return check_vnode(active, "file-write-data", vp, NULL);
}

static int vnode_check_ioctl(kauth_cred_t cred, vnode_t vp, struct label *, unsigned int) {
	return check_vnode(cred, "file-ioctl", vp, NULL);
}

static int vnode_check_exec(kauth_cred_t cred, vnode_t vp, vnode_t, struct label *, struct label *,
    struct label *, struct componentname *, u_int *, void *, size_t) {
	return check_vnode(cred, "process-exec", vp, NULL);
}

/* ---- The network ---- */

static void put_hex(char *&o, unsigned v) {
	static const char digits[] = "0123456789abcdef";
	char tmp[4];
	int n = 0;
	do {
		tmp[n++] = digits[v & 15];
		v >>= 4;
	} while (v != 0);
	while (n > 0)
		*o++ = tmp[--n];
}

/* "ip HOST:PORT" or "unix PATH", as the profiles' address filters read. */
static const char *format_address(char *buf, size_t size, const struct sockaddr *sa) {
	if (sa == NULL)
		return NULL;
	if (sa->sa_family == AF_INET) {
		const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
		const uint8_t *a = (const uint8_t *)&sin->sin_addr;
		snprintf(buf, size, "ip %u.%u.%u.%u:%u", a[0], a[1], a[2], a[3], ntohs(sin->sin_port));
		return buf;
	}
	if (sa->sa_family == AF_INET6 && size >= 64) {
		const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;
		const uint8_t *a = (const uint8_t *)&sin6->sin6_addr;
		static const uint8_t loopback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		char *o = buf;
		memcpy(o, "ip ", 3);
		o += 3;
		if (memcmp(a, loopback, 16) == 0) {
			memcpy(o, "::1", 3);
			o += 3;
		} else
			for (int i = 0; i < 8; i++) {
				if (i > 0)
					*o++ = ':';
				put_hex(o, (unsigned)(a[2 * i] << 8 | a[2 * i + 1]));
			}
		snprintf(o, size - (size_t)(o - buf), ":%u", ntohs(sin6->sin6_port));
		return buf;
	}
	if (sa->sa_family == AF_UNIX) {
		const struct sockaddr_un *sun = (const struct sockaddr_un *)sa;
		snprintf(buf, size, "unix %.*s", (int)sizeof(sun->sun_path), sun->sun_path);
		return buf;
	}
	return NULL;
}

static int check_address(kauth_cred_t cred, const char *operation, const struct sockaddr *local,
    const struct sockaddr *remote) {
	Profile *p = cred_profile(cred);
	if (p == NULL)
		return 0;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	if (remote != NULL)
		c->query.remote_addr = format_address(c->addr, sizeof(c->addr), remote);
	if (local != NULL)
		c->query.local_addr = format_address(c->addr, sizeof(c->addr), local);
	int error = decide(p, operation, c);
	check_free(c);
	return error;
}

static int socket_check_connect(kauth_cred_t cred, socket_t, struct label *, struct sockaddr *addr) {
	return check_address(cred, "network-outbound", NULL, addr);
}

static int socket_check_bind(kauth_cred_t cred, socket_t, struct label *, struct sockaddr *addr) {
	return check_address(cred, "network-bind", addr, NULL);
}

static int socket_check_listen(kauth_cred_t cred, socket_t, struct label *) {
	return check_address(cred, "network-inbound", NULL, NULL);
}

/* A socket of a system domain (kernel controls, events, routing). */
static int socket_check_create(kauth_cred_t cred, int domain, int type, int protocol) {
	Profile *p = cred_profile(cred);
	if (p == NULL || domain == AF_INET || domain == AF_INET6 || domain == AF_UNIX)
		return 0;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	c->query.ints[SBI_SOCKET_DOMAIN] = domain;
	c->query.has_ints[SBI_SOCKET_DOMAIN] = 1;
	c->query.ints[SBI_SOCKET_TYPE] = type;
	c->query.has_ints[SBI_SOCKET_TYPE] = 1;
	c->query.ints[SBI_SOCKET_PROTOCOL] = protocol;
	c->query.has_ints[SBI_SOCKET_PROTOCOL] = 1;
	int error = decide(p, "system-socket", c);
	check_free(c);
	return error;
}

/* ---- Sysctls and signals ---- */

static int system_check_sysctlbyname(kauth_cred_t cred, const char *namestring, int *, u_int, user_addr_t old,
    size_t, user_addr_t newvalue, size_t) {
	Profile *p = cred_profile(cred);
	if (p == NULL || namestring == NULL)
		return 0;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	c->query.names[SBN_SYSCTL_NAME] = namestring;
	int error = 0;
	if (old != USER_ADDR_NULL || newvalue == USER_ADDR_NULL)
		error = decide(p, "sysctl-read", c);
	if (error == 0 && newvalue != USER_ADDR_NULL)
		error = decide(p, "sysctl-write", c);
	check_free(c);
	return error;
}

static int proc_check_signal(kauth_cred_t cred, struct proc *target, int) {
	Profile *p = cred_profile(cred);
	if (p == NULL)
		return 0;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	c->query.ints[SBI_TARGET] = proc_pid(target) == proc_selfpid() ? SBT_SELF : SBT_OTHERS;
	c->query.has_ints[SBI_TARGET] = 1;
	int error = decide(p, "signal", c);
	check_free(c);
	return error;
}

/* ---- The calls of libsystem_sandbox ---- */

static int set_profile(struct proc *proc, user_addr_t arg) {
	struct sb_set_profile_args args;
	int error = copyin(arg, &args, sizeof(args));
	if (error != 0)
		return error;
	if (args.size < sizeof(struct sb_header) || args.size > SB_MAX_BLOB)
		return EINVAL;
	/* A sandboxed process cannot take another profile. */
	kauth_cred_t cred = kauth_cred_proc_ref(proc);
	Profile *current = cred_profile(cred);
	kauth_cred_unref(&cred);
	if (current != NULL)
		return EPERM;
	Profile *p = (Profile *)IOMalloc(offsetof(Profile, blob) + (size_t)args.size);
	if (p == NULL)
		return ENOMEM;
	p->refs = 1;
	p->size = (uint32_t)args.size;
	error = copyin((user_addr_t)args.blob, p->blob, (size_t)args.size);
	if (error == 0 && !sb_blob_valid(p->blob, p->size))
		error = EINVAL;
	if (error != 0) {
		profile_release(p);
		return error;
	}
	/* A label with the profile, for cred_label_update. */
	struct label *label = (struct label *)IOMalloc(sizeof(struct label));
	if (label == NULL) {
		profile_release(p);
		return ENOMEM;
	}
	bzero(label, sizeof(struct label));
	mac_label_set(label, sSlot, (intptr_t)p);
	error = kauth_proc_label_update(proc, label);
	IOFree(label, sizeof(struct label));
	profile_release(p);	/* the credential holds its own reference */
	if (error == 0) {
		char name[32];
		proc_name(proc_pid(proc), name, sizeof(name));
		LOG("%s(%d) is sandboxed", name, proc_pid(proc));
	}
	return error;
}

static int check(user_addr_t arg) {
	struct sb_check_args args;
	int error = copyin(arg, &args, sizeof(args));
	if (error != 0)
		return error;
	Check *c = check_alloc();
	if (c == NULL)
		return ENOMEM;
	char operation[64];
	size_t done;
	error = copyinstr((user_addr_t)args.operation, operation, sizeof(operation), &done);
	if (error == 0 && args.name != 0)
		error = copyinstr((user_addr_t)args.name, c->path, sizeof(c->path), &done);
	if (error == 0) {
		if (args.name == 0)
			;
		else if (args.name_kind == 0)
			c->query.path = c->path;
		else if (args.name_kind > 0 && args.name_kind < SBN_KINDS)
			c->query.names[args.name_kind] = c->path;
		args.sandboxed = 0;
		args.result = SB_ALLOW;
		proc_t target = proc_find(args.pid);
		if (target == NULL)
			error = ESRCH;
		else {
			kauth_cred_t cred = kauth_cred_proc_ref(target);
			Profile *p = cred_profile(cred);
			if (p != NULL) {
				int flags;
				args.sandboxed = 1;
				args.result = sb_evaluate(p->blob, operation, &c->query, &flags);
			}
			kauth_cred_unref(&cred);
			proc_rele(target);
			error = copyout(&args, arg, sizeof(args));
		}
	}
	check_free(c);
	return error;
}

static int policy_syscall(struct proc *p, int call, user_addr_t arg) {
	switch (call) {
	case SB_CALL_SET_PROFILE:
		return set_profile(p, arg);
	case SB_CALL_CHECK:
		return check(arg);
	default:
		return ENOSYS;
	}
}

/* ---- Registering ---- */

static struct mac_policy_ops sOps;
static const char *sLabelNames[] = { SB_POLICY_NAME };

extern "C" kern_return_t MinkarSandbox_start(kmod_info_t *ki, void *data);
extern "C" kern_return_t MinkarSandbox_stop(kmod_info_t *ki, void *data);

static struct mac_policy_conf sConf = {
	.mpc_name = SB_POLICY_NAME,
	.mpc_fullname = "Minkar sandbox policy",
	.mpc_labelnames = sLabelNames,
	.mpc_labelname_count = 1,
	.mpc_ops = &sOps,
	/* Not unloadable: credentials hold its profiles. */
	.mpc_loadtime_flags = 0,
	.mpc_field_off = &sSlot,
	.mpc_runtime_flags = 0,
	.mpc_list = NULL,
	.mpc_data = NULL,
};

extern "C" kern_return_t
MinkarSandbox_start(kmod_info_t *ki __unused, void *data)
{
	sOps.mpo_cred_label_init = cred_label_init;
	sOps.mpo_cred_label_destroy = cred_label_destroy;
	sOps.mpo_cred_label_associate = cred_label_associate;
	sOps.mpo_cred_label_update = cred_label_update;
	sOps.mpo_policy_syscall = policy_syscall;
	sOps.mpo_vnode_check_open = vnode_check_open;
	sOps.mpo_vnode_check_create = vnode_check_create;
	sOps.mpo_vnode_check_unlink = vnode_check_unlink;
	sOps.mpo_vnode_check_rename = vnode_check_rename;
	sOps.mpo_vnode_check_link = vnode_check_link;
	sOps.mpo_vnode_check_stat = vnode_check_stat;
	sOps.mpo_vnode_check_getattrlist = vnode_check_getattrlist;
	sOps.mpo_vnode_check_readlink = vnode_check_readlink;
	sOps.mpo_vnode_check_readdir = vnode_check_readdir;
	sOps.mpo_vnode_check_getextattr = vnode_check_getextattr;
	sOps.mpo_vnode_check_setextattr = vnode_check_setextattr;
	sOps.mpo_vnode_check_setmode = vnode_check_setmode;
	sOps.mpo_vnode_check_truncate = vnode_check_truncate;
	sOps.mpo_vnode_check_ioctl = vnode_check_ioctl;
	sOps.mpo_vnode_check_exec = vnode_check_exec;
	sOps.mpo_socket_check_connect = socket_check_connect;
	sOps.mpo_socket_check_bind = socket_check_bind;
	sOps.mpo_socket_check_listen = socket_check_listen;
	sOps.mpo_socket_check_create = socket_check_create;
	sOps.mpo_system_check_sysctlbyname = system_check_sysctlbyname;
	sOps.mpo_proc_check_signal = proc_check_signal;
	int error = mac_policy_register(&sConf, &sHandle, data);
	if (error) {
		LOG("mac_policy_register failed: %d", error);
		return KERN_FAILURE;
	}
	LOG("registered (label slot %d)", sSlot);
	return KERN_SUCCESS;
}

extern "C" kern_return_t
MinkarSandbox_stop(kmod_info_t *ki __unused, void *data __unused)
{
	return KERN_FAILURE;
}
