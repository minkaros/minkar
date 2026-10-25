/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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

/*
 * acmacosx.h: ACPICA's environment for the Minkar (Darwin) kernel, where
 * IOACPIFamily runs it. ACPICA's acenv.h includes this file on __APPLE__.
 * The OS services layer is ../../../../../ACPIOSLayer.cpp.
 */

#ifndef __ACMACOSX_H__
#define __ACMACOSX_H__

#include <sys/types.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <libkern/libkern.h>

/* XNU's <string.h> defines strcpy() as a 3-argument checking macro. */
#undef strcpy
#undef strcat

#define ACPI_MACHINE_WIDTH		64
#define COMPILER_DEPENDENT_INT64	int64_t
#define COMPILER_DEPENDENT_UINT64	uint64_t
#define ACPI_UINTPTR_T			uintptr_t
#define ACPI_TO_INTEGER(p)		((uintptr_t)(p))
#define ACPI_OFFSET(d, f)		__builtin_offsetof(d, f)

#define ACPI_USE_DO_WHILE_0
#define ACPI_USE_LOCAL_CACHE
#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_NATIVE_MATH64
#define ACPI_USE_SYSTEM_CLIBRARY

/* No debugger or disassembler in the kernel. */
#undef ACPI_DEBUGGER
#undef ACPI_DISASSEMBLER
#define DEBUGGER_THREADING		0

/* The kernel has no <ctype.h>: ASCII versions of what ACPICA uses. */
#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isxdigit(c)	(isdigit(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define islower(c)	((c) >= 'a' && (c) <= 'z')
#define isalpha(c)	(isupper(c) || islower(c))
#define isspace(c)	((c) == ' ' || ((c) >= '\t' && (c) <= '\r'))
#define isprint(c)	((c) >= 0x20 && (c) < 0x7f)
#define toupper(c)	(islower(c) ? (c) - 'a' + 'A' : (c))
#define tolower(c)	(isupper(c) ? (c) - 'A' + 'a' : (c))

/* x86: write back and invalidate the caches before sleep states. */
#define ACPI_FLUSH_CPU_CACHE()		__asm__ __volatile__("wbinvd" ::: "memory")

/* The FACS global lock (ACPI spec 5.2.10.1), in ACPIOSLayer.cpp. */
#ifdef __cplusplus
extern "C" {
#endif
int AcpiOsAcquireGlobalLock(void *FacsGlobalLock);
int AcpiOsReleaseGlobalLock(void *FacsGlobalLock);
#ifdef __cplusplus
}
#endif
#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq)	((Acq) = AcpiOsAcquireGlobalLock(GLptr))
#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Pnd)	((Pnd) = AcpiOsReleaseGlobalLock(GLptr))

#endif /* __ACMACOSX_H__ */
