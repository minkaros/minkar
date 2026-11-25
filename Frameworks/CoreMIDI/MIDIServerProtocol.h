/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The messages between CoreMIDI and the MIDI server (midiserver, the
 * Mach service below). Private to them.
 *
 * A process says hello once, giving the server a send right to a port of
 * its own: the server sends it notifications and the packets for its
 * input ports and virtual destinations there. Every other request is a
 * message with a reply port; the reply has the same layout, its status
 * and values filled in. Values travel as binary property lists; packet
 * lists as their bytes.
 */

#ifndef __MIDISERVERPROTOCOL__
#define __MIDISERVERPROTOCOL__

#include <mach/mach.h>
#include <stdint.h>

#define kMIDIServerName "org.minkar.midiserver"

enum {
    /* Requests. a, b, c: refs or numbers; the payload as named. */
    kMIDIOpHello = 1,           /* (complex: the client's port) pid in a */
    kMIDIOpCreateClient,        /* payload: name (UTF-8) -> a: client */
    kMIDIOpCreatePort,          /* a: client, b: 1 input / 0 output, payload: name -> a: port */
    kMIDIOpCreateEndpoint,      /* a: client, b: object type (source/destination), payload: name -> a: endpoint */
    kMIDIOpDispose,             /* a: client, port or endpoint */
    kMIDIOpConnect,             /* a: port, b: source, payload: the connection refCon (8 bytes) */
    kMIDIOpDisconnect,          /* a: port, b: source */
    kMIDIOpList,                /* a: kMIDIList..., b: parent -> payload: refs (uint32_t) */
    kMIDIOpGetParent,           /* a: object -> a: parent, b: object type, c: parent's type */
    kMIDIOpGetProperty,         /* a: object, payload: key (UTF-8) -> payload: value (plist) */
    kMIDIOpSetProperty,         /* a: object, b: key length, payload: key then value (plist; none: remove) */
    kMIDIOpGetProperties,       /* a: object, b: deep -> payload: dictionary (plist) */
    kMIDIOpFindUniqueID,        /* a: unique ID -> a: object, b: type */
    kMIDIOpSend,                /* a: port, b: destination, payload: packet list */
    kMIDIOpReceived,            /* a: source, payload: packet list */
    kMIDIOpRestart,

    /* Sent by the server to a process's port. */
    kMIDIOpNotify = 100,        /* a: message ID, b: parent/object, c: parent's/object's type;
                                   payload: child ref and type (add/remove) or the property's key */
    kMIDIOpPortInput,           /* a: port, b: source, payload: refCon (8 bytes) then the packet list */
    kMIDIOpDestinationInput,    /* a: destination, payload: the packet list */
};

enum {
    kMIDIListDevices = 0,
    kMIDIListSources,
    kMIDIListDestinations,
    kMIDIListEntities,            /* of device b */
    kMIDIListEntitySources,       /* of entity b */
    kMIDIListEntityDestinations,  /* of entity b */
    kMIDIListExternalDevices
};

/* Most a message carries: a packet list or a property value. */
#define kMIDIMaxPayload (65536 + 1024)

typedef struct {
    mach_msg_header_t header;
    uint32_t op;
    int32_t status;
    uint32_t a, b, c;
    uint32_t length;
    uint8_t payload[kMIDIMaxPayload];
} MIDIServerMessage;

/* The hello: the client's port as a descriptor. */
typedef struct {
    mach_msg_header_t header;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t port;
    uint32_t op;
    uint32_t pid;
} MIDIServerHello;

/* A message's size for a payload of length bytes. */
#define MIDIServerMessageSize(length) \
    ((mach_msg_size_t)((offsetof(MIDIServerMessage, payload) + (length) + 3) & ~(size_t)3))

#endif
