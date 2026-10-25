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

"""Host tests for codesign.py: python3 -B tests/test_codesign.py

They build small synthetic Mach-O files, so no target toolchain is
needed. Signatures made by ld64 -adhoc_codesign were also checked to
verify with this tool when it was written.
"""

import hashlib
import os
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import codesign  # noqa: E402

ENTITLEMENTS = plistlib.dumps({'com.apple.hid.system.server-access': True},
                              fmt=plistlib.FMT_XML)


def synthetic_macho(text_size=3 * 4096 + 100, headerpad=64):
    """MH_EXECUTE with __TEXT (one section) and an empty-ish __LINKEDIT."""
    seg = 72
    sect = 80
    sizeofcmds = (seg + sect) + seg
    header_end = 32 + sizeofcmds
    text_off = header_end + headerpad
    linkedit_off = 4096 * ((text_off + text_size + 4095) // 4096)

    data = bytearray(linkedit_off + 32)
    struct.pack_into('<IiiIIIII', data, 0, 0xFEEDFACF, 0x01000007, 3,
                     codesign.MH_EXECUTE, 2, sizeofcmds, 0, 0)
    off = 32
    struct.pack_into('<II16sQQQQiiII', data, off, codesign.LC_SEGMENT_64, seg + sect,
                     b'__TEXT', 0x100000000, linkedit_off, 0, linkedit_off, 5, 5, 1, 0)
    struct.pack_into('<16s16sQQIIIIIIII', data, off + seg, b'__text', b'__TEXT',
                     0x100000000 + text_off, text_size, text_off, 4, 0, 0, 0, 0, 0, 0)
    off += seg + sect
    struct.pack_into('<II16sQQQQiiII', data, off, codesign.LC_SEGMENT_64, seg,
                     b'__LINKEDIT', 0x100000000 + linkedit_off, 4096, linkedit_off,
                     32, 1, 1, 0, 0)
    for i in range(text_size):
        data[text_off + i] = (i * 7) & 0xFF
    return bytes(data)


class CodesignTests(unittest.TestCase):

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.path = os.path.join(self.dir, 'tool')
        self.write(synthetic_macho())

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, data, path=None):
        with open(path or self.path, 'wb') as f:
            f.write(data)

    def blobs(self, path=None):
        return codesign.read_signature(path or self.path)[2]

    def test_adhoc_sign_and_verify(self):
        codesign.sign(self.path)
        codesign.verify(self.path)
        cd = self.blobs()[codesign.CSSLOT_CODEDIRECTORY]
        version, flags = struct.unpack_from('>II', cd, 8)
        self.assertEqual(version, 0x20400)
        self.assertEqual(flags, codesign.CS_ADHOC)
        ident_off = struct.unpack_from('>I', cd, 20)[0]
        self.assertEqual(cd[ident_off:cd.index(b'\0', ident_off)], b'tool')

    def test_signature_layout(self):
        codesign.sign(self.path, identifier='org.example.tool')
        with open(self.path, 'rb') as f:
            data = f.read()
        macho = codesign.MachO(data)
        dataoff, datasize = macho.signature_range()
        self.assertEqual(dataoff % 16, 0)
        self.assertEqual(dataoff + datasize, len(data))
        _, le_off, le_size = macho.segments['__LINKEDIT']
        self.assertEqual(le_off + le_size, len(data))
        cd = self.blobs()[codesign.CSSLOT_CODEDIRECTORY]
        ncode, limit = struct.unpack_from('>II', cd, 28)
        self.assertEqual(limit, dataoff)
        self.assertEqual(ncode, (dataoff + 4095) // 4096)
        # Main binary: the executable segment is __TEXT.
        self.assertEqual(struct.unpack_from('>QQQ', cd, 64), (0, macho.segments['__TEXT'][2], 1))

    def test_entitlements_are_embedded_and_hashed(self):
        codesign.sign(self.path, entitlements=ENTITLEMENTS)
        codesign.verify(self.path)
        blobs = self.blobs()
        ent = blobs[codesign.CSSLOT_ENTITLEMENTS]
        self.assertEqual(struct.unpack_from('>I', ent, 0)[0], codesign.CSMAGIC_EMBEDDED_ENTITLEMENTS)
        self.assertEqual(ent[8:], ENTITLEMENTS)
        cd = blobs[codesign.CSSLOT_CODEDIRECTORY]
        hash_off = struct.unpack_from('>I', cd, 16)[0]
        slot5 = cd[hash_off - 5 * 32:hash_off - 4 * 32]
        self.assertEqual(slot5, hashlib.sha256(ent).digest())

    def test_rejects_invalid_entitlements(self):
        with self.assertRaises(codesign.SignError):
            codesign.sign(self.path, entitlements=b'not a plist')

    def test_resign_requires_force(self):
        codesign.sign(self.path)
        with self.assertRaises(codesign.SignError):
            codesign.sign(self.path)
        codesign.sign(self.path, entitlements=ENTITLEMENTS, force=True)
        codesign.verify(self.path)
        with open(self.path, 'rb') as f:
            self.assertEqual(codesign.MachO(f.read()).ncmds, 3)  # still one LC_CODE_SIGNATURE

    def test_detects_modified_code(self):
        codesign.sign(self.path)
        with open(self.path, 'r+b') as f:
            f.seek(4096 + 10)
            f.write(b'\xff')
        with self.assertRaises(codesign.SignError):
            codesign.verify(self.path)

    def test_no_room_for_load_command(self):
        self.write(synthetic_macho(headerpad=0))
        with self.assertRaises(codesign.SignError):
            codesign.sign(self.path)

    def test_bundle_identifier_and_sealed_info_plist(self):
        exe_dir = os.path.join(self.dir, 'Tool.app', 'Contents', 'MacOS')
        os.makedirs(exe_dir)
        info = os.path.join(self.dir, 'Tool.app', 'Contents', 'Info.plist')
        with open(info, 'wb') as f:
            f.write(plistlib.dumps({'CFBundleIdentifier': 'org.example.Tool'}))
        exe = os.path.join(exe_dir, 'Tool')
        self.write(synthetic_macho(), exe)
        codesign.sign(exe)
        codesign.verify(exe)
        cd = self.blobs(exe)[codesign.CSSLOT_CODEDIRECTORY]
        ident_off = struct.unpack_from('>I', cd, 20)[0]
        self.assertEqual(cd[ident_off:cd.index(b'\0', ident_off)], b'org.example.Tool')
        with open(info, 'ab') as f:
            f.write(b'\n')
        with self.assertRaises(codesign.SignError):
            codesign.verify(exe)

    def test_command_line(self):
        ent_path = os.path.join(self.dir, 'ent.plist')
        with open(ent_path, 'wb') as f:
            f.write(ENTITLEMENTS)
        tool = os.path.join(os.path.dirname(HERE), 'codesign.py')
        run = lambda *args: subprocess.run([sys.executable, '-B', tool] + list(args),
                                           capture_output=True)
        self.assertEqual(run('-s', '-', '--entitlements', ent_path, self.path).returncode, 0)
        self.assertEqual(run('--verify', self.path).returncode, 0)
        shown = run('-d', '--entitlements', '-', self.path)
        self.assertEqual(shown.stdout, ENTITLEMENTS)
        self.assertNotEqual(run('-s', 'Developer ID', self.path).returncode, 0)


if __name__ == '__main__':
    unittest.main()
