/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef MDVALUE_H
#define MDVALUE_H

#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

/* A metadata value as mdls shows it: strings quoted, dates in UTC
   ("2026-11-23 15:56:40 +0000"), booleans as 1 and 0, arrays one item a
   line between parentheses, indented by indent; NULL as "(null)". With
   raw, strings are not quoted. */
void md_print_value(FILE *out, CFTypeRef value, int indent, int raw);
/* A CFString as UTF-8 into buffer. */
const char *md_cstring(CFStringRef string, char *buffer, size_t size);

#endif
