/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * crypt(3): as on macOS, traditional DES hashes (gen/crypt.c, built with
 * its function named __crypt_des); in addition, the "$5$" (SHA-256) and
 * "$6$" (SHA-512) hashes of FreeBSD's libcrypt (BSD/lib/libcrypt), so
 * /etc/master.passwd can hold modern hashes for PAM's pam_unix.
 */

#include <sys/types.h>		/* u_long, for "crypt.h" */

#include <string.h>
#include <unistd.h>

#include "crypt.h"		/* BSD/lib/libcrypt: crypt_sha256, crypt_sha512 */

extern char *__crypt_des(const char *key, const char *setting);

/* Room for "$6$rounds=999999999$" + 16 salt + "$" + 86 hash characters. */
#define CRYPT_BUFSIZE	256

char *
crypt(const char *key, const char *setting)
{
	static char buf[CRYPT_BUFSIZE];

	if (key == NULL || setting == NULL)
		return NULL;
	if (strncmp(setting, "$6$", 3) == 0)
		return crypt_sha512(key, setting, buf) == 0 ? buf : NULL;
	if (strncmp(setting, "$5$", 3) == 0)
		return crypt_sha256(key, setting, buf) == 0 ? buf : NULL;
	return __crypt_des(key, setting);
}
