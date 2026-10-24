#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The ravynOS contributors. See ../digest.c for the
# full license text.
"""Checks cccrypt (CCCryptor, CCCrypt, PBKDF2) against the Python
cryptography package and hashlib."""

import hashlib
import random
import subprocess
import sys

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

prog = sys.argv[1]
rng = random.Random(2)
fails = checks = 0


def rc4(key, data):
    # Written out: the cryptography package takes only some RC4 key sizes.
    S, j = list(range(256)), 0
    for i in range(256):
        j = (j + S[i] + key[i % len(key)]) & 0xff
        S[i], S[j] = S[j], S[i]
    out, i, j = bytearray(), 0, 0
    for b in data:
        i = (i + 1) & 0xff
        j = (j + S[i]) & 0xff
        S[i], S[j] = S[j], S[i]
        out.append(b ^ S[(S[i] + S[j]) & 0xff])
    return bytes(out)


def reference(mode, key, iv, data, pad):
    if mode == 'rc4':
        return rc4(key, data)
    else:
        m = {'ecb': lambda: modes.ECB(), 'cbc': lambda: modes.CBC(iv),
             'ctr': lambda: modes.CTR(iv), 'cfb': lambda: modes.CFB(iv),
             'cfb8': lambda: modes.CFB8(iv), 'ofb': lambda: modes.OFB(iv)}[mode]()
        cipher = Cipher(algorithms.AES(key), m, default_backend())
    if pad:
        p = padding.PKCS7(128).padder()
        data = p.update(data) + p.finalize()
    e = cipher.encryptor()
    return e.update(data) + e.finalize()


def run(args, data):
    return subprocess.run([prog] + args, input=data, capture_output=True)


def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print('FAIL ' + what)


sizes = [0, 1, 15, 16, 17, 31, 32, 33, 100, 255, 256, 1000, 4097]
for mode in ['ecb', 'cbc', 'ctr', 'cfb', 'cfb8', 'ofb', 'rc4']:
    keylens = [5, 16, 40, 256] if mode == 'rc4' else [16, 24, 32]
    pads = [0, 1] if mode in ('ecb', 'cbc') else [0]
    for keylen in keylens:
        for pad in pads:
            for size in sizes:
                if mode in ('ecb', 'cbc') and not pad and size % 16:
                    continue
                key = rng.randbytes(keylen)
                iv = rng.randbytes(16) if mode not in ('ecb', 'rc4') else b''
                data = rng.randbytes(size)
                want = reference(mode, key, iv, data, pad)
                args = [mode, key.hex(), iv.hex(), str(pad)]
                enc = run(['enc'] + args, data)
                what = '%s key=%d pad=%d size=%d' % (mode, keylen, pad, size)
                check(enc.returncode == 0 and enc.stdout == want,
                      'enc ' + what + ' ' + enc.stderr.decode().strip())
                dec = run(['dec'] + args, want)
                check(dec.returncode == 0 and dec.stdout == data,
                      'dec ' + what + ' ' + dec.stderr.decode().strip())

# Bad padding and misaligned input must fail.
key, iv = rng.randbytes(16), rng.randbytes(16)
bad = reference('cbc', key, iv, b'x' * 15 + b'\x03', 0)   # last byte 3, not 3 3 3
r = run(['dec', 'cbc', key.hex(), iv.hex(), '1'], bad)
check(r.returncode != 0 and b'final: -4304' in r.stderr, 'bad padding: %r' % r.stderr)
r = run(['enc', 'cbc', key.hex(), iv.hex(), '0'], b'y' * 20)
check(r.returncode != 0 and b'final: -4303' in r.stderr, 'misaligned: %r' % r.stderr)
r = run(['enc', 'cbc', '00' * 15, iv.hex(), '0'], b'')
check(r.returncode != 0 and b'create: -4310' in r.stderr, 'key size: %r' % r.stderr)

# PBKDF2 (RFC 6070 vector included: "password"/"salt", 2 rounds, SHA-1).
prfs = {1: 'sha1', 2: 'sha224', 3: 'sha256', 4: 'sha384', 5: 'sha512'}
cases = [(b'password', b'salt', 1, 2, 20)] + [
    (rng.randbytes(rng.randint(0, 80)), rng.randbytes(rng.randint(0, 40)),
     prf, rng.choice([1, 2, 7, 1000]), rng.choice([1, 16, 32, 65, 130]))
    for prf in prfs for _ in range(6)]
for pw, salt, prf, rounds, dklen in cases:
    want = hashlib.pbkdf2_hmac(prfs[prf], pw, salt, rounds, dklen)
    r = run(['pbkdf2', str(prf), str(rounds), salt.hex(), str(dklen)], pw)
    check(r.returncode == 0 and r.stdout == want,
          'pbkdf2 %s rounds=%d len=%d' % (prfs[prf], rounds, dklen))

print('%d checks, %d failures' % (checks, fails))
sys.exit(1 if fails else 0)
