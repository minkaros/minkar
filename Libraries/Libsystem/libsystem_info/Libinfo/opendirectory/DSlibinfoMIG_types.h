/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The older, MIG directory service lookup: only its service's name is
 * still used (ds_module.c's _ds_running); requests go by XPC (odipc.h).
 */

#ifndef _DSLIBINFOMIG_TYPES_H_
#define _DSLIBINFOMIG_TYPES_H_

#define kDSStdMachDSLookupPortName "com.apple.system.DirectoryService.libinfo_v1"

#endif
