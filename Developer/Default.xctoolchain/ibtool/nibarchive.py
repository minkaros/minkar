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

"""The binary nib format (NIBArchive): read, write and print.

An archive is a list of objects, each a class name and an ordered list of
(key, value) pairs; object 0 is the root. A value is an int, a bool, a
float (a Python float is written as a double, a Float as a float), bytes,
None, or another object (an Obj). Strings, numbers and collections are
objects of NSString, NSNumber, NSArray... as AppKit's keyed archives have
them; helpers below make them.
"""

import struct

MAGIC = b"NIBArchive"

INT8, INT16, INT32, INT64, FALSE, TRUE, FLOAT, DOUBLE, BYTES, NIL, OBJECT = range(11)

ELEMENT_KEY = "UINibEncoderEmptyKey"


class Float(float):
    """A value written as a 32-bit float."""


class Obj:
    """An object of the archive: its class and its (key, value) pairs."""

    __slots__ = ("cls", "items")

    def __init__(self, cls, items=None):
        self.cls = cls
        self.items = list(items or [])

    def add(self, key, value):
        self.items.append((key, value))
        return self

    def get(self, key, default=None):
        for k, v in reversed(self.items):
            if k == key:
                return v
        return default

    def __repr__(self):
        return "<%s>" % self.cls


# Foundation values as the archive holds them.

def string(text, cls="NSString"):
    return Obj(cls, [("NS.bytes", text.encode("utf-8"))])


def number(value):
    if isinstance(value, bool):
        return Obj("NSNumber", [("NS.intval", int(value))])
    if isinstance(value, int):
        return Obj("NSNumber", [("NS.intval", value)])
    return Obj("NSNumber", [("NS.dblval", float(value))])


def array(elements, cls="NSArray"):
    obj = Obj(cls, [("NSInlinedValue", True)])
    for e in elements:
        obj.add(ELEMENT_KEY, e)
    return obj


def dictionary(pairs, cls="NSDictionary"):
    obj = Obj(cls, [("NSInlinedValue", True)])
    for k, v in pairs:
        obj.add(ELEMENT_KEY, k)
        obj.add(ELEMENT_KEY, v)
    return obj


def doubles(*values):
    """A point, size or rect: 0x07 and its doubles."""
    return b"\x07" + b"".join(struct.pack("<d", float(v)) for v in values)


# Variable-length numbers: 7 bits a byte, the last byte with 0x80 set.

def _flex(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(b | 0x80)
            return bytes(out)
        out.append(b)


def _read_flex(data, at):
    n = shift = 0
    while True:
        if at >= len(data):
            raise ValueError("truncated number")
        b = data[at]
        at += 1
        n |= (b & 0x7F) << shift
        shift += 7
        if b & 0x80:
            return n, at


# Writing.

def write(root):
    """The archive whose object 0 is root, as bytes.

    Objects are numbered in the order they are first reached (depth
    first), each once, so shared objects stay shared."""
    order, index = [], {}

    def visit(obj):
        stack = [obj]
        while stack:
            o = stack.pop()
            if id(o) in index:
                continue
            index[id(o)] = len(order)
            order.append(o)
            children = [v for _, v in o.items if isinstance(v, Obj)]
            stack.extend(reversed(children))

    visit(root)
    keys, key_index, classes, class_index = [], {}, [], {}
    objects_out, values_out, value_count = bytearray(), bytearray(), 0
    for o in order:
        if o.cls not in class_index:
            class_index[o.cls] = len(classes)
            classes.append(o.cls)
        objects_out += _flex(class_index[o.cls]) + _flex(value_count) + _flex(len(o.items))
        for key, value in o.items:
            if key not in key_index:
                key_index[key] = len(keys)
                keys.append(key)
            values_out += _flex(key_index[key]) + _encode(value, index)
            value_count += 1
    keys_out = b"".join(_flex(len(k.encode())) + k.encode() for k in keys)
    classes_out = b"".join(_flex(len(c.encode()) + 1) + b"\x80" + c.encode() + b"\x00" for c in classes)

    header_size = len(MAGIC) + 8 + 32
    offsets, at = [], header_size
    for part in (objects_out, keys_out, values_out, classes_out):
        offsets.append(at)
        at += len(part)
    header = MAGIC + struct.pack("<II", 1, 10)
    header += struct.pack("<8I", len(order), offsets[0], len(keys), offsets[1],
                          value_count, offsets[2], len(classes), offsets[3])
    return header + bytes(objects_out) + keys_out + bytes(values_out) + classes_out


def _encode(value, index):
    if value is None:
        return bytes([NIL])
    if isinstance(value, bool):
        return bytes([TRUE if value else FALSE])
    if isinstance(value, Obj):
        return bytes([OBJECT]) + struct.pack("<I", index[id(value)])
    if isinstance(value, Float):
        return bytes([FLOAT]) + struct.pack("<f", value)
    if isinstance(value, float):
        return bytes([DOUBLE]) + struct.pack("<d", value)
    if isinstance(value, int):
        for kind, fmt in ((INT8, "<b"), (INT16, "<h"), (INT32, "<i"), (INT64, "<q")):
            try:
                return bytes([kind]) + struct.pack(fmt, value)
            except struct.error:
                continue
        raise ValueError("integer out of range: %d" % value)
    if isinstance(value, (bytes, bytearray)):
        return bytes([BYTES]) + _flex(len(value)) + bytes(value)
    raise TypeError("cannot archive %r" % (value,))


# Reading.

def read(data):
    """The root Obj of an archive (objects shared as in the archive)."""
    if data[:len(MAGIC)] != MAGIC or len(data) < 50:
        raise ValueError("not a binary nib")
    counts = struct.unpack_from("<8I", data, 18)
    n_obj, o_obj, n_key, o_key, n_val, o_val, n_cls, o_cls = counts

    classes, at = [], o_cls
    for _ in range(n_cls):
        length, at = _read_flex(data, at)
        flag = data[at]
        at += 1
        if flag == 0x81:
            at += 4
        classes.append(data[at:at + length].rstrip(b"\x00").decode("utf-8"))
        at += length

    keys, at = [], o_key
    for _ in range(n_key):
        length, at = _read_flex(data, at)
        keys.append(data[at:at + length].decode("utf-8"))
        at += length

    raw, at = [], o_val
    for _ in range(n_val):
        k, at = _read_flex(data, at)
        kind = data[at]
        at += 1
        if kind in (INT8, INT16, INT32, INT64):
            fmt = ("<b", "<h", "<i", "<q")[kind]
            v = struct.unpack_from(fmt, data, at)[0]
            at += struct.calcsize(fmt)
        elif kind in (FALSE, TRUE):
            v = kind == TRUE
        elif kind == FLOAT:
            v = Float(struct.unpack_from("<f", data, at)[0])
            at += 4
        elif kind == DOUBLE:
            v = struct.unpack_from("<d", data, at)[0]
            at += 8
        elif kind == BYTES:
            length, at = _read_flex(data, at)
            v = bytes(data[at:at + length])
            at += length
        elif kind == NIL:
            v = None
        elif kind == OBJECT:
            v = ("ref", struct.unpack_from("<I", data, at)[0])
            at += 4
        else:
            raise ValueError("unknown value type %d" % kind)
        raw.append((keys[k], v))

    objects, spans, at = [], [], o_obj
    for _ in range(n_obj):
        c, at = _read_flex(data, at)
        first, at = _read_flex(data, at)
        count, at = _read_flex(data, at)
        objects.append(Obj(classes[c]))
        spans.append((first, count))
    for obj, (first, count) in zip(objects, spans):
        for key, v in raw[first:first + count]:
            if isinstance(v, tuple):
                v = objects[v[1]]
            obj.items.append((key, v))
    return objects[0]


# Printing: one canonical text for an archive, the same for two archives
# with the same object graph whatever their objects' order.

def _scalar(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, Float):
        return "float %r" % float(v)
    if isinstance(v, float):
        return "double %r" % v
    if isinstance(v, int):
        return "int %d" % v
    if v is None:
        return "nil"
    if isinstance(v, bytes):
        if v[:1] == b"\x07" and (len(v) - 1) % 8 == 0 and len(v) > 1:
            n = (len(v) - 1) // 8
            return "geometry (%s)" % ", ".join("%g" % x for x in struct.unpack("<%dd" % n, v[1:]))
        try:
            return "bytes %r" % v.decode("utf-8")
        except UnicodeDecodeError:
            return "bytes 0x%s" % v.hex()
    return repr(v)


COLLECTIONS = ("NSArray", "NSMutableArray", "NSDictionary", "NSMutableDictionary", "NSSet", "NSMutableSet")


def _inline(o):
    if o.cls in ("NSString", "NSMutableString", "NSLocalizableString") and isinstance(o.get("NS.bytes"), bytes):
        return "%s %r" % (o.cls, o.get("NS.bytes").decode("utf-8", "replace"))
    if o.cls == "NSNumber" and len(o.items) == 1:
        return "NSNumber %s" % _scalar(o.items[0][1])
    return None


def dump(root, sort_keys=True):
    """The archive as indented text: strings and numbers inline, other
    objects once, later references to them by the path where they were
    printed. With sort_keys, an object's keys are in name order (not in
    collections), so the text does not depend on the order of keys."""
    lines, seen = [], {}

    def walk(o, depth, path):
        seen[id(o)] = path
        items = o.items
        if sort_keys and o.cls not in COLLECTIONS:
            items = sorted(items, key=lambda kv: kv[0])
        n = 0
        for k, v in items:
            label = "[%d]" % n if k == ELEMENT_KEY else k
            if k == ELEMENT_KEY:
                n += 1
            pad = "  " * (depth + 1)
            if not isinstance(v, Obj):
                lines.append("%s%s = %s" % (pad, label, _scalar(v)))
            elif _inline(v) is not None:
                lines.append("%s%s = %s" % (pad, label, _inline(v)))
            elif id(v) in seen:
                lines.append("%s%s = -> %s" % (pad, label, seen[id(v)]))
            else:
                lines.append("%s%s = %s" % (pad, label, v.cls))
                walk(v, depth + 1, "%s.%s" % (path, label))

    lines.append(root.cls)
    walk(root, 0, "root")
    return "\n".join(lines) + "\n"
