/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An audio device: the driver of a piece of audio hardware. It sets the
 * hardware up in initHardware(), then activates one engine for each
 * sample buffer the hardware runs.
 */

#ifndef _IOKIT_IOAUDIODEVICE_H
#define _IOKIT_IOAUDIODEVICE_H

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioEngine;
class IOAudioPort;

#define kIOAudioDeviceNameKey           "IOAudioDeviceName"
#define kIOAudioDeviceShortNameKey      "IOAudioDeviceShortName"
#define kIOAudioDeviceManufacturerNameKey "IOAudioDeviceManufacturerName"
#define kIOAudioDeviceTransportTypeKey  "IOAudioDeviceTransportType"

enum {
    kIOAudioDeviceTransportTypeBuiltIn = 'bltn',
    kIOAudioDeviceTransportTypePCI     = 'pci ',
    kIOAudioDeviceTransportTypeUSB     = 'usb ',
    kIOAudioDeviceTransportTypeHdmi    = 'hdmi'
};

class IOAudioDevice : public IOService {
    OSDeclareDefaultStructors(IOAudioDevice);

protected:
    IOWorkLoop    *workLoop;
    IOCommandGate *commandGate;
    OSArray       *audioEngines;
    bool           familyManagePower;

public:
    virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

    /* The hardware set up; engines are activated here. */
    virtual bool initHardware(IOService *provider);

    virtual void setDeviceName(const char *deviceName);
    virtual void setDeviceShortName(const char *shortName);
    virtual void setManufacturerName(const char *manufacturerName);
    virtual void setDeviceTransportType(const UInt32 transportType);

    /* An engine attached to the device and started. */
    virtual IOReturn activateAudioEngine(IOAudioEngine *audioEngine);
    virtual IOReturn activateAudioEngine(IOAudioEngine *audioEngine, bool shouldStartAudioEngine);
    virtual void deactivateAllAudioEngines();

    virtual IOWorkLoop *getWorkLoop() const APPLE_KEXT_OVERRIDE;
    virtual IOCommandGate *getCommandGate() const;

    /* Power: the family leaves it to the driver here. */
    virtual void setFamilyManagePower(bool manage);
};

#endif /* _IOKIT_IOAUDIODEVICE_H */
