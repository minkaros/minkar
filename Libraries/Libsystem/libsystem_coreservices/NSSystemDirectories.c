/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "NSSystemDirectories.h"
#include "sysdir.h"

_Static_assert((int)NSPreferencePanesDirectory == (int)SYSDIR_DIRECTORY_PREFERENCE_PANES &&
    (int)NSAllLibrariesDirectory == (int)SYSDIR_DIRECTORY_ALL_LIBRARIES &&
    (int)NSSystemDomainMask == (int)SYSDIR_DOMAIN_MASK_SYSTEM &&
    (int)NSAllDomainsMask == (int)SYSDIR_DOMAIN_MASK_ALL,
    "NSSystemDirectories and sysdir values must match");

NSSearchPathEnumerationState
NSStartSearchPathEnumeration(NSSearchPathDirectory dir, NSSearchPathDomainMask domainMask)
{
	return sysdir_start_search_path_enumeration((sysdir_search_path_directory_t)dir,
	    (sysdir_search_path_domain_mask_t)domainMask);
}

NSSearchPathEnumerationState
NSGetNextSearchPathEnumeration(NSSearchPathEnumerationState state, char *path)
{
	return sysdir_get_next_search_path_enumeration(state, path);
}
