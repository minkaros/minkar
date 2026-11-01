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

"""Signs the sysroot's executables and writes its static trust cache.

    mktrustcache.py [--exclude PATH]... SYSROOT

Every thin 64-bit Mach-O executable (and dyld) in SYSROOT gets an ad-hoc
signature if it has none, or if its code changed after it was signed (the
entitlements and identifier are kept). The CDHash of each signature goes
into a version 1 trust cache (osfmk/kern/trustcache.h) at
SYSROOT/System/Library/Security/TrustCache, which MinkarCodeIntegrity
reads at boot (Docs/ADR/0001-Kernel-Entitlements.md).

--exclude leaves a sysroot path (and what is under it) out of the cache;
the tests use it for a copy that must not be a platform binary.
"""

import hashlib
import os
import struct
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'Developer',
                                'Default.xctoolchain', 'codesign'))
import codesign  # noqa: E402

MH_DYLINKER = 0x7
CS_CDHASH_LEN = 20
TRUST_CACHE_PATH = 'System/Library/Security/TrustCache'
# Entry flags (trustcache.h): none of them are used here.
ENTRY_FLAGS = 0


def macho_filetype(path):
    """The Mach-O file type of a thin 64-bit file, or None."""
    try:
        with open(path, 'rb') as f:
            header = f.read(16)
    except OSError:
        return None
    if len(header) < 16 or struct.unpack_from('<I', header, 0)[0] != codesign.MH_MAGIC_64:
        return None
    return struct.unpack_from('<I', header, 12)[0]


def identifier_for(path):
    """The signing identifier: the bundle's, else org.minkar.<name>."""
    info = codesign.bundle_info_plist(path)
    if info is not None:
        return None     # codesign.sign takes CFBundleIdentifier
    return 'org.minkar.' + os.path.basename(path)


def ensure_signed(path):
    """Sign path if it is unsigned or no longer matches its signature.
    Returns 'kept', 'signed' or 'resigned'."""
    try:
        codesign.verify(path)
        return 'kept'
    except codesign.SignError as e:
        unsigned = 'not signed at all' in str(e)
    if unsigned:
        codesign.sign(path, identifier=identifier_for(path))
        return 'signed'
    # Changed after signing (stripped, patched): sign it again with the
    # same identifier and entitlements.
    _, _, blobs = codesign.read_signature(path)
    cd = blobs[codesign.CSSLOT_CODEDIRECTORY]
    ident_off = struct.unpack_from('>I', cd, 20)[0]
    ident = cd[ident_off:cd.index(b'\0', ident_off)].decode()
    ent = blobs.get(codesign.CSSLOT_ENTITLEMENTS)
    codesign.sign(path, identifier=ident, entitlements=ent[8:] if ent else None,
                  force=True)
    return 'resigned'


def cdhash(path):
    """(CDHash, hash type) of path's signature."""
    _, _, blobs = codesign.read_signature(path)
    cd = blobs[codesign.CSSLOT_CODEDIRECTORY]
    hash_type = cd[37]
    if hash_type != codesign.CS_HASHTYPE_SHA256:
        raise codesign.SignError('%s: unsupported hash type %d' % (path, hash_type))
    return hashlib.sha256(cd).digest()[:CS_CDHASH_LEN], hash_type


def trust_cache(entries):
    """A version 1 trust cache of (cdhash, hash type) pairs. The UUID is
    derived from the entries, so the same sysroot gives the same file."""
    entries = sorted(set(entries))
    body = b''.join(struct.pack('<20sBB', h, t, ENTRY_FLAGS) for h, t in entries)
    ident = uuid.UUID(bytes=hashlib.sha256(body).digest()[:16], version=4)
    return struct.pack('<I16sI', 1, ident.bytes, len(entries)) + body


def parse_trust_cache(data):
    """The (cdhash, hash type) entries of a version 1 trust cache."""
    version, _, count = struct.unpack_from('<I16sI', data, 0)
    if version != 1 or len(data) != 24 + 22 * count:
        raise ValueError('not a version 1 trust cache')
    return [struct.unpack_from('<20sBB', data, 24 + 22 * i)[:2] for i in range(count)]


def build(sysroot, excludes=()):
    excludes = [os.path.normpath(os.path.join(sysroot, e.lstrip('/'))) for e in excludes]
    entries = []
    counts = {'kept': 0, 'signed': 0, 'resigned': 0}
    failures = []
    for root, dirs, files in os.walk(sysroot):
        dirs.sort()
        if any(root == e or root.startswith(e + os.sep) for e in excludes):
            dirs[:] = []
            continue
        for name in sorted(files):
            path = os.path.join(root, name)
            if os.path.islink(path) or path in excludes:
                continue
            if macho_filetype(path) not in (codesign.MH_EXECUTE, MH_DYLINKER):
                continue
            try:
                counts[ensure_signed(path)] += 1
                entries.append(cdhash(path))
            except codesign.SignError as e:
                failures.append(str(e))
    if failures:
        for f in failures:
            print('mktrustcache: %s' % f, file=sys.stderr)
        raise SystemExit('mktrustcache: %d executables could not be signed' % len(failures))

    out = os.path.join(sysroot, TRUST_CACHE_PATH)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    data = trust_cache(entries)
    tmp = out + '.new'
    with open(tmp, 'wb') as f:
        f.write(data)
    os.chmod(tmp, 0o644)
    os.replace(tmp, out)
    print('mktrustcache: %d executables (%d already signed, %d signed, %d signed again), '
          '%d entries in %s' % (sum(counts.values()), counts['kept'], counts['signed'],
                                counts['resigned'], len(parse_trust_cache(data)), out))
    return data


def main(argv):
    excludes = []
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == '--exclude' and i + 1 < len(argv):
            excludes.append(argv[i + 1])
            i += 2
            continue
        args.append(argv[i])
        i += 1
    if len(args) != 1:
        print(__doc__.split('\n\n')[1], file=sys.stderr)
        return 2
    build(args[0], excludes)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
