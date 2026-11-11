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

"""Host tests for ibtool.py and nibarchive.py: python3 -B tests/test_ibtool.py

The documents are written here; what is checked is the nib's object graph
as AppKit's nib loading reads it."""

import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import nibarchive as N  # noqa: E402
import ibtool  # noqa: E402

DOC = """<?xml version="1.0" encoding="UTF-8"?>
<document type="com.apple.InterfaceBuilder3.Cocoa.XIB" version="3.0" toolsVersion="1" targetRuntime="MacOSX.Cocoa" useAutolayout="YES">
    <objects>
        <customObject id="-2" userLabel="File's Owner" customClass="Controller"/>
        <customObject id="-1" userLabel="First Responder" customClass="FirstResponder"/>
        <customObject id="-3" userLabel="Application" customClass="NSObject"/>
        %s
    </objects>
</document>
"""


def compile_text(objects):
    with tempfile.NamedTemporaryFile("w", suffix=".xib", delete=False) as f:
        f.write(DOC % objects)
        path = f.name
    try:
        data, compiler = ibtool.compile_xib(path)
    finally:
        os.unlink(path)
    return N.read(data), compiler


def table(root):
    d = root.get("IB.objectdata")
    keys = [v for k, v in d.get("NSObjectsKeys").items if k == N.ELEMENT_KEY]
    values = [v for k, v in d.get("NSObjectsValues").items if k == N.ELEMENT_KEY]
    return d, keys, values


def text(obj):
    return obj.get("NS.bytes").decode("utf-8")


class ArchiveTests(unittest.TestCase):
    def test_round_trip(self):
        shared = N.string("shared")
        root = N.Obj("NSObject", [("a", 1), ("b", -70000), ("c", 2.5), ("d", N.Float(0.5)),
                                  ("e", True), ("f", None), ("g", b"\x00\x01"),
                                  ("h", N.array([shared, shared, N.number(3)])), ("i", 1 << 40)])
        back = N.read(N.write(root))
        self.assertEqual([k for k, _ in back.items], list("abcdefghi"))
        self.assertEqual(back.get("a"), 1)
        self.assertEqual(back.get("b"), -70000)
        self.assertEqual(back.get("c"), 2.5)
        self.assertIsInstance(back.get("d"), N.Float)
        self.assertIs(back.get("e"), True)
        self.assertIsNone(back.get("f"))
        self.assertEqual(back.get("g"), b"\x00\x01")
        self.assertEqual(back.get("i"), 1 << 40)
        elements = [v for k, v in back.get("h").items if k == N.ELEMENT_KEY]
        self.assertIs(elements[0], elements[1])      # shared objects stay shared
        self.assertEqual(text(elements[0]), "shared")

    def test_header(self):
        data = N.write(N.Obj("NSObject"))
        self.assertEqual(data[:10], b"NIBArchive")
        self.assertEqual(struct.unpack_from("<II", data, 10), (1, 10))

    def test_dump_is_order_independent(self):
        a = N.Obj("X", [("b", 1), ("a", 2)])
        b = N.Obj("X", [("a", 2), ("b", 1)])
        self.assertEqual(N.dump(a), N.dump(b))


class DocumentTests(unittest.TestCase):
    def test_placeholders_and_top_level(self):
        root, _ = compile_text('<customObject id="9" customClass="Helper"/>')
        d, keys, values = table(root)
        owner = d.get("NSRoot")
        self.assertEqual(text(owner.get("NSClassName")), "Controller")
        names = [text(k.get("NSClassName")) for k in keys]
        self.assertEqual(names, ["NSApplication", "Helper"])   # the unused First Responder is left out
        self.assertTrue(all(v is owner for v in values))
        oids = [v for k, v in d.get("NSOidsKeys").items if k == N.ELEMENT_KEY]
        self.assertIs(oids[0], owner)

    def test_window_and_views(self):
        root, compiler = compile_text("""
        <window title="Main" releasedWhenClosed="NO" visibleAtLaunch="NO" id="1">
            <windowStyleMask key="styleMask" titled="YES" closable="YES" resizable="YES"/>
            <windowPositionMask key="initialPositionMask" leftStrut="YES" bottomStrut="YES"/>
            <rect key="contentRect" x="100" y="200" width="300" height="150"/>
            <rect key="screenRect" x="0.0" y="0.0" width="1440" height="900"/>
            <view key="contentView" id="2">
                <rect key="frame" x="0.0" y="0.0" width="300" height="150"/>
                <subviews>
                    <textField horizontalHuggingPriority="251" id="3">
                        <rect key="frame" x="18" y="100" width="100" height="16"/>
                        <textFieldCell key="cell" lineBreakMode="clipping" selectable="YES" title="Name:" id="4">
                            <font key="font" metaFont="system"/>
                        </textFieldCell>
                    </textField>
                    <button verticalHuggingPriority="750" id="5">
                        <rect key="frame" x="200" y="13" width="80" height="32"/>
                        <autoresizingMask key="autoresizingMask" flexibleMinX="YES"/>
                        <buttonCell key="cell" type="push" title="OK" bezelStyle="rounded" alignment="center" borderStyle="border" inset="2" id="6">
                            <behavior key="behavior" pushIn="YES" lightByBackground="YES" lightByGray="YES"/>
                            <font key="font" metaFont="system"/>
                            <string key="keyEquivalent" base64-UTF8="YES">
DQ
</string>
                        </buttonCell>
                        <connections>
                            <action selector="ok:" target="-2" id="7"/>
                        </connections>
                    </button>
                </subviews>
            </view>
            <connections>
                <outlet property="delegate" destination="-2" id="8"/>
            </connections>
        </window>""")
        self.assertEqual(compiler.warnings, [])
        d, keys, _ = table(root)
        window = keys[1]
        self.assertEqual(window.cls, "NSWindowTemplate")
        self.assertEqual(text(window.get("NSWindowTitle")), "Main")
        self.assertEqual(text(window.get("NSWindowRect")), "{{100, 200}, {300, 150}}")
        self.assertEqual(window.get("NSWindowStyleMask"), 1 | 2 | 8)
        self.assertEqual(window.get("NSWTFlags") & 0x20000000, 0x20000000)
        view = window.get("NSWindowView")
        self.assertEqual(text(view.get("NSFrameSize")), "{300, 150}")
        subviews = [v for k, v in view.get("NSSubviews").items if k == N.ELEMENT_KEY]
        field, button = subviews
        self.assertIs(field.get("NSSuperview"), view)
        self.assertEqual(text(field.get("NSFrame")), "{{18, 100}, {100, 16}}")
        self.assertEqual(field.get("NSvFlags"), 256 | 4 | 8)          # default autoresizing
        self.assertEqual(text(field.get("NSHuggingPriority")), "{251, 750}")
        cell = field.get("NSCell")
        self.assertEqual(text(cell.get("NSContents")), "Name:")
        self.assertIs(cell.get("NSControlView"), field)
        self.assertTrue(cell.get("NSCellFlags") & 0x00200000)         # selectable
        self.assertEqual((cell.get("NSCellFlags2") >> 9) & 7, 2)      # clipping
        self.assertEqual(button.get("NSvFlags"), 256 | 1)
        bcell = button.get("NSCell")
        self.assertEqual(text(bcell.get("NSKeyEquivalent")), "\r")
        self.assertEqual(bcell.get("NSButtonFlags") & 0xFFFFFFFF, 0x86804000)
        self.assertEqual(bcell.get("NSButtonFlags2"), 1)
        self.assertEqual(bcell.get("NSAuxButtonType"), 7)
        connections = [v for k, v in d.get("NSConnections").items if k == N.ELEMENT_KEY]
        kinds = sorted((c.cls, text(c.get("NSLabel"))) for c in connections)
        self.assertEqual(kinds, [("NSNibControlConnector", "ok:"), ("NSNibOutletConnector", "delegate")])
        action = [c for c in connections if c.cls == "NSNibControlConnector"][0]
        self.assertIs(action.get("NSSource"), button)
        self.assertIs(action.get("NSDestination"), d.get("NSRoot"))

    def test_centred_without_position_mask(self):
        root, _ = compile_text("""
        <window id="1">
            <rect key="contentRect" x="5" y="5" width="400" height="300"/>
            <rect key="screenRect" x="0.0" y="0.0" width="1440" height="900"/>
            <view key="contentView" id="2"><rect key="frame" x="0.0" y="0.0" width="400" height="300"/></view>
        </window>""")
        _, keys, _ = table(root)
        self.assertEqual(text(keys[1].get("NSWindowRect")), "{{520, 300}, {400, 300}}")

    def test_menu(self):
        root, _ = compile_text("""
        <menu title="Main Menu" systemMenu="main" id="1">
            <items>
                <menuItem title="File" id="2">
                    <menu key="submenu" title="File" id="3">
                        <items>
                            <menuItem title="Open…" keyEquivalent="o" id="4">
                                <connections><action selector="openDocument:" target="-1" id="5"/></connections>
                            </menuItem>
                            <menuItem isSeparatorItem="YES" id="6"/>
                            <menuItem title="Save As…" keyEquivalent="S" id="7">
                                <modifierMask key="keyEquivalentModifierMask" shift="YES" command="YES"/>
                            </menuItem>
                            <menuItem id="8">
                                <string key="title">\tIndented</string>
                                <modifierMask key="keyEquivalentModifierMask"/>
                            </menuItem>
                        </items>
                    </menu>
                </menuItem>
            </items>
        </menu>""")
        d, keys, _ = table(root)
        menu = keys[1]
        self.assertEqual(text(menu.get("NSName")), "_NSMainMenu")
        file_item = [v for k, v in menu.get("NSMenuItems").items if k == N.ELEMENT_KEY][0]
        submenu = file_item.get("NSSubmenu")
        self.assertEqual(text(file_item.get("NSAction")), "submenuAction:")
        self.assertIs(file_item.get("NSTarget"), submenu)
        items = [v for k, v in submenu.get("NSMenuItems").items if k == N.ELEMENT_KEY]
        open_item, separator, save_as, indented = items
        self.assertEqual(text(open_item.get("NSKeyEquiv")), "o")
        self.assertEqual(open_item.get("NSKeyEquivModMask"), 1 << 20)
        self.assertTrue(separator.get("NSIsSeparator"))
        self.assertEqual(save_as.get("NSKeyEquivModMask"), (1 << 20) | (1 << 17))
        self.assertEqual(text(indented.get("NSTitle")), "\tIndented")
        self.assertIsNone(indented.get("NSKeyEquivModMask"))
        actions = [v for k, v in d.get("NSConnections").items if k == N.ELEMENT_KEY]
        self.assertIsNone(actions[0].get("NSDestination"))           # the First Responder
        self.assertIn(d.get("NSOidsKeys").items[-1][1], actions)       # connectors have oids, last

    def test_table_in_scroll_view(self):
        root, compiler = compile_text("""
        <scrollView autohidesScrollers="YES" horizontalLineScroll="19" horizontalPageScroll="10" verticalLineScroll="19" verticalPageScroll="10" hasHorizontalScroller="NO" id="1">
            <rect key="frame" x="0.0" y="0.0" width="200" height="100"/>
            <clipView key="contentView" id="2">
                <rect key="frame" x="1" y="1" width="198" height="98"/>
                <subviews>
                    <tableView columnAutoresizingStyle="lastColumnOnly" multipleSelection="NO" headerView="9" id="3">
                        <rect key="frame" x="0.0" y="0.0" width="198" height="75"/>
                        <tableColumns>
                            <tableColumn identifier="name" width="100" minWidth="40" maxWidth="1000" id="4">
                                <tableHeaderCell key="headerCell" title="Name"/>
                                <textFieldCell key="dataCell" lineBreakMode="truncatingTail" selectable="YES" editable="YES" id="5"/>
                                <tableColumnResizingMask key="resizingMask" resizeWithTable="YES" userResizable="YES"/>
                            </tableColumn>
                        </tableColumns>
                    </tableView>
                </subviews>
            </clipView>
            <scroller key="horizontalScroller" hidden="YES" horizontal="YES" id="6">
                <rect key="frame" x="-100" y="-100" width="183" height="15"/>
                <autoresizingMask key="autoresizingMask"/>
            </scroller>
            <scroller key="verticalScroller" hidden="YES" horizontal="NO" id="7">
                <rect key="frame" x="184" y="24" width="15" height="75"/>
                <autoresizingMask key="autoresizingMask"/>
            </scroller>
            <tableHeaderView key="headerView" id="9">
                <rect key="frame" x="0.0" y="0.0" width="198" height="23"/>
                <autoresizingMask key="autoresizingMask"/>
            </tableHeaderView>
        </scrollView>""")
        self.assertEqual(compiler.warnings, [])
        _, keys, _ = table(root)
        sv = keys[1]
        clip = sv.get("NSContentView")
        table_view = clip.get("NSDocView")
        self.assertEqual(table_view.cls, "NSTableView")
        self.assertEqual(text(clip.get("NSBounds")), "{{0, -23}, {198, 98}}")
        subviews = [v.cls for k, v in sv.get("NSSubviews").items if k == N.ELEMENT_KEY]
        self.assertEqual(subviews, ["NSClipView", "NSClipView", "NSScroller", "_NSCornerView", "NSScroller"])
        self.assertIs(sv.get("NSHeaderClipView").get("NSDocView"), table_view.get("NSHeaderView"))
        flags = sv.get("NSsFlags")
        self.assertEqual(flags & 0x30, 0x10)       # a vertical scroller only
        self.assertTrue(flags & 0x200)             # autohidden
        self.assertEqual(struct.unpack(">4f", sv.get("NSScrollAmts")), (10.0, 10.0, 19.0, 19.0))
        column = [v for k, v in table_view.get("NSTableColumns").items if k == N.ELEMENT_KEY][0]
        self.assertEqual(text(column.get("NSIdentifier")), "name")
        self.assertEqual(column.get("NSWidth"), 100.0)
        self.assertIs(column.get("NSTableView"), table_view)
        self.assertEqual(table_view.get("NSTvFlags") & 0x08000000, 0)   # single selection

    def test_custom_classes(self):
        root, _ = compile_text("""
        <customView id="1" customClass="ChartView">
            <rect key="frame" x="0.0" y="0.0" width="100" height="100"/>
            <subviews>
                <button id="2" customClass="RoundButton">
                    <rect key="frame" x="0.0" y="0.0" width="10" height="10"/>
                    <buttonCell key="cell" type="push" bezelStyle="rounded" id="3"/>
                </button>
            </subviews>
        </customView>""")
        _, keys, _ = table(root)
        view = keys[1]
        self.assertEqual((view.cls, text(view.get("NSClassName"))), ("NSCustomView", "ChartView"))
        button = [v for k, v in view.get("NSSubviews").items if k == N.ELEMENT_KEY][0]
        self.assertEqual(button.cls, "NSClassSwapper")
        self.assertEqual(text(button.get("NSClassName")), "RoundButton")
        self.assertEqual(text(button.get("NSOriginalClassName")), "NSButton")
        self.assertIs(button.get("NSCell").get("NSControlView"), button)

    def test_bindings_and_constraints(self):
        root, _ = compile_text("""
        <userDefaultsController representsSharedInstance="YES" id="1"/>
        <view id="2">
            <rect key="frame" x="0.0" y="0.0" width="200" height="50"/>
            <subviews>
                <textField translatesAutoresizingMaskIntoConstraints="NO" id="3">
                    <rect key="frame" x="20" y="20" width="100" height="22"/>
                    <textFieldCell key="cell" id="4"/>
                    <connections>
                        <binding destination="1" name="value" keyPath="values.Name" id="5"/>
                    </connections>
                </textField>
            </subviews>
            <constraints>
                <constraint firstItem="3" firstAttribute="leading" secondItem="2" secondAttribute="leading" constant="20" id="6"/>
                <constraint firstItem="3" firstAttribute="width" relation="greaterThanOrEqual" constant="100" priority="750" id="7"/>
            </constraints>
        </view>""")
        d, keys, _ = table(root)
        view = keys[2]
        field = [v for k, v in view.get("NSSubviews").items if k == N.ELEMENT_KEY][0]
        self.assertTrue(field.get("NSDoNotTranslateAutoresizingMask"))
        constraints = [v for k, v in view.get("NSViewConstraints").items if k == N.ELEMENT_KEY]
        leading, width = constraints
        self.assertIs(leading.get("NSFirstItem"), field)
        self.assertIs(leading.get("NSSecondItem"), view)
        self.assertEqual((leading.get("NSFirstAttribute"), leading.get("NSConstant")), (5, 20.0))
        self.assertEqual((width.get("NSRelation"), width.get("NSPriority")), (1, 750))
        self.assertIsNone(width.get("NSSecondItem"))
        binding = [v for k, v in d.get("NSConnections").items if k == N.ELEMENT_KEY][0]
        self.assertEqual(binding.cls, "NSNibBindingConnector")
        self.assertEqual(text(binding.get("NSLabel")), "value: values.Name")
        self.assertIs(binding.get("NSDestination"), keys[1])

    def test_unknown_element_is_reported(self):
        root, compiler = compile_text("""<levelIndicator id="1"><rect key="frame" x="0" y="0" width="1" height="1"/></levelIndicator>""")
        self.assertEqual(len(compiler.warnings), 1)
        self.assertIn("levelIndicator", compiler.warnings[0])
        _, keys, _ = table(root)
        self.assertEqual(keys[1].cls, "NSLevelIndicator")

    def test_command_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            xib, nib = os.path.join(tmp, "a.xib"), os.path.join(tmp, "a.nib")
            with open(xib, "w") as f:
                f.write(DOC % "")
            tool = os.path.join(os.path.dirname(HERE), "ibtool.py")
            r = subprocess.run([sys.executable, "-B", tool, "--compile", nib, "--errors", "--warnings", "--notices",
                                "--output-format", "human-readable-text", "--minimum-deployment-target=10.15", xib],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(nib, "rb") as f:
                self.assertEqual(f.read(10), b"NIBArchive")
            with open(xib, "w") as f:
                f.write("<notadocument/>")
            r = subprocess.run([sys.executable, "-B", tool, "--compile", nib, xib], capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("error", r.stderr)


if __name__ == "__main__":
    unittest.main()
