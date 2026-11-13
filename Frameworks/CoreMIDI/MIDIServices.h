/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __MIDISERVICES__
#define __MIDISERVICES__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

CF_EXTERN_C_BEGIN

typedef UInt32 MIDIObjectRef;
typedef MIDIObjectRef MIDIClientRef;
typedef MIDIObjectRef MIDIPortRef;
typedef MIDIObjectRef MIDIDeviceRef;
typedef MIDIObjectRef MIDIEntityRef;
typedef MIDIObjectRef MIDIEndpointRef;
typedef UInt64 MIDITimeStamp;
typedef SInt32 MIDIUniqueID;

enum {
    kMIDIInvalidClient = -10830,
    kMIDIInvalidPort = -10831,
    kMIDIWrongEndpointType = -10832,
    kMIDINoConnection = -10833,
    kMIDIUnknownEndpoint = -10834,
    kMIDIUnknownProperty = -10835,
    kMIDIWrongPropertyType = -10836,
    kMIDINoCurrentSetup = -10837,
    kMIDIMessageSendErr = -10838,
    kMIDIServerStartErr = -10839,
    kMIDISetupFormatErr = -10840,
    kMIDIWrongThread = -10841,
    kMIDIObjectNotFound = -10842,
    kMIDIIDNotUnique = -10843,
    kMIDINotPermitted = -10844,
};

#pragma pack(push, 4)
typedef struct MIDIPacket {
    MIDITimeStamp timeStamp;
    UInt16 length;
    Byte data[256];
} MIDIPacket;

typedef struct MIDIPacketList {
    UInt32 numPackets;
    MIDIPacket packet[1];
} MIDIPacketList;
#pragma pack(pop)

typedef struct MIDINotification MIDINotification;
typedef void (*MIDINotifyProc)(const MIDINotification *message, void *refCon);
typedef void (*MIDIReadProc)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);
#ifdef __BLOCKS__
typedef void (^MIDINotifyBlock)(const MIDINotification *message);
typedef void (^MIDIReadBlock)(const MIDIPacketList *pktlist, void *srcConnRefCon);
#endif

/* Object properties. */
CF_EXPORT const CFStringRef kMIDIPropertyName;
CF_EXPORT const CFStringRef kMIDIPropertyManufacturer;
CF_EXPORT const CFStringRef kMIDIPropertyModel;
CF_EXPORT const CFStringRef kMIDIPropertyUniqueID;
CF_EXPORT const CFStringRef kMIDIPropertyDisplayName;
CF_EXPORT const CFStringRef kMIDIPropertyOffline;
CF_EXPORT const CFStringRef kMIDIPropertyPrivate;

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient);
OSStatus MIDIClientDispose(MIDIClientRef client);
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon,
                             MIDIPortRef *outPort);
#ifdef __BLOCKS__
OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, MIDINotifyBlock notifyBlock);
OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort,
                                      MIDIReadBlock readBlock);
#endif
OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort);
OSStatus MIDIPortDispose(MIDIPortRef port);
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon);
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source);

ItemCount MIDIGetNumberOfDevices(void);
MIDIDeviceRef MIDIGetDevice(ItemCount deviceIndex0);
ItemCount MIDIGetNumberOfSources(void);
MIDIEndpointRef MIDIGetSource(ItemCount sourceIndex0);
ItemCount MIDIGetNumberOfDestinations(void);
MIDIEndpointRef MIDIGetDestination(ItemCount destIndex0);
OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt);

OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str);
OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue);

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist);
OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist);
OSStatus MIDIFlushOutput(MIDIEndpointRef dest);

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist);
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket,
                              MIDITimeStamp time, ByteCount nData, const Byte *data);

/* The next packet in a list (packets are not all the size of MIDIPacket). */
#define MIDIPacketNext(pkt) ((MIDIPacket *)(((uintptr_t)(&(pkt)->data[(pkt)->length]) + 3) & ~(uintptr_t)3))

CF_EXTERN_C_END

#endif
