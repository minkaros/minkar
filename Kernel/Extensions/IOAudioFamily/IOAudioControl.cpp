/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Controls: the base, levels (volume) and toggles (mute).
 */

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(IOAudioControl, IOService);

bool
IOAudioControl::init(UInt32 controlType, OSObject *initialValue, UInt32 channel,
    const char *channelName, UInt32 cntrlID, UInt32 controlSubType, UInt32 controlUsage,
    OSDictionary *properties)
{
    if (initialValue == NULL || !super::init(properties))
        return false;
    type = controlType;
    subType = controlSubType;
    usage = controlUsage;
    channelID = channel;
    controlID = cntrlID;
    value = initialValue;
    value->retain();
    setProperty(kIOAudioControlTypeKey, type, 32);
    setProperty(kIOAudioControlSubTypeKey, subType, 32);
    setProperty(kIOAudioControlUsageKey, usage, 32);
    setProperty(kIOAudioControlChannelIDKey, channelID, 32);
    setProperty(kIOAudioControlIDKey, controlID, 32);
    if (channelName != NULL)
        setProperty(kIOAudioControlChannelNameKey, channelName);
    setProperty(kIOAudioControlValueKey, value);
    return true;
}

void
IOAudioControl::free()
{
    OSSafeReleaseNULL(value);
    OSSafeReleaseNULL(valueChangeTarget);
    super::free();
}

bool
IOAudioControl::attachAndStart(IOService *provider)
{
    if (!attach(provider))
        return false;
    if (!start(provider)) {
        detach(provider);
        return false;
    }
    isStarted = true;
    registerService();
    return true;
}

void
IOAudioControl::setValueChangeHandler(IntValueChangeHandler handler, OSObject *target)
{
    intValueChangeHandler = handler;
    if (target != NULL)
        target->retain();
    OSSafeReleaseNULL(valueChangeTarget);
    valueChangeTarget = target;
}

IOReturn
IOAudioControl::validateValue(OSObject *newValue)
{
    return OSDynamicCast(OSNumber, newValue) != NULL ? kIOReturnSuccess : kIOReturnBadArgument;
}

/* The driver's handler applies the change to the hardware. */
IOReturn
IOAudioControl::performValueChange(OSObject *newValue)
{
    OSNumber *oldNumber = OSDynamicCast(OSNumber, value);
    OSNumber *newNumber = OSDynamicCast(OSNumber, newValue);

    if (intValueChangeHandler == NULL || newNumber == NULL)
        return kIOReturnSuccess;
    return intValueChangeHandler(valueChangeTarget, this,
        oldNumber != NULL ? (SInt32)oldNumber->unsigned32BitValue() : 0,
        (SInt32)newNumber->unsigned32BitValue());
}

IOReturn
IOAudioControl::setValue(OSObject *newValue)
{
    IOReturn result;

    if (newValue == NULL)
        return kIOReturnBadArgument;
    result = validateValue(newValue);
    if (result != kIOReturnSuccess)
        return result;
    result = performValueChange(newValue);
    if (result == kIOReturnSuccess)
        hardwareValueChanged(newValue);
    return result;
}

IOReturn
IOAudioControl::setValue(SInt32 intValue)
{
    OSNumber *n = OSNumber::withNumber((unsigned long long)(UInt32)intValue, 32);
    IOReturn result;

    if (n == NULL)
        return kIOReturnNoMemory;
    result = setValue(n);
    n->release();
    return result;
}

/* The hardware changed it itself (a knob): noted, not applied again. */
IOReturn
IOAudioControl::hardwareValueChanged(OSObject *newValue)
{
    if (newValue == NULL)
        return kIOReturnBadArgument;
    newValue->retain();
    OSSafeReleaseNULL(value);
    value = newValue;
    setProperty(kIOAudioControlValueKey, value);
    return kIOReturnSuccess;
}

OSObject *
IOAudioControl::getValue()
{
    return value;
}

SInt32
IOAudioControl::getIntValue()
{
    OSNumber *n = OSDynamicCast(OSNumber, value);

    return n != NULL ? (SInt32)n->unsigned32BitValue() : 0;
}

IOReturn
IOAudioControl::flushValue()
{
    return performValueChange(value);
}

UInt32 IOAudioControl::getType()      { return type; }
UInt32 IOAudioControl::getSubType()   { return subType; }
UInt32 IOAudioControl::getUsage()     { return usage; }
UInt32 IOAudioControl::getChannelID() { return channelID; }
UInt32 IOAudioControl::getControlID() { return controlID; }

/* A client sets the value through the registry (IOAudioControlValue). */
IOReturn
IOAudioControl::setProperties(OSObject *properties)
{
    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    OSObject *newValue;

    if (dict == NULL)
        return kIOReturnBadArgument;
    newValue = dict->getObject(kIOAudioControlValueKey);
    if (newValue == NULL)
        return kIOReturnUnsupported;
    return setValue(newValue);
}

/* ---- Levels ------------------------------------------------------------ */

#undef super
#define super IOAudioControl
OSDefineMetaClassAndStructors(IOAudioLevelControl, IOAudioControl);

IOAudioLevelControl *
IOAudioLevelControl::create(SInt32 initialValue, SInt32 min, SInt32 max, IOFixed16 minDecibels,
    IOFixed16 maxDecibels, UInt32 channel, const char *channelName, UInt32 cntrlID,
    UInt32 controlSubType, UInt32 controlUsage)
{
    IOAudioLevelControl *control = new IOAudioLevelControl;

    if (control != NULL && !control->init(initialValue, min, max, minDecibels, maxDecibels,
                                          channel, channelName, cntrlID, controlSubType, controlUsage)) {
        control->release();
        control = NULL;
    }
    return control;
}

IOAudioLevelControl *
IOAudioLevelControl::createVolumeControl(SInt32 initialValue, SInt32 min, SInt32 max,
    IOFixed16 minDecibels, IOFixed16 maxDecibels, UInt32 channel, const char *channelName,
    UInt32 cntrlID, UInt32 controlUsage)
{
    return create(initialValue, min, max, minDecibels, maxDecibels, channel, channelName,
                  cntrlID, kIOAudioLevelControlSubTypeVolume, controlUsage);
}

bool
IOAudioLevelControl::init(SInt32 initialValue, SInt32 min, SInt32 max, IOFixed16 minDecibels,
    IOFixed16 maxDecibels, UInt32 channel, const char *channelName, UInt32 cntrlID,
    UInt32 controlSubType, UInt32 controlUsage, OSDictionary *properties)
{
    OSNumber *n;
    bool ok;

    if (min > max)
        return false;
    n = OSNumber::withNumber((unsigned long long)(UInt32)initialValue, 32);
    if (n == NULL)
        return false;
    ok = super::init(kIOAudioControlTypeLevel, n, channel, channelName, cntrlID,
                     controlSubType, controlUsage, properties);
    n->release();
    if (!ok)
        return false;
    minValue = min;
    maxValue = max;
    minDB = minDecibels;
    maxDB = maxDecibels;
    setProperty(kIOAudioLevelControlMinValueKey, (UInt32)min, 32);
    setProperty(kIOAudioLevelControlMaxValueKey, (UInt32)max, 32);
    setProperty(kIOAudioLevelControlMinDBKey, (UInt32)minDecibels, 32);
    setProperty(kIOAudioLevelControlMaxDBKey, (UInt32)maxDecibels, 32);
    return true;
}

SInt32    IOAudioLevelControl::getMinValue() { return minValue; }
SInt32    IOAudioLevelControl::getMaxValue() { return maxValue; }
IOFixed16 IOAudioLevelControl::getMinDB()    { return minDB; }
IOFixed16 IOAudioLevelControl::getMaxDB()    { return maxDB; }

IOReturn
IOAudioLevelControl::validateValue(OSObject *newValue)
{
    OSNumber *n = OSDynamicCast(OSNumber, newValue);
    SInt32 v;

    if (n == NULL)
        return kIOReturnBadArgument;
    v = (SInt32)n->unsigned32BitValue();
    return v >= minValue && v <= maxValue ? kIOReturnSuccess : kIOReturnBadArgument;
}

/* ---- Toggles ----------------------------------------------------------- */

OSDefineMetaClassAndStructors(IOAudioToggleControl, IOAudioControl);

IOAudioToggleControl *
IOAudioToggleControl::create(bool initialValue, UInt32 channel, const char *channelName,
    UInt32 cntrlID, UInt32 controlSubType, UInt32 controlUsage)
{
    IOAudioToggleControl *control = new IOAudioToggleControl;

    if (control != NULL && !control->init(initialValue, channel, channelName, cntrlID,
                                          controlSubType, controlUsage)) {
        control->release();
        control = NULL;
    }
    return control;
}

IOAudioToggleControl *
IOAudioToggleControl::createMuteControl(bool initialValue, UInt32 channel,
    const char *channelName, UInt32 cntrlID, UInt32 controlUsage)
{
    return create(initialValue, channel, channelName, cntrlID,
                  kIOAudioToggleControlSubTypeMute, controlUsage);
}

bool
IOAudioToggleControl::init(bool initialValue, UInt32 channel, const char *channelName,
    UInt32 cntrlID, UInt32 controlSubType, UInt32 controlUsage, OSDictionary *properties)
{
    OSNumber *n = OSNumber::withNumber(initialValue ? 1ULL : 0ULL, 8);
    bool ok;

    if (n == NULL)
        return false;
    ok = super::init(kIOAudioControlTypeToggle, n, channel, channelName, cntrlID,
                     controlSubType, controlUsage, properties);
    n->release();
    return ok;
}

IOReturn
IOAudioToggleControl::validateValue(OSObject *newValue)
{
    OSNumber *n = OSDynamicCast(OSNumber, newValue);

    if (n == NULL)
        return kIOReturnBadArgument;
    return n->unsigned32BitValue() <= 1 ? kIOReturnSuccess : kIOReturnBadArgument;
}
