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
 * Host test of crypt(3)'s SHA-2 and SHA-crypt code: NIST digests, and
 * hashes made by OpenSSL ("openssl passwd -5/-6") and glibc's crypt(3).
 */

#include <stdio.h>
#include <string.h>

#include "sha256.h"
#include "sha512.h"

char *crypt(const char *key, const char *setting);

/* Stands in for libc's DES crypt, which this test does not cover. */
char *__crypt_des(const char *key, const char *setting);
char *
__crypt_des(const char *key, const char *setting)
{
	(void)key;
	(void)setting;
	return (char *)"des";
}

static int failures;

static void
hex(const unsigned char *d, size_t n, char *out)
{
	for (size_t i = 0; i < n; i++)
		sprintf(out + 2 * i, "%02x", d[i]);
}

static void
check(const char *what, const char *got, const char *want)
{
	if (got == NULL || strcmp(got, want) != 0) {
		printf("FAIL %s\n  got  %s\n  want %s\n", what, got ? got : "(null)", want);
		failures++;
	}
}

static void
digests(const char *name, const char *msg, size_t repeat, const char *want256,
    const char *want512)
{
	SHA256_CTX c256;
	SHA512_CTX c512;
	unsigned char d256[32], d512[64];
	char h[129];

	SHA256_Init(&c256);
	SHA512_Init(&c512);
	for (size_t i = 0; i < repeat; i++) {
		SHA256_Update(&c256, msg, strlen(msg));
		SHA512_Update(&c512, msg, strlen(msg));
	}
	SHA256_Final(d256, &c256);
	SHA512_Final(d512, &c512);
	hex(d256, 32, h);
	check(name, h, want256);
	hex(d512, 64, h);
	check(name, h, want512);
}

static const struct {
	const char *key, *setting, *want;
} hashes[] = {
	{ "Hello world!", "$5$saltstring", "$5$saltstring$5B8vYYiY.CVt1RlTTf8KbXBH3hsxY/GNooZaBBGWEc5" },
	{ "", "$5$abcdefgh", "$5$abcdefgh$mnv0N8gJGuJiQCFVlADjwwxPyRiUO8rGuljjETTLqw9" },
	{ "a much longer password with spaces and \xc3\xb1 unicode, beyond sixty-four bytes in total!!", "$5$x", "$5$x$i0cM73bxKQsoPZHrjgpjBDacDPsesBj70E2A7hVvraA" },
	{ "pw", "$5$toolongsaltstringIsTruncated", "$5$toolongsaltstrin$Y3vz9Yc/Xx.o6DLddrkbpSLCVwMU7/nP4IbKEArcEi9" },
	{ "Hello world!", "$5$rounds=10000$saltstringsaltstring", "$5$rounds=10000$saltstringsaltst$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA" },
	{ "Hello world!", "$6$saltstring", "$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1" },
	{ "", "$6$abcdefgh", "$6$abcdefgh$v7sYNA18/BerGOYQLppYLyjH4yJilp8kqe/ef3KYMK9hOIdzH1yzcmP74Ay.m51y1jP3QqxM7Jl75S4CxDhBq." },
	{ "a much longer password with spaces and \xc3\xb1 unicode, beyond sixty-four bytes in total!!", "$6$x", "$6$x$PRtsEjBKvd1Q1LX0e4zuwL37gXMycBlgqTwG8vV94fFA.tUgg0B751EdFoPMySLsA7Alm1sHBteSpjeU5OVZT." },
	{ "pw", "$6$toolongsaltstringIsTruncated", "$6$toolongsaltstrin$HVORu0hdHsOheLM1WO9hf3tacVjHKywcUz1aTsZ5hYxepcv6cJl00D6EYvHRP50Nvzlq8DxhE70/.e8qOYWuN/" },
	{ "Hello world!", "$6$rounds=10000$saltstringsaltstring", "$6$rounds=10000$saltstringsaltst$OW1/O6BYHV6BcXZu8QVeXbDWra3Oeqh0sbHbbMCVNSnCM/UrjmM0Dp8vOuZeHBy/YTBmSK6H9qs/y3RnOaw5v." },
	/* Anything else is a DES setting, as on macOS. */
	{ "pw", "ab", "des" },
};

int
main(void)
{
	digests("empty", "", 1,
	    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
	    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
	    "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
	digests("abc", "abc", 1,
	    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
	    "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
	    "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
	digests("448 bits", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
	    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
	    "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
	    "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445");
	digests("million a", "a", 1000000,
	    "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
	    "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
	    "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");

	for (size_t i = 0; i < sizeof(hashes) / sizeof(hashes[0]); i++)
		check(hashes[i].setting, crypt(hashes[i].key, hashes[i].setting), hashes[i].want);

	printf("%s: %zu crypt checks, 8 digests, %d failures\n", failures ? "FAIL" : "PASS",
	    sizeof(hashes) / sizeof(hashes[0]), failures);
	return failures != 0;
}
