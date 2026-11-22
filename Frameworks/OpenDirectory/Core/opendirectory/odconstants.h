/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The protocol between OpenDirectory's client (odxpc.c, transaction.c)
 * and Minkar's opendirectoryd: the service's name and the keys of the XPC
 * messages. A request carries the client's ID, the request type (0), the
 * session's and the node's UUIDs and the request itself, a binary
 * property list (schema.c); each reply carries the client's ID, the
 * result (a binary property list), an error code, and whether it is the
 * request's last reply (a query has one per record). Minkar's own: the
 * published sources do not have these files.
 */

#ifndef OPENDIRECTORY_ODCONSTANTS_H
#define OPENDIRECTORY_ODCONSTANTS_H

/* The replies' error codes are the framework's (kODErrorDaemonError...). */
#include <CFOpenDirectory/CFOpenDirectoryConstants.h>

/* The reference platform's name, so the apps' sandbox profiles, which
   allow looking it up, let them reach the service. */
#define kODAPIPortName		"com.apple.system.opendirectoryd.api"
#define kODAPIPortNameDebug	"com.apple.system.opendirectoryd.api.debug"

#define kODXPCMessageClientID	"clientid"
#define kODXPCMessageReqType	"reqtype"
#define kODXPCMessageSession	"session"
#define kODXPCMessageNode	"node"
#define kODXPCMessageRequest	"request"

#define kODXPCReplyClientID	"clientid"
#define kODXPCReplyData		"data"
#define kODXPCReplyError	"error"
#define kODXPCReplyComplete	"complete"

#endif
