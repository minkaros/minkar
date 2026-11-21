#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""Checks the relative links of Minkar's Markdown documents.

  doc-links.py [--repo R] [FILE.md ...]

Without files it checks the documentation: the top level's *.md and
everything under Docs/. Each relative link must name a file or directory
that exists, and its #anchor, when it has one, a heading of that file
(as GitHub makes anchors: lower case, punctuation dropped, spaces to
hyphens, a -1, -2... suffix for repeated headings). Links inside code
spans and code blocks, and absolute URLs, are not checked. Prints each
broken link and exits 1 if there is any.
"""

import argparse
import os
import re
import sys

LINK = re.compile(r'(?<!!)\[(?:[^\]\[]|\[[^\]]*\])*\]\(([^)\s]+)(?:\s+"[^"]*")?\)|!\[[^\]]*\]\(([^)\s]+)\)')
HEADING = re.compile(r'^(#{1,6})\s+(.*?)\s*#*\s*$')


def strip_code(text):
    """The text without code blocks and code spans, line numbers kept."""
    out, fence = [], None
    for line in text.split('\n'):
        m = re.match(r'^\s*(```+|~~~+)', line)
        if fence:
            if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence):
                fence = None
            out.append('')
            continue
        if m:
            fence = m.group(1)
            out.append('')
            continue
        if re.match(r'^(    |\t)', line) and not line.lstrip().startswith(('-', '*', '|')):
            out.append('')
            continue
        out.append(re.sub(r'`+[^`]*`+', '', line))
    return out


def slug(title):
    title = re.sub(r'<[^>]+>', '', title)
    title = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', title)
    title = title.replace('`', '').strip().lower()
    title = re.sub(r'[^\w\- ]', '', title)
    return title.replace(' ', '-')


def anchors(path, cache={}):
    if path not in cache:
        seen, found = {}, set()
        with open(path, encoding='utf-8') as f:
            text = f.read()
        fence = None
        for line in text.split('\n'):
            m = re.match(r'^\s*(```+|~~~+)', line)
            if m:
                fence = None if fence else m.group(1)
                continue
            if fence:
                continue
            h = HEADING.match(line)
            if h:
                s = slug(h.group(2))
                n = seen.get(s, 0)
                found.add(s if n == 0 else '%s-%d' % (s, n))
                seen[s] = n + 1
        found.update(re.findall(r'<a\s+(?:name|id)="([^"]+)"', text))
        cache[path] = found
    return cache[path]


def check(path, repo):
    broken = []
    with open(path, encoding='utf-8') as f:
        lines = strip_code(f.read())
    for number, line in enumerate(lines, 1):
        for m in LINK.finditer(line):
            target = m.group(1) or m.group(2)
            if re.match(r'^[a-z][a-z0-9+.-]*:', target, re.I):
                continue
            file, _, anchor = target.partition('#')
            dest = os.path.normpath(os.path.join(os.path.dirname(path), file)) if file else path
            if not os.path.exists(dest):
                broken.append((number, target, 'no such file'))
            elif anchor and dest.endswith('.md') and anchor not in anchors(dest):
                broken.append((number, target, 'no such heading'))
    return [(os.path.relpath(path, repo), n, t, why) for n, t, why in broken]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.')
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()
    files = a.files or sorted(
        [os.path.join(a.repo, f) for f in os.listdir(a.repo) if f.endswith('.md')] +
        [os.path.join(d, f) for d, _, fs in os.walk(os.path.join(a.repo, 'Docs'))
         for f in fs if f.endswith('.md')])
    broken = [b for f in files for b in check(f, a.repo)]
    for path, number, target, why in broken:
        print('%s:%d: %s (%s)' % (path, number, target, why))
    print('doc-links: %d files, %d broken links' % (len(files), len(broken)), file=sys.stderr)
    sys.exit(1 if broken else 0)


if __name__ == '__main__':
    main()
