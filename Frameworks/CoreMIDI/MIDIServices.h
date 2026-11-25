/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * MIDI Services, as CoreMIDI documents them: clients and their ports,
 * devices, entities and endpoints (sources and destinations), virtual
 * endpoints, properties, notifications, and MIDI 1.0 packet lists sent
 * and received across processes through the MIDI server (midiserver).
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

enum { kMIDIInvalidUniqueID = 0 };

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
    kMIDIUnknownError = -10845,
};

typedef SInt32 MIDIObjectType;
enum {
    kMIDIObjectType_Other = -1,
    kMIDIObjectType_Device = 0,
    kMIDIObjectType_Entity = 1,
    kMIDIObjectType_Source = 2,
    kMIDIObjectType_Destination = 3,
    kMIDIObjectType_ExternalMask = 0x10,
    kMIDIObjectType_ExternalDevice = kMIDIObjectType_ExternalMask | kMIDIObjectType_Device,
    kMIDIObjectType_ExternalEntity = kMIDIObjectType_ExternalMask | kMIDIObjectType_Entity,
    kMIDIObjectType_ExternalSource = kMIDIObjectType_ExternalMask | kMIDIObjectType_Source,
    kMIDIObjectType_ExternalDestination = kMIDIObjectType_ExternalMask | kMIDIObjectType_Destination
};

/* MIDI 1.0 packet lists. On x86-64 a packet follows the previous one's
   last byte (the packing apps' binaries assume); on arm64 packets start on
   4-byte boundaries. */
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

#if defined(__arm64__) || defined(__aarch64__)
#define MIDIPacketNext(pkt) ((MIDIPacket *)(((uintptr_t)(&(pkt)->data[(pkt)->length]) + 3) & ~(uintptr_t)3))
#else
#define MIDIPacketNext(pkt) ((MIDIPacket *)&(pkt)->data[(pkt)->length])
#endif

/* Notifications. */
typedef SInt32 MIDINotificationMessageID;
enum {
    kMIDIMsgSetupChanged = 1,
    kMIDIMsgObjectAdded = 2,
    kMIDIMsgObjectRemoved = 3,
    kMIDIMsgPropertyChanged = 4,
    kMIDIMsgThruConnectionsChanged = 5,
    kMIDIMsgSerialPortOwnerChanged = 6,
    kMIDIMsgIOError = 7
};

typedef struct MIDINotification {
    MIDINotificationMessageID messageID;
    UInt32 messageSize;
} MIDINotification;

typedef struct MIDIObjectAddRemoveNotification {
    MIDINotificationMessageID messageID;
    UInt32 messageSize;
    MIDIObjectRef parent;
    MIDIObjectType parentType;
    MIDIObjectRef child;
    MIDIObjectType childType;
} MIDIObjectAddRemoveNotification;

typedef struct MIDIObjectPropertyChangeNotification {
    MIDINotificationMessageID messageID;
    UInt32 messageSize;
    MIDIObjectRef object;
    MIDIObjectType objectType;
    CFStringRef propertyName;
} MIDIObjectPropertyChangeNotification;

typedef void (*MIDINotifyProc)(const MIDINotification *message, void *refCon);
typedef void (*MIDIReadProc)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);
typedef void (*MIDICompletionProc)(struct MIDISysexSendRequest *request);
#ifdef __BLOCKS__
typedef void (^MIDINotifyBlock)(const MIDINotification *message);
typedef void (^MIDIReadBlock)(const MIDIPacketList *pktlist, void *srcConnRefCon);
#endif

/* A system-exclusive message sent in the background: complete is set,
   then completionProc called, when it has gone. */
typedef struct MIDISysexSendRequest {
    MIDIEndpointRef destination;
    const Byte *data;
    UInt32 bytesToSend;
    Boolean complete;
    Byte reserved[3];
    MIDICompletionProc completionProc;
    void *completionRefCon;
} MIDISysexSendRequest;

/* Object properties (their documented keys). */
CF_EXPORT const CFStringRef kMIDIPropertyName;
CF_EXPORT const CFStringRef kMIDIPropertyManufacturer;
CF_EXPORT const CFStringRef kMIDIPropertyModel;
CF_EXPORT const CFStringRef kMIDIPropertyUniqueID;
CF_EXPORT const CFStringRef kMIDIPropertyDeviceID;
CF_EXPORT const CFStringRef kMIDIPropertyReceiveChannels;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitChannels;
CF_EXPORT const CFStringRef kMIDIPropertyMaxSysExSpeed;
CF_EXPORT const CFStringRef kMIDIPropertyAdvanceScheduleTimeMuSec;
CF_EXPORT const CFStringRef kMIDIPropertyIsEmbeddedEntity;
CF_EXPORT const CFStringRef kMIDIPropertyIsBroadcast;
CF_EXPORT const CFStringRef kMIDIPropertySingleRealtimeEntity;
CF_EXPORT const CFStringRef kMIDIPropertyConnectionUniqueID;
CF_EXPORT const CFStringRef kMIDIPropertyOffline;
CF_EXPORT const CFStringRef kMIDIPropertyPrivate;
CF_EXPORT const CFStringRef kMIDIPropertyDriverOwner;
CF_EXPORT const CFStringRef kMIDIPropertyImage;
CF_EXPORT const CFStringRef kMIDIPropertyDriverVersion;
CF_EXPORT const CFStringRef kMIDIPropertySupportsGeneralMIDI;
CF_EXPORT const CFStringRef kMIDIPropertySupportsMMC;
CF_EXPORT const CFStringRef kMIDIPropertyCanRoute;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesClock;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesMTC;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesNotes;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesProgramChanges;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesBankSelectMSB;
CF_EXPORT const CFStringRef kMIDIPropertyReceivesBankSelectLSB;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsClock;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsMTC;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsNotes;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsProgramChanges;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsBankSelectMSB;
CF_EXPORT const CFStringRef kMIDIPropertyTransmitsBankSelectLSB;
CF_EXPORT const CFStringRef kMIDIPropertyPanDisruptsStereo;
CF_EXPORT const CFStringRef kMIDIPropertyIsSampler;
CF_EXPORT const CFStringRef kMIDIPropertyIsDrumMachine;
CF_EXPORT const CFStringRef kMIDIPropertyIsMixer;
CF_EXPORT const CFStringRef kMIDIPropertyIsEffectUnit;
CF_EXPORT const CFStringRef kMIDIPropertyMaxReceiveChannels;
CF_EXPORT const CFStringRef kMIDIPropertyMaxTransmitChannels;
CF_EXPORT const CFStringRef kMIDIPropertyDriverDeviceEditorApp;
CF_EXPORT const CFStringRef kMIDIPropertySupportsShowControl;
CF_EXPORT const CFStringRef kMIDIPropertyDisplayName;
CF_EXPORT const CFStringRef kMIDIPropertyProtocolID;

/* Clients and ports. */
OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient);
OSStatus MIDIClientDispose(MIDIClientRef client);
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon,
                             MIDIPortRef *outPort);
OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort);
OSStatus MIDIPortDispose(MIDIPortRef port);
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon);
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source);
#ifdef __BLOCKS__
OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, MIDINotifyBlock notifyBlock);
OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort,
                                      MIDIReadBlock readBlock);
OSStatus MIDIDestinationCreateWithBlock(MIDIClientRef client, CFStringRef name, MIDIEndpointRef *outDest,
                                        MIDIReadBlock readBlock);
#endif

/* Devices, entities and endpoints. */
ItemCount MIDIGetNumberOfDevices(void);
MIDIDeviceRef MIDIGetDevice(ItemCount deviceIndex0);
ItemCount MIDIGetNumberOfExternalDevices(void);
MIDIDeviceRef MIDIGetExternalDevice(ItemCount deviceIndex0);
ItemCount MIDIDeviceGetNumberOfEntities(MIDIDeviceRef device);
MIDIEntityRef MIDIDeviceGetEntity(MIDIDeviceRef device, ItemCount entityIndex0);
ItemCount MIDIEntityGetNumberOfSources(MIDIEntityRef entity);
MIDIEndpointRef MIDIEntityGetSource(MIDIEntityRef entity, ItemCount sourceIndex0);
ItemCount MIDIEntityGetNumberOfDestinations(MIDIEntityRef entity);
MIDIEndpointRef MIDIEntityGetDestination(MIDIEntityRef entity, ItemCount destIndex0);
OSStatus MIDIEntityGetDevice(MIDIEntityRef inEntity, MIDIDeviceRef *outDevice);
ItemCount MIDIGetNumberOfSources(void);
MIDIEndpointRef MIDIGetSource(ItemCount sourceIndex0);
ItemCount MIDIGetNumberOfDestinations(void);
MIDIEndpointRef MIDIGetDestination(ItemCount destIndex0);
OSStatus MIDIEndpointGetEntity(MIDIEndpointRef inEndpoint, MIDIEntityRef *outEntity);
OSStatus MIDISourceCreate(MIDIClientRef client, CFStringRef name, MIDIEndpointRef *outSrc);
OSStatus MIDIDestinationCreate(MIDIClientRef client, CFStringRef name, MIDIReadProc readProc, void *refCon,
                               MIDIEndpointRef *outDest);
OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt);

/* Properties. */
OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue);
OSStatus MIDIObjectSetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 value);
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str);
OSStatus MIDIObjectSetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef str);
OSStatus MIDIObjectGetDataProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDataRef *outData);
OSStatus MIDIObjectSetDataProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDataRef data);
OSStatus MIDIObjectGetDictionaryProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDictionaryRef *outDict);
OSStatus MIDIObjectSetDictionaryProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDictionaryRef dict);
OSStatus MIDIObjectGetProperties(MIDIObjectRef obj, CFPropertyListRef *outProperties, Boolean deep);
OSStatus MIDIObjectRemoveProperty(MIDIObjectRef obj, CFStringRef propertyID);
OSStatus MIDIObjectFindByUniqueID(MIDIUniqueID inUniqueID, MIDIObjectRef *outObject, MIDIObjectType *outObjectType);

/* I/O. */
OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist);
OSStatus MIDISendSysex(MIDISysexSendRequest *request);
OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist);
OSStatus MIDIFlushOutput(MIDIEndpointRef dest);
OSStatus MIDIRestart(void);

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist);
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket,
                              MIDITimeStamp time, ByteCount nData, const Byte *data);

CF_EXTERN_C_END

#endif
