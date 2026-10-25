#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors. See ../digest.c for the
# full license text.
"""Checks ccdigest against Python's hashlib and hmac."""

import hashlib
import hmac
import os
import random
import subprocess
import sys

prog = sys.argv[1]
rng = random.Random(1)
algs = ['md5', 'sha1', 'sha224', 'sha256', 'sha384', 'sha512']
sizes = sorted(set(list(range(0, 300)) + [511, 512, 1000, 4096, 65537, 1 << 20]))
fails = checks = 0
for alg in algs:
    for size in sizes:
        data = rng.randbytes(size)
        cases = [None]
        if size % 7 == 0:
            cases += [b'', rng.randbytes(rng.choice([1, 20, 64, 127, 128, 129, 300]))]
        for key in cases:
            args = [prog, alg] + ([key.hex()] if key is not None else [])
            out = subprocess.run(args, input=data, capture_output=True)
            want = (hmac.new(key, data, alg).hexdigest() if key is not None
                    else hashlib.new(alg, data).hexdigest())
            got = out.stdout.decode().strip()
            checks += 1
            if out.returncode != 0 or got != want:
                fails += 1
                print('FAIL %s size=%d key=%s: %s %s' % (
                    alg, size, None if key is None else len(key), got,
                    out.stderr.decode().strip()))
print('%d checks, %d failures' % (checks, fails))
sys.exit(1 if fails else 0)
