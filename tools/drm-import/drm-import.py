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

"""
Imports files from drm-kmod (FreeBSD's port of Linux's DRM drivers) into
Kernel/Extensions/MinkarDRM/dist, as ADR 0002 says: exact copies, at the
version named in dist/FILES, and only files under MIT, BSD or a dual
license that offers one of them. GPL-only files are refused.

    drm-import.py [--source DIR] [--check]

DIR is a checkout of drm-kmod at the version FILES names (by default
$MINKAR_DRM_SOURCE). Each file listed in FILES is copied to
dist/drm-kmod/<its path>, and dist/MANIFEST records its path, SHA-256 and
license. --check copies nothing: it verifies that every file in dist is
listed, matches the source, and has an accepted license (exit status 1
otherwise).
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
DIST = os.path.join(ROOT, 'Kernel', 'Extensions', 'MinkarDRM', 'dist')

# The license of a file, from its first lines: (name, accepted).
MIT_TEXT = re.compile(r'Permission is hereby granted, free of charge')
BSD_TEXT = re.compile(r'Redistribution and use in source and binary forms')
SPDX = re.compile(r'SPDX-License-Identifier:\s*([^\n*]+?)\s*(\*/|$)', re.M)
ACCEPTED_SPDX = re.compile(r'\b(MIT|BSD-[0-9]-Clause[A-Za-z-]*|X11)\b')


def license_of(text):
    head = text[:4096]
    m = SPDX.search(head)
    if m:
        expr = m.group(1).strip()
        # A dual license is accepted when one of its choices is.
        choices = re.split(r'\s+OR\s+', expr.strip('()'))
        good = any(ACCEPTED_SPDX.search(c) for c in choices)
        return expr, good
    if MIT_TEXT.search(head):
        return 'MIT', True
    if BSD_TEXT.search(head):
        return 'BSD', True
    if 'GNU General Public License' in head:
        return 'GPL', False
    return 'unknown', False


def read_files():
    version, paths = None, []
    with open(os.path.join(DIST, 'FILES')) as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            if line.startswith('version '):
                version = line.split(None, 1)[1]
            else:
                paths.append(line)
    if version is None:
        sys.exit('drm-import: dist/FILES names no version')
    return version, paths


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__.strip().split('\n')[0])
    p.add_argument('--source', default=os.environ.get('MINKAR_DRM_SOURCE'))
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    version, paths = read_files()
    if args.source is None:
        sys.exit('drm-import: no source (--source or $MINKAR_DRM_SOURCE)')
    head = subprocess.run(['git', '-C', args.source, 'describe', '--tags', '--exact-match'],
                          capture_output=True, text=True).stdout.strip()
    if head != version:
        sys.exit('drm-import: %s is at %r, FILES wants %r' % (args.source, head or '?', version))

    bad = 0
    manifest = ['# drm-kmod %s: path, SHA-256, license (tools/drm-import)' % version]
    for path in paths:
        src = os.path.join(args.source, path)
        dst = os.path.join(DIST, 'drm-kmod', path)
        try:
            data = open(src, 'rb').read()
        except OSError:
            print('drm-import: missing in the source: %s' % path)
            bad += 1
            continue
        name, good = license_of(data.decode('utf-8', 'replace'))
        if not good:
            print('drm-import: refused (%s): %s' % (name, path))
            bad += 1
            continue
        if args.check:
            if not os.path.exists(dst) or open(dst, 'rb').read() != data:
                print('drm-import: differs from the source: %s' % path)
                bad += 1
        else:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as f:
                f.write(data)
        manifest.append('%s %s %s' % (path, sha256(data), name.replace(' ', '')))

    # Nothing in dist that FILES does not list.
    listed = set(paths)
    for base, _, names in os.walk(os.path.join(DIST, 'drm-kmod')):
        for n in names:
            rel = os.path.relpath(os.path.join(base, n), os.path.join(DIST, 'drm-kmod'))
            if rel not in listed:
                print('drm-import: not listed in FILES: %s' % rel)
                bad += 1
    if not args.check and bad == 0:
        with open(os.path.join(DIST, 'MANIFEST'), 'w') as f:
            f.write('\n'.join(manifest) + '\n')
    print('drm-import: %d file(s), %d problem(s)' % (len(paths), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
