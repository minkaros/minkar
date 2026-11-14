/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An engine's client (Minkar's audio service): it maps the engine's status
 * and its streams' buffers, starts and stops the engine, and reads its
 * configuration (IOAudioTypes.h: kIOAudioEngineMemory*, kIOAudioEngineMethod*).
 */

#ifndef _IOKIT_IOAUDIOENGINEUSERCLIENT_H
#define _IOKIT_IOAUDIOENGINEUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioEngine;

class IOAudioEngineUserClient : public IOUserClient {
    OSDeclareDefaultStructors(IOAudioEngineUserClient);

protected:
    IOAudioEngine *audioEngine;
    task_t         clientTask;
    bool           running;         /* this client started the engine */

public:
    static IOAudioEngineUserClient *withAudioEngine(IOAudioEngine *engine, task_t task,
        void *securityToken, UInt32 type);
    virtual bool initWithAudioEngine(IOAudioEngine *engine, task_t task,
        void *securityToken, UInt32 type);
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientDied() APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
        IOMemoryDescriptor **memory) APPLE_KEXT_OVERRIDE;
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
        IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;

protected:
    virtual IOReturn startClient();
    virtual IOReturn stopClient();
};

#endif /* _IOKIT_IOAUDIOENGINEUSERCLIENT_H */
