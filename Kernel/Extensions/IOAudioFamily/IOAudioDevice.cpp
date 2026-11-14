/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(IOAudioDevice, IOService);

bool
IOAudioDevice::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;
    audioEngines = OSArray::withCapacity(1);
    familyManagePower = false;
    return audioEngines != NULL;
}

void
IOAudioDevice::free()
{
    OSSafeReleaseNULL(audioEngines);
    if (commandGate != NULL && workLoop != NULL)
        workLoop->removeEventSource(commandGate);
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(workLoop);
    super::free();
}

/* A work loop and its command gate for the driver and its engines, then
   the driver's hardware set up (initHardware activates the engines). */
bool
IOAudioDevice::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    workLoop = IOWorkLoop::workLoop();
    if (workLoop == NULL)
        return false;
    commandGate = IOCommandGate::commandGate(this);
    if (commandGate == NULL || workLoop->addEventSource(commandGate) != kIOReturnSuccess)
        return false;
    if (!initHardware(provider)) {
        deactivateAllAudioEngines();
        return false;
    }
    registerService();
    return true;
}

void
IOAudioDevice::stop(IOService *provider)
{
    deactivateAllAudioEngines();
    super::stop(provider);
}

bool
IOAudioDevice::initHardware(IOService *provider)
{
    return true;
}

void
IOAudioDevice::setDeviceName(const char *deviceName)
{
    if (deviceName != NULL)
        setProperty(kIOAudioDeviceNameKey, deviceName);
}

void
IOAudioDevice::setDeviceShortName(const char *shortName)
{
    if (shortName != NULL)
        setProperty(kIOAudioDeviceShortNameKey, shortName);
}

void
IOAudioDevice::setManufacturerName(const char *manufacturerName)
{
    if (manufacturerName != NULL)
        setProperty(kIOAudioDeviceManufacturerNameKey, manufacturerName);
}

void
IOAudioDevice::setDeviceTransportType(const UInt32 transportType)
{
    setProperty(kIOAudioDeviceTransportTypeKey, transportType, 32);
}

IOReturn
IOAudioDevice::activateAudioEngine(IOAudioEngine *audioEngine)
{
    return activateAudioEngine(audioEngine, true);
}

/* The engine becomes the device's child; started, it sets its hardware up
   and is published for clients. */
IOReturn
IOAudioDevice::activateAudioEngine(IOAudioEngine *audioEngine, bool shouldStartAudioEngine)
{
    if (audioEngine == NULL || audioEngines == NULL)
        return kIOReturnBadArgument;
    if (!audioEngine->attach(this))
        return kIOReturnError;
    if (shouldStartAudioEngine && !audioEngine->start(this, this)) {
        audioEngine->detach(this);
        return kIOReturnError;
    }
    audioEngines->setObject(audioEngine);
    audioEngine->registerService();
    return kIOReturnSuccess;
}

void
IOAudioDevice::deactivateAllAudioEngines()
{
    if (audioEngines == NULL)
        return;
    while (audioEngines->getCount() > 0) {
        IOAudioEngine *engine = OSDynamicCast(IOAudioEngine, audioEngines->getObject(0));

        if (engine != NULL) {
            engine->retain();
            audioEngines->removeObject(0);
            engine->stopAudioEngine();
            engine->terminate();
            engine->release();
        } else
            audioEngines->removeObject(0);
    }
}

IOWorkLoop *
IOAudioDevice::getWorkLoop() const
{
    return workLoop;
}

IOCommandGate *
IOAudioDevice::getCommandGate() const
{
    return commandGate;
}

void
IOAudioDevice::setFamilyManagePower(bool manage)
{
    familyManagePower = manage;
}
