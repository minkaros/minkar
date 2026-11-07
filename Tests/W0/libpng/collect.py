#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after libpng's test build: puts in BUILD/w0 its
# test programs, the PNG files they read and tests.txt, one test a line:
# name, then the command, with @S@ for the source directory. Each CTest
# test runs CMake's test.cmake, which runs one program with options and
# files; Minkar has no CMake, so run.sh runs the program itself.
import json, os, re, shlex, shutil, subprocess, sys

build, src = (os.path.realpath(p) for p in sys.argv[1:3])
out = os.path.join(build, 'w0')
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out)
info = json.loads(subprocess.check_output(['ctest', '--show-only=json-v1'], cwd=build))

def cmake_var(text, name):
    m = re.search(r'set\(%s "([^"]*)"\)' % name, text)
    return [v for v in m.group(1).split(';') if v] if m else []

def mark(p):
    p = os.path.realpath(p)
    return '@S@/' + p[len(src) + 1:] if p.startswith(src + '/') else p

lines = []
for t in info['tests']:
    cmd = t['command']
    prog = next(a.split('=', 1)[1] for a in cmd if a.startswith('-DTEST_COMMAND='))
    script = cmd[cmd.index('-P') + 1]
    text = open(script).read()
    shutil.copy2(prog, out)
    args = [os.path.basename(prog)] + cmake_var(text, 'TEST_OPTIONS') + \
        [mark(f) for f in cmake_var(text, 'TEST_FILES')]
    lines.append(t['name'] + '\t' + ' '.join(shlex.quote(a) for a in args))

for f in ('pngtest.png', 'contrib/pngsuite', 'contrib/testpngs'):
    s, d = os.path.join(src, f), os.path.join(out, 'src', f)
    os.makedirs(os.path.dirname(d), exist_ok=True)
    (shutil.copytree if os.path.isdir(s) else shutil.copy2)(s, d)
with open(os.path.join(out, 'tests.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
print('collect.py: %d tests' % len(lines))
