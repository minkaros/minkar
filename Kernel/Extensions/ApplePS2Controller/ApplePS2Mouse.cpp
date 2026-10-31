/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include "ApplePS2Mouse.h"

#define super IOHIPointing
OSDefineMetaClassAndStructors(ApplePS2Mouse, IOHIPointing);

/* First packet byte. */
enum {
    kPacketLeft     = 0x01,
    kPacketRight    = 0x02,
    kPacketMiddle   = 0x04,
    kPacketSync     = 0x08,     /* always set: finds packet boundaries */
    kPacketXSign    = 0x10,
    kPacketYSign    = 0x20,
    kPacketXOver    = 0x40,
    kPacketYOver    = 0x80,
};

bool ApplePS2Mouse::init(OSDictionary * dict)
{
    if (!super::init(dict))
        return false;
    _device = NULL;
    _interruptInstalled = false;
    _wheel = false;
    _packetLength = 0;
    _events = 0;
    return true;
}

/*
 * Sends each byte to the mouse (each one is acknowledged) and, if asked,
 * reads one byte of reply. Returns the number of bytes acknowledged.
 */
UInt8 ApplePS2Mouse::sendCommands(const UInt8 * bytes, int count, bool readReply, UInt8 * reply)
{
    PS2Request * request = _device->allocateRequest();
    UInt8 done;
    int i;

    if (request == NULL)
        return 0;
    for (i = 0; i < count && i < kMaxCommands - 1; i++) {
        request->commands[i].command = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[i].inOrOut = bytes[i];
    }
    if (readReply) {
        request->commands[i].command = kPS2C_ReadDataPort;
        request->commands[i].inOrOut = 0;
        i++;
    }
    request->commandsCount = i;
    _device->submitRequestAndBlock(request);
    done = request->commandsCount;
    if (readReply && reply != NULL && done == i)
        *reply = request->commands[i - 1].inOrOut;
    _device->freeRequest(request);
    return done;
}

/* The controller's mouse interrupt and clock (command byte), then the
 * mouse's own data reporting. */
void ApplePS2Mouse::setMouseEnable(bool enable)
{
    PS2Request * request = _device->allocateRequest();
    UInt8 commandByte;

    if (request == NULL)
        return;
    request->commands[0].command = kPS2C_WriteCommandPort;
    request->commands[0].inOrOut = kCP_GetCommandByte;
    request->commands[1].command = kPS2C_ReadDataPort;
    request->commands[1].inOrOut = 0;
    request->commandsCount = 2;
    _device->submitRequestAndBlock(request);
    if (request->commandsCount != 2) {
        _device->freeRequest(request);
        return;
    }
    commandByte = request->commands[1].inOrOut;
    commandByte &= ~kCB_DisableMouseClock;
    if (enable)
        commandByte |= kCB_EnableMouseIRQ;
    else
        commandByte &= ~kCB_EnableMouseIRQ;

    request->commands[0].command = kPS2C_WriteCommandPort;
    request->commands[0].inOrOut = kCP_SetCommandByte;
    request->commands[1].command = kPS2C_WriteDataPort;
    request->commands[1].inOrOut = commandByte;
    request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut = enable ? kDP_Enable : kDP_SetDefaultsAndDisable;
    request->commandsCount = 3;
    _device->submitRequestAndBlock(request);
    _device->freeRequest(request);
}

bool ApplePS2Mouse::start(IOService * provider)
{
    static const UInt8 defaults[] = { kDP_SetDefaults };
    /* IntelliMouse: sample rates 200, 100, 80, then the device ID. */
    static const UInt8 wheelProbe[] = {
        kDP_SetMouseSampleRate, 200, kDP_SetMouseSampleRate, 100,
        kDP_SetMouseSampleRate, 80, kDP_GetId,
    };
    static const UInt8 rate[] = { kDP_SetMouseSampleRate, 100 };
    UInt8 id = 0;

    _device = OSDynamicCast(ApplePS2MouseDevice, provider);
    if (_device == NULL)
        return false;

    /* No mouse on the port: nothing acknowledges. */
    if (sendCommands(defaults, 1, false, NULL) != 1) {
        IOLog("ApplePS2Mouse: no mouse on the auxiliary port\n");
        return false;
    }
    if (sendCommands(wheelProbe, sizeof(wheelProbe), true, &id) == sizeof(wheelProbe) + 1)
        _wheel = (id == 3);
    sendCommands(rate, sizeof(rate), false, NULL);

    if (!super::start(provider))
        return false;

    setProperty("MinkarEvents", 0ULL, 32);
    setProperty("MinkarPacketBytes", _wheel ? 4ULL : 3ULL, 32);
    _device->installInterruptAction(this,
        (PS2InterruptAction)&ApplePS2Mouse::interruptOccurred);
    _interruptInstalled = true;
    setMouseEnable(true);

    IOLog("ApplePS2Mouse: started (%s, %u buttons)\n",
        _wheel ? "wheel, 4-byte packets" : "3-byte packets", (unsigned)buttonCount());
    return true;
}

void ApplePS2Mouse::stop(IOService * provider)
{
    if (_device != NULL)
        setMouseEnable(false);
    if (_interruptInstalled) {
        _device->uninstallInterruptAction();
        _interruptInstalled = false;
    }
    _device = NULL;
    super::stop(provider);
}

void ApplePS2Mouse::interruptOccurred(void * target, UInt8 data)
{
    ApplePS2Mouse * self = (ApplePS2Mouse *)target;

    /* A first byte always has the sync bit; anything else is dropped
     * until one comes, which also skips stray acknowledgements. */
    if (self->_packetLength == 0 && !(data & kPacketSync))
        return;
    self->_packet[self->_packetLength++] = data;
    if (self->_packetLength == (self->_wheel ? 4 : 3)) {
        self->_packetLength = 0;
        self->dispatchPacket();
    }
}

void ApplePS2Mouse::dispatchPacket(void)
{
    UInt8 flags = _packet[0];
    int dx = 0, dy = 0, wheel = 0;
    UInt32 buttons = 0;
    AbsoluteTime now;

    /* An overflowing axis carries no usable value. */
    if (!(flags & kPacketXOver))
        dx = (int)_packet[1] - ((flags & kPacketXSign) ? 256 : 0);
    if (!(flags & kPacketYOver))
        dy = (int)_packet[2] - ((flags & kPacketYSign) ? 256 : 0);
    dy = -dy;                       /* the mouse counts up; the screen down */

    if (flags & kPacketLeft)   buttons |= 0x1;
    if (flags & kPacketRight)  buttons |= 0x2;
    if (flags & kPacketMiddle) buttons |= 0x4;

    if (_wheel) {
        /* Four-bit signed count, positive toward the user (down). */
        wheel = _packet[3] & 0x0F;
        if (wheel & 0x08)
            wheel -= 16;
        wheel = -wheel;             /* positive up, as USB mice report */
    }

    clock_get_uptime((uint64_t *)&now);
    dispatchRelativePointerEvent(dx, dy, buttons, now);
    if (wheel != 0)
        dispatchScrollWheelEvent((short)wheel, 0, 0, now);

    _events++;
    setProperty("MinkarEvents", (unsigned long long)_events, 32);
    setProperty("MinkarDX", (unsigned long long)(uint32_t)dx, 32);
    setProperty("MinkarDY", (unsigned long long)(uint32_t)dy, 32);
    setProperty("MinkarButtons", (unsigned long long)buttons, 32);
    if (wheel != 0)     /* the last turn: a packet without one follows */
        setProperty("MinkarWheel", (unsigned long long)(uint32_t)wheel, 32);
}

IOItemCount ApplePS2Mouse::buttonCount()
{
    return 3;
}

/* The PS/2 default: 4 counts per millimetre, about 100 per inch
 * (16.16 fixed point, as IOHIPointing expects it). */
IOFixed ApplePS2Mouse::resolution()
{
    return 100 << 16;
}
