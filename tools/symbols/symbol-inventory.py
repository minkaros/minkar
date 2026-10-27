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

"""Symbol inventory: what Minkar exports against what macOS apps import.

Lists the exported symbols of every dylib and framework in a Minkar sysroot
(following re-exports, as dyld does), reads the symbols a set of macOS
binaries import from each library (two-level namespace: every import names
its library), and writes a coverage report per library.

  symbol-inventory.py --sysroot ../build/sysroot --out report \\
      [--apps DIR_OR_FILE ...]

Every Mach-O file under each --apps path is read (an .app bundle, a
directory of apps, or single binaries); fat files are read for x86_64.
The binaries are only parsed with llvm-nm and llvm-otool, never run.

Output, in --out:
  exports.tsv    library install name, symbol (Minkar's inventory)
  imports.tsv    app, binary, library, symbol, weak
  missing.tsv    library, symbol, apps that import it
  coverage.json  the numbers below, for scripts
  coverage.md    per library: imported symbols, how many Minkar exports,
                 the missing ones most used first

Only symbols are compared: Objective-C classes show up as
_OBJC_CLASS_$_Name imports, but methods are looked up at run time and are
not covered here.
"""

import argparse
import collections
import json
import os
import subprocess
import sys

DEFAULT_TOOLS = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'build',
    'Developer/Platforms/Minkar.platform/Developer/Toolchains/'
    'Default.xctoolchain/usr/bin'))

MH_MAGICS = {b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe',
             b'\xfe\xed\xfa\xcf', b'\xfe\xed\xfa\xce'}
FAT_MAGICS = {b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'}
ARCH = 'x86_64'


def macho_kind(path):
    """'thin', 'fat' or None."""
    try:
        with open(path, 'rb') as f:
            head = f.read(8)
    except OSError:
        return None
    if head[:4] in MH_MAGICS:
        return 'thin'
    # Java class files share the fat magic; they have a large "count".
    if head[:4] in FAT_MAGICS and 0 < int.from_bytes(head[4:8], 'big') < 32:
        return 'fat'
    return None


def walk_machos(root):
    """Every Mach-O file under root (or root itself), symlinks not followed."""
    if os.path.isfile(root) and not os.path.islink(root):
        if macho_kind(root):
            yield root
        return
    for dirpath, dirnames, filenames in os.walk(root, onerror=lambda e: None):
        dirnames.sort()
        for name in sorted(filenames):
            p = os.path.join(dirpath, name)
            if not os.path.islink(p) and macho_kind(p):
                yield p


class Tools:
    def __init__(self, bindir):
        self.nm = os.path.join(bindir, 'llvm-nm')
        self.otool = os.path.join(bindir, 'llvm-otool')
        for t in (self.nm, self.otool):
            if not os.access(t, os.X_OK):
                sys.exit('symbol-inventory: %s not found (use --tools)' % t)

    def run(self, cmd):
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                           universal_newlines=True, errors='replace')
        return r.stdout if r.returncode == 0 else ''

    def arch_args(self, path):
        return ['--arch=' + ARCH] if macho_kind(path) == 'fat' else []

    def load_commands(self, path):
        """(file type, install name, [(command, dylib)]) of a Mach-O file."""
        out = self.run([self.otool, '-arch', ARCH, '-h', '-l', path]
                       if macho_kind(path) == 'fat' else [self.otool, '-h', '-l', path])
        filetype, install, dylibs, cmd = None, None, [], None
        self.last_minos = None
        lines = out.splitlines()
        for i, line in enumerate(lines):
            words = line.split()
            # "magic cputype cpusubtype caps filetype ncmds ..." header row
            if filetype is None and len(words) >= 5 and words[0].startswith('0x'):
                filetype = words[4]
            if words[:1] == ['cmd'] and len(words) > 1:
                cmd = words[1]
            elif words[:1] == ['minos'] and len(words) > 1 and not self.last_minos:
                self.last_minos = words[1]
            elif (words[:1] == ['version'] and cmd == 'LC_VERSION_MIN_MACOSX'
                  and not self.last_minos):
                self.last_minos = words[1]
            elif words[:1] == ['name'] and cmd and cmd.endswith('DYLIB') and len(words) > 1:
                if cmd == 'LC_ID_DYLIB':
                    install = words[1]
                else:
                    dylibs.append((cmd, words[1]))
                cmd = None
        return filetype, install, dylibs

    def exports(self, path):
        out = self.run([self.nm, '-g', '--defined-only', '-m'] + self.arch_args(path) + [path])
        syms = set()
        for line in out.splitlines():
            words = line.split()
            if 'external' in words:
                i = words.index('external')
                if i + 1 < len(words):
                    syms.add(words[i + 1])
        return syms

    def imports(self, path):
        """[(symbol, library short name, weak)]."""
        out = self.run([self.nm, '-u', '-m'] + self.arch_args(path) + [path])
        result = []
        for line in out.splitlines():
            words = line.split()
            if 'external' not in words:
                continue
            i = words.index('external')
            if i + 1 >= len(words):
                continue
            sym = words[i + 1]
            lib = None
            if '(from' in words:
                lib = ' '.join(words[words.index('(from') + 1:]).rstrip(')')
            result.append((sym, lib, 'weak' in words[:i]))
        return result


def short_name(install):
    """The name llvm-nm prints for a library: libSystem, Foundation, libc++."""
    base = os.path.basename(install)
    if base.endswith('.dylib'):
        base = base[:-len('.dylib')]
    # libSystem.B -> libSystem, libc++.1 -> libc++
    head, _, tail = base.partition('.')
    return head if tail else base


def library_kind(filetype):
    return filetype in ('DYLIB', '6')     # MH_DYLIB


def scan_sysroot(tools, sysroot):
    """{install name: {'file', 'exports', 'reexports'}} for every dylib."""
    libs = {}
    for path in walk_machos(sysroot):
        filetype, install, dylibs = tools.load_commands(path)
        if not install or not library_kind(filetype):
            continue
        libs[install] = {
            'file': os.path.relpath(path, sysroot),
            'installed': install_exists(sysroot, install),
            'exports': tools.exports(path),
            'reexports': [d for c, d in dylibs if c == 'LC_REEXPORT_DYLIB'],
        }
    return libs


def resolve_in_sysroot(sysroot, install):
    """The file dyld would load for install, relative to the sysroot, with
    symlinks resolved inside it; None if there is none."""
    if not install.startswith('/'):
        return None        # @rpath, @loader_path...
    path, seen = install, 0
    while seen < 40:
        full = os.path.join(sysroot, path.lstrip('/'))
        if not os.path.islink(full):
            return path.lstrip('/') if os.path.isfile(full) else None
        target = os.readlink(full)
        path = target if target.startswith('/') else \
            os.path.normpath(os.path.join(os.path.dirname(path), target))
        seen += 1
    return None


def install_exists(sysroot, install):
    return resolve_in_sysroot(sysroot, install) is not None


class Matcher:
    """Finds the Minkar library an import resolves to: the one with that
    install name; else the file at that path in the sysroot, as dyld would
    load it; else one with the same short name, which dyld would not find
    (reported)."""

    def __init__(self, sysroot, libs):
        self.sysroot = sysroot
        self.by_file = {os.path.normpath(l['file']): n for n, l in libs.items()}
        self.by_short = {}
        for n in libs:
            self.by_short.setdefault(short_name(n), n)
        self.libs = libs

    def __call__(self, install):
        """(Minkar install name or None, found only by short name)."""
        if install in self.libs:
            return install, False
        f = resolve_in_sysroot(self.sysroot, install)
        if f and os.path.normpath(f) in self.by_file:
            return self.by_file[os.path.normpath(f)], False
        cand = self.by_short.get(short_name(install))
        return (cand, True) if cand else (None, False)


def resolve_exports(libs):
    """Each library's exports plus those it re-exports, recursively."""
    memo = {}

    def visit(name, stack):
        if name in memo:
            return memo[name]
        lib = libs.get(name)
        if lib is None or name in stack:
            return set()
        syms = set(lib['exports'])
        for r in lib['reexports']:
            syms |= visit(r, stack | {name})
        memo[name] = syms
        return syms

    return {name: visit(name, frozenset()) for name in libs}


SYSTEM_PREFIXES = ('/System/Library/', '/usr/lib/')


def app_roots(roots):
    """(app name, path) for each app: a bundle or binary given directly, or
    each entry of a directory of them."""
    for root in roots:
        root = os.path.abspath(root.rstrip('/'))
        if os.path.isdir(root) and not os.path.splitext(root)[1]:
            for name in sorted(os.listdir(root)):
                yield os.path.splitext(name)[0], os.path.join(root, name)
        else:
            yield os.path.splitext(os.path.basename(root))[0], root


def scan_apps(tools, roots):
    """([(app, binary, library install name, symbol, weak)], {app: minos}).

    Only imports from system libraries count: frameworks and dylibs that an
    app carries inside its bundle (@rpath/..., @loader_path/...) are its
    own business, though their imports from the system do count."""
    rows, minos = [], {}
    for app, root in app_roots(roots):
        for path in walk_machos(root):
            _, _, dylibs = tools.load_commands(path)
            if path == root or '/Contents/MacOS/' in path:
                minos.setdefault(app, tools.last_minos)
            by_short = {}
            for _, d in dylibs:
                by_short.setdefault(short_name(d), d)
            rel = os.path.relpath(path, os.path.dirname(root))
            for sym, lib, weak in tools.imports(path):
                if lib is None:
                    continue    # flat namespace or the binary's own symbols
                install = by_short.get(lib, lib)
                if install.startswith(SYSTEM_PREFIXES):
                    rows.append((app, rel, install, sym, weak))
    return rows, minos


def write_tsv(path, header, rows):
    with open(path, 'w') as f:
        f.write('\t'.join(header) + '\n')
        for r in rows:
            f.write('\t'.join(str(x) for x in r) + '\n')


def report(sysroot, libs, exported, imports, minos, out, top):
    # library -> symbol -> set of apps
    used = collections.defaultdict(lambda: collections.defaultdict(set))
    weak_only = collections.defaultdict(lambda: collections.defaultdict(lambda: True))
    for app, _, lib, sym, weak in imports:
        used[lib][sym].add(app)
        weak_only[lib][sym] &= weak
    apps = sorted({r[0] for r in imports})

    match = Matcher(sysroot, libs)
    summary, missing_rows = [], []
    for lib in sorted(used, key=lambda l: (-len(used[l]), l)):
        syms = used[lib]
        minkar_lib, renamed = match(lib)
        have = exported.get(minkar_lib) if minkar_lib else None
        present = [s for s in syms if have is not None and s in have]
        missing = sorted((s for s in syms if have is None or s not in have),
                         key=lambda s: (-len(syms[s]), s))
        for s in missing:
            missing_rows.append((lib, s, ','.join(sorted(syms[s])),
                                 'weak' if weak_only[lib][s] else ''))
        summary.append({
            'library': lib,
            'in_minkar': have is not None,
            'minkar_install_name': minkar_lib if renamed else None,
            'apps': len({a for s in syms for a in syms[s]}),
            'imported': len(syms),
            'present': len(present),
            'missing': len(missing),
            'coverage': round(100.0 * len(present) / len(syms), 1) if syms else 100.0,
            'top_missing': [{'symbol': s, 'apps': len(syms[s])} for s in missing[:top]],
        })

    per_app = []
    for a in apps:
        uniq = {(lib, sym) for app, _, lib, sym, _ in imports if app == a}
        ok = 0
        for lib, sym in uniq:
            m, _ = match(lib)
            ok += bool(m and sym in exported[m])
        per_app.append({'app': a, 'minos': minos.get(a), 'imported': len(uniq),
                        'present': ok,
                        'coverage': round(100.0 * ok / len(uniq), 1) if uniq else 100.0})

    bad_names = sorted((name, libs[name]['file']) for name in libs
                       if not libs[name]['installed'])
    total = sum(s['imported'] for s in summary)
    present = sum(s['present'] for s in summary)
    data = {
        'apps': per_app,
        'libraries': summary,
        'total': {'imported': total, 'present': present,
                  'coverage': round(100.0 * present / total, 1) if total else 100.0},
        'minkar_libraries': len(libs),
        'minkar_exports': sum(len(v) for v in exported.values()),
        'install_names_not_found': [{'install_name': n, 'file': f} for n, f in bad_names],
    }
    with open(os.path.join(out, 'coverage.json'), 'w') as f:
        json.dump(data, f, indent=1, sort_keys=True)
    write_tsv(os.path.join(out, 'missing.tsv'), ('library', 'symbol', 'apps', 'weak'),
              missing_rows)

    with open(os.path.join(out, 'coverage.md'), 'w') as f:
        f.write('# Symbol coverage\n\n')
        f.write('Minkar: %d libraries, %d exported symbols (re-exports included).\n\n'
                % (data['minkar_libraries'], data['minkar_exports']))
        if bad_names:
            f.write('## Install names dyld would not find\n\n'
                    'These libraries name themselves with a path that does not '
                    'exist in the sysroot; programs linked against them cannot '
                    'load them.\n\n| Install name | File in the sysroot |\n'
                    '| --- | --- |\n')
            for n, fl in bad_names:
                f.write('| `%s` | `/%s` |\n' % (n, fl))
            f.write('\n')
        if not apps:
            f.write('No reference apps were given (--apps): only the inventory '
                    '(exports.tsv) was written.\n')
            return data
        f.write('Imported symbols found in Minkar: %d of %d (%.1f%%).\n\n'
                % (present, total, data['total']['coverage']))
        f.write('## By app\n\nSymbols imported from system libraries; the '
                'macOS version is the minimum the app was built for.\n\n'
                '| App | macOS | Imported | In Minkar | Coverage |\n'
                '| --- | --- | ---: | ---: | ---: |\n')
        for a in per_app:
            f.write('| %s | %s | %d | %d | %.1f%% |\n'
                    % (a['app'], a['minos'] or '?', a['imported'], a['present'],
                       a['coverage']))
        f.write('\n## By library\n\n'
                '| Library | Apps | Imported | In Minkar | Missing | Coverage |\n'
                '| --- | ---: | ---: | ---: | ---: | ---: |\n')
        for s in summary:
            name = '`%s`' % s['library']
            if not s['in_minkar']:
                name += ' (absent)'
            elif s['minkar_install_name']:
                name += ' (Minkar: `%s`)' % s['minkar_install_name']
            f.write('| %s | %d | %d | %d | %d | %.1f%% |\n'
                    % (name, s['apps'], s['imported'], s['present'], s['missing'],
                       s['coverage']))
        f.write('\n## Most used missing symbols\n\n'
                'Up to %d per library, the most used first; all of them are in '
                'missing.tsv.\n' % top)
        for s in summary:
            if not s['top_missing']:
                continue
            f.write('\n### `%s`\n\n' % s['library'])
            for m in s['top_missing']:
                f.write('- `%s` (%d app%s)\n'
                        % (m['symbol'], m['apps'], '' if m['apps'] == 1 else 's'))
    return data


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--sysroot', required=True, help='Minkar root file system')
    p.add_argument('--apps', action='append', default=[],
                   help='macOS app, binary or directory of them (repeatable)')
    p.add_argument('--out', required=True, help='report directory')
    p.add_argument('--tools', default=DEFAULT_TOOLS,
                   help='directory with llvm-nm and llvm-otool')
    p.add_argument('--top', type=int, default=25,
                   help='missing symbols listed per library in coverage.md')
    args = p.parse_args(argv)

    tools = Tools(args.tools)
    os.makedirs(args.out, exist_ok=True)

    libs = scan_sysroot(tools, args.sysroot)
    exported = resolve_exports(libs)
    write_tsv(os.path.join(args.out, 'exports.tsv'), ('library', 'symbol'),
              ((lib, s) for lib in sorted(exported) for s in sorted(exported[lib])))

    imports, minos = scan_apps(tools, args.apps)
    write_tsv(os.path.join(args.out, 'imports.tsv'),
              ('app', 'binary', 'library', 'symbol', 'weak'),
              ((a, b, l, s, 'weak' if w else '') for a, b, l, s, w in imports))

    data = report(args.sysroot, libs, exported, imports, minos, args.out, args.top)
    print('symbol-inventory: %d libraries, %d exports' %
          (data['minkar_libraries'], data['minkar_exports']))
    if imports:
        print('symbol-inventory: %d apps, %d of %d imported symbols in Minkar (%.1f%%)'
              % (len(data['apps']), data['total']['present'], data['total']['imported'],
                 data['total']['coverage']))
    print('symbol-inventory: report in %s' % os.path.join(args.out, 'coverage.md'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
