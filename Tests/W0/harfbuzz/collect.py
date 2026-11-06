#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after HarfBuzz's test build: puts in BUILD/w0
# what its tests need on Minkar (their programs and data) and tests.txt,
# one test a line: name, working directory, command, with @B@ for the build
# directory and @S@ for the source one. Minkar has no Python, so the two
# Python runners become run.sh's sh ones (shape.sh, fuzz.sh); the subset
# tests, which compare through Python's fontTools, go to skipped.txt.
import json, os, shlex, shutil, subprocess, sys

build, src, here = (os.path.realpath(p) for p in sys.argv[1:4])
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
    if prog == 'run-tests.py' and '/test/shape/' in cmd[0]:
        cmd = ['@HERE@/shape.sh'] + cmd[1:]
    elif prog == 'run-fuzzer-tests.py':
        cmd = ['@HERE@/fuzz.sh'] + cmd[1:]
    elif prog.endswith('.py') or prog.startswith('python'):
        skipped.append(t['name'] + ' (a Python runner: ' + prog + ')')
        continue
    env = ['%s=%s' % (k, mark(v)) for k, v in sorted(t.get('env', {}).items())
           if k.startswith(('G_TEST', 'HB_', 'HAVE_'))]
    cmd = [mark(c) for c in cmd]
    wd = mark(t.get('workdir') or build)
    if env:
        env = ['env'] + env
    lines.append('\t'.join([t['name'], wd, ' '.join(shlex.quote(c) for c in env + cmd)]))

for d in ('test/api', 'test/shape', 'test/fuzzing', 'test/subset/data/fonts', 'perf/fonts', 'perf/texts'):
    shutil.copytree(os.path.join(src, d), os.path.join(out, 'src', d), symlinks=False)
for sh in ('shape.sh', 'fuzz.sh'):
    shutil.copy2(os.path.join(here, sh), out)
with open(os.path.join(out, 'tests.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
with open(os.path.join(out, 'skipped.txt'), 'w') as f:
    f.write('\n'.join(skipped) + '\n')
print('collect.py: %d tests, %d skipped' % (len(lines), len(skipped)))
