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

"""ibtool: compile Interface Builder documents (.xib) to binary nibs.

A subset of the reference platform's ibtool for building applications:

    ibtool --compile OUT.nib [--errors] [--warnings] [--notices]
           [--output-format human-readable-text|xml1]
           [--minimum-deployment-target VERSION] [--module NAME] IN.xib

A xib is an XML description of objects (windows, views, menus, controllers
and placeholders) and their connections. The nib written is what AppKit's
nib loading reads: the objects with the keys their initWithCoder: decodes,
the object table (NSIBObjectData) and the connectors. Elements this
compiler does not know yet are reported, with the document's line, and
compiled as plain objects of their class so the rest of the nib loads.
"""

import os
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import nibarchive as N  # noqa: E402
from nibarchive import Obj, string, number, array, dictionary  # noqa: E402

OWNER, FIRST_RESPONDER, APPLICATION = "-2", "-1", "-3"


class CompileError(Exception):
    pass


def attr(elem, name, default=None):
    """An attribute of elem, or its <string key="name"> child (for text an
    attribute cannot hold, as a tab or a base64 string)."""
    value = elem.get(name)
    if value is not None:
        return value
    child = elem.find("string[@key='%s']" % name)
    if child is None:
        return default
    return text_of(child)


def text_of(elem):
    """A <string> element's text, decoded when it is base64."""
    if elem.get("base64-UTF8") == "YES":
        import base64
        text = "".join((elem.text or "").split())
        return base64.b64decode(text + "=" * (-len(text) % 4)).decode("utf-8", "replace")
    return elem.text or ""


def yes(value):
    return value == "YES"


def num(value, default=0.0):
    if value is None:
        return default
    return float(value)


def fmt(v):
    """A coordinate as the reference tools print it (no .0)."""
    v = float(v)
    return ("%d" % v) if v == int(v) else repr(v)


def rect_string(x, y, w, h):
    return "{{%s, %s}, {%s, %s}}" % (fmt(x), fmt(y), fmt(w), fmt(h))


def size_string(w, h):
    return "{%s, %s}" % (fmt(w), fmt(h))


def rect_of(elem):
    return tuple(num(elem.get(k)) for k in ("x", "y", "width", "height"))


# Autoresizing mask: the xib's flexible edges as NSAutoresizingMaskOptions.
AUTORESIZING = (("flexibleMinX", 1), ("widthSizable", 2), ("flexibleMaxX", 4),
                ("flexibleMinY", 8), ("heightSizable", 16), ("flexibleMaxY", 32))

LINE_BREAK = {"wordWrapping": 0, "charWrapping": 1, "clipping": 2, "truncatingHead": 3,
              "truncatingTail": 4, "truncatingMiddle": 5}
# NSTextAlignment as a cell's flags hold it (left, right, center, justified, natural).
CELL_ALIGNMENT = {"left": 0, "right": 1, "center": 2, "justified": 3, "natural": 4}
# NSTextAlignment as API 10.11 and later number it.
ALIGNMENT = {"left": 0, "center": 1, "right": 2, "justified": 3, "natural": 4}
CONTROL_SIZE = {"regular": 0, "small": 1, "mini": 2, "large": 3}
BEZEL_STYLE = {"rounded": 1, "regularSquare": 2, "disclosure": 5, "shadowlessSquare": 6,
               "circular": 7, "texturedSquare": 8, "helpButton": 9, "smallSquare": 10,
               "texturedRounded": 11, "roundRect": 12, "roundedRect": 12, "recessed": 13,
               "roundedDisclosure": 14, "inline": 15, "push": 1, "flexiblePush": 2,
               "toolbar": 11, "accessoryBarAction": 12, "accessoryBar": 13,
               "pushDisclosure": 14, "badge": 15}
# A button type's highlightsBy and showsStateBy (NSCellStyleMask bits:
# contents 1, push in 2, change gray 4, change background 8).
BUTTON_TYPE = {
    "push": (2 | 4, 0), "momentaryPushIn": (2 | 4, 0),
    "momentaryLight": (2 | 4 | 8, 0), "pushOnPushOff": (2 | 4, 8),
    "toggle": (2 | 1, 1), "switch": (1, 1), "check": (1, 1), "radio": (1, 1),
    "momentaryChange": (1, 0), "onOff": (8, 8), "bevel": (2 | 4, 0),
}
# A button's <behavior> as NSButtonFlags holds it.
BEHAVIOR_BITS = (("pushIn", 0x80000000), ("changeContents", 0x40000000), ("changeBackground", 0x20000000),
                 ("changeGray", 0x10000000), ("lightByContents", 0x08000000), ("lightByBackground", 0x04000000),
                 ("lightByGray", 0x02000000), ("doesNotDimImage", 0x1000))
# Those masks as a button cell's flags hold them.
HIGHLIGHT_BITS = {2: 0x80000000, 1: 0x08000000, 8: 0x04000000, 4: 0x02000000}
SHOWS_STATE_BITS = {1: 0x40000000, 8: 0x20000000, 4: 0x10000000}
IMAGE_POSITION = {"left": 0x00380000, "right": 0x00280000, "below": 0x00180000,
                  "above": 0x00080000, "only": 0x00400000, "overlaps": 0x00480000,
                  "leading": 0x00380000, "trailing": 0x00280000}
IMAGE_SCALING = {"none": 0, "proportionallyUpOrDown": 1, "proportionallyDown": 2,
                 "axesIndependently": 3}
STYLE_MASK = (("titled", 1), ("closable", 2), ("miniaturizable", 4), ("resizable", 8),
              ("texturedBackground", 256), ("unifiedTitleAndToolbar", 4096),
              ("fullSizeContentView", 32768), ("utility", 16), ("docModal", 64),
              ("nonactivatingPanel", 128), ("HUD", 8192))
MODIFIERS = (("shift", 1 << 17), ("control", 1 << 18), ("option", 1 << 19),
             ("command", 1 << 20), ("function", 1 << 23))
# Interface Builder's system fonts (metaFont): the font's number in
# NSfFlags, its size and whether it is bold, as the reference tools write
# them ((number << 8) | 0x10 | number).
META_FONTS = {"system": (4, 13, False), "systemBold": (8, 13, True), "smallSystem": (12, 11, False),
              "smallSystemBold": (13, 11, True), "miniSystem": (14, 9, False), "miniSystemBold": (15, 9, True),
              "label": (11, 10, False), "menu": (6, 13, False), "message": (6, 13, False),
              "controlContent": (10, 12, False), "palette": (7, 11, False), "toolTip": (9, 11, False),
              "titleBar": (3, 13, True), "menuBar": (16, 14, False), "cellTitle": (19, 12, False),
              "user": (1, 12, False)}


# The content hugging priorities a class has without IB setting them.
DEFAULT_HUGGING = {"NSImageView": (251, 251), "NSBox": (250, 750),"NSTextField": (250, 750), "NSSecureTextField": (250, 750), "NSSearchField": (250, 750),
                   "NSButton": (250, 750), "NSPopUpButton": (250, 750), "NSComboBox": (250, 750),
                   "NSSlider": (250, 750), "NSSegmentedControl": (250, 750)}


class Compiler:
    def __init__(self, path, module=None):
        self.path = path
        self.module = module
        self.warnings = []
        self.errors = []
        self.by_id = {}          # xib id -> Obj
        self.parent = []         # (Obj, parent Obj) in document order
        self.oids = []           # Objs in document order
        self.connections = []    # (source Obj, connection element)
        self.visible_windows = []
        self.delayed = []        # functions run once every object exists
        self.fonts = {}
        self.images = {}
        self.elements = {}       # id(Obj) -> its element

    # Reporting.

    def warn(self, elem, message):
        self.warnings.append(message)

    def notice_unknown(self, elem):
        self.warn(elem, "element <%s> is not compiled yet; it is a plain %s" % (elem.tag, self.class_for_tag(elem.tag)))

    # Objects.

    def register(self, elem, obj, parent):
        ident = elem.get("id")
        if ident is not None:
            self.by_id[ident] = obj
        self.elements[id(obj)] = elem
        if obj not in self.oids:
            self.oids.append(obj)
        if parent is not None:
            self.parent.append((obj, parent))
        for conn in elem.findall("connections/*"):
            self.connections.append((obj, conn))
        return obj

    def ref(self, ident):
        obj = self.by_id.get(ident)
        if obj is None:
            raise CompileError("reference to an object that is not in the document: %s" % ident)
        return obj

    def class_for_tag(self, tag):
        return "NS" + tag[0].upper() + tag[1:]

    def custom(self, elem, obj, base):
        """An object of a custom class: its class swapped for it at load."""
        cls = elem.get("customClass")
        if not cls:
            return obj
        if elem.get("customModule"):
            cls = "_TtC%d%s%d%s" % (len(elem.get("customModule")), elem.get("customModule"), len(cls), cls)
        swapper = Obj("NSClassSwapper", [("NSClassName", string(cls)), ("NSOriginalClassName", string(base))])
        swapper.items.extend(obj.items)
        return swapper

    # The document.

    def compile(self, tree):
        doc = tree.getroot()
        if doc.tag != "document":
            raise CompileError("not an Interface Builder document")
        objects = doc.find("objects")
        if objects is None:
            raise CompileError("the document has no objects")

        # Views in constraints: only they stop translating their
        # autoresizing mask (IB leaves the others as they were).
        self.constrained = set()
        for holder in doc.iter():
            cs = holder.find("constraints")
            if cs is None:
                continue
            for c in cs.findall("constraint"):
                self.constrained.add(c.get("firstItem") or holder.get("id"))
                if c.get("secondItem") or c.get("secondAttribute"):
                    self.constrained.add(c.get("secondItem") or holder.get("id"))

        owner = None
        top = []
        for elem in objects:
            ident = elem.get("id")
            if elem.tag == "customObject" and ident in (OWNER, FIRST_RESPONDER, APPLICATION):
                name = {OWNER: elem.get("customClass") or "NSObject", FIRST_RESPONDER: "FirstResponder",
                        APPLICATION: "NSApplication"}[ident]
                obj = Obj("NSCustomObject", [("NSClassName", string(name))])
                if ident != FIRST_RESPONDER:
                    obj.add("IBClassReference", self.class_reference(elem))
                self.register(elem, obj, None)
                if ident == OWNER:
                    owner = obj
                else:
                    top.append(obj)
                continue
            obj = self.element(elem, None)
            if obj is not None:
                top.append(obj)
        if owner is None:
            owner = Obj("NSCustomObject", [("NSClassName", string("NSObject"))])
            self.oids.insert(0, owner)

        for fn in self.delayed:
            fn()

        connectors = []
        for source, conn in self.connections:
            try:
                c = self.connector(source, conn)
            except CompileError as e:
                self.warn(conn, "connection %s not made: %s" % (conn.get("id"), e))
                continue
            if c is not None:
                connectors.append(c)

        # The object table: each object and its parent (the owner for
        # top-level objects); the First Responder only when used.
        used = set(id(c.get("NSDestination")) for c in connectors) | set(id(c.get("NSSource")) for c in connectors)
        parents = {id(o): p for o, p in self.parent}
        top_ids = set(id(o) for o in top)
        unused_responder = [o for o in top if o is self.by_id.get(FIRST_RESPONDER) and id(o) not in used]
        keys, values = [], []
        oids = [owner]
        for obj in self.oids:
            if obj is owner or any(obj is u for u in unused_responder):
                continue
            oids.append(obj)
            if id(obj) in top_ids:
                keys.append(obj)
                values.append(owner)
            elif id(obj) in parents:
                keys.append(obj)
                values.append(parents[id(obj)])
        data = Obj("NSIBObjectData", [
            ("NSAccessibilityConnectors", array([], "NSMutableArray")),
            ("NSAccessibilityOidsKeys", array([])),
            ("NSAccessibilityOidsValues", array([])),
            ("NSConnections", array(connectors, "NSMutableArray")),
            ("NSObjectsKeys", array(keys)),
            ("NSObjectsValues", array(values)),
            ("NSOidsKeys", array(oids + connectors)),
            ("NSOidsValues", array([number(i + 1) for i in range(len(oids) + len(connectors))])),
            ("NSRoot", owner),
            ("NSVisibleWindows", array(self.visible_windows, "NSMutableSet")),
        ])
        return Obj("NSObject", [("IB.objectdata", data), ("IB.systemFontUpdateVersion", 1)])

    def element(self, elem, parent):
        handler = getattr(self, "e_" + elem.tag, None)
        if handler is None:
            self.notice_unknown(elem)
            obj = Obj(self.class_for_tag(elem.tag))
            return self.register(elem, obj, parent)
        return handler(elem, parent)

    # Connections.

    def connector(self, source, conn):
        if conn.tag == "outlet":
            return Obj("NSNibOutletConnector", [
                ("NSSource", source), ("NSDestination", self.ref(conn.get("destination"))),
                ("NSLabel", string(conn.get("property"))),
                ("NSChildControllerCreationSelectorName", None)])
        if conn.tag == "action":
            c = Obj("NSNibControlConnector", [("NSSource", source)])
            target = conn.get("target")
            if target and target != FIRST_RESPONDER:
                c.add("NSDestination", self.ref(target))
            c.add("NSLabel", string(conn.get("selector")))
            return c
        if conn.tag == "binding":
            name, path = conn.get("name"), conn.get("keyPath")
            c = Obj("NSNibBindingConnector", [
                ("NSSource", source), ("NSDestination", self.ref(conn.get("destination"))),
                ("NSLabel", string("%s: %s" % (name, path))), ("NSBinding", string(name)),
                ("NSKeyPath", string(path))])
            options = conn.find("dictionary[@key='options']")
            if options is not None:
                c.add("NSOptions", self.value(options))
            c.add("NSNibBindingConnectorVersion", 2)
            return c
        self.warn(conn, "connection <%s> is not compiled yet" % conn.tag)
        return None

    # Values: strings, numbers, dictionaries and arrays in a document.

    def value(self, elem):
        t = elem.tag
        if t == "string":
            return string(text_of(elem))
        if t == "integer":
            return number(int(elem.get("value")))
        if t == "real":
            return number(float(elem.get("value")))
        if t == "bool":
            return number(yes(elem.get("value")))
        if t == "nil":
            return None
        if t == "dictionary":
            return dictionary([(string(e.get("key")), self.value(e)) for e in elem], "NSDictionary")
        if t in ("array", "mutableArray"):
            return array([self.value(e) for e in elem], "NSMutableArray" if t == "mutableArray" else "NSArray")
        if t == "color":
            return self.color(elem)
        if t == "font":
            return self.font(elem)
        self.warn(elem, "value <%s> is not compiled yet" % t)
        return None

    # Placeholders and plain objects.

    def e_customObject(self, elem, parent):
        cls = elem.get("customClass") or "NSObject"
        if elem.get("customModule"):
            m = elem.get("customModule")
            cls = "_TtC%d%s%d%s" % (len(m), m, len(cls), cls)
        obj = Obj("NSCustomObject", [("NSClassName", string(cls)), ("IBClassReference", self.class_reference(elem))])
        return self.register(elem, obj, parent)

    def class_reference(self, elem):
        return Obj("IBClassReference", [("IBClassName", string(elem.get("customClass") or "NSObject")),
                                        ("IBModuleName", string(elem.get("customModule")) if elem.get("customModule") else None),
                                        ("IBModuleProvider", string(elem.get("customModuleProvider")) if elem.get("customModuleProvider") else None)])

    def controller(self, elem, parent, cls):
        obj = Obj(cls, [("NSEditable", not elem.get("editable") == "NO")])
        keys = elem.find("declaredKeys")
        if keys is not None:
            obj.add("NSDeclaredKeys", array([string(e.text or "") for e in keys.findall("string")], "NSMutableArray"))
        if elem.get("automaticallyPreparesContent") == "YES":
            obj.add("NSAutomaticallyPreparesContent", True)
        if elem.get("objectClassName"):
            obj.add("NSObjectClassName", string(elem.get("objectClassName")))
        return self.register(elem, self.custom(elem, obj, cls), parent)

    def e_objectController(self, elem, parent):
        return self.controller(elem, parent, "NSObjectController")

    def e_arrayController(self, elem, parent):
        obj = self.controller(elem, parent, "NSArrayController")
        if elem.get("avoidsEmptySelection") != "NO":
            obj.add("NSAvoidsEmptySelection", True)
        if elem.get("preservesSelection") != "NO":
            obj.add("NSPreservesSelection", True)
        if elem.get("selectsInsertedObjects") != "NO":
            obj.add("NSSelectsInsertedObjects", True)
        if elem.get("clearsFilterPredicateOnInsertion") != "NO":
            obj.add("NSClearsFilterPredicateOnInsertion", True)
        return obj

    def e_userDefaultsController(self, elem, parent):
        obj = Obj("NSUserDefaultsController", [("NSSharedInstance", yes(elem.get("representsSharedInstance")))])
        return self.register(elem, obj, parent)

    # Fonts and colors.

    def font(self, elem):
        meta, size = elem.get("metaFont"), elem.get("size")
        if meta in META_FONTS:
            number, default, bold = META_FONTS[meta]
            name = "Helvetica" if meta == "user" else ".AppleSystemUIFontBold" if bold else ".AppleSystemUIFont"
            pt = float(size) if size else float(default)
            flags = (number << 8) | 0x10 | number
        elif meta == "userFixedPitch":
            name, pt, flags = "Menlo-Regular", float(size or 10), 16
        elif meta or elem.get("usesAppearanceFont") == "YES":
            name, pt, flags = ".AppleSystemUIFont", float(size or 13), 0x414
        else:
            name, pt, flags = elem.get("name") or "Helvetica", float(size or 13), 16
        key = (name, pt, flags)
        if key not in self.fonts:
            self.fonts[key] = Obj("NSFont", [("NSName", string(name)), ("NSSize", pt), ("NSfFlags", flags)])
        return self.fonts[key]

    def color(self, elem):
        space = elem.get("colorSpace")
        if space == "catalog" or elem.get("catalog"):
            fallback = Obj("NSColor", [("NSColorSpace", 3), ("NSWhite", b"0\x00")])
            return Obj("NSColor", [("NSCatalogName", string(elem.get("catalog") or "System")),
                                   ("NSColor", fallback), ("NSColorName", string(elem.get("name") or "")),
                                   ("NSColorSpace", 6)])
        if elem.get("white") is not None:
            text = "%s %s" % (elem.get("white"), elem.get("alpha") or "1")
            return Obj("NSColor", [("NSColorSpace", 3 if space == "calibratedWhite" else 4),
                                   ("NSWhite", (text + "\x00").encode())])
        comps = [elem.get(k) or "0" for k in ("red", "green", "blue")]
        alpha = elem.get("alpha") or "1"
        text = " ".join(comps + ([alpha] if alpha != "1" else []))
        kind = 1 if space in ("calibratedRGB", None) else 2
        return Obj("NSColor", [("NSColorSpace", kind), ("NSRGB", (text + "\x00").encode())])

    # Views.

    def view_items(self, elem, obj, superview, cls):
        """The keys of NSView for elem, into obj (its subviews compiled)."""
        frame = elem.find("rect[@key='frame']")
        x, y, w, h = rect_of(frame) if frame is not None else (0, 0, 0, 0)
        mask = 0
        ar = elem.find("autoresizingMask")
        if ar is not None:
            for name, bit in AUTORESIZING:
                if yes(ar.get(name)):
                    mask |= bit
        elif superview is not None or elem.get("key") == "contentView":
            mask = 4 | 8    # the default but for a top-level view's (none)
        flags = 0x100 | mask
        if yes(elem.get("hidden")):
            flags |= 0x80000000
        obj.add("IBNSClipsToBounds", 1 if yes(elem.get("clipsToBounds")) else 0)
        obj.add("IBNSLayoutMarginsGuide", None)
        obj.add("IBNSSafeAreaLayoutGuide", None)
        obj.add("NSNextResponder", superview)
        obj.add("NSNibTouchBar", None)
        obj.add("NSvFlags", flags)
        if superview is None and x == 0 and y == 0:
            obj.add("NSFrameSize", string(size_string(w, h)))
        else:
            obj.add("NSFrame", string(rect_string(x, y, w, h)))
        if superview is not None:
            obj.add("NSSuperview", superview)
        # A view whose frame is fixed (IB made its constraints) keeps its
        # autoresizing mask as constraints.
        if elem.get("translatesAutoresizingMaskIntoConstraints") == "NO" and not yes(elem.get("fixedFrame")) \
                and elem.get("id") in self.constrained:
            obj.add("NSDoNotTranslateAutoresizingMask", True)
        if elem.get("toolTip"):
            obj.add("NSToolTip", string(elem.get("toolTip")))
        tag = elem.get("tag")
        if tag is not None and cls in ("NSView", "NSCustomView"):
            obj.add("NSTag", int(tag))
        # Content priorities when not the class's own.
        dh, dv = DEFAULT_HUGGING.get(cls, (250, 250))
        h = (elem.get("horizontalHuggingPriority") or str(dh), elem.get("verticalHuggingPriority") or str(dv))
        if h != (str(dh), str(dv)):
            obj.add("NSHuggingPriority", string("{%s, %s}" % h))
        r = (elem.get("horizontalCompressionResistancePriority") or "750",
             elem.get("verticalCompressionResistancePriority") or "750")
        if r != ("750", "750"):
            obj.add("NSAntiCompressionPriority", string("{%s, %s}" % r))
        obj.add("NSViewWantsBestResolutionOpenGLSurface", True)

        subviews = elem.find("subviews")
        if subviews is not None:
            subs = [self.element(s, obj) for s in subviews]
            obj.add("NSSubviews", array([s for s in subs if s is not None], "NSMutableArray"))
        constraints = elem.find("constraints")
        if constraints is not None:
            def add_constraints():
                made = []
                for c in constraints.findall("constraint"):
                    try:
                        made.append(self.constraint(c, obj))
                    except CompileError as e:
                        self.warn(c, "constraint %s left out: %s" % (c.get("id"), e))
                obj.add("NSViewConstraints", array(made))
            self.delayed.append(add_constraints)

    def constraint(self, elem, view):
        attrs = {"left": 1, "right": 2, "top": 3, "bottom": 4, "leading": 5, "trailing": 6,
                 "width": 7, "height": 8, "centerX": 9, "centerY": 10, "baseline": 11,
                 "lastBaseline": 11, "firstBaseline": 12}
        c = Obj("NSLayoutConstraint")
        first = elem.get("firstItem")
        c.add("NSFirstItem", self.ref(first) if first else view)
        c.add("NSFirstAttribute", attrs.get(elem.get("firstAttribute"), 0))
        c.add("NSFirstAttributeV2", attrs.get(elem.get("firstAttribute"), 0))
        if elem.get("secondItem") or elem.get("secondAttribute"):
            second = elem.get("secondItem")
            c.add("NSSecondItem", self.ref(second) if second else view)
            c.add("NSSecondAttribute", attrs.get(elem.get("secondAttribute"), 0))
            c.add("NSSecondAttributeV2", attrs.get(elem.get("secondAttribute"), 0))
        relation = {"lessThanOrEqual": -1, "greaterThanOrEqual": 1}.get(elem.get("relation"))
        if relation:
            c.add("NSRelation", relation)
        if elem.get("multiplier"):
            m = elem.get("multiplier")
            if ":" in m:
                a, b = m.split(":")
                m = float(a) / float(b)
            c.add("NSMultiplier", float(m))
        if elem.get("symbolic") == "YES":
            c.add("NSSymbolicConstant", string("NSSpace"))
        constant = num(elem.get("constant"))
        if constant:
            c.add("NSConstant", constant)
            c.add("NSConstantV2", constant)
        if elem.get("priority"):
            c.add("NSPriority", int(float(elem.get("priority"))))
        c.add("NSShouldBeArchived", True)
        if elem.get("identifier"):
            c.add("NSLayoutIdentifier", string(elem.get("identifier")))
        return c

    def e_view(self, elem, parent, cls="NSView"):
        obj = Obj(cls)
        self.register(elem, obj, parent)
        self.view_items(elem, obj, parent if isinstance(parent, Obj) and parent.cls != "NSWindowTemplate" else None, cls)
        result = self.custom(elem, obj, cls)
        if result is not obj:
            self.by_id[elem.get("id")] = result
            self.replace(obj, result)
        return result

    def e_customView(self, elem, parent):
        """A view of a class given by name (NSView without one)."""
        obj = Obj("NSCustomView")
        self.register(elem, obj, parent)
        cls = elem.get("customClass") or "NSView"
        if elem.get("customModule"):
            m = elem.get("customModule")
            cls = "_TtC%d%s%d%s" % (len(m), m, len(cls), cls)
        obj.add("NSClassName", string(cls))
        self.view_items(elem, obj, parent if isinstance(parent, Obj) and parent.cls != "NSWindowTemplate" else None,
                        "NSCustomView")
        # A custom view's subviews are archived last first (its loading
        # reverses them), as the reference tools write them.
        subs = obj.get("NSSubviews")
        if subs is not None:
            elements = [v for k, v in subs.items if k == N.ELEMENT_KEY]
            subs.items = [(k, v) for k, v in subs.items if k != N.ELEMENT_KEY] + \
                [(N.ELEMENT_KEY, v) for v in reversed(elements)]
        return obj

    def replace(self, old, new):
        """A swapped object in place of its plain one everywhere so far."""
        self.oids = [new if o is old else o for o in self.oids]
        self.parent = [((new if a is old else a), (new if b is old else b)) for a, b in self.parent]
        self.connections = [((new if s is old else s), c) for s, c in self.connections]
        stack, seen = list(new.items), set()
        for _, v in new.items:
            if isinstance(v, Obj):
                self._replace_in(v, old, new, seen)

    def _replace_in(self, obj, old, new, seen):
        if id(obj) in seen:
            return
        seen.add(id(obj))
        for i, (k, v) in enumerate(obj.items):
            if v is old:
                obj.items[i] = (k, new)
            elif isinstance(v, Obj):
                self._replace_in(v, old, new, seen)

    # Controls and cells.

    def control_items(self, elem, obj, cell, cell_elem=None):
        """NSControl's keys; some repeat the cell's (IB stores both)."""
        c = cell_elem if cell_elem is not None else elem
        obj.add("NSAllowsLogicalLayoutDirection", False)
        obj.add("NSCell", cell)
        obj.add("NSControlContinuous", yes(c.get("continuous")))
        obj.add("NSControlLineBreakMode", LINE_BREAK.get(c.get("lineBreakMode"), 0))
        obj.add("NSControlRefusesFirstResponder", yes(c.get("refusesFirstResponder")) or yes(elem.get("refusesFirstResponder")))
        obj.add("NSControlSendActionMask", 4)
        obj.add("NSControlSize", CONTROL_SIZE.get(c.get("controlSize") or elem.get("controlSize"), 0))
        obj.add("NSControlTextAlignment", ALIGNMENT.get(c.get("alignment"), 4))
        obj.add("NSControlUsesSingleLineMode", yes(c.get("usesSingleLineMode")))
        obj.add("NSControlWritingDirection", {"natural": -1, "leftToRight": 0, "rightToLeft": 1}.get(
            c.get("baseWritingDirection"), -1))
        obj.add("NSEnabled", elem.get("enabled") != "NO")
        if elem.get("tag"):
            obj.add("NSTag", int(elem.get("tag")))

    def cell_flags(self, elem, cell_type, extra=0, extra2=0):
        flags = extra | (cell_type << 26)
        if elem.get("state") == "on":
            flags |= 0x80000000
        if elem.get("enabled") == "NO":
            flags |= 0x20000000
        if yes(elem.get("editable")):
            flags |= 0x10000000
        border = elem.get("borderStyle") or ""
        if border in ("border", "borderAndBezel") or yes(elem.get("bordered")):
            flags |= 0x00800000
        if border.startswith("bezel") or border == "borderAndBezel":
            flags |= 0x00400000
        if yes(elem.get("selectable")) or yes(elem.get("editable")):
            flags |= 0x00200000
        if yes(elem.get("scrollable")):
            flags |= 0x00100000
        if yes(elem.get("continuous")):
            flags |= 0x00080000
        # A line that is not wrapped (clipped or truncated), and a cell
        # whose text can be selected, as the reference tools mark them.
        if elem.get("lineBreakMode") in ("clipping", "truncatingHead", "truncatingTail", "truncatingMiddle"):
            flags |= 0x40
        if yes(elem.get("selectable")) or yes(elem.get("editable")):
            flags |= 0x1
        if yes(elem.get("usesSingleLineMode")):
            flags |= 0x400
        flags2 = extra2
        flags2 |= CELL_ALIGNMENT.get(elem.get("alignment"), 4 if cell_type == 1 else 0) << 26
        if yes(elem.get("allowsMixedState")):
            flags2 |= 0x01000000
        if yes(elem.get("sendsActionOnEndEditing")):
            flags2 |= 0x00400000
        flags2 |= (LINE_BREAK.get(elem.get("lineBreakMode"), 0) & 7) << 9
        flags2 |= (CONTROL_SIZE.get(elem.get("controlSize"), 0) & 7) << 17
        if yes(elem.get("allowsEditingTextAttributes")):
            flags2 |= 0x40000000
        if yes(elem.get("refusesFirstResponder")):
            flags2 |= 0x02000000
        signed = lambda v: v - (1 << 32) if v & 0x80000000 else v
        return signed(flags & 0xFFFFFFFF), signed(flags2 & 0xFFFFFFFF)

    def cell_common(self, elem, cell, control):
        cell.add("NSContents", string(attr(elem, "title", "")))
        font = elem.find("font[@key='font']")
        cell.add("NSSupport", self.font(font) if font is not None else self.font(ET.Element("font", metaFont="system")))
        cell.add("NSControlView", control)
        if elem.get("tag"):
            cell.add("NSTag", int(elem.get("tag")))
        formatter = None
        for f in elem:
            if f.get("key") == "formatter":
                formatter = f
        if formatter is not None:
            cell.add("NSFormatter", self.element(formatter, cell))

    def e_textField(self, elem, parent, cls="NSTextField"):
        obj = Obj(cls)
        self.register(elem, obj, parent)
        self.view_items(elem, obj, parent, cls)
        cell_elem = elem.find("*[@key='cell']")
        cell = self.text_field_cell(cell_elem, obj) if cell_elem is not None else None
        self.control_items(elem, obj, cell, cell_elem)
        obj.add("NS.resolvesNaturalAlignmentWithBaseWritingDirection", False)
        obj.add("NSAllowsWritingTools", True)
        obj.add("NSTextFieldAlignmentRectInsetsVersion", 2)
        obj.add("NSTextFieldAllowsWritingToolsAffordance", False)
        if elem.get("preferredMaxLayoutWidth") and float(elem.get("preferredMaxLayoutWidth")):
            obj.add("NSPreferredMaxLayoutWidth", float(elem.get("preferredMaxLayoutWidth")))
        result = self.custom(elem, obj, cls)
        if result is not obj:
            self.by_id[elem.get("id")] = result
            self.replace(obj, result)
        return result

    def e_secureTextField(self, elem, parent):
        return self.e_textField(elem, parent, "NSSecureTextField")

    def e_searchField(self, elem, parent):
        return self.e_textField(elem, parent, "NSSearchField")

    def text_field_cell(self, elem, control, cls="NSTextFieldCell"):
        cell = Obj(cls)
        self.register(elem, cell, control)
        f, f2 = self.cell_flags(elem, 1)
        cell.add("NSCellFlags", f)
        cell.add("NSCellFlags2", f2)
        self.cell_common(elem, cell, control)
        if yes(elem.get("drawsBackground")):
            cell.add("NSDrawsBackground", True)
        if elem.get("placeholderString"):
            cell.add("NSPlaceholderString", string(elem.get("placeholderString")))
        for key, nib_key in (("backgroundColor", "NSBackgroundColor"), ("textColor", "NSTextColor")):
            c = elem.find("color[@key='%s']" % key)
            if c is not None:
                cell.add(nib_key, self.color(c))
        return self.custom(elem, cell, cls)

    def e_button(self, elem, parent):
        obj = Obj("NSButton")
        self.register(elem, obj, parent)
        self.view_items(elem, obj, parent, "NSButton")
        cell_elem = elem.find("*[@key='cell']")
        cell = self.button_cell(cell_elem, obj) if cell_elem is not None else None
        self.control_items(elem, obj, cell, cell_elem)
        result = self.custom(elem, obj, "NSButton")
        if result is not obj:
            self.by_id[elem.get("id")] = result
            self.replace(obj, result)
        return result

    def button_cell(self, elem, control, cls="NSButtonCell", swap=True):
        cell = Obj(cls)
        self.register(elem, cell, control)
        kind = elem.get("type") or "push"
        # How the button highlights and shows its state (<behavior>), its
        # border, image position and inset, as NSButtonFlags holds them.
        bflags = 0
        behavior = elem.find("behavior[@key='behavior']")
        if behavior is not None:
            for name, bit in BEHAVIOR_BITS:
                if yes(behavior.get(name)):
                    bflags |= bit
        if elem.get("borderStyle") in ("border", "borderAndBezel") or yes(elem.get("bordered")):
            bflags |= 0x00800000
        position = elem.get("imagePosition")
        if position:
            bflags |= IMAGE_POSITION.get(position, 0)
        if kind in ("check", "radio"):
            bflags |= 0x100
        bflags |= (int(elem.get("inset") or 0) & 3) << 13
        if yes(elem.get("transparent")):
            bflags |= 0x8000
        bezel = BEZEL_STYLE.get(elem.get("bezelStyle"), 0)
        bflags2 = (bezel & 7) | ((bezel & 8) << 2)
        bflags2 |= IMAGE_SCALING.get(elem.get("imageScaling"), 0) << 6
        if yes(elem.get("showsBorderOnlyWhileMouseInside")):
            bflags2 |= 0x8
        f, f2 = self.cell_flags(elem, 1 if elem.get("title") else 0, extra2=0)
        f &= ~0x00800000     # a button's border is in its button flags
        cell.add("NSCellFlags", f)
        cell.add("NSCellFlags2", f2)
        self.cell_common(elem, cell, control)
        signed = lambda v: v - (1 << 32) if v & 0x80000000 else v
        cell.add("NSButtonFlags", signed(bflags))
        cell.add("NSButtonFlags2", bflags2)
        cell.add("NSAlternateContents", string(attr(elem, "alternateTitle", "")))
        cell.add("NSAuxButtonType", self.button_type(kind, behavior))
        cell.add("NSBezelStyle", BEZEL_STYLE.get(elem.get("bezelStyle"), 0))
        cell.add("NSKeyEquivalent", string(attr(elem, "keyEquivalent", "")))
        mods = elem.find("modifierMask[@key='keyEquivalentModifierMask']")
        if mods is not None and self.modifiers(mods):
            cell.add("NSKeyEquivModMask", self.modifiers(mods))
        cell.add("NSPeriodicDelay", int(elem.get("periodicDelay") or 400))
        cell.add("NSPeriodicInterval", int(elem.get("periodicInterval") or 75))
        if kind == "check":
            cell.add("NSNormalImage", self.image_resource("NSSwitch"))
            cell.add("NSAlternateImage", self.image_resource("NSSwitch"))
        if kind == "radio":
            cell.add("NSNormalImage", self.image_resource("NSRadioButton"))
            cell.add("NSAlternateImage", self.image_resource("NSRadioButton"))
        image = elem.get("image")
        if image:
            cell.add("NSNormalImage", self.image_resource(image))
        return self.custom(elem, cell, cls) if swap else cell

    def button_type(self, kind, behavior):
        """NSButtonType from a button's style and <behavior>."""
        if kind == "check":
            return 3
        if kind == "radio":
            return 4
        b = set(k for k, v in (behavior.attrib.items() if behavior is not None else []) if v == "YES")
        b.discard("doesNotDimImage")
        if b == {"lightByContents"}:
            return 5
        if "pushIn" in b and "changeContents" in b:
            return 2
        if "pushIn" in b and "changeBackground" in b and "lightByContents" in b:
            return 2
        if "pushIn" in b and "changeBackground" in b:
            return 1
        if "changeBackground" in b:
            return 6
        if "pushIn" in b:
            return 7
        return 0

    def image_resource(self, name):
        """An image by name, one object for each name."""
        if name not in self.images:
            self.images[name] = Obj("NSCustomResource", [("NSClassName", string("NSImage")),
                                                         ("NSResourceName", string(name))])
        return self.images[name]

    def modifiers(self, elem):
        mask = 0
        for name, bit in MODIFIERS:
            if yes(elem.get(name)):
                mask |= bit
        return mask

    def control(self, elem, parent, cls, make_cell):
        """A control: its view keys, its cell (made by make_cell from the
        <... key="cell"> element) and NSControl's keys."""
        obj = Obj(cls)
        self.register(elem, obj, parent)
        self.view_items(elem, obj, parent, cls)
        cell_elem = elem.find("*[@key='cell']")
        cell = make_cell(cell_elem, obj) if cell_elem is not None else None
        self.control_items(elem, obj, cell, cell_elem)
        result = self.custom(elem, obj, cls)
        if result is not obj:
            self.by_id[elem.get("id")] = result
            self.replace(obj, result)
        return obj, result

    def e_popUpButton(self, elem, parent):
        return self.control(elem, parent, "NSPopUpButton", self.pop_up_button_cell)[1]

    def pop_up_button_cell(self, elem, control):
        cell = self.button_cell(elem, control, "NSPopUpButtonCell", swap=False)
        cell.add("NSAltersState", elem.get("altersStateOfSelectedItem") != "NO")
        cell.add("NSArrowPosition", {"noArrow": 0, "arrowAtCenter": 1, "arrowAtBottom": 2}.get(
            elem.get("arrowPosition"), 2))
        cell.add("NSMenuItemRespectAlignment", True)
        cell.add("NSPeriodicDelay", 400)
        cell.add("NSPeriodicInterval", 75)
        cell.add("NSPreferredEdge", {"minX": 0, "minY": 1, "maxX": 2, "maxY": 3}.get(elem.get("preferredEdge"), 1))
        if yes(elem.get("pullsDown")):
            cell.add("NSPullDown", True)
        cell.add("NSUsesItemFromMenu", elem.get("usesItemFromMenu") != "NO")
        if cell.get("NSAlternateContents") is None:
            cell.add("NSAlternateContents", string(""))
        menu_elem = elem.find("menu[@key='menu']")
        menu = self.e_menu(menu_elem, cell) if menu_elem is not None else None
        cell.add("NSMenu", menu)
        selected = elem.get("selectedItem")
        if selected:
            self.delayed.append(lambda: cell.add("NSMenuItem", self.by_id.get(selected)))
        else:
            cell.add("NSMenuItem", None)
        return self.custom(elem, cell, "NSPopUpButtonCell")

    def e_imageView(self, elem, parent):
        obj, result = self.control(elem, parent, "NSImageView", self.image_cell)
        obj.add("NSEditable", elem.get("editable") != "NO")
        obj.add("NSImageViewPlaceholderPrecedence", 0)
        obj.add("NSSubviews", array([], "NSMutableArray"))
        return result

    def image_cell(self, elem, control):
        cell = Obj("NSImageCell")
        self.register(elem, cell, control)
        f, f2 = self.cell_flags(elem, 0)
        cell.add("NSCellFlags", f)
        cell.add("NSCellFlags2", f2)
        cell.add("NSAlign", {"center": 0, "top": 1, "topLeft": 2, "topRight": 3, "left": 4, "bottom": 5,
                             "bottomLeft": 6, "bottomRight": 7, "right": 8}.get(elem.get("imageAlignment"), 0))
        cell.add("NSAnimates", yes(elem.get("animates")))
        cell.add("NSControlView", control)
        cell.add("NSImageAnimation", -1)
        cell.add("NSScale", {"proportionallyDown": 0, "axesIndependently": 1, "none": 2,
                             "proportionallyUpOrDown": 3}.get(elem.get("imageScaling"), 0))
        cell.add("NSStyle", {"none": 0, "photo": 1, "grayBezel": 2, "groove": 3, "button": 4}.get(
            elem.get("imageFrameStyle"), 0))
        if elem.get("image"):
            cell.add("NSContents", self.image_resource(elem.get("image")))
        return self.custom(elem, cell, "NSImageCell")

    def e_box(self, elem, parent):
        obj = Obj("NSBox")
        self.register(elem, obj, parent)
        self.view_items(elem, obj, parent, "NSBox")
        box_type = {"primary": 0, "secondary": 1, "separator": 2, "oldStyle": 3, "custom": 4}.get(
            elem.get("boxType"), 0)
        obj.add("NSBorderType", {"none": 0, "line": 1, "bezel": 2, "groove": 3}.get(elem.get("borderType"), 3))
        obj.add("NSBoxType", box_type)
        title_cell = Obj("NSTextFieldCell")
        f, f2 = self.cell_flags(ET.Element("cell", alignment="center"), 1)
        title_cell.items += [("NSCellFlags", f), ("NSCellFlags2", f2),
                             ("NSContents", string(attr(elem, "title", ""))),
                             ("NSSupport", self.font(elem.find("font[@key='titleFont']") if elem.find("font[@key='titleFont']") is not None
                                                     else ET.Element("font", metaFont="smallSystem")))]
        obj.add("NSTitleCell", title_cell)
        obj.add("NSTitlePosition", {"noTitle": 0, "aboveTop": 1, "atTop": 2, "belowTop": 3, "aboveBottom": 4,
                                    "atBottom": 5, "belowBottom": 6}.get(elem.get("titlePosition"), 2))
        obj.add("NSTransparent", yes(elem.get("transparent")))
        offsets = elem.find("size[@key='contentViewMargins']")
        obj.add("NSOffsets", string(size_string(num(offsets.get("width")), num(offsets.get("height")))
                                    if offsets is not None else "{5, 5}" if box_type != 2 else "{0, 0}"))
        if elem.get("cornerRadius"):
            obj.add("NSCornerRadius2", float(elem.get("cornerRadius")))
        for key, nib_key in (("borderColor", "NSBorderColor2"), ("fillColor", "NSFillColor2")):
            c = elem.find("color[@key='%s']" % key)
            if c is not None:
                obj.add(nib_key, self.color(c))
        content = elem.find("view[@key='contentView']")
        if content is not None:
            cv = self.e_view(content, obj)
            cv_items = dict(cv.items)
            obj.add("NSContentView", cv)
            subs = obj.get("NSSubviews")
            if subs is None:
                obj.add("NSSubviews", array([cv], "NSMutableArray"))
            else:
                subs.add(N.ELEMENT_KEY, cv)
            # The content view is the box's subview.
            cv.items = [(k, v) for k, v in cv.items if k not in ("NSNextResponder", "NSSuperview")]
            cv.items.insert(0, ("NSNextResponder", obj))
            cv.add("NSSuperview", obj)
        else:
            obj.add("NSSubviews", array([], "NSMutableArray"))
        return self.custom(elem, obj, "NSBox")

    def e_slider(self, elem, parent):
        return self.control(elem, parent, "NSSlider", self.slider_cell)[1]

    def slider_cell(self, elem, control):
        cell = Obj("NSSliderCell")
        self.register(elem, cell, control)
        f, f2 = self.cell_flags(elem, 0)
        cell.add("NSCellFlags", f)
        cell.add("NSCellFlags2", f2)
        cell.add("NSAllowsTickMarkValuesOnly", yes(elem.get("allowsTickMarkValuesOnly")))
        cell.add("NSAltIncValue", num(elem.get("altIncrementValue")))
        cell.add("NSControlView", control)
        cell.add("NSMaxValue", num(elem.get("maxValue"), 1.0))
        cell.add("NSMinValue", num(elem.get("minValue"), 0.0))
        cell.add("NSNumberOfTickMarks", int(elem.get("numberOfTickMarks") or 0))
        cell.add("NSTickMarkPosition", {"below": 0, "above": 1, "leading": 1, "trailing": 0}.get(
            elem.get("tickMarkPosition"), 0))
        cell.add("NSValue", num(elem.get("doubleValue")))
        cell.add("NSVertical", yes(elem.get("vertical")))
        if elem.get("sliderType") == "circular":
            cell.add("NSSliderType", 1)
        return self.custom(elem, cell, "NSSliderCell")

    # Scroll views, clip views, scrollers and tables.

    def e_scrollView(self, elem, parent):
        """A scroll view: its clip view (holding the document), its
        scrollers, and for a table with a header the header's clip view and
        the corner view, as subviews in the order the reference tools give."""
        sv = Obj("NSScrollView")
        self.register(elem, sv, parent)
        items_before = len(sv.items)
        self.view_items(elem, sv, parent, "NSScrollView")
        clip_elem = elem.find("clipView[@key='contentView']")
        clip = self.clip_view(clip_elem, sv) if clip_elem is not None else None
        doc = clip.get("NSDocView") if clip is not None else None

        hs_elem = elem.find("scroller[@key='horizontalScroller']")
        vs_elem = elem.find("scroller[@key='verticalScroller']")
        hs = self.scroller(hs_elem, sv) if hs_elem is not None else None
        vs = self.scroller(vs_elem, sv) if vs_elem is not None else None

        header_elem = elem.find("tableHeaderView[@key='headerView']")
        header_clip = corner = None
        if header_elem is not None and doc is not None:
            hx, hy, hw, hh = rect_of(header_elem.find("rect[@key='frame']"))
            header = Obj("NSTableHeaderView")
            self.register(header_elem, header, None)
            header_clip = Obj("NSClipView")
            header.items += [("IBNSClipsToBounds", 0), ("IBNSLayoutMarginsGuide", None), ("IBNSSafeAreaLayoutGuide", None),
                             ("NSFrameSize", string(size_string(hw, hh))), ("NSNextResponder", header_clip),
                             ("NSNibTouchBar", None), ("NSSuperview", header_clip), ("NSTableView", doc),
                             ("NSViewIsLayerTreeHost", True), ("NSViewWantsBestResolutionOpenGLSurface", True),
                             ("NSvFlags", 256)]
            offset = 1 if (elem.get("borderType") or "bezel") != "none" else 0
            header_clip.items += [("IBNSClipsToBounds", 0), ("IBNSLayoutMarginsGuide", None),
                                  ("IBNSSafeAreaLayoutGuide", None), ("NSAutomaticallyAdjustsContentInsets", True),
                                  ("NSDocView", header), ("NSFrame", string(rect_string(offset, offset, hw, hh))),
                                  ("NSNextKeyView", header), ("NSNextResponder", sv), ("NSNibTouchBar", None),
                                  ("NSSubviews", array([header], "NSMutableArray")), ("NSSuperview", sv),
                                  ("NSViewWantsBestResolutionOpenGLSurface", True), ("NSvFlags", 256)]
            doc.add("NSHeaderView", header)
            corner = Obj("_NSCornerView", [("IBNSClipsToBounds", 0), ("IBNSLayoutMarginsGuide", None),
                                           ("IBNSSafeAreaLayoutGuide", None), ("NSFrameSize", string("{17, 28}")),
                                           ("NSNextResponder", sv), ("NSNibTouchBar", None), ("NSSuperview", sv),
                                           ("NSViewWantsBestResolutionOpenGLSurface", True),
                                           ("NSvFlags", -2147483392)])
            doc.add("NSCornerView", corner)
            # The clip view's bounds start under the header.
            if clip is not None:
                cw, ch = clip_elem_size = rect_of(clip_elem.find("rect[@key='frame']"))[2:]
                clip.add("NSBounds", string(rect_string(0, -hh, cw, ch)))

        subviews = [v for v in (clip, header_clip, hs, corner, vs) if v is not None]
        sv.add("NSSubviews", array(subviews, "NSMutableArray"))
        sv.add("NSContentView", clip)
        if corner is not None:
            sv.add("NSCornerView", corner)
        if hs is not None:
            sv.add("NSHScroller", hs)
        if header_clip is not None:
            sv.add("NSHeaderClipView", header_clip)
        sv.add("NSMagnification", num(elem.get("magnification"), 1.0))
        sv.add("NSMaxMagnification", num(elem.get("maxMagnification"), 4.0))
        sv.add("NSMinMagnification", num(elem.get("minMagnification"), 0.25))
        sv.add("NSNextKeyView", clip)
        import struct
        amounts = [num(elem.get(k), 10.0) for k in ("horizontalPageScroll", "verticalPageScroll",
                                                     "horizontalLineScroll", "verticalLineScroll")]
        if any(elem.get(k) for k in ("horizontalPageScroll", "verticalLineScroll")):
            sv.add("NSScrollAmts", struct.pack(">4f", *amounts))
        if vs is not None:
            sv.add("NSVScroller", vs)
        flags = {"none": 0, "line": 1, "bezel": 2, "groove": 3}.get(elem.get("borderType"), 2)
        if elem.get("hasVerticalScroller") != "NO":
            flags |= 0x10
        if elem.get("hasHorizontalScroller") != "NO":
            flags |= 0x20
        if yes(elem.get("autohidesScrollers")):
            flags |= 0x200
        sv.add("NSsFlags", flags | 0x20800)
        return self.custom(elem, sv, "NSScrollView")

    def clip_view(self, elem, scroll_view):
        clip = Obj("NSClipView")
        self.register(elem, clip, scroll_view)
        x, y, w, h = rect_of(elem.find("rect[@key='frame']"))
        clip.items += [("IBNSClipsToBounds", 0), ("IBNSLayoutMarginsGuide", None), ("IBNSSafeAreaLayoutGuide", None),
                       ("NSAutomaticallyAdjustsContentInsets", elem.get("automaticallyAdjustsContentInsets") != "NO")]
        color = elem.find("color[@key='backgroundColor']")
        docs = elem.find("subviews")
        doc = None
        if docs is not None and len(docs):
            doc = self.element(docs[0], clip)
            for extra in list(docs)[1:]:
                self.element(extra, clip)
        if color is not None:
            clip.add("NSBGColor", self.color(color))
        elif doc is not None and doc.get("NSBackgroundColor") is not None:
            clip.add("NSBGColor", doc.get("NSBackgroundColor"))
        clip.add("NSDocView", doc)
        clip.add("NSFrame", string(rect_string(x, y, w, h)))
        clip.add("NSNextKeyView", doc)
        clip.add("NSNextResponder", scroll_view)
        clip.add("NSNibTouchBar", None)
        clip.add("NSSubviews", array([doc] if doc is not None else [], "NSMutableArray"))
        clip.add("NSSuperview", scroll_view)
        clip.add("NSViewWantsBestResolutionOpenGLSurface", True)
        cv = 0
        if elem.get("drawsBackground") != "NO":
            cv |= 4
        if elem.get("copiesOnScroll") == "NO":
            cv |= 2
        clip.add("NScvFlags", cv)
        clip.add("NSvFlags", 256)
        return self.custom(elem, clip, "NSClipView")

    def e_clipView(self, elem, parent):
        return self.clip_view(elem, parent)

    def scroller(self, elem, scroll_view):
        sc = Obj("NSScroller")
        self.register(elem, sc, scroll_view)
        x, y, w, h = rect_of(elem.find("rect[@key='frame']"))
        flags = 0x100 | (0x80000000 if yes(elem.get("hidden")) else 0)
        sc.items += [("IBNSClipsToBounds", 0), ("IBNSLayoutMarginsGuide", None), ("IBNSSafeAreaLayoutGuide", None),
                     ("NSAction", string("_doScroller:")), ("NSAllowsLogicalLayoutDirection", False),
                     ("NSControlAction", string("_doScroller:")), ("NSControlContinuous", False),
                     ("NSControlLineBreakMode", 0), ("NSControlRefusesFirstResponder", False),
                     ("NSControlSendActionMask", 4), ("NSControlSize", CONTROL_SIZE.get(elem.get("controlSize"), 0)),
                     ("NSControlTarget", scroll_view), ("NSControlTextAlignment", 0),
                     ("NSControlUsesSingleLineMode", False), ("NSControlWritingDirection", 0)]
        if elem.get("doubleValue"):
            sc.add("NSCurValue", float(elem.get("doubleValue")))
        if elem.get("enabled") == "NO":
            sc.add("NSEnabled", False)
        sc.items += [("NSFrame", string(rect_string(x, y, w, h))), ("NSNextResponder", scroll_view),
                     ("NSNibTouchBar", None)]
        if elem.get("knobProportion"):
            sc.add("NSPercent", float(elem.get("knobProportion")))
        sc.items += [("NSSuperview", scroll_view), ("NSTarget", scroll_view)]
        if yes(elem.get("wantsLayer")):
            sc.add("NSViewIsLayerTreeHost", True)
        sc.add("NSViewWantsBestResolutionOpenGLSurface", True)
        if yes(elem.get("horizontal")):
            sc.add("NSsFlags", 1)
        sc.add("NSvFlags", flags - (1 << 32) if flags & 0x80000000 else flags)
        return sc

    def e_tableView(self, elem, parent, cls="NSTableView"):
        tv = Obj(cls)
        self.register(elem, tv, parent)
        self.view_items(elem, tv, parent, cls)
        # A table's own frame is its size (it is a clip view's document).
        tv.items = [(k, v) for k, v in tv.items if k not in ("NSFrame",)]
        x, y, w, h = rect_of(elem.find("rect[@key='frame']"))
        if tv.get("NSFrameSize") is None:
            tv.add("NSFrameSize", string(size_string(w, h)))
        tv.items += [("NSAllowsLogicalLayoutDirection", False), ("NSAllowsTypeSelect", elem.get("typeSelect") != "NO")]
        bg = elem.find("color[@key='backgroundColor']")
        if bg is not None:
            tv.add("NSBackgroundColor", self.color(bg))
        tv.add("NSColumnAutoresizingStyle", {"none": 0, "uniform": 1, "sequential": 2, "reverseSequential": 3,
                                             "lastColumnOnly": 4, "firstColumnOnly": 5}.get(
            elem.get("columnAutoresizingStyle"), 1))
        if elem.get("allowsExpansionToolTips") == "YES":
            tv.add("NSControlAllowsExpansionToolTips", True)
        tv.items += [("NSControlContinuous", False), ("NSControlLineBreakMode", 0),
                     ("NSControlRefusesFirstResponder", False), ("NSControlSendActionMask", 0),
                     ("NSControlSize", 0), ("NSControlTextAlignment", 0), ("NSControlUsesSingleLineMode", False),
                     ("NSControlWritingDirection", 0), ("NSDataSource", None), ("NSDelegate", None),
                     ("NSDraggingSourceMaskForLocal", -1), ("NSDraggingSourceMaskForNonLocal", 0),
                     ("NSEnabled", True)]
        grid = elem.find("color[@key='gridColor']")
        if grid is not None:
            tv.add("NSGridColor", self.color(grid))
        grid_style = elem.find("tableViewGridLines")
        if grid_style is not None:
            mask = (1 if yes(grid_style.get("vertical")) else 0) | (2 if yes(grid_style.get("horizontal")) else 0)
            if mask:
                tv.add("NSGridStyleMask", mask)
        spacing = elem.find("size[@key='intercellSpacing']")
        sw, sh = (num(spacing.get("width")), num(spacing.get("height"))) if spacing is not None else (3.0, 2.0)
        tv.add("NSIntercellSpacingHeight", sh)
        tv.add("NSIntercellSpacingWidth", sw)
        tv.add("NSRowHeight", num(elem.get("rowHeight"), 17.0))
        self.column_index = 0
        columns = [self.table_column(c, tv) for c in elem.findall("tableColumns/tableColumn")]
        tv.add("NSTableColumns", array(columns, "NSMutableArray"))
        tv.add("NSTableViewDraggingDestinationStyle", {"none": 0, "sourceList": 1, "gap": 2}.get(
            elem.get("draggingDestinationFeedbackStyle"), 0))
        tv.add("NSTableViewGroupRowStyle", 1)
        if elem.get("autosaveName"):
            tv.add("NSAutosaveName", string(elem.get("autosaveName")))
        flags = 0
        if elem.get("columnReordering") != "NO":
            flags |= 0x80000000
        if elem.get("columnResizing") != "NO":
            flags |= 0x40000000
        if elem.get("emptySelection") != "NO":
            flags |= 0x10000000
        if elem.get("multipleSelection") != "NO":
            flags |= 0x08000000
        if yes(elem.get("columnSelection")):
            flags |= 0x04000000
        if yes(elem.get("alternatingRowBackgroundColors")):
            flags |= 0x00800000
        if elem.get("autosaveColumns") != "NO":
            flags |= 0x01000000
        flags |= 0x02200000
        tv.add("NSTvFlags", flags - (1 << 32) if flags & 0x80000000 else flags)
        return self.custom(elem, tv, cls)

    def e_outlineView(self, elem, parent):
        return self.e_tableView(elem, parent, "NSOutlineView")

    def table_column(self, elem, table):
        col = Obj("NSTableColumn")
        self.register(elem, col, table)
        data_elem = elem.find("*[@key='dataCell']")
        if data_elem is not None:
            maker = {"textFieldCell": self.text_field_cell, "imageCell": self.image_cell,
                     "buttonCell": self.button_cell, "popUpButtonCell": self.pop_up_button_cell}.get(data_elem.tag)
            col.add("NSDataCell", maker(data_elem, table) if maker else self.element(data_elem, col))
        header_elem = elem.find("tableHeaderCell[@key='headerCell']")
        if header_elem is not None:
            hc = Obj("NSTableHeaderCell")
            f, f2 = self.cell_flags(header_elem, 1)
            hc.items += [("NSCellFlags", f), ("NSCellFlags2", f2), ("NSContents", string(attr(header_elem, "title", "")))]
            font = header_elem.find("font[@key='font']")
            if font is not None:
                hc.add("NSSupport", self.font(font))
            else:
                # A header's own font: the small system font, not a metaFont.
                key = (".AppleSystemUIFont", 11.0, 16)
                if key not in self.fonts:
                    self.fonts[key] = Obj("NSFont", [("NSName", string(key[0])), ("NSSize", key[1]), ("NSfFlags", 16)])
                hc.add("NSSupport", self.fonts[key])
            for key, nib_key in (("backgroundColor", "NSBackgroundColor"), ("textColor", "NSTextColor")):
                c = header_elem.find("color[@key='%s']" % key)
                if c is not None:
                    hc.add(nib_key, self.color(c))
            col.add("NSHeaderCell", hc)
        col.add("NSIdentifier", string(elem.get("identifier") or "AutomaticTableColumnIdentifier.%d" % self.column_index))
        self.column_index += 1
        if elem.get("editable") != "NO":
            col.add("NSIsEditable", True)
        mask = elem.find("tableColumnResizingMask")
        resizing = 0
        if mask is not None:
            resizing = (1 if yes(mask.get("resizeWithTable")) else 0) | (2 if yes(mask.get("userResizable")) else 0)
        if resizing & 2:
            col.add("NSIsResizeable", True)
        col.add("NSMaxWidth", num(elem.get("maxWidth"), 1000.0))
        col.add("NSMinWidth", num(elem.get("minWidth"), 10.0))
        if mask is not None:
            col.add("NSResizingMask", resizing)
        if yes(elem.get("hidden")):
            col.add("NSHidden", True)
        col.add("NSTableView", table)
        col.add("NSWidth", num(elem.get("width"), 100.0))
        return col

    # Windows.

    def e_window(self, elem, parent, cls="NSWindow"):
        t = Obj("NSWindowTemplate")
        self.register(elem, t, parent)
        style = elem.find("windowStyleMask")
        mask = 0
        if style is not None:
            for name, bit in STYLE_MASK:
                if yes(style.get(name)):
                    mask |= bit
        content = elem.find("rect[@key='contentRect']")
        x, y, w, h = rect_of(content) if content is not None else (0, 0, 0, 0)
        screen = elem.find("rect[@key='screenRect']")
        if elem.find("windowPositionMask") is None:
            # Without a position mask the window is centred on the screen
            # it was designed on.
            sw, sh = rect_of(screen)[2:] if screen is not None else (0, 0)
            x, y = (sw - w) / 2, (sh - h) / 2
        # NSWTFlags: not released when closed, deferred, 0x00780000 always,
        # hides on deactivate, tool tips while inactive and recalculating the
        # key view loop (both on unless the document says NO).
        wt = 0x20000000 | 0x00780000
        if elem.get("releasedWhenClosed") == "NO":
            wt |= 0x40000000
        if yes(elem.get("hidesOnDeactivate")):
            wt |= 0x80000000
        if elem.get("allowsToolTipsWhenApplicationIsInactive") != "NO":
            wt |= 0x00002000
        if elem.get("autorecalculatesKeyViewLoop") != "NO":
            wt |= 0x00000800
        if elem.get("visibleAtLaunch") != "NO":
            self.visible_windows.append(t)
        t.items += [
            ("NSMaxFullScreenContentSize", string("{0, 0}")),
            ("NSMinFullScreenContentSize", string("{0, 0}")),
            ("NSScreenRect", string(rect_string(*(rect_of(screen) if screen is not None else (0, 0, 0, 0))))),
            ("NSViewClass", None),
            ("NSWTFlags", wt - (1 << 32) if wt & 0x80000000 else wt),
            ("NSWindowBacking", 2),
            ("NSWindowClass", string(elem.get("customClass") or cls)),
            ("NSWindowIsRestorable", elem.get("restorable") != "NO"),
            ("NSWindowRect", string(rect_string(x, y, w, h))),
            ("NSWindowStyleMask", mask),
            ("NSWindowTitle", string(attr(elem, "title", ""))),
            ("NSWindowSubtitle", string(attr(elem, "subtitle", ""))),
            ("NSUserInterfaceItemIdentifier", string(elem.get("identifier")) if elem.get("identifier") else None),
        ]
        if elem.get("tabbingMode"):
            t.add("NSWindowTabbingMode", {"automatic": 0, "preferred": 1, "disallowed": 2}.get(elem.get("tabbingMode"), 0))
        for key, nib_key in (("minSize", "NSMinSize"), ("maxSize", "NSMaxSize"),
                             ("contentMinSize", "NSWindowContentMinSize"),
                             ("contentMaxSize", "NSWindowContentMaxSize")):
            s = elem.find("size[@key='%s']" % key)
            if s is not None:
                t.add(nib_key, string(size_string(num(s.get("width")), num(s.get("height")))))
        if elem.find("size[@key='maxSize']") is None:
            t.add("NSMaxSize", string("{10000000000000, 10000000000000}"))
        if elem.get("frameAutosaveName"):
            t.add("NSFrameAutosaveName", string(elem.get("frameAutosaveName")))
        view = elem.find("*[@key='contentView']")
        t.add("NSWindowView", self.element(view, t) if view is not None else None)
        return t

    def e_panel(self, elem, parent):
        return self.e_window(elem, parent, "NSPanel")

    # Menus.

    def e_menu(self, elem, parent):
        menu = Obj("NSMenu")
        self.register(elem, menu, parent)
        menu.add("NSTitle", string(elem.get("title") or ""))
        items = elem.find("items")
        menu_items = [self.menu_item(i, menu) for i in (items if items is not None else [])]
        menu.add("NSMenuItems", array(menu_items, "NSMutableArray"))
        name = elem.get("systemMenu")
        if name:
            menu.add("NSName", string({"main": "_NSMainMenu", "apple": "_NSAppleMenu", "services": "_NSServicesMenu",
                                       "window": "_NSWindowsMenu", "recentDocuments": "_NSRecentDocumentsMenu",
                                       "font": "_NSFontMenu", "help": "_NSHelpMenu"}.get(name, name)))
        if elem.get("autoenablesItems") == "NO":
            menu.add("NSNoAutoenable", True)
        return self.custom(elem, menu, "NSMenu")

    def menu_item(self, elem, menu):
        item = Obj("NSMenuItem")
        self.register(elem, item, menu)
        item.add("NSAllowsKeyEquivalentLocalization", elem.get("allowsKeyEquivalentLocalization") != "NO")
        item.add("NSAllowsKeyEquivalentMirroring", elem.get("allowsAutomaticKeyEquivalentMirroring") != "NO")
        item.add("NSHiddenInRepresentation", False)
        item.add("NSMenu", menu)
        if elem.get("isSeparatorItem") == "YES":
            item.add("NSIsSeparator", True)
            item.add("NSIsDisabled", True)
        item.add("NSTitle", string(attr(elem, "title", "")))
        key = attr(elem, "keyEquivalent", "")
        item.add("NSKeyEquiv", string(key))
        mods = elem.find("modifierMask[@key='keyEquivalentModifierMask']")
        mask = self.modifiers(mods) if mods is not None else 1 << 20
        if mask:
            item.add("NSKeyEquivModMask", mask)
        item.add("NSMnemonicLoc", 0x7FFFFFFF)
        if elem.get("state") == "on":
            item.add("NSState", 1)
        elif elem.get("state") == "mixed":
            item.add("NSState", -1)
        if elem.get("enabled") == "NO":
            item.add("NSIsDisabled", True)
        if elem.get("hidden") == "YES":
            item.add("NSIsHidden", True)
        if elem.get("alternate") == "YES":
            item.add("NSIsAlternate", True)
        if elem.get("tag"):
            item.add("NSTag", int(elem.get("tag")))
        if elem.get("indentationLevel"):
            item.add("NSIndent", int(elem.get("indentationLevel")))
        item.add("NSOnImage", self.image_resource("NSMenuCheckmark"))
        item.add("NSMixedImage", self.image_resource("NSMenuMixedState"))
        sub = elem.find("menu[@key='submenu']")
        if sub is not None:
            submenu = self.e_menu(sub, item)
            item.add("NSSubmenu", submenu)
            item.add("NSAction", string("submenuAction:"))
            item.add("NSTarget", submenu)
        return self.custom(elem, item, "NSMenuItem")


def compile_xib(path, module=None):
    compiler = Compiler(path, module)
    tree = ET.parse(path)
    root = compiler.compile(tree)
    return N.write(root), compiler


def main(argv):
    out, inputs, fmt_out = None, [], "human-readable-text"
    module = None
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--compile":
            out = argv[i + 1]
            i += 2
            continue
        if a.startswith("--output-format"):
            fmt_out = a.split("=", 1)[1] if "=" in a else argv[i + 1]
            i += 1 if "=" in a else 2
            continue
        if a.startswith("--minimum-deployment-target") or a.startswith("--target-device") \
                or a.startswith("--sdk") or a.startswith("--auto-activate-custom-fonts") is False and a == "--sdk":
            i += 1 if "=" in a else 2
            continue
        if a.startswith("--module"):
            module = a.split("=", 1)[1] if "=" in a else argv[i + 1]
            i += 1 if "=" in a else 2
            continue
        if a in ("--errors", "--warnings", "--notices", "--auto-activate-custom-fonts", "--flatten"):
            i += 1
            continue
        if a.startswith("--"):
            i += 1 if "=" in a else 2
            continue
        inputs.append(a)
        i += 1
    if out is None or len(inputs) != 1:
        sys.stderr.write("usage: ibtool --compile OUT.nib [options] IN.xib\n")
        return 64
    try:
        data, compiler = compile_xib(inputs[0], module)
    except (CompileError, ET.ParseError, OSError) as e:
        sys.stderr.write("%s: error: %s\n" % (inputs[0], e))
        return 1
    for w in compiler.warnings:
        sys.stderr.write("%s: warning: %s\n" % (inputs[0], w))
    with open(out, "wb") as f:
        f.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
