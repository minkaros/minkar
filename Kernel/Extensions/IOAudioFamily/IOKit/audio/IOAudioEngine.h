/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An audio engine: one sample buffer the hardware runs through in a loop,
 * with its streams (one per direction and channel group). The driver says
 * where the hardware is (getCurrentSampleFrame) and stamps the time of
 * every wrap (takeTimeStamp); clients map the buffer and the status and
 * read and write in place, ahead of the hardware. The family does not mix
 * or convert samples: Minkar's audio service writes the device's format.
 */

#ifndef _IOKIT_IOAUDIOENGINE_H
#define _IOKIT_IOAUDIOENGINE_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioDevice;
class IOAudioStream;
class IOAudioControl;
class IOCommandGate;
class IOWorkLoop;

#define kIOAudioEngineStateKey              "IOAudioEngineState"
#define kIOAudioEngineSampleRateKey         "IOAudioEngineSampleRate"
#define kIOAudioEngineNumSampleFramesPerBufferKey "IOAudioEngineNumSampleFramesPerBuffer"
#define kIOAudioEngineDescriptionKey        "IOAudioEngineDescription"
#define kIOAudioEngineGlobalUniqueIDKey     "IOAudioEngineGlobalUniqueID"
#define kIOAudioEngineSampleOffsetKey       "IOAudioEngineSampleOffset"
#define kIOAudioEngineOutputSampleLatencyKey "IOAudioEngineOutputSampleLatency"
#define kIOAudioEngineInputSampleLatencyKey "IOAudioEngineInputSampleLatency"
#define kIOAudioEngineNumActiveUserClientsKey "IOAudioEngineNumActiveUserClients"

class IOAudioEngine : public IOService {
    OSDeclareAbstractStructors(IOAudioEngine);

    friend class IOAudioEngineUserClient;

protected:
    IOAudioDevice           *audioDevice;
    IOWorkLoop              *workLoop;
    IOCommandGate           *commandGate;
    IOAudioEngineState       state;
    IOAudioSampleRate        sampleRate;
    UInt32                   numSampleFramesPerBuffer;
    UInt32                   sampleOffset;
    UInt32                   inputSampleLatency, outputSampleLatency;
    OSArray                 *outputStreams;
    OSArray                 *inputStreams;
    OSArray                 *defaultAudioControls;
    IOAudioEngineStatus     *status;
    IOBufferMemoryDescriptor *statusDescriptor;
    UInt32                   numActiveUserClients;

public:
    virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider, IOAudioDevice *device);
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

    /* The driver's: the hardware's buffer, streams and controls. */
    virtual bool initHardware(IOService *provider);
    virtual IOReturn performAudioEngineStart() = 0;
    virtual IOReturn performAudioEngineStop() = 0;
    virtual UInt32 getCurrentSampleFrame() = 0;
    virtual IOReturn performFormatChange(IOAudioStream *audioStream,
        const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);

    /* Start and stop for clients: they nest, the hardware runs while
       anyone wants it. */
    virtual IOReturn startAudioEngine();
    virtual IOReturn stopAudioEngine();
    /* Not const: IOService::getState() const is another method. */
    virtual IOAudioEngineState getState();

    virtual IOReturn addAudioStream(IOAudioStream *stream);
    virtual IOAudioStream *getAudioStream(IOAudioStreamDirection direction, UInt32 channelID);
    virtual UInt32 getNumStreams() const;
    virtual IOAudioStream *getStreamAtIndex(UInt32 index) const;

    virtual void setSampleRate(const IOAudioSampleRate *newSampleRate);
    virtual const IOAudioSampleRate *getSampleRate();
    virtual void setNumSampleFramesPerBuffer(UInt32 numSampleFrames);
    virtual UInt32 getNumSampleFramesPerBuffer();
    virtual void setSampleOffset(UInt32 sampleOffset);
    virtual void setSampleLatency(UInt32 numSamples);
    virtual void setOutputSampleLatency(UInt32 numSamples);
    virtual void setInputSampleLatency(UInt32 numSamples);
    virtual void setDescription(const char *description);

    /* A wrap of the buffer: the loop count goes up and the time is noted
       (now, or the time the driver read from the hardware). */
    virtual void takeTimeStamp(bool incrementLoopCount = true, AbsoluteTime *timestamp = NULL);
    virtual IOReturn getLoopCountAndTimeStamp(UInt32 *loopCount, AbsoluteTime *timestamp);
    virtual const IOAudioEngineStatus *getStatus();

    virtual IOReturn addDefaultAudioControl(IOAudioControl *defaultAudioControl);
    virtual IOReturn removeDefaultAudioControl(IOAudioControl *defaultAudioControl);

    /* Sample conversion, which the family leaves to its clients here. */
    virtual IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf,
        UInt32 firstSampleFrame, UInt32 numSampleFrames,
        const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn convertInputSamples(const void *sampleBuf, void *destBuf,
        UInt32 firstSampleFrame, UInt32 numSampleFrames,
        const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

    virtual IOWorkLoop *getWorkLoop() const APPLE_KEXT_OVERRIDE;
    virtual IOCommandGate *getCommandGate() const;

    virtual IOReturn newUserClient(task_t task, void *securityID, UInt32 type,
        IOUserClient **handler) APPLE_KEXT_OVERRIDE;
    virtual void clientClosed(bool wasRunning);

protected:
    virtual void setState(IOAudioEngineState newState);
    virtual void updateStateProperty();
};

#endif /* _IOKIT_IOAUDIOENGINE_H */
