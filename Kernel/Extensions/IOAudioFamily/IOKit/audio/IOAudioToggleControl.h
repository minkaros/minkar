/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A toggle (a mute): 0 or 1.
 */

#ifndef _IOKIT_IOAUDIOTOGGLECONTROL_H
#define _IOKIT_IOAUDIOTOGGLECONTROL_H

#include <IOKit/audio/IOAudioControl.h>

class IOAudioToggleControl : public IOAudioControl {
    OSDeclareDefaultStructors(IOAudioToggleControl);

public:
    static IOAudioToggleControl *create(bool initialValue, UInt32 channelID,
        const char *channelName = NULL, UInt32 cntrlID = 0, UInt32 subType = 0, UInt32 usage = 0);
    static IOAudioToggleControl *createMuteControl(bool initialValue, UInt32 channelID,
        const char *channelName = NULL, UInt32 cntrlID = 0, UInt32 usage = 0);

    virtual bool init(bool initialValue, UInt32 channelID, const char *channelName = NULL,
        UInt32 cntrlID = 0, UInt32 subType = 0, UInt32 usage = 0, OSDictionary *properties = NULL);

protected:
    virtual IOReturn validateValue(OSObject *newValue) APPLE_KEXT_OVERRIDE;
};

#endif /* _IOKIT_IOAUDIOTOGGLECONTROL_H */
