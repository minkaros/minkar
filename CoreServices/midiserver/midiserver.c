/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * midiserver, the MIDI server (AU8): the system's MIDI setup and its
 * traffic between processes, for CoreMIDI (MIDIServerProtocol.h).
 *
 * It keeps the objects: the clients and ports processes make, the
 * devices with their entities and endpoints (the IAC Driver's bus, a
 * source and a destination that loop back to each other, offline until
 * set online, as on the reference), and the virtual sources and
 * destinations processes make. Each has its properties and a unique ID.
 * It routes packet lists: what is sent to a virtual destination goes to
 * the process that made it; what a source receives (a virtual source's
 * MIDIReceived, the IAC bus's loopback) goes to every port connected to
 * it. Packets go at once, their time stamps kept: as on the reference, a
 * virtual destination schedules what it receives itself. Processes are
 * told of objects added and removed and properties changed. A process
 * that ends takes its objects with it.
 *
 * The IAC Driver's online state is kept in
 * /Library/Preferences/org.minkar.midiserver.plist.
 */

#include "MIDIServerProtocol.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/MIDIServices.h>
#include <bsm/libbsm.h>
#include <mach/mach.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define PREFS "/Library/Preferences/org.minkar.midiserver.plist"

enum { kKindClient = 100, kKindInputPort, kKindOutputPort };

typedef struct Object {
    MIDIObjectRef ref;
    int kind;                       /* kMIDIObjectType_... or kKind... */
    MIDIObjectRef parent;           /* entity, device, or the client of a port or virtual endpoint */
    pid_t owner;                    /* 0: the server's own (a driver's) */
    CFMutableDictionaryRef props;
} Object;

typedef struct Connection {
    MIDIObjectRef port, source;
    uint64_t refCon;
} Connection;

typedef struct Process {
    pid_t pid;
    mach_port_t port;
} Process;

static Object **objects;
static size_t objectCount;
static Connection *connections;
static size_t connectionCount;
static Process *processes;
static size_t processCount;
static MIDIObjectRef nextRef;
static mach_port_t service;
static MIDIObjectRef iacDevice, iacSource, iacDestination;

/* ---- Objects ---- */

static Object *find(MIDIObjectRef ref) {
    for (size_t i = 0; i < objectCount; i++)
        if (objects[i]->ref == ref)
            return objects[i];
    return NULL;
}

static Object *findUniqueID(SInt32 uid) {
    for (size_t i = 0; i < objectCount; i++) {
        CFNumberRef n = CFDictionaryGetValue(objects[i]->props, kMIDIPropertyUniqueID);
        SInt32 v;
        if (n != NULL && CFNumberGetValue(n, kCFNumberSInt32Type, &v) && v == uid)
            return objects[i];
    }
    return NULL;
}

static void setInt(Object *o, CFStringRef key, SInt32 value) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(o->props, key, n);
    CFRelease(n);
}

static SInt32 getInt(Object *o, CFStringRef key) {
    CFNumberRef n = o != NULL ? CFDictionaryGetValue(o->props, key) : NULL;
    SInt32 v = 0;
    if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
        CFNumberGetValue(n, kCFNumberSInt32Type, &v);
    return v;
}

static Object *add(int kind, MIDIObjectRef parent, pid_t owner, CFStringRef name) {
    Object *o = calloc(1, sizeof(Object));
    Object **grown = realloc(objects, (objectCount + 1) * sizeof(Object *));
    if (o == NULL || grown == NULL) {
        free(o);
        return NULL;
    }
    objects = grown;
    o->ref = nextRef++;
    o->kind = kind;
    o->parent = parent;
    o->owner = owner;
    o->props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (name != NULL)
        CFDictionarySetValue(o->props, kMIDIPropertyName, name);
    if (kind >= kMIDIObjectType_Device && kind <= kMIDIObjectType_Destination) {
        SInt32 uid;
        do
            uid = (SInt32)arc4random();
        while (uid == kMIDIInvalidUniqueID || findUniqueID(uid) != NULL);
        setInt(o, kMIDIPropertyUniqueID, uid);
    }
    objects[objectCount++] = o;
    return o;
}

static void destroy(Object *o) {
    for (size_t i = 0; i < objectCount; i++)
        if (objects[i] == o) {
            memmove(&objects[i], &objects[i + 1], (objectCount - i - 1) * sizeof(Object *));
            objectCount--;
            break;
        }
    CFRelease(o->props);
    free(o);
}

static int isEndpoint(Object *o) {
    return o != NULL && (o->kind == kMIDIObjectType_Source || o->kind == kMIDIObjectType_Destination);
}

static Object *deviceOf(Object *o) {
    while (o != NULL && o->kind != kMIDIObjectType_Device) {
        if (o->kind != kMIDIObjectType_Entity && !isEndpoint(o))
            return NULL;
        o = o->parent != 0 ? find(o->parent) : NULL;
    }
    return o;
}

/* An endpoint of an offline device is not listed. */
static int listed(Object *o) {
    Object *device = isEndpoint(o) && find(o->parent) != NULL && find(o->parent)->kind == kMIDIObjectType_Entity
                         ? deviceOf(o) : NULL;
    return device == NULL || !getInt(device, kMIDIPropertyOffline);
}

/* ---- Processes ---- */

static Process *process(pid_t pid) {
    for (size_t i = 0; i < processCount; i++)
        if (processes[i].pid == pid)
            return &processes[i];
    return NULL;
}

static void push(Process *p, uint32_t op, uint32_t a, uint32_t b, uint32_t c, const void *payload, size_t length) {
    static MIDIServerMessage m;
    if (p == NULL || p->port == MACH_PORT_NULL || length > kMIDIMaxPayload)
        return;
    memset(&m.header, 0, sizeof(m.header));
    m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    m.header.msgh_remote_port = p->port;
    m.header.msgh_size = MIDIServerMessageSize(length);
    m.op = op;
    m.status = 0;
    m.a = a;
    m.b = b;
    m.c = c;
    m.length = (uint32_t)length;
    if (length > 0)
        memcpy(m.payload, payload, length);
    mach_msg_return_t mr = mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, m.header.msgh_size, 0,
                                    MACH_PORT_NULL, 100, MACH_PORT_NULL);
    if (mr != MACH_MSG_SUCCESS)
        syslog(LOG_WARNING, "process %d: message 0x%x not sent: 0x%x", p->pid, op, mr);
}

static void notifyAll(uint32_t messageID, uint32_t a, uint32_t b, const void *payload, size_t length) {
    for (size_t i = 0; i < processCount; i++)
        push(&processes[i], kMIDIOpNotify, (uint32_t)messageID, a, b, payload, length);
}

static MIDIObjectType publicType(Object *o) {
    return o == NULL ? kMIDIObjectType_Other : o->kind <= kMIDIObjectType_Destination ? o->kind : kMIDIObjectType_Other;
}

static void notifyAddRemove(uint32_t messageID, Object *o) {
    Object *parent = o->parent != 0 && o->kind <= kMIDIObjectType_Destination ? find(o->parent) : NULL;
    uint32_t child[2] = { o->ref, (uint32_t)publicType(o) };
    if (parent != NULL && parent->kind >= kKindClient)
        parent = NULL;   /* a virtual endpoint's parent (its client) is not said */
    notifyAll(messageID, parent != NULL ? parent->ref : 0, (uint32_t)publicType(parent), child, sizeof(child));
    notifyAll(kMIDIMsgSetupChanged, 0, 0, NULL, 0);
}

static void notifyProperty(Object *o, CFStringRef key) {
    char name[256];
    if (!CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8))
        return;
    notifyAll(kMIDIMsgPropertyChanged, o->ref, (uint32_t)publicType(o), name, strlen(name) + 1);
}

/* ---- Routing ---- */

static void deliverFromSource(Object *source, const void *packets, size_t length) {
    uint8_t buffer[8 + kMIDIMaxPayload];
    if (length > kMIDIMaxPayload - 8)
        return;
    for (size_t i = 0; i < connectionCount; i++) {
        if (connections[i].source != source->ref)
            continue;
        Object *port = find(connections[i].port);
        if (port == NULL)
            continue;
        memcpy(buffer, &connections[i].refCon, 8);
        memcpy(buffer + 8, packets, length);
        push(process(port->owner), kMIDIOpPortInput, port->ref, source->ref, 0, buffer, 8 + length);
    }
}

/* ---- Removal ---- */

static void removeObject(Object *o, int tell);

static void removeConnectionsOf(MIDIObjectRef ref) {
    size_t j = 0;
    for (size_t i = 0; i < connectionCount; i++)
        if (connections[i].port != ref && connections[i].source != ref)
            connections[j++] = connections[i];
    connectionCount = j;
}

static void removeObject(Object *o, int tell) {
    MIDIObjectRef ref = o->ref;
    /* What belongs to it first (a client's ports and endpoints). */
    for (size_t i = 0; i < objectCount;) {
        if (objects[i]->parent == ref && objects[i]->kind != kMIDIObjectType_Entity) {
            removeObject(objects[i], tell);
            i = 0;
        } else
            i++;
    }
    removeConnectionsOf(ref);
    if (tell && isEndpoint(o))
        notifyAddRemove(kMIDIMsgObjectRemoved, o);
    destroy(o);
}

static void processEnded(pid_t pid) {
    for (size_t i = 0; i < objectCount;) {
        if (objects[i]->owner == pid && objects[i]->kind == kKindClient) {
            removeObject(objects[i], 1);
            i = 0;
        } else
            i++;
    }
    for (size_t i = 0; i < processCount; i++)
        if (processes[i].pid == pid) {
            mach_port_deallocate(mach_task_self(), processes[i].port);
            processes[i] = processes[--processCount];
            break;
        }
}

/* ---- Properties ---- */

static CFDataRef encode(CFPropertyListRef value) {
    return value != NULL ? CFPropertyListCreateData(NULL, value, kCFPropertyListBinaryFormat_v1_0, 0, NULL) : NULL;
}

static CFStringRef displayName(Object *o) {
    CFStringRef name = CFDictionaryGetValue(o->props, kMIDIPropertyName);
    Object *device = isEndpoint(o) ? deviceOf(o) : NULL;
    Object *entity = isEndpoint(o) ? find(o->parent) : NULL;
    if (device != NULL && entity != NULL && entity->kind == kMIDIObjectType_Entity) {
        CFStringRef d = CFDictionaryGetValue(device->props, kMIDIPropertyName);
        CFStringRef e = CFDictionaryGetValue(entity->props, kMIDIPropertyName);
        if (d != NULL && e != NULL)
            return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ %@"), d, e);
    }
    return name != NULL ? CFRetain(name) : CFSTR("");
}

static CFDictionaryRef propertiesOf(Object *o, int deep) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, o->props);
    if (!CFDictionaryContainsKey(d, kMIDIPropertyDisplayName) && o->kind <= kMIDIObjectType_Destination) {
        CFStringRef name = displayName(o);
        CFDictionarySetValue(d, kMIDIPropertyDisplayName, name);
        CFRelease(name);
    }
    if (deep && (o->kind == kMIDIObjectType_Device || o->kind == kMIDIObjectType_Entity)) {
        CFMutableArrayRef children[3] = {
            CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks),
            CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks) };
        for (size_t i = 0; i < objectCount; i++) {
            Object *c = objects[i];
            if (c->parent != o->ref || c->kind > kMIDIObjectType_Destination)
                continue;
            CFDictionaryRef sub = propertiesOf(c, deep);
            CFArrayAppendValue(children[c->kind == kMIDIObjectType_Entity ? 0 : c->kind == kMIDIObjectType_Source ? 1 : 2], sub);
            CFRelease(sub);
        }
        if (o->kind == kMIDIObjectType_Device)
            CFDictionarySetValue(d, CFSTR("entities"), children[0]);
        else {
            CFDictionarySetValue(d, CFSTR("sources"), children[1]);
            CFDictionarySetValue(d, CFSTR("destinations"), children[2]);
        }
        for (int i = 0; i < 3; i++)
            CFRelease(children[i]);
    }
    return d;
}

static void saveIAC(void) {
    SInt32 online = !getInt(find(iacDevice), kMIDIPropertyOffline);
    CFStringRef keys[] = { CFSTR("IACOnline") };
    CFBooleanRef values[] = { online ? kCFBooleanTrue : kCFBooleanFalse };
    CFDictionaryRef prefs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef data = CFPropertyListCreateData(NULL, prefs, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    FILE *f = fopen(PREFS, "w");
    if (f != NULL && data != NULL) {
        fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f);
        fclose(f);
    }
    if (data != NULL)
        CFRelease(data);
    CFRelease(prefs);
}

static int iacOnlineSaved(void) {
    FILE *f = fopen(PREFS, "r");
    char buffer[4096];
    size_t n = f != NULL ? fread(buffer, 1, sizeof(buffer), f) : 0;
    int online = 0;
    if (f != NULL)
        fclose(f);
    if (n > 0) {
        CFDataRef data = CFDataCreate(NULL, (const UInt8 *)buffer, (CFIndex)n);
        CFPropertyListRef p = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
        if (p != NULL && CFGetTypeID(p) == CFDictionaryGetTypeID())
            online = CFDictionaryGetValue(p, CFSTR("IACOnline")) == kCFBooleanTrue;
        if (p != NULL)
            CFRelease(p);
        CFRelease(data);
    }
    return online;
}

/* The IAC Driver: one bus, its source and destination looped back. */
static void addIAC(void) {
    Object *device = add(kMIDIObjectType_Device, 0, 0, CFSTR("IAC Driver"));
    Object *entity = add(kMIDIObjectType_Entity, device->ref, 0, CFSTR("Bus 1"));
    Object *source = add(kMIDIObjectType_Source, entity->ref, 0, CFSTR("Bus 1"));
    Object *destination = add(kMIDIObjectType_Destination, entity->ref, 0, CFSTR("Bus 1"));
    CFDictionarySetValue(device->props, kMIDIPropertyManufacturer, CFSTR("Minkar"));
    CFDictionarySetValue(device->props, kMIDIPropertyModel, CFSTR("IAC Driver"));
    CFDictionarySetValue(device->props, kMIDIPropertyDriverOwner, CFSTR("org.minkar.midiserver.IAC"));
    setInt(device, kMIDIPropertyOffline, !iacOnlineSaved());
    setInt(entity, kMIDIPropertyIsEmbeddedEntity, 0);
    iacDevice = device->ref;
    iacSource = source->ref;
    iacDestination = destination->ref;
}

/* ---- Requests ---- */

static CFStringRef stringOf(const uint8_t *bytes, size_t length) {
    return CFStringCreateWithBytes(NULL, bytes, (CFIndex)length, kCFStringEncodingUTF8, false);
}

static void handle(MIDIServerMessage *m, pid_t pid, MIDIServerMessage *r) {
    Object *o;
    r->status = noErr;
    r->a = r->b = r->c = 0;
    r->length = 0;
    switch (m->op) {
    case kMIDIOpCreateClient: {
        CFStringRef name = stringOf(m->payload, m->length);
        o = add(kKindClient, 0, pid, name);
        CFRelease(name);
        r->a = o != NULL ? o->ref : 0;
        r->status = o != NULL ? noErr : kMIDIUnknownError;
        break;
    }
    case kMIDIOpCreatePort: {
        Object *client = find(m->a);
        if (client == NULL || client->kind != kKindClient) {
            r->status = kMIDIInvalidClient;
            break;
        }
        CFStringRef name = stringOf(m->payload, m->length);
        o = add(m->b ? kKindInputPort : kKindOutputPort, client->ref, pid, name);
        CFRelease(name);
        r->a = o != NULL ? o->ref : 0;
        break;
    }
    case kMIDIOpCreateEndpoint: {
        Object *client = find(m->a);
        if (client == NULL || client->kind != kKindClient) {
            r->status = kMIDIInvalidClient;
            break;
        }
        if (m->b != kMIDIObjectType_Source && m->b != kMIDIObjectType_Destination) {
            r->status = kMIDIWrongEndpointType;
            break;
        }
        CFStringRef name = stringOf(m->payload, m->length);
        o = add((int)m->b, client->ref, pid, name);
        CFRelease(name);
        if (o == NULL) {
            r->status = kMIDIUnknownError;
            break;
        }
        r->a = o->ref;
        notifyAddRemove(kMIDIMsgObjectAdded, o);
        break;
    }
    case kMIDIOpDispose:
        o = find(m->a);
        if (o == NULL || o->owner != pid || o->kind == kMIDIObjectType_Device || o->kind == kMIDIObjectType_Entity) {
            r->status = o == NULL ? kMIDIObjectNotFound : kMIDINotPermitted;
            break;
        }
        removeObject(o, 1);
        break;
    case kMIDIOpConnect: {
        Object *port = find(m->a), *source = find(m->b);
        Connection *grown;
        if (port == NULL || port->kind != kKindInputPort) {
            r->status = kMIDIInvalidPort;
            break;
        }
        if (source == NULL || source->kind != kMIDIObjectType_Source) {
            r->status = kMIDIUnknownEndpoint;
            break;
        }
        grown = realloc(connections, (connectionCount + 1) * sizeof(Connection));
        if (grown == NULL) {
            r->status = kMIDIUnknownError;
            break;
        }
        connections = grown;
        connections[connectionCount].port = port->ref;
        connections[connectionCount].source = source->ref;
        connections[connectionCount].refCon = 0;
        if (m->length >= 8)
            memcpy(&connections[connectionCount].refCon, m->payload, 8);
        connectionCount++;
        break;
    }
    case kMIDIOpDisconnect: {
        size_t j = 0;
        int found = 0;
        for (size_t i = 0; i < connectionCount; i++) {
            if (connections[i].port == m->a && connections[i].source == m->b) {
                found = 1;
                continue;
            }
            connections[j++] = connections[i];
        }
        connectionCount = j;
        r->status = found ? noErr : kMIDINoConnection;
        break;
    }
    case kMIDIOpList: {
        uint32_t *refs = (uint32_t *)r->payload;
        size_t n = 0;
        for (size_t i = 0; i < objectCount && n < kMIDIMaxPayload / 4; i++) {
            o = objects[i];
            int take = 0;
            switch (m->a) {
            case kMIDIListDevices: take = o->kind == kMIDIObjectType_Device; break;
            case kMIDIListSources: take = o->kind == kMIDIObjectType_Source && listed(o); break;
            case kMIDIListDestinations: take = o->kind == kMIDIObjectType_Destination && listed(o); break;
            case kMIDIListEntities: take = o->kind == kMIDIObjectType_Entity && o->parent == m->b; break;
            case kMIDIListEntitySources: take = o->kind == kMIDIObjectType_Source && o->parent == m->b; break;
            case kMIDIListEntityDestinations: take = o->kind == kMIDIObjectType_Destination && o->parent == m->b; break;
            default: break;
            }
            if (take)
                refs[n++] = o->ref;
        }
        r->length = (uint32_t)(n * 4);
        break;
    }
    case kMIDIOpGetParent:
        o = find(m->a);
        if (o == NULL) {
            r->status = kMIDIObjectNotFound;
            break;
        }
        {
            Object *parent = o->parent != 0 ? find(o->parent) : NULL;
            if (parent != NULL && parent->kind >= kKindClient)
                parent = NULL;
            r->a = parent != NULL ? parent->ref : 0;
            r->b = (uint32_t)publicType(o);
            r->c = (uint32_t)publicType(parent);
        }
        break;
    case kMIDIOpGetProperty: {
        o = find(m->a);
        if (o == NULL) {
            r->status = kMIDIObjectNotFound;
            break;
        }
        CFStringRef key = stringOf(m->payload, m->length);
        CFPropertyListRef value = CFDictionaryGetValue(o->props, key);
        CFStringRef computed = NULL;
        if (value == NULL && CFEqual(key, kMIDIPropertyDisplayName))
            value = computed = displayName(o);
        CFDataRef data = encode(value);
        if (data == NULL)
            r->status = kMIDIUnknownProperty;
        else if (CFDataGetLength(data) <= kMIDIMaxPayload) {
            memcpy(r->payload, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
            r->length = (uint32_t)CFDataGetLength(data);
        } else
            r->status = kMIDIUnknownError;
        if (data != NULL)
            CFRelease(data);
        if (computed != NULL)
            CFRelease(computed);
        CFRelease(key);
        break;
    }
    case kMIDIOpSetProperty: {
        o = find(m->a);
        if (o == NULL) {
            r->status = kMIDIObjectNotFound;
            break;
        }
        if (m->b > m->length) {
            r->status = kMIDIUnknownError;
            break;
        }
        CFStringRef key = stringOf(m->payload, m->b);
        /* A unique ID is the server's to give (a process may set its
           virtual endpoint's own, if no one has it); the other properties
           of the server's objects only for the IAC Driver's online state. */
        if (o->owner != pid && !(o->ref == iacDevice && CFEqual(key, kMIDIPropertyOffline)) && pid != 0) {
            r->status = kMIDINotPermitted;
            CFRelease(key);
            break;
        }
        if (m->length == m->b) {
            CFDictionaryRemoveValue(o->props, key);
        } else {
            CFDataRef data = CFDataCreate(NULL, m->payload + m->b, (CFIndex)(m->length - m->b));
            CFPropertyListRef value = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
            CFRelease(data);
            if (value == NULL) {
                r->status = kMIDIWrongPropertyType;
                CFRelease(key);
                break;
            }
            if (CFEqual(key, kMIDIPropertyUniqueID)) {
                SInt32 uid = 0;
                Object *other;
                if (CFGetTypeID(value) != CFNumberGetTypeID() ||
                    !CFNumberGetValue(value, kCFNumberSInt32Type, &uid) || uid == kMIDIInvalidUniqueID ||
                    ((other = findUniqueID(uid)) != NULL && other != o)) {
                    r->status = kMIDIIDNotUnique;
                    CFRelease(value);
                    CFRelease(key);
                    break;
                }
            }
            CFDictionarySetValue(o->props, key, value);
            CFRelease(value);
        }
        notifyProperty(o, key);
        if (o->ref == iacDevice && CFEqual(key, kMIDIPropertyOffline)) {
            saveIAC();
            notifyAll(kMIDIMsgSetupChanged, 0, 0, NULL, 0);
        }
        CFRelease(key);
        break;
    }
    case kMIDIOpGetProperties: {
        o = find(m->a);
        if (o == NULL) {
            r->status = kMIDIObjectNotFound;
            break;
        }
        CFDictionaryRef props = propertiesOf(o, (int)m->b);
        CFDataRef data = encode(props);
        if (data != NULL && CFDataGetLength(data) <= kMIDIMaxPayload) {
            memcpy(r->payload, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
            r->length = (uint32_t)CFDataGetLength(data);
        } else
            r->status = kMIDIUnknownError;
        if (data != NULL)
            CFRelease(data);
        CFRelease(props);
        break;
    }
    case kMIDIOpFindUniqueID:
        o = findUniqueID((SInt32)m->a);
        if (o == NULL)
            r->status = kMIDIObjectNotFound;
        else {
            r->a = o->ref;
            r->b = (uint32_t)publicType(o);
        }
        break;
    case kMIDIOpSend: {
        Object *port = find(m->a), *destination = find(m->b);
        if (port == NULL || port->kind != kKindOutputPort) {
            r->status = kMIDIInvalidPort;
            break;
        }
        if (destination == NULL || destination->kind != kMIDIObjectType_Destination) {
            r->status = kMIDIUnknownEndpoint;
            break;
        }
        if (destination->ref == iacDestination) {
            if (!getInt(find(iacDevice), kMIDIPropertyOffline))
                deliverFromSource(find(iacSource), m->payload, m->length);
        } else if (destination->owner != 0)
            push(process(destination->owner), kMIDIOpDestinationInput, destination->ref, 0, 0, m->payload, m->length);
        break;
    }
    case kMIDIOpReceived: {
        Object *source = find(m->a);
        if (source == NULL || source->kind != kMIDIObjectType_Source) {
            r->status = kMIDIUnknownEndpoint;
            break;
        }
        if (source->owner != pid) {
            r->status = kMIDINotPermitted;
            break;
        }
        deliverFromSource(source, m->payload, m->length);
        break;
    }
    case kMIDIOpRestart:
        break;
    default:
        r->status = kMIDIUnknownError;
        break;
    }
}

static void hello(MIDIServerHello *h, pid_t pid) {
    Process *p = process(pid);
    mach_port_t previous;
    if (p == NULL) {
        Process *grown = realloc(processes, (processCount + 1) * sizeof(Process));
        if (grown == NULL) {
            mach_port_deallocate(mach_task_self(), h->port.name);
            return;
        }
        processes = grown;
        p = &processes[processCount++];
        p->pid = pid;
    } else if (p->port != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), p->port);
    p->port = h->port.name;
    /* Told when it goes. */
    mach_port_request_notification(mach_task_self(), p->port, MACH_NOTIFY_DEAD_NAME, 0, service,
                                   MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    if (previous != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), previous);
}

int main(void) {
    static struct {
        union {
            MIDIServerMessage message;
            MIDIServerHello hello;
            mach_dead_name_notification_t dead;
        };
        mach_msg_audit_trailer_t trailer;
    } request;
    static MIDIServerMessage reply;

    openlog("midiserver", LOG_PID, LOG_DAEMON);
    nextRef = 0x10000 + (arc4random() & 0xffff) * 16;
    if (bootstrap_check_in(bootstrap_port, kMIDIServerName, &service) != KERN_SUCCESS) {
        syslog(LOG_ERR, "cannot check in %s", kMIDIServerName);
        return 1;
    }
    addIAC();
    for (;;) {
        mach_msg_return_t mr = mach_msg(&request.message.header,
            MACH_RCV_MSG | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
                MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
            0, sizeof(request), service, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (mr != MACH_MSG_SUCCESS)
            continue;
        mach_msg_header_t *header = &request.message.header;
        if (header->msgh_id == MACH_NOTIFY_DEAD_NAME) {
            mach_port_t dead = request.dead.not_port;
            for (size_t i = 0; i < processCount; i++)
                if (processes[i].port == dead) {
                    processEnded(processes[i].pid);
                    break;
                }
            mach_port_deallocate(mach_task_self(), dead);
            continue;
        }
        mach_msg_audit_trailer_t *trailer =
            (mach_msg_audit_trailer_t *)((uint8_t *)header + round_msg(header->msgh_size));
        pid_t pid = audit_token_to_pid(trailer->msgh_audit);
        if (header->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            if (request.hello.op == kMIDIOpHello && request.hello.body.msgh_descriptor_count == 1)
                hello(&request.hello, pid);
            else
                mach_msg_destroy(header);
            continue;
        }
        if (header->msgh_size < offsetof(MIDIServerMessage, payload) ||
            request.message.length > header->msgh_size - offsetof(MIDIServerMessage, payload)) {
            mach_msg_destroy(header);
            continue;
        }
        handle(&request.message, pid, &reply);
        if (header->msgh_remote_port != MACH_PORT_NULL) {
            reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(header->msgh_bits), 0);
            reply.header.msgh_remote_port = header->msgh_remote_port;
            reply.header.msgh_local_port = MACH_PORT_NULL;
            reply.header.msgh_size = MIDIServerMessageSize(reply.length);
            reply.header.msgh_id = header->msgh_id + 100;
            reply.op = request.message.op;
            mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, reply.header.msgh_size, 0, MACH_PORT_NULL,
                     1000, MACH_PORT_NULL);
        }
    }
}
