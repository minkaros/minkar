/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include <IOKit/audio/IOAudioEngineUserClient.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(IOAudioEngineUserClient, IOUserClient);

IOAudioEngineUserClient *
IOAudioEngineUserClient::withAudioEngine(IOAudioEngine *engine, task_t task,
    void *securityToken, UInt32 type)
{
    IOAudioEngineUserClient *client = new IOAudioEngineUserClient;

    if (client != NULL && !client->initWithAudioEngine(engine, task, securityToken, type)) {
        client->release();
        client = NULL;
    }
    return client;
}

bool
IOAudioEngineUserClient::initWithAudioEngine(IOAudioEngine *engine, task_t task,
    void *securityToken, UInt32 type)
{
    if (engine == NULL || !super::initWithTask(task, securityToken, type))
        return false;
    audioEngine = engine;
    audioEngine->retain();
    clientTask = task;
    return true;
}

void
IOAudioEngineUserClient::free()
{
    OSSafeReleaseNULL(audioEngine);
    super::free();
}

IOReturn
IOAudioEngineUserClient::startClient()
{
    IOReturn result;

    if (running)
        return kIOReturnSuccess;
    result = audioEngine->startAudioEngine();
    if (result == kIOReturnSuccess)
        running = true;
    return result;
}

IOReturn
IOAudioEngineUserClient::stopClient()
{
    if (!running)
        return kIOReturnSuccess;
    running = false;
    return audioEngine->stopAudioEngine();
}

/* A client that goes away gives up its share of the running engine. */
IOReturn
IOAudioEngineUserClient::clientClose()
{
    if (audioEngine != NULL)
        audioEngine->clientClosed(running);
    running = false;
    terminate();
    return kIOReturnSuccess;
}

IOReturn
IOAudioEngineUserClient::clientDied()
{
    return clientClose();
}

/* The status page (read-only) and each stream's sample buffer. */
IOReturn
IOAudioEngineUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
    IOMemoryDescriptor **memory)
{
    IOMemoryDescriptor *descriptor = NULL;

    if (type == kIOAudioEngineMemoryStatus) {
        descriptor = audioEngine->statusDescriptor;
        *options = kIOMapReadOnly;
    } else if (type >= kIOAudioEngineMemoryStream) {
        IOAudioStream *stream = audioEngine->getStreamAtIndex(type - kIOAudioEngineMemoryStream);

        if (stream != NULL) {
            descriptor = stream->getSampleBufferDescriptor();
            if (stream->getDirection() == kIOAudioStreamDirectionInput)
                *options = kIOMapReadOnly;
        }
    }
    if (descriptor == NULL)
        return kIOReturnBadArgument;
    descriptor->retain();
    *memory = descriptor;
    return kIOReturnSuccess;
}

IOReturn
IOAudioEngineUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
    switch (selector) {
    case kIOAudioEngineMethodStart:
        return startClient();
    case kIOAudioEngineMethodStop:
        return stopClient();
    case kIOAudioEngineMethodGetInfo: {
        IOAudioEngineInfo info;

        if (args->structureOutputSize < sizeof(info))
            return kIOReturnBadArgument;
        bzero(&info, sizeof(info));
        info.fState = audioEngine->getState();
        info.fNumStreams = audioEngine->getNumStreams();
        info.fNumSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
        info.fSampleRateWhole = audioEngine->getSampleRate()->whole;
        info.fSampleOffset = audioEngine->sampleOffset;
        info.fOutputLatency = audioEngine->outputSampleLatency;
        info.fInputLatency = audioEngine->inputSampleLatency;
        bcopy(&info, args->structureOutput, sizeof(info));
        args->structureOutputSize = sizeof(info);
        return kIOReturnSuccess;
    }
    case kIOAudioEngineMethodGetStream: {
        IOAudioEngineStreamInfo info;
        IOAudioStream *stream;

        if (args->scalarInputCount < 1 || args->structureOutputSize < sizeof(info))
            return kIOReturnBadArgument;
        stream = audioEngine->getStreamAtIndex((UInt32)args->scalarInput[0]);
        if (stream == NULL)
            return kIOReturnBadArgument;
        bzero(&info, sizeof(info));
        info.fDirection = stream->getDirection();
        info.fStartingChannelID = stream->getStartingChannelID();
        info.fBufferSize = stream->getSampleBufferSize();
        info.fTerminalType = stream->getTerminalType();
        info.fFormat = *stream->getFormat();
        bcopy(&info, args->structureOutput, sizeof(info));
        args->structureOutputSize = sizeof(info);
        return kIOReturnSuccess;
    }
    default:
        return kIOReturnUnsupported;
    }
}
