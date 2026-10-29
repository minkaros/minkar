/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * config.h for lsof's darwin dialect on Minkar: what lsof's configure
 * finds on macOS, written by hand (lsof is built with bmake, not autotools).
 */

#ifndef MINKAR_LSOF_CONFIG_H
#define MINKAR_LSOF_CONFIG_H

#define PACKAGE "lsof"
#define PACKAGE_NAME "lsof"
#define PACKAGE_VERSION "4.99.7"
#define PACKAGE_STRING "lsof 4.99.7"
#define VERSION "4.99.7"

#define API_EXPORT __attribute__((visibility("default")))

#define HAVE_RPC_RPC_H 1
#define HAVE_DECL___GLIBC__ 0
#define HAVE_DECL_AF_INET6 1
#define HAVE_NETINET6_IN6_H 1
#define HAVE_NETINET_IP6_H 1
#define HAVE_DECL_TCP_ESTABLISHED 0
#define HAVE_DECL_TTYAUX_MAJOR 0
#define HAVE_DECL_SS_CONNECTED 0
#define HAVE_STRFTIME 1
#define HAVE_UTMPX_H 1
#define HAVE_WCTYPE_H 1
#define HAVE_SYS_PIPE_H 1
#define HAVE_NFS_NFSPROTO_H 1
#define HAVE_STRUCT_STATVFS 1
#define HAVE_DECL_DUP2 1
#define HAVE_DECL_CLOSEFROM 0

#endif
