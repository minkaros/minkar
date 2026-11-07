#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after a meson project's test build (gate W0):
# puts in BUILD/w0 what its tests need on Minkar and tests.txt, one test a
# line: name, working directory, command, with @B@ for the build directory,
# @S@ for the source one and @HERE@ for w0. Usage:
#   meson-collect.py BUILD SRC DIR [RUNNER=SCRIPT]... [DATA]...
# The programs the tests run are copied; DATA are source directories the
# tests read. Minkar has no Python: a test run by the Python script RUNNER
# runs DIR's sh SCRIPT instead (copied to w0), and the other Python tests
# go to skipped.txt.
import json, os, shlex, shutil, subprocess, sys

build, src, here = (os.path.realpath(p) for p in sys.argv[1:4])
runners = dict(a.split('=', 1) for a in sys.argv[4:] if '=' in a)
data = [a for a in sys.argv[4:] if '=' not in a]
out = os.path.join(build, 'w0')
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out)
tests = json.loads(subprocess.check_output(['meson', 'introspect', '--tests', build]))

def mark(s):
    if not s.startswith('/') and not s.startswith('-') and os.path.exists(os.path.join(build, s)):
        s = os.path.join(build, s)
    s = os.path.normpath(s) if s.startswith('/') else s
    if s.startswith(build + '/'):
        rel = s[len(build) + 1:]
        if os.path.isfile(s) and os.access(s, os.X_OK):
            dst = os.path.join(out, 'build', rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(s, dst)
        return '@B@/' + rel
    if s == build:
        return '@B@'
    if s == src:
        return '@S@'
    if s.startswith(src + '/'):
        return '@S@/' + s[len(src) + 1:]
    return s

lines, skipped = [], []
for t in tests:
    cmd = t['cmd']
    prog = os.path.basename(cmd[0])
    rel = cmd[0][len(src) + 1:] if cmd[0].startswith(src + '/') else prog
    if rel in runners or prog in runners:
        cmd = ['@HERE@/' + runners.get(rel, runners.get(prog))] + cmd[1:]
    elif prog.endswith('.py') or prog.startswith('python'):
        skipped.append(t['name'] + ' (a Python runner: ' + prog + ')')
        continue
    env = ['%s=%s' % (k, mark(v)) for k, v in sorted(t.get('env', {}).items())
           if k not in ('PATH', 'LD_LIBRARY_PATH') and not k.startswith('DYLD_')]
    cmd = [mark(c) for c in cmd]
    wd = mark(t.get('workdir') or build)
    if env:
        env = ['env'] + env
    lines.append('\t'.join([t['name'], wd, ' '.join(shlex.quote(c) for c in env + cmd)]))

for d in data:
    shutil.copytree(os.path.join(src, d), os.path.join(out, 'src', d), symlinks=False)
for sh in set(runners.values()):
    shutil.copy2(os.path.join(here, sh), out)
with open(os.path.join(out, 'tests.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
with open(os.path.join(out, 'skipped.txt'), 'w') as f:
    f.write('\n'.join(skipped) + '\n')
print('collect.py: %d tests, %d skipped' % (len(lines), len(skipped)))
