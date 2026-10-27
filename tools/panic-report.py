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

"""Make a kernel panic log readable: name the function of every frame.

The kernel in the boot image carries no symbol table, so its panic
backtrace is only addresses even with keepsyms=1. This reads the panic
from a console log and names each frame from the build's unstripped
kernel (and, for frames in a kext, from that kext's binary).

  tools/panic-report.py --kernel kernel.unstripped [--kexts DIR] console.log

The kernel's UUID must match the one in the panic, or the names would be
wrong; the slide comes from "Kernel slide:" or from the __HIB text base.
With llvm-symbolizer on PATH and a dSYM next to the kernel, frames also
get their file and line. Exit status: 0 a panic was found and
symbolicated, 1 no panic in the log, 2 a usage or input error.
"""

import argparse
import bisect
import os
import plistlib
import re
import shutil
import struct
import subprocess
import sys

MH_MAGIC_64 = 0xfeedfacf
LC_SEGMENT_64 = 0x19
LC_SYMTAB = 0x2
LC_UUID = 0x1b
N_STAB = 0xe0
N_TYPE = 0x0e
N_SECT = 0x0e


class MachO:
    """The parts of a 64-bit Mach-O file that symbolication needs."""

    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            data = f.read()
        magic, _cpu, _sub, _ftype, ncmds, _size, _flags, _res = struct.unpack_from('<8I', data, 0)
        if magic != MH_MAGIC_64:
            raise ValueError('%s: not a 64-bit Mach-O file' % path)
        self.segments = {}      # name -> (vmaddr, vmsize)
        self.uuid = None
        symoff = nsyms = stroff = 0
        off = 32
        for _ in range(ncmds):
            cmd, cmdsize = struct.unpack_from('<2I', data, off)
            if cmd == LC_SEGMENT_64:
                name = data[off + 8:off + 24].split(b'\0')[0].decode()
                vmaddr, vmsize = struct.unpack_from('<2Q', data, off + 24)
                self.segments[name] = (vmaddr, vmsize)
            elif cmd == LC_UUID:
                u = data[off + 8:off + 24].hex().upper()
                self.uuid = '%s-%s-%s-%s-%s' % (u[:8], u[8:12], u[12:16], u[16:20], u[20:])
            elif cmd == LC_SYMTAB:
                symoff, nsyms, stroff, _strsize = struct.unpack_from('<4I', data, off + 8)
            off += cmdsize
        syms = {}
        for i in range(nsyms):
            strx, ntype, _sect, _desc, value = struct.unpack_from('<IBBHQ', data, symoff + 16 * i)
            if ntype & N_STAB or (ntype & N_TYPE) != N_SECT or value == 0:
                continue
            end = data.index(b'\0', stroff + strx)
            name = data[stroff + strx:end].decode('utf-8', 'replace')
            # Prefer a global name over a local one at the same address.
            if value not in syms or (ntype & 0x01 and not syms[value][1]):
                syms[value] = (name, bool(ntype & 0x01))
        self.addrs = sorted(syms)
        self.names = [syms[a][0] for a in self.addrs]

    def lookup(self, addr):
        """(name, offset) of the symbol at or before addr, or None."""
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return None
        return self.names[i], addr - self.addrs[i]


def demangle(names):
    """C++ names demangled with c++filt when there is one."""
    tool = shutil.which('c++filt') or shutil.which('llvm-cxxfilt')
    if not tool or not names:
        return {n: n for n in names}
    # Mach-O symbols carry a leading underscore: strip it first.
    plain = [n[1:] if n.startswith('_') else n for n in names]
    try:
        out = subprocess.run([tool], input='\n'.join(plain) + '\n', capture_output=True,
                             text=True, timeout=30).stdout.splitlines()
    except (OSError, subprocess.SubprocessError):
        out = []
    if len(out) != len(names):
        out = plain
    return dict(zip(names, out))


def source_lines(binary, addrs):
    """{addr: 'file:line'} from llvm-symbolizer and a dSYM, when present."""
    tool = shutil.which('llvm-symbolizer')
    dsym = os.path.join(binary + '.dSYM', 'Contents', 'Resources', 'DWARF',
                        os.path.basename(binary))
    if not tool or not addrs:
        return {}
    if not os.path.exists(dsym):
        alt = os.path.join(os.path.dirname(binary), 'kernel.dSYM', 'Contents', 'Resources',
                           'DWARF', 'kernel')
        if not os.path.exists(alt):
            return {}
        dsym = alt
    try:
        out = subprocess.run([tool, '--obj=' + dsym, '--no-inlines', '--output-style=GNU'] +
                             ['0x%x' % a for a in addrs], capture_output=True, text=True,
                             timeout=60).stdout.splitlines()
    except (OSError, subprocess.SubprocessError):
        return {}
    # Two lines per address: function, then file:line.
    result = {}
    for a, loc in zip(addrs, out[1::2]):
        if loc and not loc.startswith('??'):
            result[a] = loc.strip()
    return result


PANIC_RE = re.compile(r'panic\(cpu \d+ caller (0x[0-9a-f]+)\): (.*)')
FRAME_RE = re.compile(r'^\s*(0x[0-9a-f]+) : (0x[0-9a-f]+)')
KEXT_RE = re.compile(r'([\w.\-]+)\(([^)]*)\)\[([0-9A-F-]+)\]@(0x[0-9a-f]+)->(0x[0-9a-f]+)')


def parse_panic(text):
    """The last panic in a console log, as a dict, or None."""
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if PANIC_RE.search(line):
            start = i
    if start is None:
        return None
    m = PANIC_RE.search(lines[start])
    panic = {'caller': int(m.group(1), 16), 'message': m.group(2).strip(), 'frames': [],
             'kexts': [], 'uuid': None, 'slide': None, 'hib': None, 'process': None}
    for line in lines[start + 1:]:
        if line.startswith('panic(cpu'):
            break
        f = FRAME_RE.match(line)
        if f:
            panic['frames'].append(int(f.group(2), 16))
            continue
        k = KEXT_RE.search(line)
        if k:
            panic['kexts'].append((k.group(1), k.group(2), k.group(3),
                                   int(k.group(4), 16), int(k.group(5), 16)))
            continue
        for key, pat in (('uuid', r'Kernel UUID: ([0-9A-F-]+)'),
                         ('slide', r'Kernel slide:\s+(0x[0-9a-f]+)'),
                         ('hib', r'__HIB\s+text base: (0x[0-9a-f]+)'),
                         ('process', r'BSD process name corresponding to current thread: (\S+)')):
            mm = re.search(pat, line)
            if mm:
                panic[key] = mm.group(1)
    return panic


def find_kext(kexts_dir, bundle_id):
    """The binary of the kext with this bundle identifier under kexts_dir."""
    if not kexts_dir:
        return None
    for root, dirs, files in os.walk(kexts_dir):
        if 'Info.plist' in files and os.path.basename(root) == 'Contents':
            try:
                with open(os.path.join(root, 'Info.plist'), 'rb') as f:
                    info = plistlib.load(f)
            except Exception:
                continue
            if info.get('CFBundleIdentifier') == bundle_id:
                exe = os.path.join(root, 'MacOS', info.get('CFBundleExecutable', ''))
                if os.path.isfile(exe):
                    return exe
    return None


def report(panic, kernel, kexts_dir):
    out = []
    out.append('Panic: %s' % panic['message'])
    if panic['process']:
        out.append('Process: %s' % panic['process'])
    if panic['uuid'] and kernel.uuid and panic['uuid'] != kernel.uuid:
        out.append('WARNING: the panic is from kernel %s, but %s is %s: names may be wrong'
                   % (panic['uuid'], kernel.path, kernel.uuid))
    if panic['slide'] is not None:
        slide = int(panic['slide'], 16)
    elif panic['hib'] is not None and '__HIB' in kernel.segments:
        slide = int(panic['hib'], 16) - kernel.segments['__HIB'][0]
    else:
        slide = 0
    out.append('Kernel %s, slide 0x%x' % (kernel.uuid or '(no UUID)', slide))

    # Each frame: in a kext from the backtrace list, else in the kernel.
    loaded = []
    for bundle, version, uuid, start, end in panic['kexts']:
        exe = find_kext(kexts_dir, bundle)
        loaded.append((start, end, bundle, MachO(exe) if exe else None))
    rows = []
    for addr in [panic['caller']] + panic['frames']:
        where, binary, unslid = 'kernel', kernel, addr - slide
        for start, end, bundle, macho in loaded:
            if start <= addr < end:
                where, binary = bundle, macho
                text = macho.segments.get('__TEXT', (0, 0))[0] if macho else 0
                unslid = addr - start + text
                break
        # A return address is just past its call: look up the call.
        sym = binary.lookup(unslid - 1) if binary else None
        if sym:
            sym = (sym[0], sym[1] + 1)
        rows.append((addr, where, binary, unslid, sym))
    names = demangle(sorted({r[4][0] for r in rows if r[4]}))
    lines = source_lines(kernel.path, [r[3] - 1 for r in rows if r[1] == 'kernel'])
    for i, (addr, where, binary, unslid, sym) in enumerate(rows):
        label = 'caller' if i == 0 else '#%-3d' % (i - 1)
        if i == 1:
            out.append('Backtrace:')
        if sym:
            text = '%s + 0x%x' % (names.get(sym[0], sym[0]), sym[1])
        else:
            text = '?'
        loc = lines.get(unslid - 1) if where == 'kernel' else None
        out.append('  %-6s 0x%016x  %-24s %s%s' % (label, addr, where, text,
                                                  '  (%s)' % loc if loc else ''))
    return '\n'.join(out)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--kernel', required=True, help='the unstripped kernel of the same build')
    p.add_argument('--kexts', help='a directory with the kexts of the same build')
    p.add_argument('log', help='a console log with a panic in it')
    args = p.parse_args(argv)
    try:
        with open(args.log, 'rb') as f:
            text = f.read().decode('utf-8', 'replace').replace('\r', '')
        panic = parse_panic(text)
        if panic is None:
            print('panic-report: no panic in %s' % args.log, file=sys.stderr)
            return 1
        kernel = MachO(args.kernel)
    except (OSError, ValueError) as e:
        print('panic-report: %s' % e, file=sys.stderr)
        return 2
    print(report(panic, kernel, args.kexts))
    return 0


if __name__ == '__main__':
    sys.exit(main())
