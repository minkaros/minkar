/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __XPC_ENDPOINT_H__
#define __XPC_ENDPOINT_H__

__BEGIN_DECLS

/* An endpoint for a connection, to send in a message; the receiver makes a
 * connection to it with xpc_connection_create_from_endpoint(). */
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_endpoint_t xpc_endpoint_create(xpc_connection_t connection);

__END_DECLS

#endif /* __XPC_ENDPOINT_H__ */
