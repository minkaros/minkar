/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __XPC_DEBUG_H__
#define __XPC_DEBUG_H__

__BEGIN_DECLS

/* For a debugger: why XPC stopped the process, or NULL. */
XPC_DEBUGGER_EXCL
const char *xpc_debugger_api_misuse_info(void);

__END_DECLS

#endif /* __XPC_DEBUG_H__ */
