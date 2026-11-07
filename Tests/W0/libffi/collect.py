#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after libffi's build: builds each test of its
# DejaGnu suite (testsuite/libffi.*) into BUILD/w0 as lib/libffi.exp does
# for clang (-W -Wall, once at -O0 and once at -O2, with the test's
# dg-options; bhaible's two programs once per DGTEST), and writes the
# output each must print (its dg-output patterns, joined, after Tcl's
# backslash substitution) next to it. tests.txt has a line per test:
# name, program, pattern file. dgmatch (dgmatch.c) checks an output.
# dg-options for other targets are left out; none of the suite's xfail
# targets is x86-64.
import fnmatch, os, re, shlex, shutil, subprocess, sys

build, src, dgmatch = (os.path.realpath(p) for p in sys.argv[1:4])
suite = os.path.join(src, 'testsuite')
out = os.path.join(build, 'w0')
shutil.rmtree(out, ignore_errors=True)
os.makedirs(os.path.join(out, 'bin'))
os.makedirs(os.path.join(out, 'expect'))
TARGET = 'x86_64-apple-darwin19'
cc = shlex.split(os.environ['CC'])
cxx = shlex.split(os.environ['CXX'])

def tcl_subst(s):
    return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), s)

def directives(path):
    text = open(path, errors='replace').read()
    opts = []
    # dg-options "X" or X, for every target or for { target selectors }.
    for m in re.finditer(r'\{\s*dg-options\s+(?:"([^"]*)"|(\S+))\s*(?:\{\s*target\s+([^}]*)\})?', text):
        targets = (m.group(3) or '*').replace(' or ', ' ').split()
        if any(fnmatch.fnmatch(TARGET, t) for t in targets):
            opts += shlex.split(m.group(1) if m.group(1) is not None else m.group(2))
    expect = ''.join(tcl_subst(m.group(1))
                     for m in re.finditer(r'\{\s*dg-output\s+"((?:[^"\\]|\\.)*)"', text))
    return opts, expect

lines, failed = [], []
def add(name, path, extra):
    opts, expect = directives(path)
    compiler = cxx if path.endswith('.cc') else cc
    for opt in ('-O0', '-O2'):
        test = '%s%s' % (name, opt)
        exe = os.path.join(out, 'bin', test.replace('/', '.'))
        cmd = compiler + ['-W', '-Wall', opt] + extra + opts + [
            '-I', os.path.join(build, 'include'), '-I', os.path.join(src, 'include'), '-I', build,
            '-I', os.path.dirname(path), path, '-o', exe,
            '-L', os.path.join(build, '.libs'), '-lffi', '-Wno-unused-command-line-argument']
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if r.returncode != 0:
            failed.append((test, r.stdout))
            continue
        pattern = os.path.join(out, 'expect', test.replace('/', '.'))
        open(pattern, 'w').write(expect)
        lines.append('%s\tbin/%s\texpect/%s' % (test, os.path.basename(exe), os.path.basename(pattern)))

for d in ('libffi.call', 'libffi.closures', 'libffi.complex', 'libffi.go', 'libffi.threads'):
    for f in sorted(os.listdir(os.path.join(suite, d))):
        if f.endswith(('.c', '.cc')):
            add('%s/%s' % (d[len('libffi.'):], f.rsplit('.', 1)[0]), os.path.join(suite, d, f), [])
bh = os.path.join(suite, 'libffi.bhaible')
warn = ['-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-uninitialized']
for i in range(1, 82):
    add('bhaible/test-call-%d' % i, os.path.join(bh, 'test-call.c'), ['-DDGTEST=%d' % i] + warn)
for i in range(1, 81):
    add('bhaible/test-callback-%d' % i, os.path.join(bh, 'test-callback.c'), ['-DDGTEST=%d' % i] + warn)

subprocess.check_call(cc + ['-O2', '-Wall', dgmatch, '-o', os.path.join(out, 'dgmatch')])
open(os.path.join(out, 'tests.txt'), 'w').write('\n'.join(lines) + '\n')
for test, log in failed:
    print('collect.py: %s did not build:\n%s' % (test, log), file=sys.stderr)
print('collect.py: %d tests, %d did not build' % (len(lines), len(failed)))
sys.exit(1 if failed else 0)
