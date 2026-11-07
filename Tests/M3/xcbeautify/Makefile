# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: xcbeautify (MANIFEST), on swift-argument-parser, Colorizer and
# XMLCoder at the versions its Package.resolved names, and its tests,
# with the language features its package turns on.

PKG =		xcbeautify
MANIFEST =	${.CURDIR}/MANIFEST
DEPPKGS =	argument-parser colorizer xmlcoder
MANIFEST_argument-parser = ../argument-parser/MANIFEST
MANIFEST_colorizer = MANIFEST.colorizer
MANIFEST_xmlcoder = MANIFEST.xmlcoder

LIBS =		ArgumentParserToolInfo ArgumentParser Colorizer XMLCoder XcbeautifyLib
PROGS =		xcbeautify
TESTS =		XcbeautifyLibTests

DIR_ArgumentParserToolInfo = .deps/argument-parser/Sources/ArgumentParserToolInfo
DIR_ArgumentParser =	.deps/argument-parser/Sources/ArgumentParser
DIR_Colorizer =		.deps/colorizer/Sources/Colorizer
DIR_XMLCoder =		.deps/xmlcoder/Sources/XMLCoder
DEPS_ArgumentParser =	ArgumentParserToolInfo
DEPS_XcbeautifyLib =	Colorizer XMLCoder
DEPS_xcbeautify =	ArgumentParserToolInfo ArgumentParser Colorizer XMLCoder XcbeautifyLib
DEPS_XcbeautifyLibTests = Colorizer XMLCoder XcbeautifyLib

FEATURES =	-enable-experimental-feature StrictConcurrency=targeted
.for f in BareSlashRegexLiterals ConciseMagicFile DeprecateApplicationMain \
    DisableOutwardActorInference DynamicActorIsolation ExistentialAny \
    ForwardTrailingClosures FullTypedThrows GlobalActorIsolatedTypesUsability \
    ImplicitOpenExistentials ImportObjcForwardDeclarations InferSendableFromCaptures \
    InternalImportsByDefault IsolatedDefaultValues RegionBasedIsolation
FEATURES +=	-enable-upcoming-feature ${f}
.endfor
FLAGS_XcbeautifyLib =	${FEATURES}
FLAGS_xcbeautify =	${FEATURES}
FLAGS_XcbeautifyLibTests = ${FEATURES}
# .process("TestData"): its files at the bundle's top.
RES_XcbeautifyLibTests = Tests/XcbeautifyLibTests/TestData/*

.include "../swiftpkg.mk"
