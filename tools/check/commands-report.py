#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""The command-line tools section of Docs/Userland.md.

  commands-report.py --sysroot S [--repo R] [--since COMMIT] --data FILE
  commands-report.py --data FILE --page Docs/Userland.md

The first form measures and writes FILE (Docs/progress/commands.tsv): the
commands in the sysroot's /bin, /sbin, /usr/bin and /usr/sbin; the command
directories in the repository's BSD/ tree now and at COMMIT (default
60aa4ff4b1, where Minkar left ravynOS); and which commands of
reference-commands.txt (next to this script) the sysroot has.

The second form, run by "bmake progress", replaces what lies between the
page's <!-- commands:begin --> and <!-- commands:end --> markers with the
tables, from FILE and reference-commands.txt only, so CI can check the page
is current without a build.
"""

import argparse
import os
import subprocess
import sys

DIRS = ['bin', 'sbin', 'usr/bin', 'usr/sbin']
TREES = ['BSD/bin', 'BSD/sbin', 'BSD/usr.bin', 'BSD/usr.sbin']

KINDS = [
    ('bsd', 'BSD and other portable tools',
     'Ported from FreeBSD main (or their own upstream), as the ones before.'),
    ('platform', "The reference system's own tools",
     'Written for Minkar; many need the graphical system or frameworks of '
     'later phases.'),
    ('darwin', "Darwin's own components",
     "Kept from the original open source (launchd, configd, syslogd...)."),
    ('fs', 'File system tools',
     'Need the file system in the kernel first (APFS and SMB come after 19.6.0).'),
    ('licence', 'Licence to approve',
     'Their licence (GPL, Vim, CDDL...) needs the project\'s approval first.'),
    ('builtin', 'Shell builtins as files',
     'The reference system has /usr/bin/cd and the like: small scripts that run the '
     'builtin.'),
]


def sysroot_commands(sysroot):
    names = {}
    for d in DIRS:
        path = os.path.join(sysroot, d)
        try:
            entries = os.listdir(path)
        except OSError:
            entries = []
        names[d] = sorted(e for e in entries
                          if os.path.isfile(os.path.join(path, e))
                          or os.path.islink(os.path.join(path, e)))
    return names


def tree_dirs(repo, commit, tree):
    ref = '%s:%s' % (commit, tree)
    out = subprocess.run(['git', '-C', repo, 'ls-tree', '-d', '--name-only',
                          ref], capture_output=True, text=True)
    return len(out.stdout.split()) if out.returncode == 0 else 0


def reference(path):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.split('#')[0].split()
            if len(line) == 3:
                rows.append(tuple(line))
    return rows


def here(name):
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), name)


def measure(a):
    """Writes the data file from a sysroot and the repository."""
    have = sysroot_commands(a.sysroot)
    present = set()
    for d in DIRS:
        present.update(have[d])
    lines = ['# Written by tools/check/commands-report.py --sysroot from a built sysroot;',
             '# "bmake progress" draws Docs/Userland.md\'s command tables from it.',
             'since\t%s' % a.since]
    lines += ['image\t/%s\t%d' % (d, len(have[d])) for d in DIRS]
    lines += ['tree\t%s\t%d\t%d' % (t, tree_dirs(a.repo, a.since, t), tree_dirs(a.repo, 'HEAD', t))
              for t in TREES]
    lines += ['present\t%s' % n for _, n, _ in reference(here('reference-commands.txt'))
              if n in present]
    with open(a.data, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def tables(data):
    """The section's lines, from the data file."""
    since, image, trees, present = '', [], [], set()
    with open(data) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            k, *v = line.rstrip('\n').split('\t')
            if k == 'since':
                since = v[0]
            elif k == 'image':
                image.append((v[0], int(v[1])))
            elif k == 'tree':
                trees.append((v[0], int(v[1]), int(v[2])))
            elif k == 'present':
                present.add(v[0])
    ref = reference(here('reference-commands.txt'))
    out = ['In the system image:', '', '| Directory | Commands |', '|---|---:|']
    out += ['| `%s` | %d |' % (d, n) for d, n in image]
    out += ['| **Total** | **%d** |' % sum(n for _, n in image), '']
    out += ['Command directories under `BSD/`, at `%s` (where Minkar left ravynOS) and now:' % since,
            '', '| Directory | Then | Now |', '|---|---:|---:|']
    out += ['| `%s` | %d | %d |' % t for t in trees]
    out += ['| **Total** | **%d** | **%d** |' % (sum(t[1] for t in trees), sum(t[2] for t in trees)),
            '']
    have_n = sum(1 for _, n, _ in ref if n in present)
    out += ['%d of the %d reference commands are in the image; %d are missing.'
            % (have_n, len(ref), len(ref) - have_n), '',
            '| Kind | In the image | Missing | What it takes |', '|---|---:|---:|---|']
    for kind, title, note in KINDS:
        rows = [n for _, n, k in ref if k == kind]
        h = sum(1 for n in rows if n in present)
        out.append('| %s | %d | %d | %s |' % (title, h, len(rows) - h, note))
    out += ['', 'Missing:', '']
    for kind, title, _ in KINDS:
        miss = sorted(n for _, n, k in ref if k == kind and n not in present)
        if miss:
            out.append('- **%s** (%d): %s' % (title, len(miss), ', '.join('`%s`' % n for n in miss)))
    return out


def page(data, path):
    begin, end = '<!-- commands:begin -->', '<!-- commands:end -->'
    with open(path) as f:
        old = f.read()
    if begin not in old or end not in old:
        sys.exit('commands-report.py: %s has no %s ... %s' % (path, begin, end))
    head, rest = old.split(begin, 1)
    _, tail = rest.split(end, 1)
    text = head + begin + '\n' + '\n'.join(tables(data)) + '\n' + end + tail
    if text != old:
        with open(path, 'w') as f:
            f.write(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sysroot')
    ap.add_argument('--repo', default='.')
    ap.add_argument('--since', default='60aa4ff4b1')
    ap.add_argument('--data', required=True)
    ap.add_argument('--page')
    a = ap.parse_args()
    if a.sysroot:
        measure(a)
    elif a.page:
        page(a.data, a.page)
    else:
        ap.error('--sysroot or --page')


if __name__ == '__main__':
    main()
