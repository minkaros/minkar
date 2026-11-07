# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: SwiftFormat (MANIFEST), its command-line tool and its tests.
# The tests find the project's files from #file: they are compiled by
# their paths in the package, and the files they read are installed.

PKG =		swiftformat
MANIFEST =	${.CURDIR}/MANIFEST
LIBS =		SwiftFormat
PROGS =		CommandLineTool
TESTS =		SwiftFormatTests
DIR_SwiftFormat = Sources
DIR_CommandLineTool = CommandLineTool
DIR_SwiftFormatTests = Tests
DEPS_CommandLineTool = SwiftFormat
DEPS_SwiftFormatTests = SwiftFormat
RELATIVE_SwiftFormatTests = yes
PROGNAME_CommandLineTool = swiftformat
# About 4900 tests: half an hour or more in a VM.
RUN_ENV =	M3_TIMEOUT=3600
# The whole package: the tests read its sources, its configuration
# (.swiftformat) and its git history.
GIT_REPO =	yes
DATA =		.

.include "../swiftpkg.mk"
