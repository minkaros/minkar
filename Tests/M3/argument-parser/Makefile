# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: swift-argument-parser (MANIFEST) and its tests, as its
# Package.swift lays them out for the reference platform.

PKG =		argument-parser
MANIFEST =	${.CURDIR}/MANIFEST
LIBS =		ArgumentParserToolInfo ArgumentParser ArgumentParserTestHelpers
PROGS =		roll math repeat count-lines generate-manual changelog-authors
TESTS =		ArgumentParserEndToEndTests ArgumentParserExampleTests \
		ArgumentParserGenerateManualTests ArgumentParserPackageManagerTests \
		ArgumentParserUnitTests

DEPS_ArgumentParser =		ArgumentParserToolInfo
DEPS_ArgumentParserTestHelpers = ArgumentParserToolInfo ArgumentParser
.for p in roll math repeat count-lines changelog-authors
DIR_${p} =	Examples/${p}
DEPS_${p} =	ArgumentParserToolInfo ArgumentParser
.endfor
DIR_generate-manual =		Tools/generate-manual
DIR_changelog-authors =		Tools/changelog-authors
DEPS_generate-manual =		ArgumentParserToolInfo ArgumentParser
.for t in ${TESTS}
DEPS_${t} =	ArgumentParserToolInfo ArgumentParser ArgumentParserTestHelpers
.endfor
RES_ArgumentParserExampleTests = Tests/ArgumentParserExampleTests/CountLinesTest.txt

.include "../swiftpkg.mk"
