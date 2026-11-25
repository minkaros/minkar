/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Music devices: instruments (kAudioUnitType_MusicDevice) played with MIDI
 * events and notes. The functions are AudioToolbox's; each calls the
 * unit's method for its selector.
 */

#ifndef __MUSICDEVICE__
#define __MUSICDEVICE__

#include <AudioUnit/AUComponent.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef AudioComponentInstance MusicDeviceComponent;
typedef UInt32 MusicDeviceInstrumentID;
typedef UInt32 MusicDeviceGroupID;
typedef UInt32 NoteInstanceID;

typedef struct MusicDeviceStdNoteParams {
    UInt32 argCount;
    Float32 mPitch;
    Float32 mVelocity;
} MusicDeviceStdNoteParams;

typedef struct NoteParamsControlValue {
    AudioUnitParameterID mID;
    AudioUnitParameterValue mValue;
} NoteParamsControlValue;

typedef struct MusicDeviceNoteParams {
    UInt32 argCount;
    Float32 mPitch;
    Float32 mVelocity;
    NoteParamsControlValue mControls[1];
} MusicDeviceNoteParams;

enum {
    kMusicNoteEvent_UseGroupInstrument = 0xFFFFFFFF,
    kMusicNoteEvent_Unused = 0xFFFFFFFF,
};

enum {
    kMusicDeviceRange = 0x0100,
    kMusicDeviceMIDIEventSelect = 0x0101,
    kMusicDeviceSysExSelect = 0x0102,
    kMusicDevicePrepareInstrumentSelect = 0x0103,
    kMusicDeviceReleaseInstrumentSelect = 0x0104,
    kMusicDeviceStartNoteSelect = 0x0105,
    kMusicDeviceStopNoteSelect = 0x0106,
    kMusicDeviceMIDIEventListSelect = 0x0107,
};

enum { kMusicDeviceProperty_InstrumentCount = 1000 };

typedef OSStatus (*MusicDeviceMIDIEventProc)(void *self, UInt32 inStatus, UInt32 inData1, UInt32 inData2,
                                             UInt32 inOffsetSampleFrame);
typedef OSStatus (*MusicDeviceSysExProc)(void *self, const UInt8 *inData, UInt32 inLength);
typedef OSStatus (*MusicDeviceStartNoteProc)(void *self, MusicDeviceInstrumentID inInstrument,
                                             MusicDeviceGroupID inGroupID, NoteInstanceID *outNoteInstanceID,
                                             UInt32 inOffsetSampleFrame, const MusicDeviceNoteParams *inParams);
typedef OSStatus (*MusicDeviceStopNoteProc)(void *self, MusicDeviceGroupID inGroupID,
                                            NoteInstanceID inNoteInstanceID, UInt32 inOffsetSampleFrame);

OSStatus MusicDeviceMIDIEvent(MusicDeviceComponent inUnit, UInt32 inStatus, UInt32 inData1, UInt32 inData2,
                              UInt32 inOffsetSampleFrame);
OSStatus MusicDeviceSysEx(MusicDeviceComponent inUnit, const UInt8 *inData, UInt32 inLength);
OSStatus MusicDeviceStartNote(MusicDeviceComponent inUnit, MusicDeviceInstrumentID inInstrument,
                              MusicDeviceGroupID inGroupID, NoteInstanceID *outNoteInstanceID,
                              UInt32 inOffsetSampleFrame, const MusicDeviceNoteParams *inParams);
OSStatus MusicDeviceStopNote(MusicDeviceComponent inUnit, MusicDeviceGroupID inGroupID,
                             NoteInstanceID inNoteInstanceID, UInt32 inOffsetSampleFrame);

#ifdef __cplusplus
}
#endif

#endif
