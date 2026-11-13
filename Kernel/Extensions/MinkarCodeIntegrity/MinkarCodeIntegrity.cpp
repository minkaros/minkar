/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The code integrity policy (Docs/ADR/0001-Kernel-Entitlements.md).
 *
 * The kernel gives every well-formed code signature CS_VALID and then asks
 * the MAC policies about it (mac_vnode_check_signature). This policy adds
 * CS_SIGNED, so the kernel reads the signature's entitlements, and
 * CS_PLATFORM_BINARY when the CodeDirectory's hash is in the static trust
 * cache that the image build writes. Entitlements are restricted except
 * for com.apple.security.* and, decided on 13 Nov 2026 for third-party
 * apps, the identity and provisioning profile ones
 * (com.apple.application-identifier, com.apple.developer.*): those let a
 * validly signed app start but give it no privilege, as no Minkar service
 * grants access on them (their profile is not checked yet). A signature
 * outside the trust cache that carries a restricted one is refused, and
 * its exec fails.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSUnserialize.h>
#include <mach/kmod.h>

extern "C" {
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/codesign.h>
#include <kern/cs_blobs.h>
#include <kern/trustcache.h>
#include <security/mac_policy.h>
}

/*
 * The kernel frees a fatal failure description with kfree(desc, len)
 * (mac_vnode_check_signature), so it is allocated with kalloc.
 */
extern "C" void *kalloc(vm_size_t size);

#define LOG(fmt, ...)	IOLog("MinkarCodeIntegrity: " fmt "\n", ##__VA_ARGS__)

#define TRUST_CACHE_PATH	"/System/Library/Security/TrustCache"
#define TRUST_CACHE_MAX		(8 * 1024 * 1024)
#define ENTITLEMENTS_MAX	(256 * 1024)
/* Entitlements any valid signature may carry: by prefix, or whole. */
static const char *const kUnrestrictedPrefixes[] = {
	"com.apple.security.",
	"com.apple.developer.",
};
static const char *const kUnrestrictedNames[] = {
	"com.apple.application-identifier",
};

static bool
is_unrestricted(const char *name)
{
	for (size_t i = 0; i < sizeof(kUnrestrictedPrefixes) / sizeof(kUnrestrictedPrefixes[0]); i++) {
		if (strncmp(name, kUnrestrictedPrefixes[i], strlen(kUnrestrictedPrefixes[i])) == 0) {
			return true;
		}
	}
	for (size_t i = 0; i < sizeof(kUnrestrictedNames) / sizeof(kUnrestrictedNames[0]); i++) {
		if (strcmp(name, kUnrestrictedNames[i]) == 0) {
			return true;
		}
	}
	return false;
}

static IOLock *sLock;
static struct trust_cache_module1 *sCache;	/* NULL until loaded */
static uint32_t sCacheSize;
static bool sCacheFailed;			/* logged once */
static mac_policy_handle_t sHandle;

/* Reads and checks the trust cache. Called with sLock held. */
static int
load_trust_cache(void)
{
	vfs_context_t ctx = vfs_context_current();
	vnode_t vp = NULLVP;
	struct vnode_attr va;
	struct trust_cache_module1 *module = NULL;
	uint32_t size = 0;
	int resid = 0;
	int error;

	error = vnode_open(TRUST_CACHE_PATH, FREAD, 0, 0, &vp, ctx);
	if (error) {
		return error;
	}
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	error = vnode_getattr(vp, &va, ctx);
	if (error) {
		goto out;
	}
	if (va.va_data_size < sizeof(*module) || va.va_data_size > TRUST_CACHE_MAX) {
		error = EFBIG;
		goto out;
	}
	size = (uint32_t)va.va_data_size;
	module = (struct trust_cache_module1 *)IOMalloc(size);
	if (module == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = vn_rdwr(UIO_READ, vp, (caddr_t)module, (int)size, 0, UIO_SYSSPACE,
	    0, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
	if (error == 0 && resid != 0) {
		error = EIO;
	}
	if (error) {
		goto out;
	}

	/* Version 1, the size its entry count says, sorted with no repeats. */
	if (module->version != 1
	    || module->num_entries > (size - sizeof(*module)) / sizeof(module->entries[0])
	    || sizeof(*module) + module->num_entries * sizeof(module->entries[0]) != size) {
		error = EINVAL;
		goto out;
	}
	for (uint32_t i = 1; i < module->num_entries; i++) {
		if (memcmp(module->entries[i - 1].cdhash, module->entries[i].cdhash,
		    CS_CDHASH_LEN) >= 0) {
			error = EINVAL;
			goto out;
		}
	}

	sCache = module;
	sCacheSize = size;
	module = NULL;
	LOG("trust cache loaded, %u entries", sCache->num_entries);
out:
	if (module != NULL) {
		IOFree(module, size);
	}
	vnode_close(vp, FREAD, ctx);
	return error;
}

static bool
in_trust_cache(const uint8_t *cdhash, uint8_t hash_type)
{
	struct trust_cache_module1 const *module;
	uint32_t low = 0, high;

	IOLockLock(sLock);
	if (sCache == NULL) {
		int error = load_trust_cache();
		if (error && !sCacheFailed) {
			sCacheFailed = true;
			LOG("cannot load %s (error %d): no binary is a platform binary",
			    TRUST_CACHE_PATH, error);
		}
	}
	module = sCache;
	IOLockUnlock(sLock);
	if (module == NULL || cdhash == NULL) {
		return false;
	}

	/* The cache is never freed once loaded, so no lock is needed here. */
	high = module->num_entries;
	while (low < high) {
		uint32_t mid = low + (high - low) / 2;
		int order = memcmp(module->entries[mid].cdhash, cdhash, CS_CDHASH_LEN);
		if (order == 0) {
			return module->entries[mid].hash_type == hash_type;
		}
		if (order < 0) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	return false;
}

/*
 * The first restricted entitlement of the signature, copied into name.
 * Returns 0 and an empty name when there is none, or an error when the
 * entitlements cannot be read.
 */
static int
find_restricted_entitlement(struct cs_blob *blob, char *name, size_t namelen)
{
	void *start = NULL;
	size_t length = 0;
	char *xml = NULL;
	OSObject *obj = NULL;
	OSDictionary *dict;
	OSCollectionIterator *iter = NULL;
	OSString *errorString = NULL;
	OSSymbol const *key;
	int error;

	name[0] = '\0';
	error = csblob_get_entitlements(blob, &start, &length);
	if (error || start == NULL) {
		return error;
	}
	if (length <= offsetof(CS_GenericBlob, data)) {
		return EBADEXEC;
	}
	length -= offsetof(CS_GenericBlob, data);
	if (length > ENTITLEMENTS_MAX) {
		return E2BIG;
	}

	/* OSUnserializeXML wants a terminated string. */
	xml = (char *)IOMalloc(length + 1);
	if (xml == NULL) {
		return ENOMEM;
	}
	memcpy(xml, ((CS_GenericBlob *)start)->data, length);
	xml[length] = '\0';
	obj = OSUnserializeXML(xml, length + 1, &errorString);
	IOFree(xml, length + 1);
	if (errorString != NULL) {
		errorString->release();
	}
	dict = OSDynamicCast(OSDictionary, obj);
	if (dict == NULL) {
		error = EBADEXEC;
		goto out;
	}
	iter = OSCollectionIterator::withCollection(dict);
	if (iter == NULL) {
		error = ENOMEM;
		goto out;
	}
	while ((key = OSDynamicCast(OSSymbol, iter->getNextObject())) != NULL) {
		if (!is_unrestricted(key->getCStringNoCopy())) {
			strlcpy(name, key->getCStringNoCopy(), namelen);
			break;
		}
	}
out:
	if (iter != NULL) {
		iter->release();
	}
	if (obj != NULL) {
		obj->release();
	}
	return error;
}

static int
check_signature(struct vnode *vp, struct label *label __unused,
    cpu_type_t cpu_type __unused, struct cs_blob *blob, unsigned int *cs_flags,
    unsigned int *signer_type __unused, int flags __unused,
    char **fatal_failure_desc, size_t *fatal_failure_desc_len)
{
	char entitlement[128];
	bool platform;
	int error;

	platform = in_trust_cache(csblob_get_cdhash(blob), csblob_get_hashtype(blob));
	if (!platform) {
		error = find_restricted_entitlement(blob, entitlement, sizeof(entitlement));
		if (error || entitlement[0] != '\0') {
			char path[MAXPATHLEN];
			int pathlen = sizeof(path);
			if (vn_getpath(vp, path, &pathlen) != 0) {
				strlcpy(path, "(unknown path)", sizeof(path));
			}
			char reason[256];
			if (error) {
				snprintf(reason, sizeof(reason),
				    "its entitlements cannot be read (error %d)", error);
			} else {
				snprintf(reason, sizeof(reason),
				    "restricted entitlement %s outside the trust cache", entitlement);
			}
			LOG("refused %s: %s", path, reason);
			/*
			 * A fatal failure makes the exec fail (the image activator
			 * checks ip_cs_error); a plain error would only leave the
			 * binary unsigned, since signatures are not enforced.
			 */
			size_t len = strlen(reason) + 1;
			char *desc = (char *)kalloc(len);
			if (desc != NULL) {
				memcpy(desc, reason, len);
				*fatal_failure_desc = desc;
				*fatal_failure_desc_len = len;
			}
			return EPERM;
		}
	}

	*cs_flags |= CS_SIGNED;
	if (platform) {
		*cs_flags |= CS_PLATFORM_BINARY;
	}
	return 0;
}

static struct mac_policy_ops sOps;

extern "C" kern_return_t MinkarCodeIntegrity_start(kmod_info_t *ki, void *data);
extern "C" kern_return_t MinkarCodeIntegrity_stop(kmod_info_t *ki, void *data);

static struct mac_policy_conf sConf = {
	.mpc_name = "MinkarCodeIntegrity",
	.mpc_fullname = "Minkar code integrity policy",
	.mpc_labelnames = NULL,
	.mpc_labelname_count = 0,
	.mpc_ops = &sOps,
	/* Not unloadable: signatures already marked depend on it. */
	.mpc_loadtime_flags = 0,
	.mpc_field_off = NULL,
	.mpc_runtime_flags = 0,
	.mpc_list = NULL,
	.mpc_data = NULL,
};

extern "C" kern_return_t
MinkarCodeIntegrity_start(kmod_info_t *ki __unused, void *data __unused)
{
	int error;

	sLock = IOLockAlloc();
	if (sLock == NULL) {
		return KERN_FAILURE;
	}
	sOps.mpo_vnode_check_signature = check_signature;
	error = mac_policy_register(&sConf, &sHandle, data);
	if (error) {
		LOG("mac_policy_register failed: %d", error);
		IOLockFree(sLock);
		sLock = NULL;
		return KERN_FAILURE;
	}
	LOG("registered");
	return KERN_SUCCESS;
}

extern "C" kern_return_t
MinkarCodeIntegrity_stop(kmod_info_t *ki __unused, void *data __unused)
{
	return KERN_FAILURE;
}
