/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioEngineUserClient.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <kern/clock.h>

#define super IOService
OSDefineMetaClassAndAbstractStructors(IOAudioEngine, IOService);

bool
IOAudioEngine::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;
    state = kIOAudioEngineStopped;
    outputStreams = OSArray::withCapacity(1);
    inputStreams = OSArray::withCapacity(1);
    defaultAudioControls = OSArray::withCapacity(2);
    /* The status, in a page clients map read-only. */
    statusDescriptor = IOBufferMemoryDescriptor::withOptions(
        kIODirectionInOut | kIOMemoryKernelUserShared, PAGE_SIZE, PAGE_SIZE);
    if (outputStreams == NULL || inputStreams == NULL || defaultAudioControls == NULL ||
        statusDescriptor == NULL)
        return false;
    status = (IOAudioEngineStatus *)statusDescriptor->getBytesNoCopy();
    bzero(status, PAGE_SIZE);
    status->fVersion = kIOAudioEngineCurrentStatusStructVersion;
    updateStateProperty();
    return true;
}

void
IOAudioEngine::free()
{
    OSSafeReleaseNULL(outputStreams);
    OSSafeReleaseNULL(inputStreams);
    OSSafeReleaseNULL(defaultAudioControls);
    OSSafeReleaseNULL(statusDescriptor);
    status = NULL;
    if (commandGate != NULL && workLoop != NULL)
        workLoop->removeEventSource(commandGate);
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(workLoop);
    super::free();
}

bool
IOAudioEngine::start(IOService *provider)
{
    IOAudioDevice *device = OSDynamicCast(IOAudioDevice, provider);

    return start(provider, device);
}

/* The device's work loop shared, then the driver's hardware: its buffer,
   streams and controls. */
bool
IOAudioEngine::start(IOService *provider, IOAudioDevice *device)
{
    if (device == NULL || !super::start(provider))
        return false;
    audioDevice = device;
    workLoop = device->getWorkLoop();
    if (workLoop == NULL)
        return false;
    workLoop->retain();
    commandGate = IOCommandGate::commandGate(this);
    if (commandGate == NULL || workLoop->addEventSource(commandGate) != kIOReturnSuccess)
        return false;
    return initHardware(provider);
}

void
IOAudioEngine::stop(IOService *provider)
{
    if (state == kIOAudioEngineRunning) {
        performAudioEngineStop();
        setState(kIOAudioEngineStopped);
    }
    for (UInt32 i = 0; defaultAudioControls != NULL && i < defaultAudioControls->getCount(); i++) {
        IOAudioControl *control = OSDynamicCast(IOAudioControl, defaultAudioControls->getObject(i));
        if (control != NULL)
            control->terminate();
    }
    super::stop(provider);
}

bool
IOAudioEngine::initHardware(IOService *provider)
{
    return true;
}

IOReturn
IOAudioEngine::performFormatChange(IOAudioStream *audioStream,
    const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    return kIOReturnSuccess;
}

/* ---- Running ----------------------------------------------------------- */

IOReturn
IOAudioEngine::startAudioEngine()
{
    IOReturn result = kIOReturnSuccess;

    if (state == kIOAudioEngineRunning) {
        numActiveUserClients++;
        setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, 32);
        return kIOReturnSuccess;
    }
    /* A fresh count of loops: clients compute from the first stamp. */
    status->fSequence = status->fSequence + 1;
    status->fCurrentLoopCount = 0;
    status->fLastLoopTime = 0;
    status->fEraseHeadSampleFrame = 0;
    status->fSequence = status->fSequence + 1;
    result = performAudioEngineStart();
    if (result == kIOReturnSuccess) {
        numActiveUserClients++;
        setState(kIOAudioEngineRunning);
        setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, 32);
    }
    return result;
}

IOReturn
IOAudioEngine::stopAudioEngine()
{
    IOReturn result = kIOReturnSuccess;

    if (state != kIOAudioEngineRunning)
        return kIOReturnSuccess;
    if (numActiveUserClients > 1) {
        numActiveUserClients--;
        setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, 32);
        return kIOReturnSuccess;
    }
    result = performAudioEngineStop();
    numActiveUserClients = 0;
    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, 32);
    setState(kIOAudioEngineStopped);
    return result;
}

IOAudioEngineState
IOAudioEngine::getState()
{
    return state;
}

void
IOAudioEngine::setState(IOAudioEngineState newState)
{
    state = newState;
    updateStateProperty();
}

void
IOAudioEngine::updateStateProperty()
{
    setProperty(kIOAudioEngineStateKey, (UInt32)state, 32);
}

/* The loop count and the time of the last wrap, written between two
   steps of the sequence number (IOAudioTypes.h), so a client never takes
   the time of one wrap with the count of another. */
void
IOAudioEngine::takeTimeStamp(bool incrementLoopCount, AbsoluteTime *timestamp)
{
    uint64_t now;

    if (status == NULL)
        return;
    if (timestamp != NULL)
        now = *(uint64_t *)timestamp;
    else
        clock_get_uptime(&now);
    status->fSequence = status->fSequence + 1;
    __asm__ volatile("sfence" ::: "memory");
    status->fLastLoopTime = now;
    if (incrementLoopCount)
        status->fCurrentLoopCount = status->fCurrentLoopCount + 1;
    __asm__ volatile("sfence" ::: "memory");
    status->fSequence = status->fSequence + 1;
}

IOReturn
IOAudioEngine::getLoopCountAndTimeStamp(UInt32 *loopCount, AbsoluteTime *timestamp)
{
    UInt32 sequence, count;
    uint64_t time;

    if (status == NULL)
        return kIOReturnNotReady;
    do {
        sequence = status->fSequence;
        __asm__ volatile("lfence" ::: "memory");
        count = status->fCurrentLoopCount;
        time = status->fLastLoopTime;
        __asm__ volatile("lfence" ::: "memory");
    } while ((sequence & 1) != 0 || sequence != status->fSequence);
    if (loopCount != NULL)
        *loopCount = count;
    if (timestamp != NULL)
        *(uint64_t *)timestamp = time;
    return kIOReturnSuccess;
}

const IOAudioEngineStatus *
IOAudioEngine::getStatus()
{
    return status;
}

/* ---- Streams and configuration ----------------------------------------- */

IOReturn
IOAudioEngine::addAudioStream(IOAudioStream *stream)
{
    OSArray *list;

    if (stream == NULL)
        return kIOReturnBadArgument;
    list = stream->getDirection() == kIOAudioStreamDirectionOutput ? outputStreams : inputStreams;
    if (!stream->attach(this))
        return kIOReturnError;
    list->setObject(stream);
    stream->registerService();
    return kIOReturnSuccess;
}

IOAudioStream *
IOAudioEngine::getAudioStream(IOAudioStreamDirection direction, UInt32 channelID)
{
    OSArray *list = direction == kIOAudioStreamDirectionOutput ? outputStreams : inputStreams;

    for (UInt32 i = 0; list != NULL && i < list->getCount(); i++) {
        IOAudioStream *s = OSDynamicCast(IOAudioStream, list->getObject(i));
        UInt32 first = s->getStartingChannelID();

        if (channelID >= first && channelID < first + s->getMaxNumChannels())
            return s;
    }
    return NULL;
}

/* Output streams first, then input streams. */
UInt32
IOAudioEngine::getNumStreams() const
{
    return outputStreams->getCount() + inputStreams->getCount();
}

IOAudioStream *
IOAudioEngine::getStreamAtIndex(UInt32 index) const
{
    if (index < outputStreams->getCount())
        return OSDynamicCast(IOAudioStream, outputStreams->getObject(index));
    index -= outputStreams->getCount();
    if (index < inputStreams->getCount())
        return OSDynamicCast(IOAudioStream, inputStreams->getObject(index));
    return NULL;
}

void
IOAudioEngine::setSampleRate(const IOAudioSampleRate *newSampleRate)
{
    if (newSampleRate == NULL)
        return;
    sampleRate = *newSampleRate;
    setProperty(kIOAudioEngineSampleRateKey, sampleRate.whole, 32);
}

const IOAudioSampleRate *
IOAudioEngine::getSampleRate()
{
    return &sampleRate;
}

void
IOAudioEngine::setNumSampleFramesPerBuffer(UInt32 numSampleFrames)
{
    numSampleFramesPerBuffer = numSampleFrames;
    setProperty(kIOAudioEngineNumSampleFramesPerBufferKey, numSampleFrames, 32);
}

UInt32
IOAudioEngine::getNumSampleFramesPerBuffer()
{
    return numSampleFramesPerBuffer;
}

void
IOAudioEngine::setSampleOffset(UInt32 offset)
{
    sampleOffset = offset;
    setProperty(kIOAudioEngineSampleOffsetKey, offset, 32);
}

void
IOAudioEngine::setSampleLatency(UInt32 numSamples)
{
    setOutputSampleLatency(numSamples);
    setInputSampleLatency(numSamples);
}

void
IOAudioEngine::setOutputSampleLatency(UInt32 numSamples)
{
    outputSampleLatency = numSamples;
    setProperty(kIOAudioEngineOutputSampleLatencyKey, numSamples, 32);
}

void
IOAudioEngine::setInputSampleLatency(UInt32 numSamples)
{
    inputSampleLatency = numSamples;
    setProperty(kIOAudioEngineInputSampleLatencyKey, numSamples, 32);
}

void
IOAudioEngine::setDescription(const char *description)
{
    if (description != NULL)
        setProperty(kIOAudioEngineDescriptionKey, description);
}

IOReturn
IOAudioEngine::addDefaultAudioControl(IOAudioControl *control)
{
    if (control == NULL)
        return kIOReturnBadArgument;
    if (!control->attachAndStart(this))
        return kIOReturnError;
    defaultAudioControls->setObject(control);
    return kIOReturnSuccess;
}

IOReturn
IOAudioEngine::removeDefaultAudioControl(IOAudioControl *control)
{
    int index = defaultAudioControls->getNextIndexOfObject(control, 0);

    if (index < 0)
        return kIOReturnNotFound;
    control->terminate();
    defaultAudioControls->removeObject((unsigned)index);
    return kIOReturnSuccess;
}

/* Minkar's audio service writes and reads the device's own format in the
   buffer itself: there is nothing to convert here. */
IOReturn
IOAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame,
    UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    return kIOReturnUnsupported;
}

IOReturn
IOAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame,
    UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    return kIOReturnUnsupported;
}

IOWorkLoop *
IOAudioEngine::getWorkLoop() const
{
    return workLoop;
}

IOCommandGate *
IOAudioEngine::getCommandGate() const
{
    return commandGate;
}

/* ---- Clients ----------------------------------------------------------- */

/* Administrators only for now (the audio service runs as root). */
IOReturn
IOAudioEngine::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
    IOAudioEngineUserClient *client;

    if (IOUserClient::clientHasPrivilege(securityID, kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
        return kIOReturnNotPrivileged;
    client = IOAudioEngineUserClient::withAudioEngine(this, task, securityID, type);
    if (client == NULL)
        return kIOReturnNoMemory;
    if (!client->attach(this)) {
        client->release();
        return kIOReturnError;
    }
    if (!client->start(this)) {
        client->detach(this);
        client->release();
        return kIOReturnError;
    }
    *handler = client;
    return kIOReturnSuccess;
}

/* A client gone that had the engine running stops its share of it. */
void
IOAudioEngine::clientClosed(bool wasRunning)
{
    if (wasRunning)
        stopAudioEngine();
}
