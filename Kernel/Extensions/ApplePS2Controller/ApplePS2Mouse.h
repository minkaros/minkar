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

#ifndef _APPLEPS2MOUSE_H
#define _APPLEPS2MOUSE_H

#include <IOKit/hidsystem/IOHIPointing.h>
#include "ApplePS2MouseDevice.h"

/*
 * ApplePS2Mouse: the mouse on the 8042's auxiliary port, published by
 * ApplePS2Controller as an ApplePS2MouseDevice nub, as an IOHIPointing.
 *
 * Standard PS/2 packets of three bytes; a wheel mouse (IntelliMouse,
 * device ID 3) is detected with the sample rate sequence 200, 100, 80
 * and sends four. Like the USB pointer it publishes what it last
 * dispatched (MinkarEvents, MinkarDX, MinkarDY, MinkarButtons, and
 * MinkarWheel for the last wheel turn) and its packet size
 * (MinkarPacketBytes) for tests and diagnosis.
 */
class ApplePS2Mouse : public IOHIPointing
{
    OSDeclareDefaultStructors(ApplePS2Mouse);

private:
    ApplePS2MouseDevice * _device;
    bool                  _interruptInstalled;
    bool                  _wheel;          /* IntelliMouse: 4-byte packets */
    UInt8                 _packet[4];
    UInt8                 _packetLength;   /* bytes received so far */
    UInt32                _events;

    UInt8 sendCommands(const UInt8 * bytes, int count, bool readReply, UInt8 * reply);
    void  setMouseEnable(bool enable);
    void  dispatchPacket(void);

public:
    virtual bool init(OSDictionary * dict) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService * provider) APPLE_KEXT_OVERRIDE;

    /* Called by the controller with each byte from the mouse. */
    static void interruptOccurred(void * target, UInt8 data);

    /* IOHIPointing */
    virtual IOItemCount buttonCount() APPLE_KEXT_OVERRIDE;
    virtual IOFixed     resolution() APPLE_KEXT_OVERRIDE;
};

#endif /* _APPLEPS2MOUSE_H */
