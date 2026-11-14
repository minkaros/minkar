/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A stream of an engine: one direction, a group of channels, the formats
 * the hardware takes, the one in use, and the samples' buffer.
 */

#ifndef _IOKIT_IOAUDIOSTREAM_H
#define _IOKIT_IOAUDIOSTREAM_H

#include <IOKit/IOService.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioEngine;
class IOMemoryDescriptor;

#define kIOAudioStreamDirectionKey          "IOAudioStreamDirection"
#define kIOAudioStreamStartingChannelIDKey  "IOAudioStreamStartingChannelID"
#define kIOAudioStreamFormatKey             "IOAudioStreamFormat"
#define kIOAudioStreamAvailableFormatsKey   "IOAudioStreamAvailableFormats"
#define kIOAudioStreamTerminalTypeKey       "IOAudioStreamTerminalType"

class IOAudioStream : public IOService {
    OSDeclareDefaultStructors(IOAudioStream);

    friend class IOAudioEngine;
    friend class IOAudioEngineUserClient;

public:
    struct AvailableFormat {
        IOAudioStreamFormat format;
        IOAudioSampleRate   minimumSampleRate, maximumSampleRate;
    };

protected:
    IOAudioEngine          *audioEngine;
    IOAudioStreamDirection  direction;
    UInt32                  startingChannelID;
    UInt32                  maxNumChannels;
    IOAudioStreamFormat     format;
    AvailableFormat        *availableFormats;
    UInt32                  numAvailableFormats;
    void                   *sampleBuffer;
    UInt32                  sampleBufferSize;
    IOMemoryDescriptor     *sampleBufferDescriptor;
    UInt32                  terminalType;

public:
    virtual bool initWithAudioEngine(IOAudioEngine *engine, IOAudioStreamDirection dir,
        UInt32 startChannelID, const char *streamDescription = NULL,
        OSDictionary *properties = NULL);
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual IOAudioStreamDirection getDirection();
    virtual UInt32 getStartingChannelID();
    virtual UInt32 getMaxNumChannels();

    /* The buffer the hardware reads or writes; a client maps it. A buffer
       from an IOBufferMemoryDescriptor or other descriptor is given as
       one, so it can be mapped. */
    virtual void setSampleBuffer(void *buffer, UInt32 size);
    virtual void setSampleBuffer(IOMemoryDescriptor *descriptor);
    virtual void *getSampleBuffer();
    virtual UInt32 getSampleBufferSize();
    virtual IOMemoryDescriptor *getSampleBufferDescriptor();

    virtual void addAvailableFormat(const IOAudioStreamFormat *format,
        const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate,
        const IOAudioStreamFormatExtension *formatExtension = NULL);
    virtual void clearAvailableFormats();
    virtual bool validateFormat(IOAudioStreamFormat *format, IOAudioStreamFormatExtension *formatExtension = NULL);
    virtual IOReturn setFormat(const IOAudioStreamFormat *streamFormat, bool callDriver = true);
    virtual const IOAudioStreamFormat *getFormat();
    virtual void setTerminalType(const UInt32 terminalType);
    virtual UInt32 getTerminalType() const;

protected:
    virtual void updateFormatProperty();
};

#endif /* _IOKIT_IOAUDIOSTREAM_H */
