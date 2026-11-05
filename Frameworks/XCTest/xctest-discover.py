#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Writes the main.swift of a test program built with XCTest from the
# test sources it is given: the XCTestCase subclasses they declare
# (directly or through other test classes) and their test methods, the
# instance methods named test... that take no arguments, in the classes
# and in their extensions; a class gets the ones it inherits, too. It
# stands in for the package manager's test discovery, for test programs
# built without it.
#
#   xctest-discover.py [-o main.swift] [--import Module] file.swift...
#
# The sources are read as Swift tokens (comments and string literals
# skipped), well enough to find declarations at the top level and in
# type bodies. Private, generic and nested test classes are skipped.

import argparse
import re
import sys

IDENT = r'[A-Za-z_][A-Za-z0-9_]*'


def strip(text):
    """The source with its comments and string literals blanked out
    (newlines kept), so that braces and keywords are only code's."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            depth, i = 1, i + 2
            while i < n and depth:
                if text.startswith('/*', i):
                    depth, i = depth + 1, i + 2
                elif text.startswith('*/', i):
                    depth, i = depth - 1, i + 2
                else:
                    if text[i] == '\n':
                        out.append('\n')
                    i += 1
        elif c == '"' or (c == '#' and re.match(r'#+"', text[i:])):
            m = re.match(r'(#*)("""|")', text[i:])
            hashes, quote = m.group(1), m.group(2)
            close = quote + hashes
            i += len(m.group(0))
            out.append('""')
            while i < n:
                if not hashes and text[i] == '\\':
                    if text.startswith('\\(', i):
                        # An interpolation: skip to its closing parenthesis.
                        depth, i = 1, i + 2
                        while i < n and depth:
                            depth += {'(': 1, ')': -1}.get(text[i], 0)
                            i += 1
                        continue
                    i += 2
                    continue
                if text.startswith(close, i):
                    i += len(close)
                    break
                if text[i] == '\n':
                    out.append('\n')
                i += 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def body_end(code, start):
    """The index just past the brace that closes the one at start."""
    depth = 0
    for i in range(start, len(code)):
        if code[i] == '{':
            depth += 1
        elif code[i] == '}':
            depth -= 1
            if depth == 0:
                return i + 1
    return len(code)


def top_level(code, start, end):
    """The text between start and end with nested bodies blanked out
    (spaces: offsets into it are offsets into code[start:end])."""
    out, depth = [], 0
    for c in code[start:end]:
        if c == '}':
            depth -= 1
        out.append(c if depth == 0 or c == '\n' else ' ')
        if c == '{':
            depth += 1
    return ''.join(out)


DECL = re.compile(r'((?:@' + IDENT + r'(?:\([^)]*\))?\s+|' + IDENT + r'\s+)*)'
                  r'(class|extension|struct|enum|actor|protocol)\s+'
                  r'(' + IDENT + r'(?:\.' + IDENT + r')*)\s*(<[^>{]*>)?\s*'
                  r'(?::\s*([^{]*?))?\s*(?:where[^{]*)?\{')
TEST = re.compile(r'((?:@' + IDENT + r'(?:\([^)]*\))?\s+|' + IDENT + r'\s+)*)'
                  r'func\s+(test[A-Za-z0-9_]*)\s*\(\s*\)\s*'
                  r'((?:async\s*)?(?:throws\s*)?)(?:->\s*(\S+))?\s*\{')


def scan(code):
    """(classes, tests): classes maps a name to (superclass, usable),
    tests maps a type name to its test methods (name, is_async)."""
    classes, tests = {}, {}
    level = top_level(code, 0, len(code))
    for m in DECL.finditer(level):
        mods, kind, name, generic, inherits = m.groups()
        start = m.end() - 1
        end = body_end(code, start)
        if kind in ('class', 'extension'):
            if kind == 'class':
                first = (inherits or '').split(',')[0].strip()
                usable = not generic and not ({'private', 'fileprivate'} & set(mods.split()))
                classes[name] = (first, usable)
            body = top_level(code, start + 1, end - 1)
            for t in TEST.finditer(body):
                tmods = set(t.group(1).split())
                if tmods & {'private', 'fileprivate', 'static', 'class'}:
                    continue
                if t.group(4) and t.group(4) not in ('Void', '()'):
                    continue
                tests.setdefault(name, []).append((t.group(2), 'async' in t.group(3)))
    return classes, tests


def main(argv):
    p = argparse.ArgumentParser(description='Writes an XCTest main.swift.')
    p.add_argument('-o', '--output', default='-')
    p.add_argument('--import', dest='imports', action='append', default=[],
                   help='a module the tests are in (@testable import)')
    p.add_argument('sources', nargs='+')
    args = p.parse_args(argv)

    classes, tests = {}, {}
    for path in args.sources:
        with open(path, encoding='utf-8') as f:
            c, t = scan(strip(f.read()))
        classes.update(c)
        for k, v in t.items():
            tests.setdefault(k, []).extend(v)

    def is_test_class(name, seen=()):
        sup = classes.get(name, ('', False))[0]
        if sup in ('XCTestCase', 'XCTest.XCTestCase'):
            return True
        return sup in classes and sup not in seen and is_test_class(sup, seen + (name,))

    def all_tests(name):
        own = list(tests.get(name, []))
        sup = classes[name][0]
        if sup in classes:
            names = {t[0] for t in own}
            own += [t for t in all_tests(sup) if t[0] not in names]
        return own

    lines = ['// Generated by xctest-discover.py: do not edit.', 'import XCTest']
    lines += ['@testable import %s' % m for m in args.imports]
    entries = []
    for name in sorted(classes):
        if not classes[name][1] or not is_test_class(name):
            continue
        found = sorted(all_tests(name))
        if not found:
            continue
        lines.append('extension %s {' % name)
        lines.append('    static let __discoveredTests_%s: [(String, (%s) -> () throws -> Void)] = ['
                     % (name, name))
        lines += ['        ("%s", %s),' % (t, 'asyncTest(%s)' % t if a else t) for t, a in found]
        lines.append('    ]')
        lines.append('}')
        entries.append('testCase(%s.__discoveredTests_%s)' % (name, name))
    lines.append('XCTMain([')
    lines += ['    %s,' % e for e in entries]
    lines.append('])')
    text = '\n'.join(lines) + '\n'
    if args.output == '-':
        sys.stdout.write(text)
    else:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
