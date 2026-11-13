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

/* What GetProcessInformation fills in (the 64-bit layout, packed to 2). */
#include <CoreServices/Files.h>
#pragma pack(push, 2)
struct ProcessInfoRec {
    UInt32 processInfoLength;
    StringPtr processName;		/* a Pascal string buffer of 32 bytes, or NULL */
    ProcessSerialNumber processNumber;
    UInt32 processType;
    OSType processSignature;
    UInt32 processMode;
    Ptr processLocation;
    UInt32 processSize;
    UInt32 processFreeMem;
    ProcessSerialNumber processLauncher;
    UInt32 processLaunchDate;
    UInt32 processActiveTime;
    FSRef *processAppRef;		/* or NULL */
};
#pragma pack(pop)
typedef struct ProcessInfoRec ProcessInfoRec;
typedef ProcessInfoRec *ProcessInfoRecPtr;

enum {
    modeDeskAccessory = 0x00020000,
    modeMultiLaunch = 0x00010000,
    modeNeedSuspendResume = 0x00004000,
    modeCanBackground = 0x00001000,
    modeDoesActivateOnFGSwitch = 0x00000800,
    modeOnlyBackground = 0x00000400,
    modeGetFrontClicks = 0x00000200,
    modeGetAppDiedMsg = 0x00000100,
    mode32BitCompatible = 0x00000080,
    modeHighLevelEventAware = 0x00000040,
    modeLocalAndRemoteHLEvents = 0x00000020,
    modeStationeryAware = 0x00000010,
    modeUseTextEditServices = 0x00000008,
    modeDisplayManagerAware = 0x00000004
};

OSErr GetProcessInformation(const ProcessSerialNumber *PSN, ProcessInfoRec *info);
CFDictionaryRef ProcessInformationCopyDictionary(const ProcessSerialNumber *PSN, UInt32 infoToReturn);
OSStatus GetProcessBundleLocation(const ProcessSerialNumber *psn, FSRef *location);

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
