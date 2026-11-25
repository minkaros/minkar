/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreMIDI's client side (AU8): every object is the MIDI server's
 * (midiserver, MIDIServerProtocol.h); this process keeps only what calls
 * it back, its clients' notification procs, its input ports' and virtual
 * destinations' read procs. The process says hello to the server once,
 * with a port of its own; a thread of its own (CoreMIDI's) waits on that
 * port and calls the procs and blocks, as the reference calls them on a
 * thread of CoreMIDI's.
 */

#include "MIDIServices.h"
#include "MIDIServerProtocol.h"
#include <Block.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { paramErr = -50, memFullErr = -108 };

/* The property keys, with their documented values. */
#define KEY(name, value) const CFStringRef kMIDIProperty##name = CFSTR(value)
KEY(Name, "name");
KEY(Manufacturer, "manufacturer");
KEY(Model, "model");
KEY(UniqueID, "uniqueID");
KEY(DeviceID, "deviceID");
KEY(ReceiveChannels, "receiveChannels");
KEY(TransmitChannels, "transmitChannels");
KEY(MaxSysExSpeed, "maxSysExSpeed");
KEY(AdvanceScheduleTimeMuSec, "advanceScheduleTimeMuSec");
KEY(IsEmbeddedEntity, "embedded");
KEY(IsBroadcast, "broadcast");
KEY(SingleRealtimeEntity, "singleRealtimeEntity");
KEY(ConnectionUniqueID, "connUniqueID");
KEY(Offline, "offline");
KEY(Private, "private");
KEY(DriverOwner, "driver");
KEY(Image, "image");
KEY(DriverVersion, "driverVersion");
KEY(SupportsGeneralMIDI, "supportsGeneralMIDI");
KEY(SupportsMMC, "supportsMMC");
KEY(CanRoute, "canRoute");
KEY(ReceivesClock, "receivesClock");
KEY(ReceivesMTC, "receivesMTC");
KEY(ReceivesNotes, "receivesNotes");
KEY(ReceivesProgramChanges, "receivesProgramChanges");
KEY(ReceivesBankSelectMSB, "receivesBankSelectMSB");
KEY(ReceivesBankSelectLSB, "receivesBankSelectLSB");
KEY(TransmitsClock, "transmitsClock");
KEY(TransmitsMTC, "transmitsMTC");
KEY(TransmitsNotes, "transmitsNotes");
KEY(TransmitsProgramChanges, "transmitsProgramChanges");
KEY(TransmitsBankSelectMSB, "transmitsBankSelectMSB");
KEY(TransmitsBankSelectLSB, "transmitsBankSelectLSB");
KEY(PanDisruptsStereo, "panDisruptsStereo");
KEY(IsSampler, "isSampler");
KEY(IsDrumMachine, "isDrumMachine");
KEY(IsMixer, "isMixer");
KEY(IsEffectUnit, "isEffectUnit");
KEY(MaxReceiveChannels, "maxReceiveChannels");
KEY(MaxTransmitChannels, "maxTransmitChannels");
KEY(DriverDeviceEditorApp, "driverDeviceEditorApp");
KEY(SupportsShowControl, "supportsShowControl");
KEY(DisplayName, "displayName");
KEY(ProtocolID, "protocol");

/* ---- What calls back ---- */

enum { kLocalClient = 1, kLocalInputPort, kLocalDestination };

typedef struct Local {
    MIDIObjectRef ref;
    int kind;
    void *proc;     /* MIDINotifyProc or MIDIReadProc */
    void *refCon;
    void *block;    /* MIDINotifyBlock or MIDIReadBlock, copied */
} Local;

static pthread_mutex_t localLock = PTHREAD_MUTEX_INITIALIZER;
static Local *locals;
static size_t localCount;

static void addLocal(MIDIObjectRef ref, int kind, void *proc, void *refCon, void *block) {
    pthread_mutex_lock(&localLock);
    Local *grown = realloc(locals, (localCount + 1) * sizeof(Local));
    if (grown != NULL) {
        locals = grown;
        locals[localCount++] = (Local){ ref, kind, proc, refCon, block != NULL ? _Block_copy(block) : NULL };
    }
    pthread_mutex_unlock(&localLock);
}

static void removeLocal(MIDIObjectRef ref) {
    pthread_mutex_lock(&localLock);
    for (size_t i = 0; i < localCount; i++)
        if (locals[i].ref == ref) {
            if (locals[i].block != NULL)
                _Block_release(locals[i].block);
            locals[i] = locals[--localCount];
            break;
        }
    pthread_mutex_unlock(&localLock);
}

/* A copy of the local entry (its block retained), or 0. */
static int copyLocal(MIDIObjectRef ref, Local *out) {
    int found = 0;
    pthread_mutex_lock(&localLock);
    for (size_t i = 0; i < localCount; i++)
        if (locals[i].ref == ref) {
            *out = locals[i];
            if (out->block != NULL)
                out->block = _Block_copy(out->block);
            found = 1;
            break;
        }
    pthread_mutex_unlock(&localLock);
    return found;
}

/* ---- The server ---- */

static pthread_once_t connectOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t callLock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t server = MACH_PORT_NULL, clientPort = MACH_PORT_NULL, replyPort = MACH_PORT_NULL;

static void *receive(void *unused);

static void connectToServer(void) {
    MIDIServerHello hello;
    pthread_t thread;
    if (bootstrap_look_up(bootstrap_port, kMIDIServerName, &server) != KERN_SUCCESS) {
        server = MACH_PORT_NULL;
        return;
    }
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort) != KERN_SUCCESS ||
        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &replyPort) != KERN_SUCCESS) {
        server = MACH_PORT_NULL;
        return;
    }
    memset(&hello, 0, sizeof(hello));
    hello.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    hello.header.msgh_remote_port = server;
    hello.header.msgh_size = sizeof(hello);
    hello.body.msgh_descriptor_count = 1;
    hello.port.name = clientPort;
    hello.port.disposition = MACH_MSG_TYPE_MAKE_SEND;
    hello.port.type = MACH_MSG_PORT_DESCRIPTOR;
    hello.op = kMIDIOpHello;
    hello.pid = (uint32_t)getpid();
    if (mach_msg(&hello.header, MACH_SEND_MSG, sizeof(hello), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE,
                 MACH_PORT_NULL) != MACH_MSG_SUCCESS) {
        server = MACH_PORT_NULL;
        return;
    }
    pthread_create(&thread, NULL, receive, NULL);
    pthread_detach(thread);
}

/* A request and its reply (in *reply); the reply's status, or an error
   if the server cannot be reached. */
static OSStatus call(uint32_t op, uint32_t a, uint32_t b, uint32_t c, const void *payload, size_t length,
                     MIDIServerMessage *reply) {
    static struct {
        MIDIServerMessage m;
        mach_msg_trailer_t trailer;
    } buffer;
    MIDIServerMessage *mp = &buffer.m;
#define m (*mp)
    pthread_once(&connectOnce, connectToServer);
    if (server == MACH_PORT_NULL)
        return kMIDIServerStartErr;
    if (length > kMIDIMaxPayload)
        return kMIDIMessageSendErr;
    pthread_mutex_lock(&callLock);
    memset(&m.header, 0, sizeof(m.header));
    m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    m.header.msgh_remote_port = server;
    m.header.msgh_local_port = replyPort;
    m.header.msgh_size = MIDIServerMessageSize(length);
    m.header.msgh_id = (mach_msg_id_t)op;
    m.op = op;
    m.status = 0;
    m.a = a;
    m.b = b;
    m.c = c;
    m.length = (uint32_t)length;
    if (length > 0)
        memcpy(m.payload, payload, length);
    mach_msg_return_t mr = mach_msg(&m.header, MACH_SEND_MSG | MACH_RCV_MSG, m.header.msgh_size, sizeof(buffer),
                                    replyPort, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (mr == MACH_MSG_SUCCESS)
        memcpy(reply, &m, offsetof(MIDIServerMessage, payload) + (m.length <= kMIDIMaxPayload ? m.length : 0));
    pthread_mutex_unlock(&callLock);
#undef m
    if (mr != MACH_MSG_SUCCESS)
        return kMIDIMessageSendErr;
    return reply->status;
}

static MIDIServerMessage *newReply(void) {
    return malloc(sizeof(MIDIServerMessage));
}

/* A UTF-8 copy of a CFString (NULL: empty), its length in *length. */
static char *utf8(CFStringRef s, size_t *length) {
    CFIndex max = s != NULL ? CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1 : 1;
    char *buffer = malloc((size_t)max);
    if (buffer == NULL)
        return NULL;
    buffer[0] = '\0';
    if (s != NULL)
        CFStringGetCString(s, buffer, max, kCFStringEncodingUTF8);
    *length = strlen(buffer);
    return buffer;
}

/* The size of a packet list: its packets, one after the other. */
static size_t packetListSize(const MIDIPacketList *list) {
    const MIDIPacket *p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; i++)
        p = MIDIPacketNext(p);
    return (size_t)((const uint8_t *)p - (const uint8_t *)list);
}

/* ---- CoreMIDI's thread ---- */

static void callNotify(const MIDINotification *n) {
    Local *copy;
    size_t count;
    pthread_mutex_lock(&localLock);
    count = localCount;
    copy = malloc((count + 1) * sizeof(Local));
    for (size_t i = 0; copy != NULL && i < count; i++) {
        copy[i] = locals[i];
        if (copy[i].block != NULL)
            copy[i].block = _Block_copy(copy[i].block);
    }
    pthread_mutex_unlock(&localLock);
    for (size_t i = 0; copy != NULL && i < count; i++) {
        if (copy[i].kind == kLocalClient) {
            if (copy[i].block != NULL)
                ((MIDINotifyBlock)copy[i].block)(n);
            else if (copy[i].proc != NULL)
                ((MIDINotifyProc)copy[i].proc)(n, copy[i].refCon);
        }
        if (copy[i].block != NULL)
            _Block_release(copy[i].block);
    }
    free(copy);
}

static void callRead(MIDIObjectRef ref, const MIDIPacketList *list, void *connRefCon) {
    Local l;
    if (!copyLocal(ref, &l))
        return;
    if (l.block != NULL) {
        ((MIDIReadBlock)l.block)(list, connRefCon);
        _Block_release(l.block);
    } else if (l.proc != NULL)
        ((MIDIReadProc)l.proc)(list, l.refCon, connRefCon);
}

static void *receive(void *unused) {
    static struct {
        MIDIServerMessage m;
        mach_msg_trailer_t trailer;
    } in;
    pthread_setname_np("com.minkar.coremidi");
    for (;;) {
        mach_msg_return_t mr = mach_msg(&in.m.header, MACH_RCV_MSG, 0, sizeof(in), clientPort,
                                        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (mr != MACH_MSG_SUCCESS || in.m.length > kMIDIMaxPayload)
            continue;
        switch (in.m.op) {
        case kMIDIOpNotify:
            if (in.m.a == kMIDIMsgObjectAdded || in.m.a == kMIDIMsgObjectRemoved) {
                MIDIObjectAddRemoveNotification n = { (MIDINotificationMessageID)in.m.a, sizeof(n), in.m.b,
                                                      (MIDIObjectType)in.m.c, 0, kMIDIObjectType_Other };
                if (in.m.length >= 8) {
                    uint32_t child[2];
                    memcpy(child, in.m.payload, 8);
                    n.child = child[0];
                    n.childType = (MIDIObjectType)child[1];
                }
                callNotify((const MIDINotification *)&n);
            } else if (in.m.a == kMIDIMsgPropertyChanged) {
                CFStringRef name = CFStringCreateWithBytes(NULL, in.m.payload,
                    in.m.length > 0 ? (CFIndex)in.m.length - 1 : 0, kCFStringEncodingUTF8, false);
                MIDIObjectPropertyChangeNotification n = { kMIDIMsgPropertyChanged, sizeof(n), in.m.b,
                                                           (MIDIObjectType)in.m.c, name };
                callNotify((const MIDINotification *)&n);
                if (name != NULL)
                    CFRelease(name);
            } else {
                MIDINotification n = { (MIDINotificationMessageID)in.m.a, sizeof(n) };
                callNotify(&n);
            }
            break;
        case kMIDIOpPortInput:
            if (in.m.length >= 8 + sizeof(UInt32)) {
                uint64_t refCon;
                memcpy(&refCon, in.m.payload, 8);
                callRead(in.m.a, (const MIDIPacketList *)(in.m.payload + 8), (void *)(uintptr_t)refCon);
            }
            break;
        case kMIDIOpDestinationInput:
            if (in.m.length >= sizeof(UInt32))
                callRead(in.m.a, (const MIDIPacketList *)in.m.payload, NULL);
            break;
        }
    }
    return NULL;
}

/* ---- Clients and ports ---- */

static OSStatus createNamed(uint32_t op, uint32_t a, uint32_t b, CFStringRef name, MIDIObjectRef *out) {
    size_t length;
    char *bytes = utf8(name, &length);
    MIDIServerMessage *reply = newReply();
    OSStatus status;
    if (bytes == NULL || reply == NULL) {
        free(bytes);
        free(reply);
        return memFullErr;
    }
    status = call(op, a, b, 0, bytes, length, reply);
    if (status == noErr)
        *out = reply->a;
    free(bytes);
    free(reply);
    return status;
}

static OSStatus clientCreate(CFStringRef name, void *proc, void *refCon, void *block, MIDIClientRef *outClient) {
    MIDIClientRef client = 0;
    if (outClient == NULL)
        return paramErr;
    OSStatus status = createNamed(kMIDIOpCreateClient, 0, 0, name, &client);
    if (status == noErr)
        addLocal(client, kLocalClient, proc, refCon, block);
    *outClient = client;
    return status;
}

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient) {
    return clientCreate(name, (void *)notifyProc, notifyRefCon, NULL, outClient);
}

OSStatus MIDIClientCreateWithBlock(CFStringRef name, MIDIClientRef *outClient, MIDINotifyBlock notifyBlock) {
    return clientCreate(name, NULL, NULL, (void *)notifyBlock, outClient);
}

static OSStatus dispose(MIDIObjectRef ref) {
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpDispose, ref, 0, 0, NULL, 0, reply);
    free(reply);
    if (status == noErr)
        removeLocal(ref);
    return status;
}

OSStatus MIDIClientDispose(MIDIClientRef client) {
    OSStatus status = dispose(client);
    return status == kMIDIObjectNotFound ? kMIDIInvalidClient : status;
}

static OSStatus inputPortCreate(MIDIClientRef client, CFStringRef name, void *proc, void *refCon, void *block,
                                MIDIPortRef *outPort) {
    MIDIPortRef port = 0;
    if (outPort == NULL)
        return paramErr;
    OSStatus status = createNamed(kMIDIOpCreatePort, client, 1, name, &port);
    if (status == noErr)
        addLocal(port, kLocalInputPort, proc, refCon, block);
    *outPort = port;
    return status;
}

OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *refCon,
                             MIDIPortRef *outPort) {
    return inputPortCreate(client, portName, (void *)readProc, refCon, NULL, outPort);
}

OSStatus MIDIInputPortCreateWithBlock(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort,
                                      MIDIReadBlock readBlock) {
    return inputPortCreate(client, portName, NULL, NULL, (void *)readBlock, outPort);
}

OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef portName, MIDIPortRef *outPort) {
    if (outPort == NULL)
        return paramErr;
    *outPort = 0;
    return createNamed(kMIDIOpCreatePort, client, 0, portName, outPort);
}

OSStatus MIDIPortDispose(MIDIPortRef port) {
    OSStatus status = dispose(port);
    return status == kMIDIObjectNotFound ? kMIDIInvalidPort : status;
}

OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void *connRefCon) {
    uint64_t refCon = (uint64_t)(uintptr_t)connRefCon;
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpConnect, port, source, 0, &refCon, 8, reply);
    free(reply);
    return status;
}

OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source) {
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpDisconnect, port, source, 0, NULL, 0, reply);
    free(reply);
    return status;
}

/* ---- Devices, entities, endpoints ---- */

/* The listing's count, or its index'th object (0 if none). */
static uint32_t listing(uint32_t kind, uint32_t parent, ItemCount index, int count) {
    MIDIServerMessage *reply = newReply();
    uint32_t result = 0;
    if (reply == NULL)
        return 0;
    if (call(kMIDIOpList, kind, parent, 0, NULL, 0, reply) == noErr) {
        uint32_t n = reply->length / 4;
        if (count)
            result = n;
        else if (index < n)
            memcpy(&result, reply->payload + index * 4, 4);
    }
    free(reply);
    return result;
}

ItemCount MIDIGetNumberOfDevices(void) { return listing(kMIDIListDevices, 0, 0, 1); }
MIDIDeviceRef MIDIGetDevice(ItemCount i) { return listing(kMIDIListDevices, 0, i, 0); }
ItemCount MIDIGetNumberOfExternalDevices(void) { return 0; }
MIDIDeviceRef MIDIGetExternalDevice(ItemCount i) { return 0; }
ItemCount MIDIDeviceGetNumberOfEntities(MIDIDeviceRef device) { return listing(kMIDIListEntities, device, 0, 1); }
MIDIEntityRef MIDIDeviceGetEntity(MIDIDeviceRef device, ItemCount i) { return listing(kMIDIListEntities, device, i, 0); }
ItemCount MIDIEntityGetNumberOfSources(MIDIEntityRef entity) { return listing(kMIDIListEntitySources, entity, 0, 1); }
MIDIEndpointRef MIDIEntityGetSource(MIDIEntityRef entity, ItemCount i) { return listing(kMIDIListEntitySources, entity, i, 0); }
ItemCount MIDIEntityGetNumberOfDestinations(MIDIEntityRef entity) { return listing(kMIDIListEntityDestinations, entity, 0, 1); }
MIDIEndpointRef MIDIEntityGetDestination(MIDIEntityRef entity, ItemCount i) {
    return listing(kMIDIListEntityDestinations, entity, i, 0);
}
ItemCount MIDIGetNumberOfSources(void) { return listing(kMIDIListSources, 0, 0, 1); }
MIDIEndpointRef MIDIGetSource(ItemCount i) { return listing(kMIDIListSources, 0, i, 0); }
ItemCount MIDIGetNumberOfDestinations(void) { return listing(kMIDIListDestinations, 0, 0, 1); }
MIDIEndpointRef MIDIGetDestination(ItemCount i) { return listing(kMIDIListDestinations, 0, i, 0); }

static OSStatus parentOf(MIDIObjectRef object, MIDIObjectType expected, MIDIObjectRef *outParent) {
    MIDIServerMessage *reply = newReply();
    if (outParent == NULL)
        return paramErr;
    *outParent = 0;
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpGetParent, object, 0, 0, NULL, 0, reply);
    if (status == noErr) {
        MIDIObjectType type = (MIDIObjectType)reply->b;
        if (expected == kMIDIObjectType_Entity && type != kMIDIObjectType_Entity)
            status = kMIDIObjectNotFound;
        else if (expected == kMIDIObjectType_Source && type != kMIDIObjectType_Source &&
                 type != kMIDIObjectType_Destination)
            status = kMIDIWrongEndpointType;
        else
            *outParent = reply->a;
    }
    free(reply);
    return status;
}

OSStatus MIDIEntityGetDevice(MIDIEntityRef inEntity, MIDIDeviceRef *outDevice) {
    return parentOf(inEntity, kMIDIObjectType_Entity, outDevice);
}

/* A virtual endpoint has no entity: 0, without an error. */
OSStatus MIDIEndpointGetEntity(MIDIEndpointRef inEndpoint, MIDIEntityRef *outEntity) {
    return parentOf(inEndpoint, kMIDIObjectType_Source, outEntity);
}

OSStatus MIDISourceCreate(MIDIClientRef client, CFStringRef name, MIDIEndpointRef *outSrc) {
    if (outSrc == NULL)
        return paramErr;
    *outSrc = 0;
    return createNamed(kMIDIOpCreateEndpoint, client, kMIDIObjectType_Source, name, outSrc);
}

static OSStatus destinationCreate(MIDIClientRef client, CFStringRef name, void *proc, void *refCon, void *block,
                                  MIDIEndpointRef *outDest) {
    MIDIEndpointRef dest = 0;
    if (outDest == NULL)
        return paramErr;
    /* Known here before the server tells anyone of it: packets sent to it
       at once are not lost. */
    OSStatus status = createNamed(kMIDIOpCreateEndpoint, client, kMIDIObjectType_Destination, name, &dest);
    if (status == noErr)
        addLocal(dest, kLocalDestination, proc, refCon, block);
    *outDest = dest;
    return status;
}

OSStatus MIDIDestinationCreate(MIDIClientRef client, CFStringRef name, MIDIReadProc readProc, void *refCon,
                               MIDIEndpointRef *outDest) {
    return destinationCreate(client, name, (void *)readProc, refCon, NULL, outDest);
}

OSStatus MIDIDestinationCreateWithBlock(MIDIClientRef client, CFStringRef name, MIDIEndpointRef *outDest,
                                        MIDIReadBlock readBlock) {
    return destinationCreate(client, name, NULL, NULL, (void *)readBlock, outDest);
}

OSStatus MIDIEndpointDispose(MIDIEndpointRef endpt) {
    OSStatus status = dispose(endpt);
    return status == kMIDIObjectNotFound ? kMIDIUnknownEndpoint : status;
}

/* ---- Properties ---- */

/* The property's value (+1), or NULL with *status set. */
static CFPropertyListRef getProperty(MIDIObjectRef obj, CFStringRef key, OSStatus *status) {
    size_t length;
    char *bytes = utf8(key, &length);
    MIDIServerMessage *reply = newReply();
    CFPropertyListRef value = NULL;
    if (bytes == NULL || reply == NULL) {
        free(bytes);
        free(reply);
        *status = memFullErr;
        return NULL;
    }
    *status = call(kMIDIOpGetProperty, obj, 0, 0, bytes, length, reply);
    if (*status == noErr) {
        CFDataRef data = CFDataCreate(NULL, reply->payload, (CFIndex)reply->length);
        value = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
        CFRelease(data);
        if (value == NULL)
            *status = kMIDIUnknownError;
    }
    free(bytes);
    free(reply);
    return value;
}

static OSStatus setProperty(MIDIObjectRef obj, CFStringRef key, CFPropertyListRef value) {
    size_t length;
    char *bytes = utf8(key, &length);
    CFDataRef data = value != NULL ? CFPropertyListCreateData(NULL, value, kCFPropertyListBinaryFormat_v1_0, 0, NULL) : NULL;
    size_t total = length + (data != NULL ? (size_t)CFDataGetLength(data) : 0);
    uint8_t *payload = malloc(total + 1);
    MIDIServerMessage *reply = newReply();
    OSStatus status;
    if (bytes == NULL || payload == NULL || reply == NULL || (value != NULL && data == NULL)) {
        status = value != NULL && data == NULL ? kMIDIWrongPropertyType : memFullErr;
    } else {
        memcpy(payload, bytes, length);
        if (data != NULL)
            memcpy(payload + length, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
        status = call(kMIDIOpSetProperty, obj, (uint32_t)length, 0, payload, total, reply);
    }
    if (data != NULL)
        CFRelease(data);
    free(bytes);
    free(payload);
    free(reply);
    return status;
}

static OSStatus getTyped(MIDIObjectRef obj, CFStringRef key, CFTypeID type, CFTypeRef *out) {
    OSStatus status;
    if (out == NULL || key == NULL)
        return paramErr;
    *out = NULL;
    CFPropertyListRef value = getProperty(obj, key, &status);
    if (value == NULL)
        return status;
    if (CFGetTypeID(value) != type) {
        CFRelease(value);
        return kMIDIWrongPropertyType;
    }
    *out = value;
    return noErr;
}

OSStatus MIDIObjectGetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 *outValue) {
    CFTypeRef value;
    if (outValue == NULL)
        return paramErr;
    OSStatus status = getTyped(obj, propertyID, CFNumberGetTypeID(), &value);
    if (status == noErr) {
        CFNumberGetValue(value, kCFNumberSInt32Type, outValue);
        CFRelease(value);
    }
    return status;
}

OSStatus MIDIObjectSetIntegerProperty(MIDIObjectRef obj, CFStringRef propertyID, SInt32 value) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    OSStatus status = setProperty(obj, propertyID, n);
    CFRelease(n);
    return status;
}

OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef *str) {
    return getTyped(obj, propertyID, CFStringGetTypeID(), (CFTypeRef *)str);
}

OSStatus MIDIObjectSetStringProperty(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef str) {
    return str != NULL ? setProperty(obj, propertyID, str) : paramErr;
}

OSStatus MIDIObjectGetDataProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDataRef *outData) {
    return getTyped(obj, propertyID, CFDataGetTypeID(), (CFTypeRef *)outData);
}

OSStatus MIDIObjectSetDataProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDataRef data) {
    return data != NULL ? setProperty(obj, propertyID, data) : paramErr;
}

OSStatus MIDIObjectGetDictionaryProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDictionaryRef *outDict) {
    return getTyped(obj, propertyID, CFDictionaryGetTypeID(), (CFTypeRef *)outDict);
}

OSStatus MIDIObjectSetDictionaryProperty(MIDIObjectRef obj, CFStringRef propertyID, CFDictionaryRef dict) {
    return dict != NULL ? setProperty(obj, propertyID, dict) : paramErr;
}

OSStatus MIDIObjectRemoveProperty(MIDIObjectRef obj, CFStringRef propertyID) {
    return propertyID != NULL ? setProperty(obj, propertyID, NULL) : paramErr;
}

OSStatus MIDIObjectGetProperties(MIDIObjectRef obj, CFPropertyListRef *outProperties, Boolean deep) {
    MIDIServerMessage *reply = newReply();
    if (outProperties == NULL)
        return paramErr;
    *outProperties = NULL;
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpGetProperties, obj, deep ? 1 : 0, 0, NULL, 0, reply);
    if (status == noErr) {
        CFDataRef data = CFDataCreate(NULL, reply->payload, (CFIndex)reply->length);
        *outProperties = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
        CFRelease(data);
        if (*outProperties == NULL)
            status = kMIDIUnknownError;
    }
    free(reply);
    return status;
}

OSStatus MIDIObjectFindByUniqueID(MIDIUniqueID inUniqueID, MIDIObjectRef *outObject, MIDIObjectType *outObjectType) {
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpFindUniqueID, (uint32_t)inUniqueID, 0, 0, NULL, 0, reply);
    if (outObject != NULL)
        *outObject = status == noErr ? reply->a : 0;
    if (outObjectType != NULL)
        *outObjectType = status == noErr ? (MIDIObjectType)reply->b : kMIDIObjectType_Other;
    free(reply);
    return status;
}

/* ---- I/O ---- */

static OSStatus sendList(uint32_t op, uint32_t a, uint32_t b, const MIDIPacketList *pktlist) {
    if (pktlist == NULL)
        return paramErr;
    size_t size = packetListSize(pktlist);
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(op, a, b, 0, pktlist, size, reply);
    free(reply);
    return status;
}

OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest, const MIDIPacketList *pktlist) {
    return sendList(kMIDIOpSend, port, dest, pktlist);
}

OSStatus MIDIReceived(MIDIEndpointRef src, const MIDIPacketList *pktlist) {
    return sendList(kMIDIOpReceived, src, 0, pktlist);
}

OSStatus MIDIFlushOutput(MIDIEndpointRef dest) {
    return noErr;
}

OSStatus MIDIRestart(void) {
    MIDIServerMessage *reply = newReply();
    if (reply == NULL)
        return memFullErr;
    OSStatus status = call(kMIDIOpRestart, 0, 0, 0, NULL, 0, reply);
    free(reply);
    return status;
}

/* A system-exclusive message sent in the background, in packets, through
   an output port of CoreMIDI's own; complete set, then the completion
   proc called. */
OSStatus MIDISendSysex(MIDISysexSendRequest *request) {
    static MIDIClientRef client;
    static MIDIPortRef port;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    OSStatus status = noErr;
    if (request == NULL || (request->data == NULL && request->bytesToSend > 0))
        return paramErr;
    pthread_mutex_lock(&lock);
    if (port == 0 && (status = MIDIClientCreate(CFSTR("SysEx"), NULL, NULL, &client)) == noErr)
        status = MIDIOutputPortCreate(client, CFSTR("SysEx"), &port);
    pthread_mutex_unlock(&lock);
    if (status != noErr)
        return status;
    request->complete = false;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        uint8_t buffer[1024];
        MIDIPacketList *list = (MIDIPacketList *)buffer;
        while (request->bytesToSend > 0 && !request->complete) {
            UInt32 chunk = request->bytesToSend > 256 ? 256 : request->bytesToSend;
            MIDIPacket *packet = MIDIPacketListInit(list);
            MIDIPacketListAdd(list, sizeof(buffer), packet, 0, chunk, request->data);
            if (MIDISend(port, request->destination, list) != noErr)
                break;
            request->data += chunk;
            request->bytesToSend -= chunk;
        }
        request->complete = true;
        if (request->completionProc != NULL)
            request->completionProc(request);
    });
    return noErr;
}

MIDIPacket *MIDIPacketListInit(MIDIPacketList *pktlist) {
    pktlist->numPackets = 0;
    return &pktlist->packet[0];
}

/* Appends to the packet when it has the same time and room (not across a
   system-exclusive message), else starts a new one after it; NULL when
   the list has no room left. */
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *pktlist, ByteCount listSize, MIDIPacket *curPacket,
                              MIDITimeStamp time, ByteCount nData, const Byte *data) {
    uintptr_t end = (uintptr_t)pktlist + listSize;
    if (pktlist->numPackets > 0 && curPacket->timeStamp == time && nData > 0 && data[0] < 0xf0 &&
        curPacket->length > 0 && curPacket->data[0] < 0xf0 &&
        (uintptr_t)&curPacket->data[curPacket->length + nData] <= end && curPacket->length + nData <= 65535) {
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
