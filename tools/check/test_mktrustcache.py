#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.


"""Host tests for mktrustcache.py: python3 -B tools/check/test_mktrustcache.py

They use the synthetic Mach-O files of codesign.py's tests, so no target
toolchain is needed.
"""

import hashlib
import os
import plistlib
import shutil
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, 'Developer', 'Default.xctoolchain', 'codesign', 'tests'))
import mktrustcache  # noqa: E402
from mktrustcache import codesign  # noqa: E402
from test_codesign import synthetic_macho  # noqa: E402

ENTITLEMENTS = plistlib.dumps({'com.apple.hid.system.server-access': True},
                              fmt=plistlib.FMT_XML)


class TrustCacheTests(unittest.TestCase):

    def setUp(self):
        self.root = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.root)

    def put(self, rel, data):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)
        return path

    def entries(self):
        with open(os.path.join(self.root, mktrustcache.TRUST_CACHE_PATH), 'rb') as f:
            return mktrustcache.parse_trust_cache(f.read())

    def test_signs_and_lists_executables(self):
        a = self.put('bin/a', synthetic_macho())
        b = self.put('usr/libexec/b', synthetic_macho(text_size=5000))
        self.put('usr/share/notes.txt', b'not a binary')
        mktrustcache.build(self.root)
        for path in (a, b):
            codesign.verify(path)
        hashes = {mktrustcache.cdhash(p)[0] for p in (a, b)}
        self.assertEqual({h for h, _ in self.entries()}, hashes)
        self.assertTrue(all(t == codesign.CS_HASHTYPE_SHA256 for _, t in self.entries()))

    def test_identifier(self):
        a = self.put('sbin/tool', synthetic_macho())
        mktrustcache.build(self.root)
        cd = codesign.read_signature(a)[2][codesign.CSSLOT_CODEDIRECTORY]
        off = struct.unpack_from('>I', cd, 20)[0]
        self.assertEqual(cd[off:cd.index(b'\0', off)], b'org.minkar.tool')

    def test_keeps_existing_signature_and_entitlements(self):
        a = self.put('System/Library/Server', synthetic_macho())
        codesign.sign(a, identifier='org.example.server', entitlements=ENTITLEMENTS)
        with open(a, 'rb') as f:
            before = f.read()
        mktrustcache.build(self.root)
        with open(a, 'rb') as f:
            self.assertEqual(f.read(), before)
        self.assertIn(mktrustcache.cdhash(a)[0], {h for h, _ in self.entries()})

    def test_resigns_modified_code_keeping_entitlements(self):
        a = self.put('System/Library/Server', synthetic_macho())
        codesign.sign(a, identifier='org.example.server', entitlements=ENTITLEMENTS)
        with open(a, 'r+b') as f:
            f.seek(1000)
            f.write(b'\x90' * 8)
        mktrustcache.build(self.root)
        codesign.verify(a)
        blobs = codesign.read_signature(a)[2]
        self.assertEqual(blobs[codesign.CSSLOT_ENTITLEMENTS][8:], ENTITLEMENTS)

    def test_exclude(self):
        a = self.put('bin/a', synthetic_macho())
        b = self.put('usr/tests/outside/b', synthetic_macho(text_size=6000))
        mktrustcache.build(self.root, excludes=['/usr/tests/outside'])
        hashes = {h for h, _ in self.entries()}
        self.assertIn(mktrustcache.cdhash(a)[0], hashes)
        with self.assertRaises(codesign.SignError):
            codesign.read_signature(b)      # left alone, unsigned

    def test_format(self):
        self.put('bin/a', synthetic_macho())
        self.put('bin/b', synthetic_macho(text_size=7000))
        data = mktrustcache.build(self.root)
        version, ident, count = struct.unpack_from('<I16sI', data, 0)
        self.assertEqual((version, count, len(data)), (1, 2, 24 + 22 * 2))
        hashes = [data[24 + 22 * i:44 + 22 * i] for i in range(count)]
        self.assertEqual(hashes, sorted(hashes))
        # Same sysroot, same file.
        self.assertEqual(mktrustcache.build(self.root), data)

    def test_cdhash_is_truncated_sha256_of_code_directory(self):
        a = self.put('bin/a', synthetic_macho())
        codesign.sign(a)
        cd = codesign.read_signature(a)[2][codesign.CSSLOT_CODEDIRECTORY]
        self.assertEqual(mktrustcache.cdhash(a)[0], hashlib.sha256(cd).digest()[:20])


if __name__ == '__main__':
    unittest.main()
