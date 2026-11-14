/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#define super IOService
OSDefineMetaClassAndStructors(IOAudioStream, IOService);

bool
IOAudioStream::initWithAudioEngine(IOAudioEngine *engine, IOAudioStreamDirection dir,
    UInt32 startChannelID, const char *streamDescription, OSDictionary *properties)
{
    if (engine == NULL || !super::init(properties))
        return false;
    audioEngine = engine;
    direction = dir;
    startingChannelID = startChannelID;
    setProperty(kIOAudioStreamDirectionKey, (UInt32)dir, 32);
    setProperty(kIOAudioStreamStartingChannelIDKey, startChannelID, 32);
    if (streamDescription != NULL)
        setProperty("IOAudioStreamDescription", streamDescription);
    return true;
}

void
IOAudioStream::free()
{
    if (availableFormats != NULL)
        IOFree(availableFormats, numAvailableFormats * sizeof(AvailableFormat));
    OSSafeReleaseNULL(sampleBufferDescriptor);
    super::free();
}

IOAudioStreamDirection
IOAudioStream::getDirection()
{
    return direction;
}

UInt32
IOAudioStream::getStartingChannelID()
{
    return startingChannelID;
}

UInt32
IOAudioStream::getMaxNumChannels()
{
    return maxNumChannels;
}

/* A buffer in the kernel's memory: a descriptor of its pages, so a client
   can map it. */
void
IOAudioStream::setSampleBuffer(void *buffer, UInt32 size)
{
    OSSafeReleaseNULL(sampleBufferDescriptor);
    sampleBuffer = buffer;
    sampleBufferSize = size;
    if (buffer != NULL && size > 0)
        sampleBufferDescriptor = IOMemoryDescriptor::withAddressRange(
            (mach_vm_address_t)buffer, size, kIODirectionInOut, kernel_task);
}

void
IOAudioStream::setSampleBuffer(IOMemoryDescriptor *descriptor)
{
    IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, descriptor);

    if (descriptor != NULL)
        descriptor->retain();
    OSSafeReleaseNULL(sampleBufferDescriptor);
    sampleBufferDescriptor = descriptor;
    sampleBufferSize = descriptor != NULL ? (UInt32)descriptor->getLength() : 0;
    sampleBuffer = bmd != NULL ? bmd->getBytesNoCopy() : NULL;
}

void *
IOAudioStream::getSampleBuffer()
{
    return sampleBuffer;
}

UInt32
IOAudioStream::getSampleBufferSize()
{
    return sampleBufferSize;
}

IOMemoryDescriptor *
IOAudioStream::getSampleBufferDescriptor()
{
    return sampleBufferDescriptor;
}

void
IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *newFormat,
    const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate,
    const IOAudioStreamFormatExtension *formatExtension)
{
    AvailableFormat *grown;

    if (newFormat == NULL || minRate == NULL || maxRate == NULL)
        return;
    grown = (AvailableFormat *)IOMalloc((numAvailableFormats + 1) * sizeof(AvailableFormat));
    if (grown == NULL)
        return;
    if (availableFormats != NULL) {
        bcopy(availableFormats, grown, numAvailableFormats * sizeof(AvailableFormat));
        IOFree(availableFormats, numAvailableFormats * sizeof(AvailableFormat));
    }
    grown[numAvailableFormats].format = *newFormat;
    grown[numAvailableFormats].minimumSampleRate = *minRate;
    grown[numAvailableFormats].maximumSampleRate = *maxRate;
    availableFormats = grown;
    numAvailableFormats++;
    if (newFormat->fNumChannels > maxNumChannels)
        maxNumChannels = newFormat->fNumChannels;

    OSArray *list = OSArray::withCapacity(numAvailableFormats);
    for (UInt32 i = 0; list != NULL && i < numAvailableFormats; i++) {
        OSDictionary *d = OSDictionary::withCapacity(6);
        if (d == NULL)
            continue;
        OSNumber *n;
#define PUT(key, v) if ((n = OSNumber::withNumber((unsigned long long)(v), 32)) != NULL) { d->setObject(key, n); n->release(); }
        PUT("IOAudioStreamNumChannels", availableFormats[i].format.fNumChannels);
        PUT("IOAudioStreamSampleFormat", availableFormats[i].format.fSampleFormat);
        PUT("IOAudioStreamBitDepth", availableFormats[i].format.fBitDepth);
        PUT("IOAudioStreamBitWidth", availableFormats[i].format.fBitWidth);
        PUT("IOAudioStreamMinimumSampleRate", availableFormats[i].minimumSampleRate.whole);
        PUT("IOAudioStreamMaximumSampleRate", availableFormats[i].maximumSampleRate.whole);
#undef PUT
        list->setObject(d);
        d->release();
    }
    if (list != NULL) {
        setProperty(kIOAudioStreamAvailableFormatsKey, list);
        list->release();
    }
}

void
IOAudioStream::clearAvailableFormats()
{
    if (availableFormats != NULL)
        IOFree(availableFormats, numAvailableFormats * sizeof(AvailableFormat));
    availableFormats = NULL;
    numAvailableFormats = 0;
    removeProperty(kIOAudioStreamAvailableFormatsKey);
}

/* A format the hardware takes: the one listed with the same channels,
   kind, depth and width. */
bool
IOAudioStream::validateFormat(IOAudioStreamFormat *wanted, IOAudioStreamFormatExtension *formatExtension)
{
    for (UInt32 i = 0; i < numAvailableFormats; i++) {
        const IOAudioStreamFormat *f = &availableFormats[i].format;

        if (f->fNumChannels == wanted->fNumChannels && f->fSampleFormat == wanted->fSampleFormat &&
            f->fBitDepth == wanted->fBitDepth && f->fBitWidth == wanted->fBitWidth) {
            wanted->fDriverTag = f->fDriverTag;
            return true;
        }
    }
    return numAvailableFormats == 0;
}

IOReturn
IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, bool callDriver)
{
    IOAudioStreamFormat wanted;
    IOReturn result = kIOReturnSuccess;

    if (streamFormat == NULL)
        return kIOReturnBadArgument;
    wanted = *streamFormat;
    if (!validateFormat(&wanted))
        return kIOReturnUnsupported;
    if (callDriver && audioEngine != NULL)
        result = audioEngine->performFormatChange(this, &wanted, NULL);
    if (result == kIOReturnSuccess) {
        format = wanted;
        updateFormatProperty();
    }
    return result;
}

const IOAudioStreamFormat *
IOAudioStream::getFormat()
{
    return &format;
}

void
IOAudioStream::setTerminalType(const UInt32 type)
{
    terminalType = type;
    setProperty(kIOAudioStreamTerminalTypeKey, type, 32);
}

UInt32
IOAudioStream::getTerminalType() const
{
    return terminalType;
}

void
IOAudioStream::updateFormatProperty()
{
    OSDictionary *d = OSDictionary::withCapacity(6);
    OSNumber *n;

    if (d == NULL)
        return;
#define PUT(key, v) if ((n = OSNumber::withNumber((unsigned long long)(v), 32)) != NULL) { d->setObject(key, n); n->release(); }
    PUT("IOAudioStreamNumChannels", format.fNumChannels);
    PUT("IOAudioStreamSampleFormat", format.fSampleFormat);
    PUT("IOAudioStreamNumericRepresentation", format.fNumericRepresentation);
    PUT("IOAudioStreamBitDepth", format.fBitDepth);
    PUT("IOAudioStreamBitWidth", format.fBitWidth);
    PUT("IOAudioStreamByteOrder", format.fByteOrder);
#undef PUT
    setProperty(kIOAudioStreamFormatKey, d);
    d->release();
}
