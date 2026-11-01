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


"""Selector inventory: the Objective-C messages macOS apps send that no
Minkar class implements.

symbol-inventory.py compares linked symbols only: classes show up there,
but their methods are found at run time. This lists, for a set of apps,
the selectors their binaries send (__objc_selrefs) that neither the app
itself (any binary in its bundle, embedded frameworks included) nor any
Minkar library implements (the method lists of the classes and categories
of every Mach-O file in the sysroot's /System/Library and /usr/lib).

  selector-inventory.py --sysroot ../build/sysroot --out report \\
      --apps DIR_OR_FILE [--apps ...]

A selector counts as implemented when any class has a method by that
name: a message to the wrong class is not caught, and a method that only
raises (a stub) counts as implemented. The methods of the protocols an app
declares count as the app's (they are sent to the objects that adopt them).
The binaries are only read, never run.

Output, in --out:
  selectors-minkar.tsv   selector, the Minkar methods by that name
  selectors-used.tsv     app, selector, sent by the app, implemented by it
  selectors-missing.tsv  selector, apps that send it
  selectors.md           per app and overall: sent, implemented by the app,
                         by Minkar, missing; the missing ones most used first
"""

import argparse
import collections
import importlib.util
import json
import os
import re
import struct
import sys

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('symbol_inventory',
                                               os.path.join(_here, 'symbol-inventory.py'))
inventory = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(inventory)

# The Objective-C metadata is read from the files directly: llvm-otool -ov
# leaves out the metaclass (so the class methods) of exported classes.

LC_SEGMENT_64 = 0x19
LC_DYLD_CHAINED_FIXUPS = 0x80000034
FAT = (0xcafebabe, 0xcafebabf)

def thin_slice(data, cputype=0x01000007):
    magic = struct.unpack('>I', data[:4])[0]
    if magic not in FAT:
        return data
    n = struct.unpack('>I', data[4:8])[0]
    wide = magic == 0xcafebabf
    for i in range(n):
        if wide:
            ct, st, off, size = struct.unpack('>iiQQ', data[8 + i * 32:8 + i * 32 + 24])
        else:
            ct, st, off, size = struct.unpack('>iiII', data[8 + i * 20:8 + i * 20 + 16])
        if ct == cputype:
            return data[off:off + size]
    return None

class MachO:
    def __init__(self, data):
        self.d = data
        magic, cpu, sub, ftype, ncmds, size, flags, _ = struct.unpack('<IiiIIIII', data[:32])
        if magic != 0xfeedfacf:
            raise ValueError('not a 64-bit Mach-O')
        self.segments, self.sections = [], {}
        self.chained = False
        self.base = None
        off = 32
        for _ in range(ncmds):
            cmd, cmdsize = struct.unpack('<II', data[off:off + 8])
            if cmd == LC_SEGMENT_64:
                segname = data[off + 8:off + 24].rstrip(b'\0').decode()
                vmaddr, vmsize, fileoff, filesize = struct.unpack('<QQQQ', data[off + 24:off + 56])
                nsects = struct.unpack('<I', data[off + 64:off + 68])[0]
                self.segments.append((vmaddr, vmsize, fileoff, filesize))
                if segname == '__TEXT':
                    self.base = vmaddr
                s = off + 72
                for _ in range(nsects):
                    sect = data[s:s + 16].rstrip(b'\0').decode()
                    seg = data[s + 16:s + 32].rstrip(b'\0').decode()
                    addr, size, offset = struct.unpack('<QQI', data[s + 32:s + 52])
                    self.sections[(seg, sect)] = (addr, size, offset)
                    s += 80
            elif cmd == LC_DYLD_CHAINED_FIXUPS:
                self.chained = True
            off += cmdsize
        if self.base is None:
            self.base = 0

    def offset(self, addr):
        for vmaddr, vmsize, fileoff, filesize in self.segments:
            if vmaddr <= addr < vmaddr + filesize:
                return fileoff + addr - vmaddr
        return None

    def u32(self, addr):
        o = self.offset(addr)
        return None if o is None else struct.unpack('<I', self.d[o:o + 4])[0]

    def i32(self, addr):
        o = self.offset(addr)
        return None if o is None else struct.unpack('<i', self.d[o:o + 4])[0]

    def ptr(self, addr):
        """The target of a pointer in the file: 0 for a bind (external)."""
        o = self.offset(addr)
        if o is None:
            return 0
        v = struct.unpack('<Q', self.d[o:o + 8])[0]
        if not self.chained:
            return v
        if v >> 63:
            return 0                                    # bind
        target = v & 0xfffffffff                        # 36 bits
        high8 = (v >> 36) & 0xff
        if target < self.base:                          # DYLD_CHAINED_PTR_64_OFFSET
            target += self.base
        return target | (high8 << 56)

    def cstring(self, addr):
        o = self.offset(addr)
        if o is None:
            return None
        e = self.d.find(b'\0', o)
        return self.d[o:e].decode('utf-8', 'replace')

    def methods(self, addr):
        """The selectors of a method_list_t."""
        if not addr:
            return []
        flags = self.u32(addr)
        count = self.u32(addr + 4)
        if flags is None or count is None or count > 100000:
            return []
        entsize = flags & 0xfffc
        relative = flags & 0x80000000
        result = []
        for i in range(count):
            e = addr + 8 + i * entsize
            if relative:
                rel = self.i32(e)
                if rel is None:
                    continue
                selref = e + rel
                name = self.cstring(self.ptr(selref))
            else:
                name = self.cstring(self.ptr(e))
            if name:
                result.append(name)
        return result

    def class_methods(self, cls):
        """(class name, instance methods) of an objc_class at cls."""
        data = self.ptr(cls + 32) & ~7 & ((1 << 56) - 1)
        if not data:
            return None, []
        name = self.cstring(self.ptr(data + 24))
        return name, self.methods(self.ptr(data + 32))

    def implemented(self):
        """{(class, '+'/'-', selector)} of the classes and categories."""
        result = set()
        for seg in ('__DATA', '__DATA_CONST'):
            sect = self.sections.get((seg, '__objc_classlist'))
            if sect:
                addr, size, _ = sect
                for i in range(size // 8):
                    cls = self.ptr(addr + i * 8)
                    if not cls:
                        continue
                    name, inst = self.class_methods(cls)
                    for sel in inst:
                        result.add((name, '-', sel))
                    meta = self.ptr(cls)
                    if meta:
                        _, cm = self.class_methods(meta)
                        for sel in cm:
                            result.add((name, '+', sel))
            sect = self.sections.get((seg, '__objc_catlist'))
            if sect:
                addr, size, _ = sect
                for i in range(size // 8):
                    cat = self.ptr(addr + i * 8)
                    if not cat:
                        continue
                    cname = self.cstring(self.ptr(cat)) or '?'
                    for sel in self.methods(self.ptr(cat + 16)):
                        result.add(('(' + cname + ')', '-', sel))
                    for sel in self.methods(self.ptr(cat + 24)):
                        result.add(('(' + cname + ')', '+', sel))
        return result

    def protocol_methods(self):
        """The selectors the protocols of the file declare: messages to the
        objects that adopt them, not to the system."""
        result = set()
        for seg in ('__DATA', '__DATA_CONST'):
            sect = self.sections.get((seg, '__objc_protolist'))
            if sect:
                addr, size, _ = sect
                for i in range(size // 8):
                    proto = self.ptr(addr + i * 8)
                    if not proto:
                        continue
                    for field in (24, 32, 40, 48):
                        result.update(self.methods(self.ptr(proto + field)))
        return result

    def selrefs(self):
        result = set()
        for seg in ('__DATA', '__DATA_CONST'):
            sect = self.sections.get((seg, '__objc_selrefs'))
            if sect:
                addr, size, _ = sect
                for i in range(size // 8):
                    s = self.cstring(self.ptr(addr + i * 8))
                    if s:
                        result.add(s)
        return result


def objc(path):
    """The MachO of a file's x86_64 image, or None."""
    try:
        with open(path, 'rb') as f:
            data = thin_slice(f.read())
        return MachO(data) if data else None
    except (OSError, ValueError, struct.error):
        return None


def scan_minkar(sysroot):
    """{selector: {"+[Class sel]" ...}} for every Mach-O under /System/Library and /usr/lib."""
    found = collections.defaultdict(set)
    for top in ('System/Library', 'usr/lib'):
        for path in inventory.walk_machos(os.path.join(sysroot, top)):
            m = objc(path)
            if m is None:
                continue
            for cls, kind, sel in m.implemented():
                found[sel].add('%s[%s %s]' % (kind, cls, sel))
    return found


def scan_app(root):
    """(selectors sent, selectors implemented) by all the binaries of an app."""
    sends, defines = set(), set()
    for path in inventory.walk_machos(root):
        m = objc(path)
        if m is None:
            continue
        sends |= m.selrefs()
        defines |= {sel for _, _, sel in m.implemented()}
        defines |= m.protocol_methods()
    return sends, defines


def write_tsv(path, header, rows):
    with open(path, 'w') as f:
        f.write('\t'.join(header) + '\n')
        for row in rows:
            f.write('\t'.join(str(c) for c in row) + '\n')


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--sysroot', required=True, help='Minkar root file system')
    p.add_argument('--apps', action='append', default=[], required=True,
                   help='macOS app, binary or directory of them (repeatable)')
    p.add_argument('--out', required=True, help='report directory')
    p.add_argument('--top', type=int, default=60,
                   help='missing selectors listed in selectors.md')
    args = p.parse_args(argv)

    os.makedirs(args.out, exist_ok=True)

    minkar = scan_minkar(args.sysroot)
    write_tsv(os.path.join(args.out, 'selectors-minkar.tsv'), ('selector', 'methods'),
              ((s, ' '.join(sorted(minkar[s]))) for s in sorted(minkar)))

    used_rows, missing, per_app = [], collections.defaultdict(set), {}
    for app, root in inventory.app_roots(args.apps):
        sends, defines = scan_app(root)
        counts = collections.Counter()
        for sel in sorted(sends):
            own = sel in defines
            ours = sel in minkar
            used_rows.append((app, sel, 'app' if own else '', 'minkar' if ours else ''))
            if own:
                counts['app'] += 1
            elif ours:
                counts['minkar'] += 1
            else:
                counts['missing'] += 1
                missing[sel].add(app)
        counts['sent'] = len(sends)
        per_app[app] = counts
    write_tsv(os.path.join(args.out, 'selectors-used.tsv'),
              ('app', 'selector', 'implemented_by_app', 'implemented_by_minkar'), used_rows)
    ranked = sorted(missing, key=lambda s: (-len(missing[s]), s))
    write_tsv(os.path.join(args.out, 'selectors-missing.tsv'), ('selector', 'apps'),
              ((s, ','.join(sorted(missing[s]))) for s in ranked))

    system = sum(c['sent'] - c['app'] for c in per_app.values())
    present = sum(c['minkar'] for c in per_app.values())
    with open(os.path.join(args.out, 'selectors.md'), 'w') as f:
        f.write('# Objective-C selectors\n\n')
        f.write('Selectors each app sends that it does not implement itself, and how many of '
                'them some Minkar class implements (%d selectors implemented in Minkar).\n\n'
                % len(minkar))
        f.write('| App | Sent | By the app | System | In Minkar | Missing | Coverage |\n')
        f.write('|-----|-----:|-----------:|-------:|----------:|--------:|---------:|\n')
        for app in sorted(per_app):
            c = per_app[app]
            sys_ = c['sent'] - c['app']
            f.write('| %s | %d | %d | %d | %d | %d | %.1f%% |\n' %
                    (app, c['sent'], c['app'], sys_, c['minkar'], c['missing'],
                     100.0 * c['minkar'] / sys_ if sys_ else 100.0))
        f.write('| **All** | | | %d | %d | %d | %.1f%% |\n\n' %
                (system, present, system - present, 100.0 * present / system if system else 100.0))
        f.write('## Missing, most used first\n\n')
        for sel in ranked[:args.top]:
            f.write('- `%s` (%s)\n' % (sel, ', '.join(sorted(missing[sel]))))
        f.write('\n%d distinct selectors missing; all in selectors-missing.tsv.\n' % len(ranked))
    with open(os.path.join(args.out, 'selectors.json'), 'w') as f:
        json.dump({'minkar_selectors': len(minkar), 'system_sent': system, 'present': present,
                   'missing_distinct': len(ranked),
                   'apps': {a: dict(c) for a, c in per_app.items()}}, f, indent=1, sort_keys=True)
    print('selector-inventory: %d selectors implemented in Minkar' % len(minkar))
    print('selector-inventory: %d of %d system selectors the apps send are there (%.1f%%), '
          '%d distinct missing' % (present, system, 100.0 * present / system if system else 100.0,
                                   len(ranked)))
    print('selector-inventory: report in %s' % os.path.join(args.out, 'selectors.md'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
