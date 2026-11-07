#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after libjpeg-turbo's test build: puts in
# BUILD/w0 the programs its CTest list runs (cjpeg, djpeg, jpegtran,
# example, md5cmp), its test images, and tests.txt, one test a line in
# CTest's order (later tests read what earlier ones wrote): name, then the
# command, with @W@ for w0 and @S@ for the source directory. Every test
# runs from the build directory; run.sh runs them from a scratch one.
import json, os, shlex, shutil, subprocess, sys

build, src = (os.path.realpath(p) for p in sys.argv[1:3])
out = os.path.join(build, 'w0')
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out)
info = json.loads(subprocess.check_output(['ctest', '--show-only=json-v1'], cwd=build))
shutil.copytree(os.path.join(src, 'testimages'), os.path.join(out, 'src', 'testimages'))

programs = set()
lines = []
for t in info['tests']:
    props = {p['name']: p['value'] for p in t.get('properties', [])}
    assert os.path.realpath(props['WORKING_DIRECTORY']) == build, t['name']
    prog = os.path.realpath(t['command'][0])
    assert prog.startswith(build + '/'), prog
    programs.add(prog)
    args = []
    for a in t['command'][1:]:
        p = os.path.realpath(os.path.join(build, a)) if '/' in a and not a[0].isdigit() else None
        if p and p.startswith(src + '/'):
            a = '@S@/' + p[len(src) + 1:]
        args.append(shlex.quote(a).replace("'@S@", "@S@'"))
    lines.append('%s\t@W@/%s %s' % (t['name'], os.path.basename(prog), ' '.join(args)))
for prog in programs:
    shutil.copy2(prog, out)
open(os.path.join(out, 'tests.txt'), 'w').write('\n'.join(lines) + '\n')
print('collect.py: %d tests, programs %s' % (len(lines), ' '.join(sorted(os.path.basename(p) for p in programs))))
