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

"""Writes the generated parts of Docs/Compatibility.md and Compatibility.es.md.

  matrix.py RESULTS.tsv FRAMEWORKS.tsv OUT.md OUT.es.md
  matrix.py --frameworks SYMBOLS_DIR FRAMEWORKS.tsv

RESULTS.tsv is Docs/progress/compatibility.tsv, which tools/compat/bc-gate.py
--update writes. FRAMEWORKS.tsv is Docs/progress/frameworks.tsv: for each
framework the designated apps import, how many apps import it, how many
distinct symbols, and how many of those Minkar exports. The second form
writes it from a tools/symbols/symbol-inventory.py report (its imports.tsv
and missing.tsv), run over the designated apps.

Each page keeps its hand-written text; only what lies between the markers
<!-- compat:matrix:begin/end --> and <!-- compat:frameworks:begin/end -->
is replaced. The frameworks table lists each imported framework Minkar
exports none of the symbols of, and each one the page's own hand-written
list below the markers gives a status ("- **Name**: Status — note"); the
first kind without a status is "Decide". Missing symbols of frameworks
Minkar has are the noisy-stub log's and symbol-inventory's business. Run through "bmake progress"; the output
depends only on the data and the pages, so CI can check they are current.
"""

import collections
import csv
import re
import sys

# The API 19 set (Docs/ROADMAP.md, lane BC, BC4): these apps by name, not
# every app whose minimum is 10.15 or lower.
SET_19 = ('Firefox', 'Thunderbird', 'HexFiend', 'TextMate', 'Platypus', 'KeepingYouAwake',
          'Sloth')
STAGES = ['install', 'signature', 'no-load', 'before-main', 'at-start', 'no-window', 'window']
ORDER = STAGES[:6] + ['min-version', 'not-run', 'window']

TEXT = {
    'en': dict(
        checked='Checked: %s',
        sets=('The API 19 set (BC4)', 'The next designated apps (BC5 on)'),
        head='| App | Version | Minimum | Architectures | Result | Menus | Reason |',
        summary='%d of %d apps show a window.',
        legend='Results, from the least far to the furthest:',
        words={'install': 'the archive could not be installed',
               'signature': 'the kernel refused its signature',
               'no-load': 'dyld could not load it (a library or a symbol is missing)',
               'before-main': "it ended before its libraries' initializers had run",
               'at-start': 'it ended after them, before showing a window',
               'no-window': 'it runs, but showed no window',
               'window': 'it shows a window',
               'min-version': ('its minimum version is above the one Minkar reports, '
                               'and it did not show a window (the reason says how far it got)'),
               'not-run': 'the gate did not reach it'},
        yes='yes', no='no',
        fw_head='| Framework | Apps | Symbols in Minkar | Status |'),
    'es': dict(
        checked='Comprobado: %s',
        sets=('El conjunto 19 (BC4)',
              'Las apps designadas siguientes (BC5 en adelante)'),
        head='| App | Versión | Mínima | Arquitecturas | Resultado | Menús | Motivo |',
        summary='%d de %d apps muestran una ventana.',
        legend='Resultados, del que menos llega al que más:',
        words={'install': 'no se pudo instalar el archivo descargado',
               'signature': 'el kernel rechazó su firma',
               'no-load': 'dyld no pudo cargarla (falta una biblioteca o un símbolo)',
               'before-main': 'terminó antes de que corrieran los inicializadores de sus bibliotecas',
               'at-start': 'terminó después, antes de mostrar una ventana',
               'no-window': 'corre, pero no mostró ninguna ventana',
               'window': 'muestra una ventana',
               'min-version': ('su versión mínima es mayor que la que reporta Minkar y no '
                               'mostró ventana (el motivo dice hasta dónde llegó)'),
               'not-run': 'el gate no llegó a ella'},
        yes='sí', no='no',
        fw_head='| Framework | Apps | Símbolos en Minkar | Estado |'),
}


def version_key(v):
    return [int(x) if x.isdigit() else 0 for x in v.split('.')] + [0, 0, 0]


def read(path):
    notes, rows, header = [], [], None
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('#'):
                notes.append(line[1:].strip())
            elif line.strip():
                fields = line.split('\t')
                if header is None:
                    header = fields
                else:
                    rows.append(dict(zip(header, fields)))
    return notes, rows


def matrix(notes, rows, lang):
    t = TEXT[lang]
    checked = next((n for n in notes if n.startswith('Checked ')), '')
    out = []
    if checked:
        out += ['_%s_' % (t['checked'] % checked[len('Checked '):]), '']
    out += [t['summary'] % (sum(r['result'] == 'window' for r in rows), len(rows)), '']
    groups = ([r for r in rows if r['app'] in SET_19], [r for r in rows if r['app'] not in SET_19])
    for title, group in zip(t['sets'], groups):
        out += ['### ' + title, '', t['head'], '| --- | --- | --- | --- | --- | --- | --- |']
        for r in sorted(group, key=lambda r: r['app'].lower()):
            reason = r['reason'].replace('|', '\\|') or '—'
            out.append('| %s | %s | %s | %s | `%s` | %s | %s |' % (
                r['app'], r['version'], r['minimum'], r['arch'].replace('+', ', '),
                r['result'], t['yes'] if r['menus'] == 'yes' else t['no'], reason))
        out.append('')
    out += [t['legend'], '']
    out += ['- `%s`: %s' % (w, t['words'][w]) for w in ORDER]
    return out


# A hand-written status line: "- **Name**: Status (detail) — note".
STATUS = re.compile(r'^- \*\*(?P<name>[^*]+)\*\*: (?P<status>(?:Plan|Partial|Decide|Out)\b[^—]*?)'
                    r'\s*(?:—.*)?$')


def statuses(text):
    found = {}
    for line in text.splitlines():
        m = STATUS.match(line)
        if m:
            found[m.group('name')] = m.group('status').strip()
    return found


def frameworks(table, text, lang):
    """The frameworks the designated apps import that Minkar lacks, or that
    the page gives a status: most apps first."""
    t = TEXT[lang]
    given = statuses(text)
    out = [t['fw_head'], '| --- | ---: | ---: | --- |']
    for f in sorted(table, key=lambda f: (-int(f['apps']), f['framework'].lower())):
        apps, total, have = int(f['apps']), int(f['symbols']), int(f['exported'])
        name = f['framework']
        if have and name not in given:
            continue
        status = given.get(name, 'Decide')
        out.append('| %s | %d | %d of %d | %s |' % (name, apps, have, total, status)
                   if lang == 'en' else
                   '| %s | %d | %d de %d | %s |' % (name, apps, have, total, status))
    return out


def splice(text, name, lines, path):
    begin, end = '<!-- compat:%s:begin -->' % name, '<!-- compat:%s:end -->' % name
    if begin not in text or end not in text:
        sys.exit('matrix.py: %s has no %s ... %s' % (path, begin, end))
    head, rest = text.split(begin, 1)
    _, tail = rest.split(end, 1)
    return head + begin + '\n' + '\n'.join(lines) + '\n' + end + tail


def read_frameworks(path):
    with open(path, encoding='utf-8') as f:
        return [r for r in csv.DictReader((l for l in f if not l.startswith('#')),
                                          delimiter='\t')]


def write_frameworks(symbols, path):
    """FRAMEWORKS.tsv from a symbol-inventory report."""
    def framework(library):
        m = re.match(r'/System/Library/(Private)?Frameworks/([^/]+)\.framework/', library)
        return m and m.group(2) + (' (private)' if m.group(1) else '')
    apps, imported, missing = (collections.defaultdict(set) for _ in range(3))
    with open('%s/imports.tsv' % symbols, encoding='utf-8') as f:
        for r in csv.DictReader(f, delimiter='\t'):
            name = framework(r['library'])
            if name:
                apps[name].add(r['app'])
                imported[name].add(r['symbol'])
    with open('%s/missing.tsv' % symbols, encoding='utf-8') as f:
        for r in csv.DictReader(f, delimiter='\t'):
            name = framework(r['library'])
            if name:
                missing[name].add(r['symbol'])
    lines = ['# Written by tools/compat/matrix.py --frameworks from a symbol-inventory',
             '# report over the designated apps (tools/symbols/reference-apps.tsv).',
             'framework\tapps\tsymbols\texported']
    for name in sorted(apps, key=str.lower):
        lines.append('%s\t%d\t%d\t%d' % (name, len(apps[name]), len(imported[name]),
                                          len(imported[name]) - len(missing[name])))
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


def main(argv):
    if len(argv) == 4 and argv[1] == '--frameworks':
        write_frameworks(argv[2], argv[3])
        return
    if len(argv) != 5:
        sys.exit('usage: matrix.py RESULTS.tsv FRAMEWORKS.tsv OUT.md OUT.es.md\n'
                 '       matrix.py --frameworks SYMBOLS_DIR FRAMEWORKS.tsv')
    notes, rows = read(argv[1])
    table = read_frameworks(argv[2])
    for path, lang in ((argv[3], 'en'), (argv[4], 'es')):
        with open(path, encoding='utf-8') as f:
            old = f.read()
        text = splice(old, 'matrix', matrix(notes, rows, lang), path)
        text = splice(text, 'frameworks', frameworks(table, text, lang), path)
        if text != old:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(text)


if __name__ == '__main__':
    main(sys.argv)
