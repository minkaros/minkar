#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""
Silent stubs: the exported functions of a sysroot's libraries and
frameworks that do nothing. A function whose code, without its frame
set-up, is only a return (of nothing, zero, a constant or its first
argument) is listed: most are stubs that answer without doing the work,
and should become noisy stubs (<minkar/stub.h>) or real code. Some are
genuinely empty (hooks, no-ops by design): the list is for review.

    python3 -B tools/symbols/trivial-functions.py \\
        --sysroot ../build/sysroot --out ../build/symbols/trivial.tsv

The binaries are only disassembled with llvm-objdump, never run.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOOLS = os.path.normpath(os.path.join(
    HERE, '..', '..', '..', 'build', 'Developer', 'Platforms',
    'Minkar.platform', 'Developer', 'Toolchains', 'Default.xctoolchain',
    'usr', 'bin'))

# Frame set-up and padding: not the function's work.
FRAME = re.compile(r'^(push(q)?\s+%rbp|mov(q)?\s+%rsp,\s*%rbp|pop(q)?\s+%rbp|'
                   r'nop\w*(\s.*)?|endbr64|int3|leave(q)?)$')
# What a do-nothing body may do before returning.
ANSWERS = [
    (re.compile(r'^xor(l|q)?\s+%(e|r)ax,\s*%(e|r)ax$'), 'zero'),
    (re.compile(r'^mov(l|q)?\s+\$-?0x[0-9a-f]+,\s*%(e|r)ax$'), 'constant'),
    (re.compile(r'^mov(l|q)?\s+%(e|r)di,\s*%(e|r)ax$'), 'argument'),
    (re.compile(r'^(xorps|pxor)\s+%xmm0,\s*%xmm0$'), 'zero'),
    (re.compile(r'^mov(b|w)?\s+\$-?0x[0-9a-f]+,\s*%(al|ax)$'), 'constant'),
    (re.compile(r'^movzbl\s+%dil,\s*%eax$'), 'argument'),
]
RET = re.compile(r'^ret(q)?$')


def is_macho(path):
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
    except OSError:
        return False
    return magic in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe')


def libraries(root):
    """The sysroot's dylibs and framework binaries (no programs)."""
    for top in ('usr/lib', 'System/Library/Frameworks',
                'System/Library/PrivateFrameworks'):
        base = os.path.join(root, top)
        for dirpath, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in ('Headers', 'Resources',
                                                     'Modules')]
            for f in files:
                p = os.path.join(dirpath, f)
                if os.path.islink(p):
                    continue
                if f.endswith('.dylib') or ('.framework' in dirpath and
                                            '.' not in f):
                    if is_macho(p):
                        yield p


def classify(body):
    """'empty', 'zero', 'constant', 'argument', or None for real code."""
    work = [i for i in body if not FRAME.match(i)]
    if not work or not RET.match(work[-1]):
        return None
    work = work[:-1]
    if not work:
        return 'empty'
    if len(work) == 1:
        for pattern, kind in ANSWERS:
            if pattern.match(work[0]):
                return kind
    return None


def functions(objdump, path, exported):
    """(name, instructions) of each exported function in __text."""
    r = subprocess.run([objdump, '-d', '--no-show-raw-insn',
                        '--no-leading-addr', '--section=__text',
                        '--x86-asm-syntax=att', path],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                       universal_newlines=True, errors='replace')
    name, body = None, []
    for line in r.stdout.splitlines():
        m = re.match(r'^<(.+)>:$', line.strip())
        if m:
            if name in exported:
                yield name, body
            name, body = m.group(1), []
            continue
        text = line.strip()
        if text and name is not None and not text.startswith(('Disassembly',
                                                              ';')):
            body.append(re.sub(r'\s+#.*$', '', text))
    if name in exported:
        yield name, body


def exported_text(nm, path):
    r = subprocess.run([nm, '-gU', '--defined-only', path],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                       universal_newlines=True, errors='replace')
    return {w[2] for w in (l.split() for l in r.stdout.splitlines())
            if len(w) == 3 and w[1] == 'T'}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--sysroot', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--tools', default=DEFAULT_TOOLS)
    a = p.parse_args()
    nm = os.path.join(a.tools, 'llvm-nm')
    objdump = os.path.join(a.tools, 'llvm-objdump')
    for t in (nm, objdump):
        if not os.access(t, os.X_OK):
            sys.exit('trivial-functions: %s not found (use --tools)' % t)
    rows = []
    for lib in sorted(libraries(a.sysroot)):
        exported = exported_text(nm, lib)
        for name, body in functions(objdump, lib, exported):
            kind = classify(body)
            if kind:
                rows.append((os.path.relpath(lib, a.sysroot), name, kind))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'w') as f:
        f.write('library\tsymbol\tbody\n')
        for row in rows:
            f.write('\t'.join(row) + '\n')
    per = {}
    for lib, _, _ in rows:
        per[lib] = per.get(lib, 0) + 1
    for lib, n in sorted(per.items(), key=lambda x: -x[1])[:25]:
        print('%5d  %s' % (n, lib))
    print('trivial-functions: %d functions do nothing, in %d libraries; '
          'list in %s' % (len(rows), len(per), a.out))


if __name__ == '__main__':
    main()
