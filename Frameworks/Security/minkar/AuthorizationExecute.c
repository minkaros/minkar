/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AuthorizationExecuteWithPrivileges: a tool run as root for the holder of
 * an authorization. The administrator's right (system.privilege.admin) is
 * asked of authd first, in the caller's authorization (so the user is asked
 * for an administrator's name and password, as the flags allow); then the
 * setuid trampoline (/usr/libexec/authorization-trampoline) is given the
 * authorization's external form on descriptor 3 (a socket), checks the
 * right again with authd, without asking, answers with the result there,
 * and runs the tool as root. This call returns once that answer has come,
 * so the caller may free its authorization at once. The tool's standard
 * input and output are one socket: the pipe returned.
 *
 * The rest of Authorization's calls are the APSL client
 * (OSX/libsecurity_authorization/lib/Authorization.c), which talks to authd.
 */

#include "Security.h"
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define kTrampoline "/usr/libexec/authorization-trampoline"

OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef authorization, const char *pathToTool, AuthorizationFlags options,
                                            char *const *arguments, FILE **communicationsPipe) {
    AuthorizationItem item = { kAuthorizationRightExecute, strlen(pathToTool ? pathToTool : ""), (void *)pathToTool, 0 };
    AuthorizationRights rights = { 1, &item };
    AuthorizationExternalForm external;
    int sockets[2] = { -1, -1 }, form[2] = { -1, -1 };
    posix_spawn_file_actions_t actions;
    size_t argc = 0;
    OSStatus status;
    pid_t pid;
    int err;

    if (authorization == NULL)
        return errAuthorizationInvalidRef;
    if (pathToTool == NULL)
        return errAuthorizationInvalidPointer;
    status = AuthorizationCopyRights(authorization, &rights, NULL,
                                     kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed |
                                         kAuthorizationFlagExtendRights, NULL);
    if (status != errAuthorizationSuccess)
        return status;
    if ((status = AuthorizationMakeExternalForm(authorization, &external)) != errAuthorizationSuccess)
        return status;

    while (arguments != NULL && arguments[argc] != NULL)
        argc++;
    const char **argv = calloc(argc + 3, sizeof(char *));
    if (argv == NULL)
        return errAuthorizationInternal;
    argv[0] = kTrampoline;
    argv[1] = pathToTool;
    for (size_t i = 0; i < argc; i++)
        argv[i + 2] = arguments[i];
    argv[argc + 2] = NULL;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, form) != 0 ||
        (communicationsPipe != NULL && socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)) {
        free(argv);
        return errAuthorizationToolEnvironmentError;
    }
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, form[0], 3);
    posix_spawn_file_actions_addclose(&actions, form[1]);
    if (communicationsPipe != NULL) {
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, sockets[0]);
    }
    err = posix_spawn(&pid, kTrampoline, &actions, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    free(argv);
    close(form[0]);
    if (communicationsPipe != NULL)
        close(sockets[1]);
    if (err != 0) {
        close(form[1]);
        if (communicationsPipe != NULL)
            close(sockets[0]);
        return errAuthorizationToolExecuteFailure;
    }
    /* The authorization, for the trampoline to check; then its answer. */
    OSStatus checked = errAuthorizationToolEnvironmentError;
    if (write(form[1], &external, sizeof(external)) != (ssize_t)sizeof(external) ||
        read(form[1], &checked, sizeof(checked)) != (ssize_t)sizeof(checked))
        checked = errAuthorizationToolExecuteFailure;
    close(form[1]);
    if (checked != errAuthorizationSuccess) {
        if (communicationsPipe != NULL)
            close(sockets[0]);
        waitpid(pid, NULL, 0);
        return checked;
    }
    if (communicationsPipe != NULL)
        *communicationsPipe = fdopen(sockets[0], "r+");
    return errAuthorizationSuccess;
}
