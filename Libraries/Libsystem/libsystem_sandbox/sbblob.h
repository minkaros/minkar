/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A compiled sandbox profile, as libsystem_sandbox hands it to the
 * kernel's sandbox policy (MinkarSandbox) and both evaluate it: a default
 * action and rules, each an operation, an action and a filter. The last
 * rule that matches an operation decides; none matching, the default does.
 *
 * Layout (little endian, offsets from the start):
 *   header, rules[nrules], filter bytes, NUL-terminated strings.
 * Filters are a prefix encoding: a type byte, then its operands.
 *
 * Shared by userland and the kernel: plain C, also valid C++.
 */

#ifndef MINKAR_SBBLOB_H
#define MINKAR_SBBLOB_H

#include <stdint.h>
#include <stddef.h>

#define SB_MAGIC	0x42534b4du	/* "MKSB" */
#define SB_VERSION	1
#define SB_MAX_BLOB	(1u << 20)

/* The policy's name for __mac_syscall, and its calls. */
#define SB_POLICY_NAME	"MinkarSandbox"
enum {
	SB_CALL_SET_PROFILE = 1,	/* struct sb_set_profile_args */
	SB_CALL_CHECK = 2,		/* struct sb_check_args */
};

enum { SB_DENY = 0, SB_ALLOW = 1 };

/* Rule flags (from (with ...)). */
enum { SB_FLAG_NO_LOG = 1 << 0, SB_FLAG_REPORT = 1 << 1 };

enum sb_filter_type {
	SBF_TRUE = 1,
	SBF_ALL = 2,		/* u16 count, children */
	SBF_ANY = 3,		/* u16 count, children */
	SBF_NOT = 4,		/* child */
	SBF_PATH = 5,		/* u8 match, u32 string */
	SBF_NAME = 6,		/* u8 kind, u8 match, u32 string */
	SBF_INT = 7,		/* u8 kind, u32 value */
	SBF_ADDR = 8,		/* u8 kind (local/remote), u32 string */
	SBF_NEVER = 9,		/* a filter this system does not know */
};

/* How a string operand matches. */
enum { SBM_LITERAL = 1, SBM_SUBPATH = 2, SBM_PREFIX = 3, SBM_REGEX = 4 };

/* SBF_NAME kinds: what the name is. */
enum {
	SBN_GLOBAL_NAME = 1,		/* a Mach service (mach-lookup, mach-register) */
	SBN_LOCAL_NAME,
	SBN_SYSCTL_NAME,
	SBN_IPC_POSIX_NAME,
	SBN_IOKIT_USER_CLIENT_CLASS,
	SBN_IOKIT_PROPERTY,
	SBN_IOKIT_REGISTRY_ENTRY_CLASS,
	SBN_IOKIT_CONNECTION,
	SBN_PREFERENCE_DOMAIN,
	SBN_XPC_SERVICE_NAME,
	SBN_CONTROL_NAME,
	SBN_EXTENSION,
	SBN_EXTENSION_CLASS,
	SBN_SIGNAL_NAME,
	SBN_KINDS
};

/* SBF_INT kinds. */
enum {
	SBI_VNODE_TYPE = 1,
	SBI_TARGET,
	SBI_SOCKET_DOMAIN,
	SBI_SOCKET_TYPE,
	SBI_SOCKET_PROTOCOL,
	SBI_FILE_MODE,
	SBI_KINDS
};

/* vnode-type values. */
enum { SBV_REGULAR_FILE = 1, SBV_DIRECTORY, SBV_SYMLINK, SBV_CHARACTER_DEVICE,
	SBV_BLOCK_DEVICE, SBV_FIFO, SBV_SOCKET };

/* target values (signal, process-info). */
enum { SBT_SELF = 1, SBT_OTHERS, SBT_CHILDREN, SBT_PGRP, SBT_SAME_SANDBOX };

/* SBF_ADDR kinds. */
enum { SBA_LOCAL = 1, SBA_REMOTE = 2 };

struct sb_header {
	uint32_t magic;
	uint32_t version;
	uint32_t size;			/* the whole blob */
	uint32_t default_action;
	uint32_t default_flags;
	uint32_t nrules;
	uint32_t rules_offset;
	uint32_t filters_offset;
	uint32_t strings_offset;
};

struct sb_rule {
	uint32_t operation;		/* string offset: "file-read*", ... */
	uint8_t action;
	uint8_t flags;
	uint16_t reserved;
	uint32_t filter;		/* offset of its filter, or SB_NO_FILTER */
};

#define SB_NO_FILTER 0xffffffffu

/*
 * What an operation is asked about. Fields an operation does not have
 * are 0 / NULL; a filter on a field the query lacks does not match.
 */
struct sb_query {
	const char *path;		/* file operations */
	int vnode_type;			/* SBV_*, 0 unknown */
	const char *names[SBN_KINDS];	/* by SBN_* kind */
	int ints[SBI_KINDS];		/* by SBI_* kind; 0 unknown */
	int has_ints[SBI_KINDS];
	const char *local_addr;		/* "ip host:port" / "unix path" */
	const char *remote_addr;
	void *regex_workspace;		/* sb_regex_workspace_size() bytes, or NULL (userland) */
};

/* sb_set_profile_args: a blob for the calling process (and what it
   starts afterwards). sb_check_args: would this process (pid) be allowed
   an operation on one name or path? */
struct sb_set_profile_args {
	uint64_t blob;			/* user address */
	uint64_t size;
};

struct sb_check_args {
	int32_t pid;
	int32_t name_kind;		/* SBN_*, or 0 for a path */
	uint64_t operation;		/* user address of a C string */
	uint64_t name;			/* user address of a C string, or 0 */
	int32_t result;			/* out: SB_ALLOW / SB_DENY */
	int32_t sandboxed;		/* out: the process has a profile */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the blob is well formed (offsets and strings inside it). */
int sb_blob_valid(const uint8_t *blob, size_t size);

/* The action for an operation ("file-read-data") and its query; *flags
   gets the deciding rule's flags. */
int sb_evaluate(const uint8_t *blob, const char *operation, const struct sb_query *query, int *flags);

/* POSIX extended regular expressions (the profiles' #"..."), searched
   in the subject: 1 matches, 0 not, -1 the pattern is invalid. */
int sb_regex_search(const char *pattern, const char *subject);
size_t sb_regex_workspace_size(void);
int sb_regex_search_ws(const char *pattern, const char *subject, void *workspace);

#ifdef __cplusplus
}
#endif

#endif
