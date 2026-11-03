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

"""Writes Minkar's progress views from Docs/progress/status.toml.

    gen.py [--lang auto|en|es] STATUS.toml OUTDIR [PAGE.md ...]

Writes the metro map of milestones by plan and the work board to OUTDIR,
each in a light and a dark version (metro-light.svg, metro-dark.svg,
board-light.svg, board-dark.svg), and rewrites the summary table between
the progress:summary markers of each PAGE.md. The table's header, legend
and date line are in the page's language: Spanish for a page whose name
ends in .es.md, English otherwise, unless --lang says which. Plan and
milestone names, and the SVGs, stay in English.

Run through "bmake progress" at the top of the tree. Standard library
only: tomllib from Python 3.11, else the small reader below for the TOML
that status.toml uses. The output depends only on the data, so CI can
check that the committed files are current.
"""

import argparse
import ast
import os
import subprocess
import re
import sys
from html import escape

# ---------------------------------------------------------------- TOML

def _value(text):
    """A TOML value of the kinds status.toml uses: basic strings, integers,
    floats, booleans and arrays of them (Python's literals but for true
    and false)."""
    def convert(node):
        if isinstance(node, ast.Name) and node.id in ('true', 'false'):
            return node.id == 'true'
        if isinstance(node, ast.List):
            return [convert(n) for n in node.elts]
        return ast.literal_eval(node)
    return convert(ast.parse(text.strip(), mode='eval').body)


def _strip_comment(line):
    """The line without a # comment (one outside a string)."""
    quote = None
    for i, ch in enumerate(line):
        if quote:
            if ch == '\\':
                continue
            if ch == quote:
                quote = None
        elif ch in '"\'':
            quote = ch
        elif ch == '#':
            return line[:i]
    return line


def read_toml(path):
    """tomllib's result for path, with tomllib when there is one."""
    try:
        import tomllib
    except ImportError:
        tomllib = None
    if tomllib is not None:
        with open(path, 'rb') as f:
            return tomllib.load(f)

    root, table = {}, None
    table = root
    pending, key = None, None
    with open(path, encoding='utf-8') as f:
        for number, raw in enumerate(f, 1):
            line = _strip_comment(raw).strip()
            if pending is not None:
                # A value over several lines: until its brackets close.
                pending += ' ' + line
                if pending.count('[') == pending.count(']'):
                    table[key] = _value(pending)
                    pending = None
                continue
            if not line:
                continue
            m = re.fullmatch(r'\[\[\s*([\w.]+)\s*\]\]', line)
            if m:
                *parents, last = m.group(1).split('.')
                t = root
                for p in parents:
                    t = t.setdefault(p, {})
                    if isinstance(t, list):
                        t = t[-1]
                table = {}
                t.setdefault(last, []).append(table)
                continue
            m = re.fullmatch(r'\[\s*([\w.]+)\s*\]', line)
            if m:
                table = root
                for p in m.group(1).split('.'):
                    table = table.setdefault(p, {})
                continue
            m = re.fullmatch(r'([\w-]+)\s*=\s*(.+)', line)
            if not m:
                sys.exit('%s:%d: cannot read: %s' % (path, number, raw.rstrip()))
            key, text = m.group(1), m.group(2)
            if text.count('[') != text.count(']'):
                pending = text
            else:
                table[key] = _value(text)
    return root

# ---------------------------------------------------------------- drawing

FONT = ("-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, "
        "Arial, sans-serif")

THEMES = {
    'light': dict(bg='#ffffff', fg='#1f2328', muted='#59636e', faint='#d1d9e0',
                  col='#f6f8fa', card='#ffffff', cardline='#d1d9e0', gate='#1f2328',
                  colors=dict(blue='#0969da', purple='#8250df', orange='#bc4c00',
                              teal='#0f7b6f', pink='#bf3989', green='#1a7f37',
                              red='#cf222e', brown='#9a6700', slate='#57606a',
                              gray='#6e7781')),
    'dark': dict(bg='#0d1117', fg='#e6edf3', muted='#9198a1', faint='#3d444d',
                 col='#151b23', card='#0d1117', cardline='#3d444d', gate='#e6edf3',
                 colors=dict(blue='#4493f8', purple='#ab7df8', orange='#f0883e',
                             teal='#3bc4b5', pink='#f778ba', green='#3fb950',
                             red='#f85149', brown='#d29922', slate='#9198a1',
                             gray='#8b949e')),
}

STATES = ('done', 'current', 'partial', 'todo')


def tw(s, size):
    """A rough width of s in pixels, for placing labels."""
    return len(s) * size * 0.56


def wrap(s, n):
    out, cur = [], ''
    for w in s.split():
        if cur and len(cur) + 1 + len(w) > n:
            out.append(cur)
            cur = w
        else:
            cur = (cur + ' ' + w).strip()
    if cur:
        out.append(cur)
    return out


def num(v):
    """A coordinate as written in the SVG: no trailing .0."""
    return ('%.1f' % v).rstrip('0').rstrip('.')


def text(x, y, s, size, fill, weight=400, anchor='start', extra=''):
    return ('<text x="%s" y="%s" font-size="%s" font-weight="%s" fill="%s" '
            'text-anchor="%s"%s>%s</text>' % (num(x), num(y), size, weight, fill, anchor,
                                              ' ' + extra if extra else '', escape(s)))


def header(width, height, label, theme):
    return ['<svg xmlns="http://www.w3.org/2000/svg" width="%s" height="%s" '
            'viewBox="0 0 %s %s" font-family="%s" role="img" aria-label="%s">'
            % (num(width), num(height), num(width), num(height), FONT, escape(label)),
            '<title>%s</title>' % escape(label),
            '<rect width="%s" height="%s" fill="%s"/>' % (num(width), num(height),
                                                           theme['bg'])]

# ---------------------------------------------------------------- metro map

def metro(d, t):
    C = t['colors']
    lines, gates = d['line'], d['gate']
    top, step, left = 122, 64, 20
    rowy = {l['id']: top + i * step for i, l in enumerate(lines)}
    W = 1200
    H = top + (len(lines) - 1) * step + 100
    o = header(W, H, 'Minkar milestones by plan, %s' % d['updated'], t)
    o.append(text(left, 30, 'Minkar milestones by plan', 20, t['fg'], 700))
    o.append(text(left, 54, '%s · devel %s' % (d['updated'], d['commit']), 14, t['muted']))

    # Gate chips, guides and capsules, under the lines.
    chip_y = 84
    for g in gates:
        x, done = g['x'], g['state'] == 'done'
        w = max(42, tw(g['label'], 13.5) + 18)
        rows = g.get('rows')
        guide_to = (min(rowy[r] for r in rows) - 32) if rows else rowy[lines[0]['id']] - 12
        o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-opacity=".35" '
                 'stroke-width="1.5" stroke-dasharray="2 4"/>'
                 % (num(x), num(chip_y + 11), num(x), num(guide_to), t['gate']))
        if rows:
            ys = sorted(rowy[r] for r in rows)
            runs, start, prev = [], ys[0], ys[0]
            for y in ys[1:]:
                if y - prev > step:
                    runs.append((start, prev))
                    start = y
                prev = y
            runs.append((start, prev))
            for i, (a, b) in enumerate(runs):
                if i:
                    o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" '
                             'stroke-opacity=".35" stroke-width="1.5" stroke-dasharray="2 4"/>'
                             % (num(x), num(runs[i - 1][1] + 40), num(x), num(a - 32),
                                t['gate']))
                o.append('<rect x="%s" y="%s" width="52" height="%s" rx="14" fill="%s" '
                         'fill-opacity=".05" stroke="%s" stroke-opacity=".5" stroke-width="1.5" '
                         'stroke-dasharray="5 4"/>'
                         % (num(x - 26), num(a - 32), num(b - a + 72), t['gate'], t['gate']))
        fill = t['gate'] if done else t['bg']
        o.append('<rect x="%s" y="%s" width="%s" height="24" rx="12" fill="%s" stroke="%s" '
                 'stroke-width="1.5"/>' % (num(x - w / 2), num(chip_y - 12), num(w), fill,
                                           t['gate']))
        o.append(text(x, chip_y + 5, g['label'], 13.5, t['bg'] if done else t['gate'], 700,
                      'middle'))

    # Dependencies: dashed, from a column of one line to the start of another.
    for frm, x, to in d.get('deps', []):
        target = next(l for l in lines if l['id'] == to)['stations'][0][1]
        y1, y2 = rowy[frm] + 54, rowy[to]
        o.append('<path d="M%s %s V%s H%s" fill="none" stroke="%s" stroke-width="1.5" '
                 'stroke-dasharray="4 4"/>' % (num(x), num(y1), num(y2), num(target - 13),
                                               t['muted']))

    here = d['board'].get('here')
    for l in lines:
        y, col = rowy[l['id']], C[l['color']]
        st = l['stations']
        o.append(text(left, y + 5, l['name'], 15, col, 700))
        xs = [s[1] for s in st]
        lastdone = max([s[1] for s in st if s[2] == 'done'], default=None)
        lastact = max([s[1] for s in st if s[2] != 'todo'], default=None)

        def seg(a, b, op):
            return ('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-opacity="%s" '
                    'stroke-width="8" stroke-linecap="round"/>' % (num(a), num(y), num(b),
                                                                  num(y), col, op))
        o.append(seg(xs[0], xs[-1], '.22'))
        if lastact is not None:
            o.append(seg(xs[0], lastact, '.55'))
        if lastdone is not None:
            o.append(seg(xs[0], lastdone, '1'))
        prev_end = {1: -1e9, -1: -1e9}
        for label, x, s in st:
            if s == 'done':
                o.append('<circle cx="%s" cy="%s" r="10" fill="%s" stroke="%s" stroke-width="3"/>'
                         '<path d="M%s %s l3 3 l6 -6.5" fill="none" stroke="%s" '
                         'stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>'
                         % (num(x), num(y), col, t['bg'], num(x - 4.5), num(y), t['bg']))
            elif s == 'current':
                o.append('<circle cx="%s" cy="%s" r="17" fill="%s" fill-opacity=".2"/>'
                         '<circle cx="%s" cy="%s" r="10" fill="%s" stroke="%s" stroke-width="5"/>'
                         % (num(x), num(y), col, num(x), num(y), t['bg'], col))
            elif s == 'partial':
                o.append('<circle cx="%s" cy="%s" r="9" fill="%s" stroke="%s" stroke-width="3"/>'
                         '<path d="M%s %s A6 6 0 0 0 %s %s Z" fill="%s"/>'
                         % (num(x), num(y), t['bg'], col, num(x), num(y - 6), num(x),
                            num(y + 6), col))
            else:
                o.append('<circle cx="%s" cy="%s" r="8" fill="%s" stroke="%s" '
                         'stroke-opacity=".7" stroke-width="2.5"/>' % (num(x), num(y),
                                                                       t['bg'], col))
            # Labels below the line, or above when the one before is too close.
            w = tw(label, 13.5)
            side = 1 if x - w / 2 > prev_end[1] + 6 else -1
            prev_end[side] = x + w / 2
            ly = y + 29 if side == 1 else y - 18
            weight = 700 if s == 'current' else 500
            o.append(text(x, ly, label, 13.5, t['fg'] if s != 'todo' else t['muted'], weight,
                          'middle'))
            if label == here:
                o.append(text(x, y + 46, 'WE ARE HERE', 11.5, col, 800, 'middle',
                              'letter-spacing=".06em"'))

    # Legend.
    ly, lx = H - 30, left
    for s, lab in (('done', 'done'), ('current', 'in progress'), ('partial', 'partial'),
                   ('todo', 'to do')):
        x = lx + 10
        if s == 'done':
            o.append('<circle cx="%s" cy="%s" r="8" fill="%s"/>' % (num(x), num(ly), t['fg']))
        elif s == 'current':
            o.append('<circle cx="%s" cy="%s" r="7" fill="%s" stroke="%s" stroke-width="4"/>'
                     % (num(x), num(ly), t['bg'], t['fg']))
        elif s == 'partial':
            o.append('<circle cx="%s" cy="%s" r="7" fill="%s" stroke="%s" stroke-width="2.5"/>'
                     '<path d="M%s %s A4.5 4.5 0 0 0 %s %s Z" fill="%s"/>'
                     % (num(x), num(ly), t['bg'], t['fg'], num(x), num(ly - 4.5), num(x),
                        num(ly + 4.5), t['fg']))
        else:
            o.append('<circle cx="%s" cy="%s" r="7" fill="%s" stroke="%s" stroke-width="2.5"/>'
                     % (num(x), num(ly), t['bg'], t['muted']))
        o.append(text(x + 16, ly + 4.5, lab, 13, t['muted']))
        lx += 40 + tw(lab, 13)
    gate_text = 'gate: the lines it needs pass through it'
    o.append('<rect x="%s" y="%s" width="26" height="18" rx="7" fill="none" stroke="%s" '
             'stroke-opacity=".6" stroke-dasharray="4 3"/>' % (num(lx + 4), num(ly - 9),
                                                              t['gate']))
    o.append(text(lx + 38, ly + 4.5, gate_text, 13, t['muted']))
    lx += 52 + tw(gate_text, 13)
    o.append('<path d="M%s %s h28" stroke="%s" stroke-width="1.5" stroke-dasharray="4 4"/>'
             % (num(lx), num(ly), t['muted']))
    o.append(text(lx + 36, ly + 4.5, 'starts after', 13, t['muted']))
    o.append('</svg>')
    return '\n'.join(o) + '\n'

# ---------------------------------------------------------------- board

def board(d, t):
    C = t['colors']
    color_of = {l['id']: C[l['color']] for l in d['line']}
    b = d['board']
    W, pad, gap = 1120, 16, 14
    cw = (W - 2 * pad - 3 * gap) / 4
    head_h = 70
    cols = [('DONE · LATEST', b.get('done', [])), ('NOW', b.get('now', [])),
            ('NEXT', b.get('next', [])), ('LATER', b.get('later', []))]
    body, heights = [], []
    for ci, (title, items) in enumerate(cols):
        x0 = pad + ci * (cw + gap)
        y = head_h + 44
        parts = []
        if title != 'LATER':
            for it in items:
                tl = wrap(it['title'], int((cw - 44) / 8.4))
                nl = wrap(it.get('note', ''), int((cw - 44) / 7.3))
                h = 18 + 21 * len(tl) + 19 * len(nl) + (22 if 'total' in it else 0) + 10
                col = color_of[it['line']]
                lead = title == 'NOW' and it is items[0]
                parts.append('<rect x="%s" y="%s" width="%s" height="%s" rx="8" fill="%s" '
                             'stroke="%s" stroke-width="%s"/>'
                             '<rect x="%s" y="%s" width="5" height="%s" rx="2" fill="%s"/>'
                             % (num(x0 + 10), num(y), num(cw - 20), num(h), t['card'],
                                col if lead else t['cardline'], 2 if lead else 1,
                                num(x0 + 10), num(y), num(h), col))
                ty = y + 24
                for s in tl:
                    parts.append(text(x0 + 26, ty, s, 15, t['fg'], 650))
                    ty += 21
                for s in nl:
                    parts.append(text(x0 + 26, ty, s, 13.5, t['muted']))
                    ty += 19
                if 'total' in it:
                    bw = cw - 90
                    parts.append('<rect x="%s" y="%s" width="%s" height="7" rx="3.5" fill="%s" '
                                 'fill-opacity=".2"/><rect x="%s" y="%s" width="%s" height="7" '
                                 'rx="3.5" fill="%s"/>'
                                 % (num(x0 + 26), num(ty - 4), num(bw), col, num(x0 + 26),
                                    num(ty - 4), num(round(bw * it['done'] / it['total'])),
                                    col))
                    parts.append(text(x0 + 32 + bw, ty + 3, '%d/%d' % (it['done'], it['total']),
                                      12, t['muted'], 600))
                if it.get('partial'):
                    parts.append('<rect x="%s" y="%s" width="54" height="18" rx="9" fill="%s" '
                                 'fill-opacity=".15"/>' % (num(x0 + cw - 74), num(y + 10), col))
                    parts.append(text(x0 + cw - 47, y + 23, 'partial', 11, col, 600, 'middle'))
                y += h + 10
        else:
            for grp in items:
                parts.append(text(x0 + 14, y + 14, grp['group'], 14.5, t['fg'], 700))
                y += 28
                for label, line in grp['items']:
                    for k, s in enumerate(wrap(label, int((cw - 44) / 7.4))):
                        if k == 0:
                            parts.append('<circle cx="%s" cy="%s" r="4.5" fill="%s"/>'
                                         % (num(x0 + 20), num(y + 5), color_of[line]))
                        parts.append(text(x0 + 32, y + 10, s, 14, t['muted']))
                        y += 20
                    y += 4
                y += 10
        heights.append(y)
        body.append((x0, title, parts))
    H = max(heights) + 16
    o = header(W, H, 'Minkar work board, %s' % d['updated'], t)
    o.append(text(pad, 30, 'Minkar work board', 20, t['fg'], 700))
    o.append(text(pad, 52, '%s · devel %s' % (d['updated'], d['commit']), 13, t['muted']))
    for x0, title, parts in body:
        o.append('<rect x="%s" y="%s" width="%s" height="%s" rx="10" fill="%s"/>'
                 % (num(x0), num(head_h), num(cw), num(H - head_h - 8), t['col']))
        o.append(text(x0 + 14, head_h + 26, title, 13, t['muted'], 700, 'start',
                      'letter-spacing=".08em"'))
        o.extend(parts)
    o.append('</svg>')
    return '\n'.join(o) + '\n'

# ---------------------------------------------------------------- summary table

BEGIN, END = '<!-- progress:summary:begin -->', '<!-- progress:summary:end -->'
SQUARE = {'done': '🟩', 'current': '🟦', 'partial': '🟨', 'todo': '⬜'}
WORDS = {
    'en': dict(updated='Updated %s, devel `%s`', head=('Plan', 'Progress', '', 'Now', 'Next'),
               gates='**Gates**', done='done', current='in progress', partial='partial',
               todo='to do'),
    'es': dict(updated='Actualizado el %s, devel `%s`',
               head=('Plan', 'Avance', '', 'Ahora', 'Siguiente'), gates='**Gates**',
               done='hecho', current='en curso', partial='parcial', todo='por hacer'),
}


def summary(d, lang):
    w = WORDS[lang]
    rows = ['| %s |' % ' | '.join(w['head']), '| --- | --- | --- | --- | --- |']

    def row(name, states, labels):
        bar = ''.join(SQUARE[s] for s in states)
        done = sum(s == 'done' for s in states)
        now = ([l for l, s in zip(labels, states) if s == 'current'] or
               [l for l, s in zip(labels, states) if s == 'partial'])
        nxt = next((l for l, s in zip(labels, states) if s == 'todo'), '—')
        rows.append('| %s | %s | %d / %d | %s | %s |' % (name, bar, done, len(states),
                                                          ', '.join(now) or '—', nxt))
    row(w['gates'], [g['state'] for g in d['gate']], [g['title'] for g in d['gate']])
    for l in d['line']:
        doc = l.get('doc_es', l['doc']) if lang == 'es' else l['doc']
        row('[%s](%s)' % (l['name'], doc), [s[2] for s in l['stations']],
            [s[0] for s in l['stations']])
    legend = ' · '.join('%s %s' % (SQUARE[s], w[s]) for s in STATES)
    return '\n'.join([BEGIN, '_%s_' % (w['updated'] % (d['updated'], d['commit'])), '',
                      *rows, '', legend, END])


def page_language(path, forced):
    if forced != 'auto':
        return forced
    return 'es' if path.endswith('.es.md') else 'en'


def rewrite(path, d, lang):
    with open(path, encoding='utf-8') as f:
        page = f.read()
    try:
        a, b = page.index(BEGIN), page.index(END) + len(END)
    except ValueError:
        sys.exit('gen.py: %s has no %s ... %s block' % (path, BEGIN, END))
    new = page[:a] + summary(d, lang) + page[b:]
    if new != page:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new)

# ---------------------------------------------------------------- checks

def check(d):
    """Stops on data the views cannot draw."""
    lines = {l['id'] for l in d['line']}
    colors = set(THEMES['light']['colors'])
    problems = []
    # A gate is done or not; "current" marks only the next one.
    for g in d['gate']:
        if g['state'] not in ('done', 'current', 'todo'):
            problems.append('gate %s: state %r (done, current or todo)' % (g['id'], g['state']))
        problems += ['gate %s: no line %r' % (g['id'], r) for r in g.get('rows', [])
                     if r not in lines]
    if sum(g['state'] == 'current' for g in d['gate']) > 1:
        problems.append('more than one current gate')
    for l in d['line']:
        if l['color'] not in colors:
            problems.append('line %s: colour %r' % (l['id'], l['color']))
        xs = [s[1] for s in l['stations']]
        if xs != sorted(xs):
            problems.append('line %s: stations not left to right' % l['id'])
        problems += ['line %s, %s: state %r' % (l['id'], s[0], s[2]) for s in l['stations']
                     if s[2] not in STATES]
    for frm, _, to in d.get('deps', []):
        problems += ['dependency: no line %r' % n for n in (frm, to) if n not in lines]
    b = d['board']
    for column in ('done', 'now', 'next'):
        problems += ['board %s %r: no line %r' % (column, it['title'], it['line'])
                     for it in b.get(column, []) if it['line'] not in lines]
    if len(b.get('done', [])) > 6:
        problems.append('board: keep the six most recent done items')
    for grp in b.get('later', []):
        problems += ['board later %r: no line %r' % (label, line)
                     for label, line in grp['items'] if line not in lines]
    if problems:
        sys.exit('gen.py: ' + '\ngen.py: '.join(problems))


def warn_if_stale(d, status):
    """Says so when status.toml's commit is not the last change outside
    Docs/: the page would claim to summarize an older tree. Only a warning
    (the CI's shallow clone may not know the history)."""
    top = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(status))))
    try:
        last = subprocess.run(['git', '-C', top, 'log', '-1', '--format=%h', '--abbrev=10',
                               '--', '.', ':(exclude)Docs'],
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return
    if last and not (last.startswith(d['commit']) or d['commit'].startswith(last)):
        print('progress: status.toml names devel %s, but the last change outside Docs/ is '
              '%s: set "commit" (and "updated") to it' % (d['commit'], last), file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description='Writes the progress views from status.toml.')
    p.add_argument('--lang', choices=('auto', 'en', 'es'), default='auto',
                   help="the summary table's language (auto: es for *.es.md)")
    p.add_argument('status')
    p.add_argument('outdir')
    p.add_argument('pages', nargs='*')
    args = p.parse_args()
    d = read_toml(args.status)
    check(d)
    warn_if_stale(d, args.status)
    for name, theme in THEMES.items():
        for view, draw in (('metro', metro), ('board', board)):
            out = os.path.join(args.outdir, '%s-%s.svg' % (view, name))
            with open(out, 'w', encoding='utf-8') as f:
                f.write(draw(d, theme))
    for page in args.pages:
        rewrite(page, d, page_language(page, args.lang))


if __name__ == '__main__':
    main()
