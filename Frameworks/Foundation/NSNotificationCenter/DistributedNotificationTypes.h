/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What distnoted and Foundation's NSDistributedNotificationCenter share.
 */

#ifndef DISTRIBUTED_NOTIFICATION_TYPES_H
#define DISTRIBUTED_NOTIFICATION_TYPES_H

#include <mach/mach.h>

typedef char *dn_string_t;
typedef char *dn_bytes_t;

#define kDNServiceName          "org.minkar.distnoted"

/* A notification's property list. */
#define kDNKeyName              "name"
#define kDNKeyObject            "object"
#define kDNKeyUserInfo          "userInfo"

/* post's options: the reference platform's NSDistributedNotificationOptions. */
enum {
    kDNPostDeliverImmediately = 1 << 0,
    kDNPostToAllSessions = 1 << 1,
};

enum {
    kDNSuccess = 0,
    kDNNotCheckedIn = 1,
    kDNBadNotification = 2,
    kDNTooMany = 3,
};

/* What distnoted sends a process: the notification, out of line. */
#define kDNMessageNotification  0x444e0001
typedef struct {
    mach_msg_header_t header;
    mach_msg_body_t body;
    mach_msg_ool_descriptor_t notification;
    int32_t options;
} DNMessage;

#define kDNMaxNotification      (256 * 1024)
#define kDNMaxObservations      4096

#endif
