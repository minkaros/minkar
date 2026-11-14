/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * MinkarHDAEngine: one output stream of an HD Audio controller feeding one
 * output's DAC. Its sample buffer is the stream's own DMA buffer (48 kHz,
 * 16-bit stereo, 4096 frames), described to the controller as two halves;
 * the second half's descriptor interrupts, so every interrupt is a wrap
 * and stamps the engine's time. The hardware's position is the stream's
 * link position (LPIB). Volume and mute are the DAC's output amplifier.
 */

#include "MinkarHDA.h"
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/IOLib.h>

#define super IOAudioEngine
#define LOG(fmt, ...) IOLog("MinkarHDA: " fmt "\n", ##__VA_ARGS__)

OSDefineMetaClassAndStructors(MinkarHDAEngine, IOAudioEngine);

static const UInt32 kFrames = 4096;
static const UInt32 kRate = 48000;

bool
MinkarHDAEngine::initWithController(MinkarHDA *controller, int output, unsigned stream)
{
    if (controller == NULL || !super::init(NULL))
        return false;
    fController = controller;
    fOutput = output;
    fStream = stream;
    fBytesPerFrame = 4;
    fFormat = HDA_FORMAT_BITS_16 | HDA_FORMAT_CHANNELS(2);     /* 48 kHz base */
    return true;
}

void
MinkarHDAEngine::free()
{
    OSSafeReleaseNULL(fBuffer);
    OSSafeReleaseNULL(fBDL);
    super::free();
}

/* Physically contiguous, below 4 GB, shared with user space. */
static IOBufferMemoryDescriptor *
allocDMA(IOByteCount size)
{
    IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
        kIOMemoryPhysicallyContiguous | kIODirectionInOut | kIOMemoryKernelUserShared,
        round_page(size), 0x00000000FFFFF000ULL);

    if (mem != NULL)
        bzero(mem->getBytesNoCopy(), round_page(size));
    return mem;
}

bool
MinkarHDAEngine::initHardware(IOService *provider)
{
    IOAudioSampleRate rate = { kRate, 0 };
    IOAudioStreamFormat format;
    IOAudioStream *stream;
    const HDAOutput *out = &fController->fOutputs[fOutput];

    if (!super::initHardware(provider))
        return false;
    fBuffer = allocDMA(kFrames * fBytesPerFrame);
    fBDL = allocDMA(PAGE_SIZE);
    if (fBuffer == NULL || fBDL == NULL)
        return false;

    /* Two halves; the second one interrupts (a wrap). */
    HDABufferDescriptor *bdl = (HDABufferDescriptor *)fBDL->getBytesNoCopy();
    uint64_t phys = fBuffer->getPhysicalAddress();
    uint32_t half = kFrames * fBytesPerFrame / 2;
    bdl[0].address = phys;
    bdl[0].length = half;
    bdl[0].flags = 0;
    bdl[1].address = phys + half;
    bdl[1].length = half;
    bdl[1].flags = 1;

    bzero(&format, sizeof(format));
    format.fNumChannels = 2;
    format.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
    format.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
    format.fBitDepth = 16;
    format.fBitWidth = 16;
    format.fAlignment = kIOAudioStreamAlignmentLowByte;
    format.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
    format.fIsMixable = 1;

    stream = new IOAudioStream;
    if (stream == NULL || !stream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput, 1, "Output")) {
        OSSafeReleaseNULL(stream);
        return false;
    }
    stream->addAvailableFormat(&format, &rate, &rate);
    stream->setFormat(&format, false);
    stream->setSampleBuffer(fBuffer);
    stream->setTerminalType(out->device == HDA_DEVICE_SPEAKER ? OUTPUT_SPEAKER :
                            out->device == HDA_DEVICE_HP_OUT ? OUTPUT_HEADPHONES :
                            EXTERNAL_LINE_CONNECTOR);
    addAudioStream(stream);
    stream->release();

    setDescription("Output");
    setSampleRate(&rate);
    setNumSampleFramesPerBuffer(kFrames);
    /* The controller fetches ahead of LPIB (up to its FIFO); stay a
       little further ahead than that. */
    setSampleOffset(64);
    setOutputSampleLatency(32);
    createControls();
    return true;
}

/* ---- Running ----------------------------------------------------------- */

IOReturn
MinkarHDAEngine::performAudioEngineStart()
{
    MinkarHDA *c = fController;
    const HDAOutput *out = &c->fOutputs[fOutput];
    uint32_t sd = c->outputStream(fStream);
    unsigned index = c->outputStreamIndex(fStream);
    uint8_t tag = (uint8_t)(fStream + 1);
    uint64_t bdl = fBDL->getPhysicalAddress();

    /* Through reset, then the buffer, its length, the last valid
       descriptor, the format and the stream's tag. */
    c->write8(sd + HDA_SD_CTL, HDA_SDCTL_SRST);
    for (int i = 0; i < 1000 && !(c->read8(sd + HDA_SD_CTL) & HDA_SDCTL_SRST); i++)
        IODelay(10);
    c->write8(sd + HDA_SD_CTL, 0);
    for (int i = 0; i < 1000 && (c->read8(sd + HDA_SD_CTL) & HDA_SDCTL_SRST); i++)
        IODelay(10);
    c->write32(sd + HDA_SD_BDPL, (uint32_t)bdl);
    c->write32(sd + HDA_SD_BDPU, (uint32_t)(bdl >> 32));
    c->write32(sd + HDA_SD_CBL, kFrames * fBytesPerFrame);
    c->write16(sd + HDA_SD_LVI, 1);
    c->write16(sd + HDA_SD_FMT, fFormat);
    c->write8(sd + HDA_SD_CTL + 2, (uint8_t)(tag << 4));
    c->write8(sd + HDA_SD_STS, HDA_SDSTS_BCIS | HDA_SDSTS_FIFOE | HDA_SDSTS_DESE);

    /* The DAC takes the stream's tag, channels from 0, and its format. */
    c->verb(out->cad, out->dac, HDA_VERB_SET_CONVERTER_STREAM, (uint32_t)(tag << 4));
    c->verb4(out->cad, out->dac, HDA_VERB_SET_CONVERTER_FORMAT, fFormat);

    c->write32(HDA_INTCTL, c->read32(HDA_INTCTL) | HDA_INTCTL_GIE | (1U << index));
    takeTimeStamp(false);
    c->write8(sd + HDA_SD_CTL, HDA_SDCTL_RUN | HDA_SDCTL_IOCE);
    return kIOReturnSuccess;
}

IOReturn
MinkarHDAEngine::performAudioEngineStop()
{
    MinkarHDA *c = fController;
    const HDAOutput *out = &c->fOutputs[fOutput];
    uint32_t sd = c->outputStream(fStream);
    unsigned index = c->outputStreamIndex(fStream);

    c->write8(sd + HDA_SD_CTL, 0);
    for (int i = 0; i < 1000 && (c->read8(sd + HDA_SD_CTL) & HDA_SDCTL_RUN); i++)
        IODelay(10);
    c->write32(HDA_INTCTL, c->read32(HDA_INTCTL) & ~(1U << index));
    c->write8(sd + HDA_SD_STS, HDA_SDSTS_BCIS | HDA_SDSTS_FIFOE | HDA_SDSTS_DESE);
    c->verb(out->cad, out->dac, HDA_VERB_SET_CONVERTER_STREAM, 0);
    return kIOReturnSuccess;
}

UInt32
MinkarHDAEngine::getCurrentSampleFrame()
{
    uint32_t position = fController->read32(fController->outputStream(fStream) + HDA_SD_LPIB);

    return (position / fBytesPerFrame) % kFrames;
}

IOReturn
MinkarHDAEngine::performFormatChange(IOAudioStream *audioStream,
    const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    /* One format for now: 48 kHz, 16-bit stereo. */
    if (newSampleRate != NULL && newSampleRate->whole != kRate)
        return kIOReturnUnsupported;
    return kIOReturnSuccess;
}

void
MinkarHDAEngine::bufferCompleted()
{
    takeTimeStamp();
}

/* ---- Controls ---------------------------------------------------------- */

/* The DAC's output amplifier: a volume of its steps (0 dB at its offset)
   and a mute when it has one. */
bool
MinkarHDAEngine::createControls()
{
    const HDAOutput *out = &fController->fOutputs[fOutput];
    HDAWidget *dac = fController->widgetAt(out->cad, out->dac);
    uint32_t caps = dac != NULL ? dac->outAmpCaps : 0;
    uint32_t steps = HDA_AMPCAP_STEPS(caps), offset = HDA_AMPCAP_OFFSET(caps);
    /* A step's size in 16.16 dB: (n + 1) quarter decibels. */
    IOFixed16 step = (IOFixed16)((HDA_AMPCAP_STEP_SIZE(caps) + 1) << 14);

    fVolume = (SInt32)offset;
    if (dac == NULL || !(dac->caps & HDA_WCAP_OUT_AMP))
        return false;
    if (steps > 0) {
        IOAudioLevelControl *volume = IOAudioLevelControl::createVolumeControl((SInt32)offset,
            0, (SInt32)steps, -(IOFixed16)offset * step, (IOFixed16)(steps - offset) * step,
            kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0,
            kIOAudioControlUsageOutput);
        if (volume != NULL) {
            volume->setValueChangeHandler(volumeChanged, this);
            addDefaultAudioControl(volume);
            volume->release();
        }
    }
    if (caps & HDA_AMPCAP_MUTE) {
        IOAudioToggleControl *mute = IOAudioToggleControl::createMuteControl(false,
            kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0,
            kIOAudioControlUsageOutput);
        if (mute != NULL) {
            mute->setValueChangeHandler(muteChanged, this);
            addDefaultAudioControl(mute);
            mute->release();
        }
    }
    return true;
}

void
MinkarHDAEngine::applyOutputAmp()
{
    const HDAOutput *out = &fController->fOutputs[fOutput];

    fController->verb4(out->cad, out->dac, HDA_VERB_SET_AMP, HDA_AMP_SET_OUTPUT |
        HDA_AMP_SET_LEFT | HDA_AMP_SET_RIGHT | (fMuted ? HDA_AMP_MUTE : 0) | (uint32_t)fVolume);
}

IOReturn
MinkarHDAEngine::volumeChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
    MinkarHDAEngine *self = OSDynamicCast(MinkarHDAEngine, target);

    if (self == NULL)
        return kIOReturnBadArgument;
    self->fVolume = newValue;
    self->applyOutputAmp();
    return kIOReturnSuccess;
}

IOReturn
MinkarHDAEngine::muteChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
    MinkarHDAEngine *self = OSDynamicCast(MinkarHDAEngine, target);

    if (self == NULL)
        return kIOReturnBadArgument;
    self->fMuted = newValue != 0;
    self->applyOutputAmp();
    return kIOReturnSuccess;
}
