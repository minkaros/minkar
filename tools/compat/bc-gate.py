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

"""Gate BC: the designated apps' published binaries, run in the image.

  bc-gate.py --image IMG --apps DIR [--results TSV] [--update] [boottest options]

Downloads the apps of tools/symbols/reference-apps.tsv into DIR (with
fetch-reference-apps.py: kept, and checked by SHA-256; never put in the
repository or the image), reads each one's bundle, executable, minimum
version and architectures from its unpacked copy, and boots the image
with boottest's gate BC: each app's archive is copied into the VM and
Tests/BC/bc-run.sh installs it as a user would and starts it. Each app
gets a result, from the furthest it got:

  install, signature, no-load, before-main, at-start, no-window, window

and min-version when its minimum is above the ProductVersion Minkar
reports and it did not show a window (the reason says how far it got).

The results are compared with the last ones (--results, by default
Docs/progress/compatibility.tsv, the source of Docs/Compatibility.md): an
app that does not start yet does not fail the gate, but one that got
less far than before with the same version does (a regression), as does a
boot that does not reach the end. --update writes the new results there;
otherwise they go to --out.

Exit status: 0 with no regression, 1 otherwise.
"""

import argparse
import datetime
import json
import os
import plistlib
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, '..', '..'))
MANIFEST = os.path.join(TOP, 'tools', 'symbols', 'reference-apps.tsv')
FETCH = os.path.join(TOP, 'tools', 'symbols', 'fetch-reference-apps.py')
BOOTTEST = os.path.join(TOP, 'Image', 'boottest.py')
RESULTS = os.path.join(TOP, 'Docs', 'progress', 'compatibility.tsv')
REMOTE = '/private/tmp/bc'
RUNNER = '/usr/local/tests/bc/bc-run.sh'

STAGES = ['install', 'signature', 'no-load', 'before-main', 'at-start', 'no-window', 'window']
COLUMNS = ['app', 'version', 'minimum', 'arch', 'result', 'stage', 'menus', 'reason']

CPU = {0x01000007: 'x86_64', 0x0100000c: 'arm64', 7: 'i386'}


def read_manifest(path):
    apps = []
    with open(path) as f:
        for line in f:
            line = line.rstrip('\n')
            if line.strip() and not line.startswith('#'):
                name, version, minimum, url, sha = line.split('\t')
                apps.append(dict(name=name, version=version, minimum=minimum, url=url))
    return apps


def archive_path(apps_dir, app):
    # The name fetch-reference-apps.py gives the download.
    ext = '.zip' if app['url'].endswith('.zip') else '.dmg' if app['url'].endswith('.dmg') \
        else '.tar'
    return os.path.join(apps_dir, 'downloads', '%s-%s%s' % (app['name'], app['version'], ext))


def find_bundle(root):
    """The first .app, shallowest first, not inside another bundle."""
    level = [root]
    for _ in range(4):
        nxt = []
        for d in level:
            try:
                names = sorted(os.listdir(d))
            except OSError:
                continue
            for n in names:
                p = os.path.join(d, n)
                if os.path.isdir(p) and not os.path.islink(p):
                    if n.endswith('.app'):
                        return p
                    nxt.append(p)
        level = nxt
    return None


def architectures(path):
    with open(path, 'rb') as f:
        head = f.read(4096)
    magic = struct.unpack('>I', head[:4])[0]
    if magic in (0xcafebabe, 0xcafebabf):
        n = struct.unpack('>I', head[4:8])[0]
        size = 20 if magic == 0xcafebabe else 32
        cpus = [struct.unpack('>I', head[8 + i * size:12 + i * size])[0] for i in range(n)]
    elif magic in (0xcffaedfe, 0xcefaedfe):
        cpus = [struct.unpack('<I', head[4:8])[0]]
    else:
        return '?'
    return '+'.join(sorted(CPU.get(c, hex(c)) for c in cpus))


def describe(apps_dir, app):
    bundle = find_bundle(os.path.join(apps_dir, 'apps', app['name']))
    if bundle is None:
        return None
    with open(os.path.join(bundle, 'Contents', 'Info.plist'), 'rb') as f:
        info = plistlib.load(f)
    exe = info.get('CFBundleExecutable') or os.path.splitext(os.path.basename(bundle))[0]
    return dict(bundle=os.path.basename(bundle), exe=exe,
                arch=architectures(os.path.join(bundle, 'Contents', 'MacOS', exe)))


def version_key(v):
    return [int(x) if x.isdigit() else 0 for x in v.split('.')] + [0, 0, 0]


def plan(apps, wait):
    steps = [{'ssh': 'mkdir -p %s' % REMOTE}]
    for app in apps:
        d = app['described']
        remote = '%s/%s.archive' % (REMOTE, app['name'])
        steps += [{'put': [app['archive'], remote]},
                  {'ssh': "sh %s '%s' '%s' '%s' '%s' %d" % (
                      RUNNER, app['name'], remote, d['bundle'], d['exe'], wait)},
                  {'expect': 'bc: %s end' % app['name']}]
    return steps


def parse_log(path):
    product, found = None, {}
    with open(path, 'rb') as f:
        for raw in f:
            line = raw.decode('utf-8', 'replace').rstrip('\r\n')
            if not line.startswith('ssh| bc: '):
                continue
            text = line[len('ssh| bc: '):]
            if text.startswith('product-version '):
                product = text.split()[1]
                continue
            name, _, rest = text.partition(' ')
            if not rest.startswith('stage='):
                continue
            fields = {}
            for key in ('stage', 'result', 'menus'):
                value, _, rest = rest.partition(' ')
                fields[key] = value.split('=', 1)[1]
            fields['reason'] = rest.split('=', 1)[1] if '=' in rest else ''
            found[name] = fields
    return product, found


def judge(app, got, product):
    stage = int(got['stage']) if got else -1
    word = got['result'] if got else 'not-run'
    reason = got['reason'] if got else 'the gate did not reach it'
    below = product and version_key(app['minimum']) > version_key(product)
    if below and stage < STAGES.index('window'):
        reason = 'requires %s, Minkar reports %s; run anyway: %s: %s' % (
            app['minimum'], product, word, reason)
        word = 'min-version'
    elif below:
        reason = 'requires %s, Minkar reports %s; %s' % (app['minimum'], product, reason)
    return dict(app=app['name'], version=app['version'], minimum=app['minimum'],
                arch=app['described']['arch'] if app['described'] else '?',
                result=word, stage=str(stage), menus=(got or {}).get('menus', 'no'),
                reason=reason)


def read_results(path):
    rows = {}
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                if line.startswith('#') or not line.strip():
                    continue
                fields = line.rstrip('\n').split('\t')
                if fields[0] == 'app':
                    continue
                rows[fields[0]] = dict(zip(COLUMNS, fields))
    return rows


def write_results(path, rows, product, seconds):
    with open(path, 'w') as f:
        f.write('# Gate BC\'s results (tools/compat/bc-gate.py --update), the source of\n'
                '# Docs/Compatibility.md ("bmake progress"). Stages: %s.\n'
                '# Checked %s; ProductVersion %s; the gate took %d minutes.\n'
                % (', '.join('%d %s' % x for x in enumerate(STAGES)),
                   datetime.date.today().isoformat(), product or '?', round(seconds / 60)))
        f.write('\t'.join(COLUMNS) + '\n')
        for r in rows:
            f.write('\t'.join(r[c].replace('\t', ' ') for c in COLUMNS) + '\n')


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--image', required=True)
    p.add_argument('--apps', required=True, help='download and unpack the apps here')
    p.add_argument('--manifest', default=MANIFEST)
    p.add_argument('--results', default=RESULTS, help='the last results, compared against')
    p.add_argument('--out', help='write the results here (default: next to --log)')
    p.add_argument('--update', action='store_true', help='write the results to --results')
    p.add_argument('--log', default='boottest-BC.log')
    p.add_argument('--wait', type=int, default=int(os.environ.get('BC_WAIT', 40)),
                   help='seconds each app gets to show a window (default: $BC_WAIT or 40)')
    p.add_argument('--timeout', type=int, default=int(os.environ.get('BC_TIMEOUT', 7200)))
    p.add_argument('--only', action='append', help='only this app (repeatable; for development)')
    args, boottest_args = p.parse_known_args(argv)

    started = time.monotonic()
    fetch = subprocess.run([sys.executable, '-B', FETCH, '--dest', args.apps,
                            '--manifest', args.manifest])
    apps = [a for a in read_manifest(args.manifest) if not args.only or a['name'] in args.only]
    runnable = []
    for app in apps:
        app['archive'] = archive_path(args.apps, app)
        app['described'] = describe(args.apps, app) if os.path.exists(app['archive']) else None
        if app['described']:
            runnable.append(app)
        else:
            print('bc-gate: %s %s: not downloaded or no app in it' % (app['name'], app['version']),
                  file=sys.stderr)

    plan_path = os.path.join(args.apps, 'bc-plan.json')
    with open(plan_path, 'w') as f:
        json.dump(plan(runnable, args.wait), f, indent=1)
    boot = subprocess.run([sys.executable, '-B', BOOTTEST, '--image', args.image, '--gate', 'BC',
                           '--bc-plan', plan_path, '--log', args.log,
                           '--timeout', str(args.timeout)] + boottest_args)
    seconds = time.monotonic() - started
    product, found = parse_log(args.log) if os.path.exists(args.log) else (None, {})

    last = read_results(args.results)
    rows, regressions = [], []
    for app in apps:
        r = judge(app, found.get(app['name']), product)
        rows.append(r)
        before = last.get(app['name'])
        if before and before['version'] == r['version'] and int(r['stage']) < int(before['stage']):
            regressions.append('%s: %s (stage %s) before, %s (stage %s) now: %s' % (
                r['app'], before['result'], before['stage'], r['result'], r['stage'], r['reason']))
        print('bc-gate: %-16s %-12s %s' % (r['app'], r['result'], r['reason'][:110]))

    out = args.results if args.update else (
        args.out or os.path.join(os.path.dirname(os.path.abspath(args.log)), 'bc-results.tsv'))
    write_results(out, rows, product, seconds)
    print('bc-gate: %d apps, %d with a window, in %d minutes; results in %s' % (
        len(rows), sum(r['result'] == 'window' for r in rows), round(seconds / 60), out))
    for line in regressions:
        print('bc-gate: REGRESSION %s' % line, file=sys.stderr)
    if fetch.returncode != 0:
        print('bc-gate: some downloads failed (above)', file=sys.stderr)
    if boot.returncode != 0:
        print('bc-gate: the boot did not reach the end of the gate', file=sys.stderr)
    return 1 if regressions or boot.returncode != 0 else 0


if __name__ == '__main__':
    sys.exit(main())
