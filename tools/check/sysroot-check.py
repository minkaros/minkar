#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""Checks on a built Minkar sysroot, for the per-layer `check` targets.

  sysroot-check.py --sysroot S [--tools T] links DIR...
      every library a Mach-O under DIR loads (by path, @loader_path,
      @executable_path or @rpath) is in the sysroot, as dyld would find it;
      weak ones may be missing
  sysroot-check.py --sysroot S plists DIR...
      every property list under DIR parses: XML and binary ones here,
      OpenStep text ones with the toolchain's plutil -lint
  sysroot-check.py --sysroot S [--tools T] kexts DIR
      every kext bundle under DIR has an Info.plist naming an executable
      that is a Mach-O kext bundle (codeless kexts excepted)
  sysroot-check.py --sysroot S [--tools T] macho FILE...
      each FILE is an x86_64 Mach-O
  sysroot-check.py --sysroot S kernelcache FILE...
      each FILE is a kernelcache: a fat file whose x86_64 slice has a
      "comp" header (lzss, lzvn or null) and, uncompressed, a Mach-O

DIR and FILE are paths inside the sysroot. Prints each problem and exits
1 if there is one. The tools (llvm-otool) default to the toolchain in
../build next to the sources.
"""

import argparse
import os
import plistlib
import struct
import subprocess
import sys

MH_MAGIC_64 = b'\xcf\xfa\xed\xfe'
FAT_MAGICS = (b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')
MH_KEXT_BUNDLE = 0xb
CPU_TYPE_X86_64 = 0x01000007
COMP_HEADER = 384       # the "comp" header before a kernelcache's payload


def is_macho(path):
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
    except OSError:
        return False
    return magic == MH_MAGIC_64 or magic in FAT_MAGICS


def walk_files(root):
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            path = os.path.join(dirpath, name)
            if not os.path.islink(path):
                yield path


class Checker:
    def __init__(self, sysroot, tools):
        self.sysroot = os.path.abspath(sysroot)
        self.otool = os.path.join(tools, 'llvm-otool')
        self.plutil = os.path.join(tools, 'plutil')
        if not os.access(self.otool, os.X_OK):
            sys.exit('sysroot-check: %s not found (use --tools)' % self.otool)
        self.problems = []

    def inside(self, path):
        return os.path.join(self.sysroot, path.lstrip('/'))

    def load_commands(self, path):
        """The dylibs (name, weak) and rpaths of a Mach-O, x86_64 slice."""
        out = subprocess.run([self.otool, '-l', '-arch', 'x86_64', path],
                             capture_output=True, text=True).stdout
        dylibs, rpaths, cmd = [], [], None
        for line in out.splitlines():
            words = line.split()
            if len(words) >= 2 and words[0] == 'cmd':
                cmd = words[1]
            elif len(words) >= 2 and words[0] == 'name' and cmd in (
                    'LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB',
                    'LC_REEXPORT_DYLIB', 'LC_LOAD_UPWARD_DYLIB'):
                dylibs.append((words[1], cmd == 'LC_LOAD_WEAK_DYLIB'))
            elif len(words) >= 2 and words[0] == 'path' and cmd == 'LC_RPATH':
                rpaths.append(words[1])
        return dylibs, rpaths

    def resolve(self, name, binary, rpaths):
        """Where dyld would find name, as a path in the sysroot, or None."""
        here = os.path.dirname('/' + os.path.relpath(binary, self.sysroot))
        candidates = []
        if name.startswith('@loader_path/') or \
                name.startswith('@executable_path/'):
            candidates.append(os.path.join(here, name.split('/', 1)[1]))
        elif name.startswith('@rpath/'):
            for rp in rpaths:
                if rp.startswith('@loader_path') or \
                        rp.startswith('@executable_path'):
                    rp = here + rp.split('_path', 1)[1]
                candidates.append(os.path.join(rp, name[len('@rpath/'):]))
        else:
            candidates.append(name)
        for c in candidates:
            p = self.inside(os.path.normpath(c))
            if os.path.exists(p):
                return p
        return None

    def links(self, dirs):
        for d in dirs:
            for path in walk_files(self.inside(d)):
                if not is_macho(path):
                    continue
                dylibs, rpaths = self.load_commands(path)
                for name, weak in dylibs:
                    if weak or self.resolve(name, path, rpaths):
                        continue
                    self.problems.append('%s: loads %s, which is not in '
                                         'the sysroot' % (
                                             os.path.relpath(path, self.sysroot),
                                             name))

    def plists(self, dirs):
        for d in dirs:
            for path in walk_files(self.inside(d)):
                if not path.endswith('.plist'):
                    continue
                try:
                    with open(path, 'rb') as f:
                        plistlib.load(f)
                    continue
                except Exception as e:  # any parse error is a problem
                    error = str(e)
                # OpenStep text ({...} or (...)), which CoreFoundation reads.
                with open(path, 'rb') as f:
                    head = f.read(256).lstrip()
                if head[:1] in (b'{', b'(', b'/') and \
                        os.access(self.plutil, os.X_OK):
                    r = subprocess.run([self.plutil, '-lint', path],
                                       capture_output=True, text=True)
                    if r.returncode == 0:
                        continue
                    error = (r.stdout + r.stderr).strip()
                self.problems.append('%s: %s' % (
                    os.path.relpath(path, self.sysroot), error))

    def filetype(self, path):
        out = subprocess.run([self.otool, '-h', '-arch', 'x86_64', path],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            words = line.split()
            # magic cputype cpusubtype caps filetype ncmds ...
            if len(words) >= 5 and words[0].startswith('0x'):
                return int(words[4])
        return None

    def kexts(self, top):
        for dirpath, dirs, _files in os.walk(self.inside(top)):
            for name in dirs:
                if not name.endswith('.kext'):
                    continue
                kext = os.path.join(dirpath, name)
                rel = os.path.relpath(kext, self.sysroot)
                info = os.path.join(kext, 'Contents', 'Info.plist')
                if not os.path.exists(info):
                    info = os.path.join(kext, 'Info.plist')
                try:
                    with open(info, 'rb') as f:
                        plist = plistlib.load(f)
                except Exception as e:
                    self.problems.append('%s: Info.plist: %s' % (rel, e))
                    continue
                exe = plist.get('CFBundleExecutable')
                if exe is None:
                    continue        # a codeless kext
                for p in (os.path.join(kext, 'Contents', 'MacOS', exe),
                          os.path.join(kext, exe)):
                    if os.path.exists(p):
                        break
                else:
                    self.problems.append('%s: no executable %s' % (rel, exe))
                    continue
                if self.filetype(p) != MH_KEXT_BUNDLE:
                    self.problems.append('%s: %s is not a kext bundle' % (
                        rel, exe))

    def kernelcache(self, files):
        for f in files:
            p = f if os.path.isabs(f) and os.path.exists(f) else self.inside(f)
            try:
                with open(p, 'rb') as fh:
                    data = fh.read(4096)
            except OSError as e:
                self.problems.append('%s: %s' % (f, e))
                continue
            if data[:4] != b'\xca\xfe\xba\xbe':
                self.problems.append('%s: not a fat file' % f)
                continue
            (narch,) = struct.unpack('>I', data[4:8])
            off = None
            for i in range(narch):
                cputype, _sub, offset, _size, _align = struct.unpack(
                    '>iiIII', data[8 + 20 * i:28 + 20 * i])
                if cputype == CPU_TYPE_X86_64:
                    off = offset
            if off is None:
                self.problems.append('%s: no x86_64 slice' % f)
                continue
            with open(p, 'rb') as fh:
                fh.seek(off)
                head = fh.read(COMP_HEADER + 8)
            if head[:4] != b'comp' or head[4:8] not in (b'lzss', b'lzvn',
                                                         b'null'):
                self.problems.append('%s: no comp header' % f)
            elif head[4:8] == b'null' and \
                    head[COMP_HEADER:COMP_HEADER + 4] != MH_MAGIC_64:
                self.problems.append('%s: the payload is not a Mach-O' % f)

    def macho(self, files):
        for f in files:
            p = self.inside(f) if not os.path.isabs(f) or \
                not os.path.exists(f) else f
            if not is_macho(p) or self.filetype(p) is None:
                self.problems.append('%s: not an x86_64 Mach-O' % f)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_tools = os.path.normpath(os.path.join(
        here, '../../../build/Developer/Platforms/Minkar.platform/Developer/'
        'Toolchains/Default.xctoolchain/usr/bin'))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--sysroot', required=True)
    ap.add_argument('--tools', default=default_tools)
    ap.add_argument('check', choices=('links', 'plists', 'kexts', 'macho',
                                      'kernelcache'))
    ap.add_argument('paths', nargs='+')
    args = ap.parse_args()

    c = Checker(args.sysroot, args.tools)
    if args.check == 'links':
        c.links(args.paths)
    elif args.check == 'plists':
        c.plists(args.paths)
    elif args.check == 'kexts':
        for p in args.paths:
            c.kexts(p)
    elif args.check == 'kernelcache':
        c.kernelcache(args.paths)
    else:
        c.macho(args.paths)
    for p in c.problems:
        print(p)
    print('sysroot-check %s: %s' % (args.check, 'ok' if not c.problems
                                    else '%d problem(s)' % len(c.problems)))
    return 1 if c.problems else 0


if __name__ == '__main__':
    sys.exit(main())
