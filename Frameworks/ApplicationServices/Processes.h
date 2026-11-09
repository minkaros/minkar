/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Process serial numbers and the calls on them that are still in use:
 * this process's, a process's by pid and back, its name, and making this
 * process a foreground application, a UI element or a background one.
 */

#ifndef __PROCESSES__
#define __PROCESSES__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ProcessSerialNumber is in MacTypes.h. */

enum {
    kNoProcess = 0,
    kSystemProcess = 1,
    kCurrentProcess = 2
};

typedef UInt32 ProcessApplicationTransformState;
enum {
    kProcessTransformToForegroundApplication = 1,
    kProcessTransformToBackgroundApplication = 2,
    kProcessTransformToUIElementApplication = 4
};

OSErr GetCurrentProcess(ProcessSerialNumber *PSN);
OSStatus GetProcessForPID(pid_t pid, ProcessSerialNumber *psn);
OSStatus GetProcessPID(const ProcessSerialNumber *psn, pid_t *pid);
OSStatus CopyProcessName(const ProcessSerialNumber *psn, CFStringRef *name);
OSErr SameProcess(const ProcessSerialNumber *PSN1, const ProcessSerialNumber *PSN2, Boolean *result);
OSErr GetFrontProcess(ProcessSerialNumber *PSN);
OSErr SetFrontProcess(const ProcessSerialNumber *PSN);
OSErr GetNextProcess(ProcessSerialNumber *PSN);
OSErr KillProcess(const ProcessSerialNumber *inProcess);
OSStatus TransformProcessType(const ProcessSerialNumber *psn, ProcessApplicationTransformState transformState);

#ifdef __cplusplus
}
#endif

#endif
