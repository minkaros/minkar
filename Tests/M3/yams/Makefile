# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: Yams (MANIFEST), on the libyaml it carries (CYaml), and its
# tests.

PKG =		yams
MANIFEST =	${.CURDIR}/MANIFEST
CLIBS =		CYaml
LIBS =		Yams
TESTS =		YamsTests
CFLAGS_CYaml =	-DYAML_DECLARE_STATIC
FLAGS_Yams =	-Xcc -DYAML_DECLARE_STATIC
FLAGS_YamsTests = -Xcc -DYAML_DECLARE_STATIC
DEPS_Yams =	CYaml
DEPS_YamsTests = Yams CYaml
RES_YamsTests =	Tests/YamsTests/Fixtures/SourceKitten\#289/debug.yaml
# PerformanceTests reads its fixtures by path from the package's root
# when TEST_WORKSPACE is set (as when another build tool runs it).
DATA =		Tests/YamsTests/Fixtures
RUN_ENV =	TEST_WORKSPACE=1

.include "../swiftpkg.mk"
