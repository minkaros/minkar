/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
/*
 * OpenPAM configuration for Minkar (Darwin), written by hand in place of
 * the one configure generates (see README.minkar).
 */

#define HAVE_ASPRINTF		1
#define HAVE_DLFCN_H		1
#define HAVE_FPURGE		1
#define HAVE_INTTYPES_H		1
#define HAVE_SETLOGMASK		1
#define HAVE_STDINT_H		1
#define HAVE_STDIO_H		1
#define HAVE_STDLIB_H		1
#define HAVE_STRINGS_H		1
#define HAVE_STRING_H		1
#define HAVE_STRLCAT		1
#define HAVE_STRLCPY		1
#define HAVE_SYS_STAT_H		1
#define HAVE_SYS_TYPES_H	1
#define HAVE_UNISTD_H		1
#define HAVE_VASPRINTF		1
#define HAVE_WCHAR_H		1
#define STDC_HEADERS		1

/* Modules are /usr/lib/pam/pam_<name>.so.2, as on macOS. */
#define LIB_MAJ			2
#define OPENPAM_MODULES_DIRECTORY "/usr/lib/pam"

#define PACKAGE			"openpam"
#define PACKAGE_NAME		"OpenPAM"
#define PACKAGE_TARNAME		"openpam"
#define PACKAGE_VERSION		"20230627"
#define PACKAGE_STRING		"OpenPAM 20230627"
#define PACKAGE_BUGREPORT	"des@des.dev"
#define PACKAGE_URL		"https://openpam.org/"
#define VERSION			"20230627"
