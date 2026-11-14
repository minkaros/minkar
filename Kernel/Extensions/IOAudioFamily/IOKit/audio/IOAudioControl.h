/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A control of an audio device (a volume, a mute, a source): a value the
 * user changes and the driver applies through its value change handler.
 */

#ifndef _IOKIT_IOAUDIOCONTROL_H
#define _IOKIT_IOAUDIOCONTROL_H

#include <IOKit/IOService.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioEngine;
class IOCommandGate;

#define kIOAudioControlTypeKey          "IOAudioControlType"
#define kIOAudioControlSubTypeKey       "IOAudioControlSubType"
#define kIOAudioControlUsageKey         "IOAudioControlUsage"
#define kIOAudioControlChannelIDKey     "IOAudioControlChannelID"
#define kIOAudioControlChannelNameKey   "IOAudioControlChannelName"
#define kIOAudioControlValueKey         "IOAudioControlValue"
#define kIOAudioControlIDKey            "IOAudioControlID"

class IOAudioControl : public IOService {
    OSDeclareDefaultStructors(IOAudioControl);

public:
    typedef IOReturn (*IntValueChangeHandler)(OSObject *target, IOAudioControl *audioControl,
        SInt32 oldValue, SInt32 newValue);

protected:
    UInt32                 type, subType, usage, channelID, controlID;
    OSObject              *value;
    IntValueChangeHandler  intValueChangeHandler;
    OSObject              *valueChangeTarget;
    IOCommandGate         *commandGate;
    bool                   isStarted;

public:
    virtual bool init(UInt32 type, OSObject *initialValue, UInt32 channelID,
        const char *channelName = NULL, UInt32 cntrlID = 0, UInt32 subType = 0,
        UInt32 usage = 0, OSDictionary *properties = NULL);
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual bool attachAndStart(IOService *provider);

    virtual void setValueChangeHandler(IntValueChangeHandler intValueChangeHandler, OSObject *target);

    /* A new value: the driver's handler applies it to the hardware. */
    virtual IOReturn setValue(OSObject *newValue);
    virtual IOReturn setValue(SInt32 intValue);
    virtual IOReturn hardwareValueChanged(OSObject *newValue);
    virtual OSObject *getValue();
    virtual SInt32 getIntValue();
    virtual IOReturn flushValue();

    virtual UInt32 getType();
    virtual UInt32 getSubType();
    virtual UInt32 getUsage();
    virtual UInt32 getChannelID();
    virtual UInt32 getControlID();

    virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;

protected:
    virtual IOReturn validateValue(OSObject *newValue);
    virtual IOReturn performValueChange(OSObject *newValue);
};

#endif /* _IOKIT_IOAUDIOCONTROL_H */
