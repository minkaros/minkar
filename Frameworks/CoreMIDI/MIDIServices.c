/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "MIDIServices.h"
#include <Block.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

enum { paramErr = -50, memFullErr = -108 };

#define KEY(name) const CFStringRef k##name = CFSTR(#name)
KEY(MIDIPropertyName);
KEY(MIDIPropertyManufacturer);
KEY(MIDIPropertyModel);
KEY(MIDIPropertyUniqueID);
KEY(MIDIPropertyDisplayName);
KEY(MIDIPropertyOffline);
KEY(MIDIPropertyPrivate);

/* Clients and ports, by number: 0 is never one. */
enum { kKindClient = 1, kKindInputPort, kKindOutputPort };

typedef struct MIDIObject {
    int kind;
    MIDIClientRef client;
    CFStringRef name;
    void *block;
} MIDIObject;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static MIDIObject *objects;
static UInt32 objectCount;

static MIDIObjectRef addObject(int kind, MIDIClientRef client, CFStringRef name, void *block) {
    pthread_mutex_lock(&lock);
    MIDIObject *grown = realloc(objects, (objectCount + 1) * sizeof(MIDIObject));
    MIDIObjectRef ref = 0;
    if (grown != NULL) {
        objects = grown;
        objects[objectCount] = (MIDIObject){ kind, client, name ? CFRetain(name) : NULL, block };
        ref = ++objectCount;
    }
    pthread_mutex_unlock(&lock);
    return ref;
}

static int kindOf(MIDIObjectRef ref) {
    pthread_mutex_lock(&lock);
    int kind = (ref > 0 && ref <= objectCount) ? objects[ref - 1].kind : 0;
    pthread_mutex_unlock(&lock);
    return kind;
}

static void removeObject(MIDIObjectRef ref) {
    pthread_mutex_lock(&lock);
    if (ref > 0 && ref <= objectCount && objects[ref - 1].kind != 0) {
        MIDIObject *o = &objects[ref - 1];
        if (o->name != NULL)
            CFRelease(o->name);
        if (o->block != NULL)
            _Block_release(o->block);
        o->kind = 0;
        o->name = NULL;
        o->block = NULL;
    }
    pthread_mutex_unlock(&lock);
}

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient) {
    if (outClient == NULL)
        return paramErr;
    *outClient = addObject(kKindClient, 0, name, NULL);
    return *outClient != 0 ? noErr : memFullErr;
}

OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, MIDINotifyBlock notifyBlock) {
    return MIDIClientCreate(name, NULL, NULL, outClient);
}

OSStatus MIDIClientDispose(MIDIClientRef client) {
    if (kindOf(client) != kKindClient)
        return kMIDIInvalidClient;
    pthread_mutex_lock(&lock);
    UInt32 count = objectCount;
    pthread_mutex_unlock(&lock);
    for (UInt32 i = 1; i <= count; i++) {
        pthread_mutex_lock(&lock);
        int owned = objects[i - 1].kind != 0 && objects[i - 1].client == client;
        pthread_mutex_unlock(&lock);
        if (owned)
            removeObject(i);
    }
    removeObject(client);
    return noErr;
}

static OSStatus createPort(MIDIClientRef client, int kind, CFStringRef name, void *block, MIDIPortRef *outPort) {
    if (outPort == NULL)
        return paramErr;
    if (kindOf(client) != kKindClient)
        return kMIDIInvalidClient;
    *outPort = addObject(kind, client, name, block);
    return *outPort != 0 ? noErr : memFullErr;
}

OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon,
                             MIDIPortRef *outPort) {
    return createPort(client, kKindInputPort, portName, NULL, outPort);
}

OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort,
                                      MIDIReadBlock readBlock) {
    return createPort(client, kKindInputPort, portName, readBlock ? _Block_copy(readBlock) : NULL, outPort);
}

OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort) {
    return createPort(client, kKindOutputPort, portName, NULL, outPort);
}

OSStatus MIDIPortDispose(MIDIPortRef port) {
    int kind = kindOf(port);
    if (kind != kKindInputPort && kind != kKindOutputPort)
        return kMIDIInvalidPort;
    removeObject(port);
    return noErr;
}

/* There are no endpoints: connecting, sending and receiving find none. */
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon) {
    return kindOf(port) == kKindInputPort ? kMIDIUnknownEndpoint : kMIDIInvalidPort;
}

OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source) {
    return kindOf(port) == kKindInputPort ? kMIDINoConnection : kMIDIInvalidPort;
}

ItemCount MIDIGetNumberOfDevices(void) { return 0; }
MIDIDeviceRef MIDIGetDevice(ItemCount i) { return 0; }
ItemCount MIDIGetNumberOfSources(void) { return 0; }
MIDIEndpointRef MIDIGetSource(ItemCount i) { return 0; }
ItemCount MIDIGetNumberOfDestinations(void) { return 0; }
MIDIEndpointRef MIDIGetDestination(ItemCount i) { return 0; }

OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt) {
    return kMIDIUnknownEndpoint;
}

OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str) {
    if (str == NULL)
        return paramErr;
    *str = NULL;
    pthread_mutex_lock(&lock);
    CFStringRef name = (obj > 0 && obj <= objectCount && objects[obj - 1].kind != 0) ? objects[obj - 1].name : NULL;
    int found = obj > 0 && obj <= objectCount && objects[obj - 1].kind != 0;
    if (name != NULL)
        CFRetain(name);
    pthread_mutex_unlock(&lock);
    if (!found)
        return kMIDIObjectNotFound;
    if (!CFEqual(propertyID, kMIDIPropertyName) && !CFEqual(propertyID, kMIDIPropertyDisplayName)) {
        if (name != NULL)
            CFRelease(name);
        return kMIDIUnknownProperty;
    }
    *str = name != NULL ? name : CFSTR("");
    return noErr;
}

OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue) {
    if (kindOf(obj) == 0)
        return kMIDIObjectNotFound;
    if (outValue != NULL && CFEqual(propertyID, kMIDIPropertyUniqueID)) {
        *outValue = (SInt32)obj;
        return noErr;
    }
    return kMIDIUnknownProperty;
}

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist) {
    return kindOf(port) == kKindOutputPort ? kMIDIUnknownEndpoint : kMIDIInvalidPort;
}

OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist) {
    return kMIDIUnknownEndpoint;
}

OSStatus MIDIFlushOutput(MIDIEndpointRef dest) {
    return noErr;
}

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist) {
    pktlist->numPackets = 0;
    return &pktlist->packet[0];
}

/* Appends to the packet when it has the same time and room, else starts a
   new one; NULL when the list has no room left. */
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket,
                              MIDITimeStamp time, ByteCount nData, const Byte *data) {
    uintptr_t end = (uintptr_t)pktlist + listSize;
    if (pktlist->numPackets > 0 && curPacket->timeStamp == time && data[0] < 0xf0 &&
        curPacket->data[0] < 0xf0 && curPacket->length + nData <= sizeof(curPacket->data) &&
        (uintptr_t)&curPacket->data[curPacket->length + nData] <= end) {
        memcpy(&curPacket->data[curPacket->length], data, nData);
        curPacket->length += (UInt16)nData;
        return curPacket;
    }
    MIDIPacket *packet = pktlist->numPackets == 0 ? &pktlist->packet[0] : MIDIPacketNext(curPacket);
    if (nData > 65535 || (uintptr_t)&packet->data[nData] > end)
        return NULL;
    packet->timeStamp = time;
    packet->length = (UInt16)nData;
    memcpy(packet->data, data, nData);
    pktlist->numPackets++;
    return packet;
}
