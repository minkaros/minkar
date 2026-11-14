/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A device's IO procedures, through the audio service (audiod,
 * AudioServer.defs). Starting one opens a session: a shared area and two
 * semaphores; a real-time thread waits for each cycle, calls the procedure
 * with the cycle's output buffer (in the shared area, cleared first) and
 * its times, and tells the service it is done. The service mixes every
 * client's samples in the same cycle.
 */

#include "AudioHardware.h"
#include "AudioDevices.h"
#include "AudioServerTypes.h"
#include "AudioServerUser.h"
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/semaphore.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdlib.h>
#include <string.h>

#define kMaxProcs 16

typedef struct IOProc {
    AudioDeviceID       device;
    AudioDeviceIOProc   proc;
    void               *clientData;
    volatile int        running;
    pthread_t           thread;
    int32_t             session;
    AudioServerArea    *area;
    vm_size_t           areaSize;
    semaphore_t         wake, done;
    mach_port_t         client;     /* its death ends the session */
} IOProc;

static pthread_mutex_t procsLock = PTHREAD_MUTEX_INITIALIZER;
static IOProc procs[kMaxProcs];
static mach_port_t server;

static mach_port_t audioServer(void) {
    if (server == MACH_PORT_NULL &&
        bootstrap_look_up(bootstrap_port, kAudioServerName, &server) != KERN_SUCCESS)
        server = MACH_PORT_NULL;
    return server;
}

/* A procedure's ID is its slot, so one function may serve several
   procedures (each with its own client data). */
static AudioDeviceIOProcID idOf(IOProc *p) {
    return (AudioDeviceIOProcID)(uintptr_t)p;
}

static IOProc *findProc(AudioDeviceID device, AudioDeviceIOProcID id) {
    for (int i = 0; i < kMaxProcs; i++)
        if (procs[i].proc != NULL && procs[i].device == device && idOf(&procs[i]) == id)
            return &procs[i];
    return NULL;
}

OSStatus AudioDeviceCreateIOProcID(AudioObjectID device, AudioDeviceIOProc proc,
                                   void *clientData, AudioDeviceIOProcID *outID) {
    OSStatus status = kAudioHardwareIllegalOperationError;

    if (!_AudioDeviceExists(device))
        return kAudioHardwareBadDeviceError;
    if (proc == NULL || outID == NULL)
        return kAudioHardwareIllegalOperationError;
    pthread_mutex_lock(&procsLock);
    for (int i = 0; i < kMaxProcs; i++)
        if (procs[i].proc == NULL) {
            memset(&procs[i], 0, sizeof(procs[i]));
            procs[i].device = device;
            procs[i].proc = proc;
            procs[i].clientData = clientData;
            *outID = idOf(&procs[i]);
            status = noErr;
            break;
        }
    pthread_mutex_unlock(&procsLock);
    return status;
}

/* The thread runs at the service's period, with a quarter of it to
   compute. */
static void makeRealtime(const AudioServerArea *area) {
    mach_timebase_info_data_t tb;
    struct thread_time_constraint_policy policy;
    double periodNanos = area->framesPerCycle / area->sampleRate * 1e9;

    mach_timebase_info(&tb);
    policy.period = (uint32_t)(periodNanos * tb.denom / tb.numer);
    policy.computation = policy.period / 4;
    policy.constraint = policy.period / 2;
    policy.preemptible = 1;
    thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
                      (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}

static void *ioThread(void *arg) {
    IOProc *p = arg;
    AudioServerArea *area = p->area;
    float *samples = (float *)((char *)area + area->samplesOffset);
    AudioBufferList output;
    AudioTimeStamp now, outputTime;

    makeRealtime(area);
    while (p->running) {
        if (semaphore_wait(p->wake) != KERN_SUCCESS || !p->running)
            break;
        memset(&now, 0, sizeof(now));
        memset(&outputTime, 0, sizeof(outputTime));
        now.mSampleTime = area->nowSampleTime;
        now.mHostTime = area->nowHostTime;
        now.mRateScalar = 1;
        now.mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
        outputTime.mSampleTime = area->outputSampleTime;
        outputTime.mHostTime = area->outputHostTime;
        outputTime.mRateScalar = 1;
        outputTime.mFlags = now.mFlags;

        output.mNumberBuffers = 1;
        output.mBuffers[0].mNumberChannels = area->channels;
        output.mBuffers[0].mDataByteSize = area->framesPerCycle * area->channels * sizeof(float);
        output.mBuffers[0].mData = samples;
        memset(samples, 0, output.mBuffers[0].mDataByteSize);
        p->proc(p->device, &now, NULL, &now, &output, &outputTime, p->clientData);
        semaphore_signal(p->done);
    }
    return NULL;
}

/* A start with no procedure starts nothing here: the device runs while it
   has procedures running. */
OSStatus AudioDeviceStart(AudioObjectID device, AudioDeviceIOProcID id) {
    IOProc *p;
    char uid[512];
    int32_t session = 0, result = 0;
    mach_port_t area = MACH_PORT_NULL, wake = MACH_PORT_NULL, done = MACH_PORT_NULL;
    uint32_t size = 0;
    vm_address_t address = 0;

    if (!_AudioDeviceExists(device))
        return kAudioHardwareBadDeviceError;
    if (id == NULL)
        return noErr;
    pthread_mutex_lock(&procsLock);
    p = findProc(device, id);
    if (p == NULL) {
        pthread_mutex_unlock(&procsLock);
        return kAudioHardwareIllegalOperationError;
    }
    if (p->running) {
        pthread_mutex_unlock(&procsLock);
        return noErr;
    }
    if (audioServer() == MACH_PORT_NULL || !_AudioDeviceGetUIDCString(device, uid, sizeof(uid))) {
        pthread_mutex_unlock(&procsLock);
        return kAudioHardwareNotRunningError;
    }
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p->client);
    if (as_start_io(server, uid, p->client, &session, &area, &size, &wake, &done, &result) != KERN_SUCCESS ||
        result != 0) {
        mach_port_mod_refs(mach_task_self(), p->client, MACH_PORT_RIGHT_RECEIVE, -1);
        p->client = MACH_PORT_NULL;
        pthread_mutex_unlock(&procsLock);
        return result != 0 ? result : kAudioHardwareNotRunningError;
    }
    if (vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE, area, 0, FALSE,
               VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS) {
        as_stop_io(server, session, &result);
        mach_port_deallocate(mach_task_self(), area);
        pthread_mutex_unlock(&procsLock);
        return kAudioHardwareUnspecifiedError;
    }
    mach_port_deallocate(mach_task_self(), area);
    p->session = session;
    p->area = (AudioServerArea *)address;
    p->areaSize = size;
    p->wake = wake;
    p->done = done;
    p->running = 1;
    if (pthread_create(&p->thread, NULL, ioThread, p) != 0) {
        p->running = 0;
        as_stop_io(server, session, &result);
        pthread_mutex_unlock(&procsLock);
        return kAudioHardwareUnspecifiedError;
    }
    pthread_mutex_unlock(&procsLock);
    return noErr;
}

static void stopProc(IOProc *p) {
    int32_t result;

    if (!p->running)
        return;
    p->running = 0;
    /* The service lets the thread go (it signals wake as the session
       ends). */
    as_stop_io(server, p->session, &result);
    semaphore_signal(p->wake);
    pthread_join(p->thread, NULL);
    vm_deallocate(mach_task_self(), (vm_address_t)p->area, p->areaSize);
    mach_port_deallocate(mach_task_self(), p->wake);
    mach_port_deallocate(mach_task_self(), p->done);
    mach_port_mod_refs(mach_task_self(), p->client, MACH_PORT_RIGHT_RECEIVE, -1);
    p->area = NULL;
    p->client = MACH_PORT_NULL;
}

OSStatus AudioDeviceStop(AudioObjectID device, AudioDeviceIOProcID id) {
    IOProc *p;

    if (!_AudioDeviceExists(device))
        return kAudioHardwareBadDeviceError;
    if (id == NULL)
        return noErr;
    pthread_mutex_lock(&procsLock);
    p = findProc(device, id);
    if (p != NULL)
        stopProc(p);
    pthread_mutex_unlock(&procsLock);
    return p != NULL ? noErr : kAudioHardwareIllegalOperationError;
}

OSStatus AudioDeviceDestroyIOProcID(AudioObjectID device, AudioDeviceIOProcID id) {
    IOProc *p;

    pthread_mutex_lock(&procsLock);
    p = findProc(device, id);
    if (p != NULL) {
        stopProc(p);
        memset(p, 0, sizeof(*p));
    }
    pthread_mutex_unlock(&procsLock);
    return p != NULL ? noErr : kAudioHardwareIllegalOperationError;
}

/* The device's time now: its sample time from the latest cycle of a
   running procedure, and the host time. */
OSStatus AudioDeviceGetCurrentTime(AudioObjectID device, AudioTimeStamp *outTime) {
    OSStatus status = kAudioHardwareNotRunningError;

    if (!_AudioDeviceExists(device))
        return kAudioHardwareBadDeviceError;
    if (outTime == NULL)
        return kAudioHardwareIllegalOperationError;
    pthread_mutex_lock(&procsLock);
    for (int i = 0; i < kMaxProcs; i++) {
        IOProc *p = &procs[i];
        if (p->proc != NULL && p->device == device && p->running && p->area != NULL) {
            uint64_t host = mach_absolute_time();
            mach_timebase_info_data_t tb;
            mach_timebase_info(&tb);
            double elapsed = (double)(host - p->area->nowHostTime) * tb.numer / tb.denom / 1e9;
            memset(outTime, 0, sizeof(*outTime));
            outTime->mSampleTime = p->area->nowSampleTime + elapsed * p->area->sampleRate;
            outTime->mHostTime = host;
            outTime->mRateScalar = 1;
            outTime->mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
            status = noErr;
            break;
        }
    }
    pthread_mutex_unlock(&procsLock);
    return status;
}
