/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The request and result format shared by OpenDirectory's client and
 * Minkar's opendirectoryd (schema.c). Minkar's own.
 */

#ifndef OPENDIRECTORY_ODUTILS_H
#define OPENDIRECTORY_ODUTILS_H

#include <CoreFoundation/CoreFoundation.h>
#include <stdarg.h>
/* The sources that use the protocol also name UUIDs (kODNullUUID). */
#include <uuid/uuid.h>
#include <CFOpenDirectory/CFOpenDirectoryConstantsPriv.h>

__BEGIN_DECLS

/* A request: the function's name and its parameters (CF property list
   objects, any of them NULL). */
CFDictionaryRef schema_construct_request(CFStringRef funcname, CFIndex count, ...);
CFDictionaryRef schema_construct_requestv(CFStringRef funcname, CFIndex count, va_list ap);
/* A request's or a result's parts: the function's name and its values,
   as an array whose NULL values schema_get_value_at_index gives back as
   NULL (both copied; NULL when the dictionary is not one). */
void schema_deconstruct_result(CFDictionaryRef dict, CFStringRef *funcname, CFArrayRef *values);
CFTypeRef schema_get_value_at_index(CFArrayRef values, CFIndex index);
/* A result: the function's name and its values (NULL ones allowed). */
CFDictionaryRef schema_construct_result(CFStringRef funcname, CFIndex count, const CFTypeRef *values);

__END_DECLS

#endif
