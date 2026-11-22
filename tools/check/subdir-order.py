#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""Checks that a stage's Makefile builds what each directory needs first.

  subdir-order.py [--repo R] [MAKEFILE_DIR ...]

A stage whose Makefile does not set SUBDIR_PARALLEL builds its SUBDIR in
the order listed; its "dir: dep dep" rules say what each directory links
or includes, and do not order anything. An incremental build finds what
it needs in the SDK from an earlier build; a build from scratch does not,
and stops (CUPS before Security, AppKit before AudioToolbox, CoreData
before libsqlite3). This reads SUBDIR with bmake -V and prints each
dependency listed after the directory that needs it: those the stage's
rules name, and the frameworks (-framework X) and libraries (-lX) each
directory's own Makefile links that another directory of the stage
builds (FRAMEWORK = X, LIB = X). Exits 1 if there is any. Default:
Frameworks.
"""

import argparse
import os
import re
import subprocess
import sys

RULE = re.compile(r'^([A-Za-z0-9.][A-Za-z0-9./_+-]*):\s+([^=].*)$')
PRODUCT = re.compile(r'^(FRAMEWORK|LIB)\s*\??=\s*(\S+)\s*$', re.M)
LINKS = re.compile(r'-framework\s+([A-Za-z0-9_]+)|(?<![A-Za-z0-9_-])-l([A-Za-z0-9_+.]+)')


def makefile_text(repo, stage, directory):
    path = os.path.normpath(os.path.join(repo, stage, directory, 'Makefile'))
    try:
        with open(path) as f:
            # Comments are not flags.
            return re.sub(r'#.*', '', f.read())
    except OSError:
        return ''


def linked(repo, stage, order, position):
    """(directory, needed directory, what) for each -framework/-l another
    directory of the stage builds."""
    makes = {d: makefile_text(repo, stage, d) for d in position}
    builds = {}
    for d, text in makes.items():
        for kind, name in PRODUCT.findall(text):
            builds[('framework' if kind == 'FRAMEWORK' else 'lib', name)] = d
    for d, text in makes.items():
        for framework, lib in LINKS.findall(text):
            key = ('framework', framework) if framework else ('lib', lib)
            producer = builds.get(key)
            if producer is not None and producer != d:
                yield d, producer, ('-framework ' + framework) if framework else ('-l' + lib)


def subdir_order(repo, directory):
    out = subprocess.run(
        ['bmake', '-m', os.path.join(repo, 'BSD/share/mk'), '-C', os.path.join(repo, directory),
         '-V', 'SUBDIR', '-V', 'SUBDIR_PARALLEL'],
        capture_output=True, text=True, check=True).stdout.split('\n')
    return out[0].split(), out[1].strip() != '' if len(out) > 1 else False


def check(repo, directory):
    order, parallel = subdir_order(repo, directory)
    if parallel:
        return 0        # SUBDIR_DEPEND_ orders it
    position = {d: i for i, d in enumerate(order) if d != '.WAIT'}
    problems = 0
    with open(os.path.join(repo, directory, 'Makefile')) as makefile:
        for number, line in enumerate(makefile, 1):
            rule = RULE.match(line.rstrip())
            if rule is None or rule.group(1) not in position:
                continue
            target = rule.group(1)
            for dep in rule.group(2).split():
                if dep in position and position[dep] > position[target]:
                    problems += 1
                    print(f'{directory}/Makefile:{number}: {target} needs {dep}, '
                          f'which SUBDIR lists after it')
    for d, producer, what in sorted(set(linked(repo, directory, order, position))):
        if position[producer] > position[d]:
            problems += 1
            print(f'{directory}/{d}/Makefile: {d} links {what}, which {producer} builds, '
                  f'listed after it in SUBDIR')
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--repo', default=os.path.join(os.path.dirname(__file__), '..', '..'))
    parser.add_argument('dirs', nargs='*', default=['Frameworks'])
    args = parser.parse_args()
    repo = os.path.abspath(args.repo)
    problems = sum(check(repo, d) for d in args.dirs)
    if problems:
        print(f'subdir-order: {problems} dependencies built after what needs them', file=sys.stderr)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
