/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Authorization: rights granted to a process. There is no authentication
 * dialog yet: rights are granted to administrators (root, or members of
 * the admin or wheel group) and denied to others, without interaction.
 */

#ifndef _SECURITY_AUTHORIZATION_H_
#define _SECURITY_AUTHORIZATION_H_

#include "SecBase.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

enum {
    errAuthorizationSuccess = 0,
    errAuthorizationInvalidSet = -60001,
    errAuthorizationInvalidRef = -60002,
    errAuthorizationInvalidTag = -60003,
    errAuthorizationInvalidPointer = -60004,
    errAuthorizationDenied = -60005,
    errAuthorizationCanceled = -60006,
    errAuthorizationInteractionNotAllowed = -60007,
    errAuthorizationInternal = -60008,
    errAuthorizationExternalizeNotAllowed = -60009,
    errAuthorizationInternalizeNotAllowed = -60010,
    errAuthorizationInvalidFlags = -60011,
    errAuthorizationToolExecuteFailure = -60031,
    errAuthorizationToolEnvironmentError = -60032,
    errAuthorizationBadAddress = -60033
};

typedef UInt32 AuthorizationFlags;
enum {
    kAuthorizationFlagDefaults = 0,
    kAuthorizationFlagInteractionAllowed = (1 << 0),
    kAuthorizationFlagExtendRights = (1 << 1),
    kAuthorizationFlagPartialRights = (1 << 2),
    kAuthorizationFlagDestroyRights = (1 << 3),
    kAuthorizationFlagPreAuthorize = (1 << 4),
    kAuthorizationFlagNoData = (1 << 20)
};

#define kAuthorizationEmptyEnvironment NULL
#define kAuthorizationRightExecute "system.privilege.admin"
#define kAuthorizationEnvironmentUsername "username"
#define kAuthorizationEnvironmentPassword "password"
#define kAuthorizationEnvironmentPrompt "prompt"
#define kAuthorizationEnvironmentIcon "icon"
#define kAuthorizationExternalFormLength 32

typedef const struct AuthorizationOpaqueRef *AuthorizationRef;
typedef const char *AuthorizationString;

typedef struct AuthorizationItem {
    AuthorizationString name;
    size_t valueLength;
    void *value;
    UInt32 flags;
} AuthorizationItem;

typedef struct AuthorizationItemSet {
    UInt32 count;
    AuthorizationItem *items;
} AuthorizationItemSet;

typedef struct AuthorizationExternalForm {
    char bytes[kAuthorizationExternalFormLength];
} AuthorizationExternalForm;

typedef AuthorizationItemSet AuthorizationRights;
typedef AuthorizationItemSet AuthorizationEnvironment;

OSStatus AuthorizationCreate(const AuthorizationRights *rights, const AuthorizationEnvironment *environment,
                             AuthorizationFlags flags, AuthorizationRef *authorization);
OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags);
OSStatus AuthorizationCopyRights(AuthorizationRef authorization, const AuthorizationRights *rights,
                                 const AuthorizationEnvironment *environment, AuthorizationFlags flags,
                                 AuthorizationRights **authorizedRights);
OSStatus AuthorizationCopyInfo(AuthorizationRef authorization, AuthorizationString tag, AuthorizationItemSet **info);
OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization, AuthorizationExternalForm *extForm);
OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm, AuthorizationRef *authorization);
OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set);
/* Runs the tool as root: directly when this process is root, else through
   sudo without a password prompt (an administrator allowed to). */
OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef authorization, const char *pathToTool, AuthorizationFlags options,
                                            char *const *arguments, FILE **communicationsPipe);


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
