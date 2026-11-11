#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""Print a binary nib's objects (nibdump FILE.nib)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import nibarchive  # noqa: E402

for path in sys.argv[1:]:
    if os.path.isdir(path):
        names = sorted(n for n in os.listdir(path) if n.startswith("keyedobjects") and n.endswith(".nib"))
        path = os.path.join(path, names[-1])
    with open(path, "rb") as f:
        sys.stdout.write(nibarchive.dump(nibarchive.read(f.read())))
