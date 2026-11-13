/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The directory service's requests (ds_module.c): Minkar's directoryd
 * answers them under the reference platform's service name, which the
 * apps' sandbox profiles allow. A request names its routine (OD_RPC_NAME)
 * and carries its keys; a reply is a dictionary of the item's fields, or
 * holds a list under OD_RPC_RESULT, or an error under OD_RPC_ERROR.
 */

#ifndef _OPENDIRECTORY_ODIPC_H_
#define _OPENDIRECTORY_ODIPC_H_

#define kODMachLibinfoPortName		"com.apple.system.opendirectoryd.libinfo"
#define kODMachLibinfoPortNameDebug	"com.apple.system.opendirectoryd.libinfo.debug"
#define kODMachMembershipPortName	"com.apple.system.opendirectoryd.membership"
#define kODMachMembershipPortNameDebug	"com.apple.system.opendirectoryd.membership.debug"

#define OD_RPC_NAME	"rpc_name"
#define OD_RPC_VERSION	"rpc_version"
#define OD_RPC_RESULT	"result"
#define OD_RPC_ERROR	"error"

#endif
