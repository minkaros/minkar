/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A level (a volume): an integer value between a minimum and a maximum,
 * each with its value in decibels (16.16).
 */

#ifndef _IOKIT_IOAUDIOLEVELCONTROL_H
#define _IOKIT_IOAUDIOLEVELCONTROL_H

#include <IOKit/audio/IOAudioControl.h>

#define kIOAudioLevelControlMinValueKey "IOAudioLevelControlMinValue"
#define kIOAudioLevelControlMaxValueKey "IOAudioLevelControlMaxValue"
#define kIOAudioLevelControlMinDBKey    "IOAudioLevelControlMinDB"
#define kIOAudioLevelControlMaxDBKey    "IOAudioLevelControlMaxDB"

class IOAudioLevelControl : public IOAudioControl {
    OSDeclareDefaultStructors(IOAudioLevelControl);

protected:
    SInt32    minValue, maxValue;
    IOFixed16 minDB, maxDB;

public:
    static IOAudioLevelControl *create(SInt32 initialValue, SInt32 minValue, SInt32 maxValue,
        IOFixed16 minDB, IOFixed16 maxDB, UInt32 channelID, const char *channelName = NULL,
        UInt32 cntrlID = 0, UInt32 subType = 0, UInt32 usage = 0);
    static IOAudioLevelControl *createVolumeControl(SInt32 initialValue, SInt32 minValue,
        SInt32 maxValue, IOFixed16 minDB, IOFixed16 maxDB, UInt32 channelID,
        const char *channelName = NULL, UInt32 cntrlID = 0, UInt32 usage = 0);

    virtual bool init(SInt32 initialValue, SInt32 minValue, SInt32 maxValue,
        IOFixed16 minDB, IOFixed16 maxDB, UInt32 channelID, const char *channelName = NULL,
        UInt32 cntrlID = 0, UInt32 subType = 0, UInt32 usage = 0, OSDictionary *properties = NULL);

    virtual SInt32 getMinValue();
    virtual SInt32 getMaxValue();
    virtual IOFixed16 getMinDB();
    virtual IOFixed16 getMaxDB();

protected:
    virtual IOReturn validateValue(OSObject *newValue) APPLE_KEXT_OVERRIDE;
};

#endif /* _IOKIT_IOAUDIOLEVELCONTROL_H */
