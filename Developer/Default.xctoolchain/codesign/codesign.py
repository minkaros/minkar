#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The ravynOS contributors
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

"""Ad-hoc code signing for 64-bit Mach-O files, with entitlements.

A subset of macOS codesign(1) for the build host:

    codesign -s - [-f] [-i identifier] [--entitlements file.plist] file...
    codesign --verify [-v] file...
    codesign -d --entitlements - file

The signature is an embedded SuperBlob with a SHA-256 CodeDirectory
(version 0x20400), an empty requirements set and, when given, the XML
entitlements, which the kernel reads through IOUserClient entitlement
checks. Ad-hoc signatures carry no certificate.
"""

import hashlib
import os
import plistlib
import struct
import sys

MH_MAGIC_64 = 0xFEEDFACF
MH_EXECUTE = 0x2
LC_SEGMENT_64 = 0x19
LC_CODE_SIGNATURE = 0x1D

CSMAGIC_REQUIREMENTS = 0xFADE0C01
CSMAGIC_CODEDIRECTORY = 0xFADE0C02
CSMAGIC_EMBEDDED_SIGNATURE = 0xFADE0CC0
CSMAGIC_EMBEDDED_ENTITLEMENTS = 0xFADE7171

CSSLOT_CODEDIRECTORY = 0
CSSLOT_INFOSLOT = 1
CSSLOT_REQUIREMENTS = 2
CSSLOT_ENTITLEMENTS = 5
SPECIAL_SLOTS = 5               # slots 1 (Info.plist) to 5 (entitlements)

CS_ADHOC = 0x2
CS_SUPPORTSEXECSEG = 0x20400
CS_HASHTYPE_SHA256 = 2
CS_EXECSEG_MAIN_BINARY = 0x1
HASH_SIZE = 32
PAGE_SHIFT = 12
PAGE_SIZE = 1 << PAGE_SHIFT
CD_HEADER_SIZE = 88             # CodeDirectory fields through execSegFlags


class SignError(Exception):
    pass


def bundle_info_plist(path):
    """Info.plist of the bundle whose main executable is path, if any."""
    contents = os.path.dirname(os.path.dirname(os.path.abspath(path)))
    if os.path.basename(contents) != 'Contents':
        return None
    info = os.path.join(contents, 'Info.plist')
    return info if os.path.isfile(info) else None


def align(value, to):
    return (value + to - 1) & ~(to - 1)


class MachO:
    """The load commands of a thin 64-bit little-endian Mach-O."""

    def __init__(self, data):
        self.data = data
        if len(data) < 32 or struct.unpack_from('<I', data, 0)[0] != MH_MAGIC_64:
            raise SignError('not a thin 64-bit Mach-O file')
        (_, _, _, self.filetype, self.ncmds, self.sizeofcmds, _, _) = \
            struct.unpack_from('<IiiIIIII', data, 0)
        self.segments = {}
        self.codesig = None     # offset of LC_CODE_SIGNATURE
        self.first_section = len(data)
        off = 32
        for _ in range(self.ncmds):
            cmd, size = struct.unpack_from('<II', data, off)
            if cmd == LC_SEGMENT_64:
                name = data[off + 8:off + 24].rstrip(b'\0').decode()
                vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', data, off + 24)
                nsects = struct.unpack_from('<I', data, off + 64)[0]
                self.segments[name] = (off, fileoff, filesize)
                for i in range(nsects):
                    sect = off + 72 + i * 80
                    sect_off = struct.unpack_from('<I', data, sect + 48)[0]
                    sect_size = struct.unpack_from('<Q', data, sect + 40)[0]
                    if sect_off and sect_size:
                        self.first_section = min(self.first_section, sect_off)
            elif cmd == LC_CODE_SIGNATURE:
                self.codesig = off
            off += size
        self.end_of_cmds = off
        if '__LINKEDIT' not in self.segments:
            raise SignError('no __LINKEDIT segment')

    def signature_range(self):
        if self.codesig is None:
            return None
        return struct.unpack_from('<II', self.data, self.codesig + 8)


def blob(magic, payload):
    return struct.pack('>II', magic, 8 + len(payload)) + payload


def empty_requirements():
    return struct.pack('>III', CSMAGIC_REQUIREMENTS, 12, 0)


def entitlements_blob(xml):
    return blob(CSMAGIC_EMBEDDED_ENTITLEMENTS, xml)


def code_directory(identifier, code, special, exec_seg, main_binary):
    """special maps slot number -> blob whose hash goes in that slot."""
    ident = identifier.encode() + b'\0'
    ncode = (len(code) + PAGE_SIZE - 1) // PAGE_SIZE
    hash_offset = CD_HEADER_SIZE + len(ident) + SPECIAL_SLOTS * HASH_SIZE
    length = hash_offset + ncode * HASH_SIZE

    header = struct.pack('>IIIIIIIIIBBBBI', CSMAGIC_CODEDIRECTORY, length,
                         CS_SUPPORTSEXECSEG, CS_ADHOC, hash_offset,
                         CD_HEADER_SIZE, SPECIAL_SLOTS, ncode, len(code),
                         HASH_SIZE, CS_HASHTYPE_SHA256, 0, PAGE_SHIFT, 0)
    header += struct.pack('>IIIQQQQ', 0, 0, 0, 0, exec_seg[0], exec_seg[1],
                          CS_EXECSEG_MAIN_BINARY if main_binary else 0)
    assert len(header) == CD_HEADER_SIZE

    # Special slots are stored before hashOffset, highest slot first.
    specials = b''
    for slot in range(SPECIAL_SLOTS, 0, -1):
        if slot in special:
            specials += hashlib.sha256(special[slot]).digest()
        else:
            specials += b'\0' * HASH_SIZE

    pages = b''.join(hashlib.sha256(code[i:i + PAGE_SIZE]).digest()
                     for i in range(0, len(code), PAGE_SIZE))
    cd = header + ident + specials + pages
    assert len(cd) == length
    return cd


def superblob(entries):
    """entries: list of (slot, blob), sorted by slot."""
    header_size = 12 + 8 * len(entries)
    index = b''
    body = b''
    for slot, data in entries:
        index += struct.pack('>II', slot, header_size + len(body))
        body += data
    return struct.pack('>III', CSMAGIC_EMBEDDED_SIGNATURE,
                       header_size + len(body), len(entries)) + index + body


def sign(path, identifier=None, entitlements=None, force=False):
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    macho = MachO(data)

    sig = macho.signature_range()
    if sig is not None:
        if not force:
            raise SignError('%s: is already signed (use -f to replace)' % path)
        dataoff, _ = sig
        del data[dataoff:]
    else:
        # Room for a new LC_CODE_SIGNATURE after the existing commands?
        if macho.end_of_cmds + 16 > macho.first_section:
            raise SignError('%s: no room for LC_CODE_SIGNATURE '
                            '(link with -headerpad)' % path)
        struct.pack_into('<IIII', data, macho.end_of_cmds, LC_CODE_SIGNATURE, 16, 0, 0)
        struct.pack_into('<II', data, 16, macho.ncmds + 1, macho.sizeofcmds + 16)
        macho = MachO(data)

    special = {CSSLOT_REQUIREMENTS: empty_requirements()}

    # Like codesign(1): a bundle's main executable seals its Info.plist
    # and takes its identifier from CFBundleIdentifier.
    info = bundle_info_plist(path)
    if info is not None:
        with open(info, 'rb') as f:
            special[CSSLOT_INFOSLOT] = f.read()
        if identifier is None:
            try:
                identifier = plistlib.loads(special[CSSLOT_INFOSLOT]).get('CFBundleIdentifier')
            except Exception as e:
                raise SignError('%s: cannot read Info.plist: %s' % (path, e))
    if identifier is None:
        identifier = os.path.basename(path)
    entries = [(CSSLOT_REQUIREMENTS, special[CSSLOT_REQUIREMENTS])]
    if entitlements is not None:
        try:
            plistlib.loads(entitlements, fmt=plistlib.FMT_XML)
        except Exception as e:
            raise SignError('entitlements are not an XML property list: %s' % e)
        special[CSSLOT_ENTITLEMENTS] = entitlements_blob(entitlements)
        entries.append((CSSLOT_ENTITLEMENTS, special[CSSLOT_ENTITLEMENTS]))

    # The signature goes at the end of __LINKEDIT, 16-byte aligned.
    code_limit = align(len(data), 16)
    data += b'\0' * (code_limit - len(data))
    ncode = (code_limit + PAGE_SIZE - 1) // PAGE_SIZE
    cd_size = CD_HEADER_SIZE + len(identifier.encode()) + 1 + \
        (SPECIAL_SLOTS + ncode) * HASH_SIZE
    sig_size = 12 + 8 * (len(entries) + 1) + cd_size + \
        sum(len(b) for _, b in entries)
    sig_size = align(sig_size, 16)

    # Fix up the load commands before hashing: they are part of page 0.
    struct.pack_into('<II', data, macho.codesig + 8, code_limit, sig_size)
    seg_off, le_fileoff, _ = macho.segments['__LINKEDIT']
    le_filesize = code_limit + sig_size - le_fileoff
    struct.pack_into('<QQ', data, seg_off + 32, align(le_filesize, PAGE_SIZE), le_fileoff)
    struct.pack_into('<Q', data, seg_off + 48, le_filesize)

    text = macho.segments.get('__TEXT', (0, 0, 0))
    cd = code_directory(identifier, bytes(data[:code_limit]), special,
                        (text[1], text[2]), macho.filetype == MH_EXECUTE)
    signature = superblob([(CSSLOT_CODEDIRECTORY, cd)] + entries)
    assert len(signature) <= sig_size
    data += signature + b'\0' * (sig_size - len(signature))

    tmp = path + '.cstemp'
    with open(tmp, 'wb') as f:
        f.write(data)
    os.chmod(tmp, os.stat(path).st_mode)
    os.replace(tmp, path)


def read_signature(path):
    """Return (data, blobs by slot, CodeDirectory fields) or raise SignError."""
    with open(path, 'rb') as f:
        data = f.read()
    macho = MachO(data)
    sig = macho.signature_range()
    if sig is None:
        raise SignError('%s: code object is not signed at all' % path)
    dataoff, datasize = sig
    sb = data[dataoff:dataoff + datasize]
    magic, length, count = struct.unpack_from('>III', sb, 0)
    if magic != CSMAGIC_EMBEDDED_SIGNATURE or length > len(sb):
        raise SignError('%s: invalid signature SuperBlob' % path)
    blobs = {}
    for i in range(count):
        slot, off = struct.unpack_from('>II', sb, 12 + 8 * i)
        blen = struct.unpack_from('>I', sb, off + 4)[0]
        blobs[slot] = sb[off:off + blen]
    if CSSLOT_CODEDIRECTORY not in blobs:
        raise SignError('%s: no CodeDirectory' % path)
    return data, dataoff, blobs


def verify(path, verbose=False):
    data, dataoff, blobs = read_signature(path)
    cd = blobs[CSSLOT_CODEDIRECTORY]
    (magic, length, version, flags, hash_offset, ident_offset, nspecial,
     ncode, code_limit, hash_size, hash_type, _, page_shift, _) = \
        struct.unpack_from('>IIIIIIIIIBBBBI', cd, 0)
    if magic != CSMAGIC_CODEDIRECTORY or hash_type != CS_HASHTYPE_SHA256 \
            or hash_size != HASH_SIZE:
        raise SignError('%s: unsupported CodeDirectory' % path)
    if code_limit != dataoff:
        raise SignError('%s: code limit does not match the signature offset' % path)
    page = 1 << page_shift
    for i in range(ncode):
        expect = cd[hash_offset + i * hash_size:hash_offset + (i + 1) * hash_size]
        chunk = data[i * page:min((i + 1) * page, code_limit)]
        if hashlib.sha256(chunk).digest() != expect:
            raise SignError('%s: code page %d was modified' % (path, i))
    for slot, b in blobs.items():
        if slot == CSSLOT_CODEDIRECTORY or slot > nspecial:
            continue
        expect = cd[hash_offset - slot * hash_size:hash_offset - (slot - 1) * hash_size]
        if hashlib.sha256(b).digest() != expect:
            raise SignError('%s: special slot %d does not match' % (path, slot))
    if nspecial >= CSSLOT_INFOSLOT:
        expect = cd[hash_offset - HASH_SIZE:hash_offset]
        info = bundle_info_plist(path)
        if expect != b'\0' * HASH_SIZE:
            if info is None:
                raise SignError('%s: Info.plist is sealed but missing' % path)
            with open(info, 'rb') as f:
                if hashlib.sha256(f.read()).digest() != expect:
                    raise SignError('%s: Info.plist was modified' % path)
    if verbose:
        ident = cd[ident_offset:cd.index(b'\0', ident_offset)].decode()
        print('%s: valid on disk (identifier %s, flags 0x%x, %d pages)'
              % (path, ident, flags, ncode), file=sys.stderr)


def display_entitlements(path):
    _, _, blobs = read_signature(path)
    ent = blobs.get(CSSLOT_ENTITLEMENTS)
    if ent is not None:
        sys.stdout.buffer.write(ent[8:])


def usage():
    print('usage: codesign -s - [-f] [-i identifier] [--entitlements file] file...\n'
          '       codesign --verify [-v] file...\n'
          '       codesign -d --entitlements - file', file=sys.stderr)
    sys.exit(2)


def main(argv):
    identity = identifier = entitlements = None
    force = verify_mode = display = verbose = False
    files = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ('-s', '--sign'):
            i += 1
            identity = argv[i] if i < len(argv) else usage()
        elif arg in ('-i', '--identifier'):
            i += 1
            identifier = argv[i] if i < len(argv) else usage()
        elif arg == '--entitlements':
            i += 1
            entitlements = argv[i] if i < len(argv) else usage()
        elif arg in ('-f', '--force'):
            force = True
        elif arg in ('-v', '--verbose'):
            verbose = True
        elif arg == '--verify':
            verify_mode = True
        elif arg in ('-d', '--display'):
            display = True
        elif arg.startswith('-') and len(arg) > 1:
            usage()
        else:
            files.append(arg)
        i += 1
    if not files:
        usage()

    try:
        if display:
            for f in files:
                display_entitlements(f)
        elif verify_mode:
            for f in files:
                verify(f, verbose)
        elif identity is not None:
            if identity != '-':
                raise SignError('only ad-hoc signing (-s -) is supported')
            ents = None
            if entitlements is not None:
                with open(entitlements, 'rb') as f:
                    ents = f.read()
            for f in files:
                sign(f, identifier, ents, force)
                if verbose:
                    print('%s: signed Mach-O thin (x86_64)' % f, file=sys.stderr)
        else:
            usage()
    except (SignError, OSError) as e:
        print('codesign: %s' % e, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
