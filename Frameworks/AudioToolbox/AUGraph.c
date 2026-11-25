/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AUGraph (AUGraph.h): a list of nodes and of interactions (connections
 * and input callbacks). Opening makes each node's unit; an interaction is
 * applied to the units at once while the graph is open (a connection is
 * the destination's MakeConnection property, a callback its
 * SetRenderCallback), so AUGraphUpdate has nothing left to do. Starting
 * starts the head node, the graph's one output unit.
 */

#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define kGraphMagic 0x41554752u /* 'AUGR' */

typedef struct Node {
    AUNode id;
    AudioComponentDescription description;
    AudioUnit unit;
} Node;

struct OpaqueAUGraph {
    uint32_t magic;
    pthread_mutex_t lock;
    Node *nodes;
    UInt32 nodeCount;
    AUNodeInteraction *interactions;
    UInt32 interactionCount;
    AUNode nextID;
    AUNode head;
    Boolean open, initialized, running;
    struct {
        AURenderCallback proc;
        void *refCon;
    } notifies[8];
    UInt32 notifyCount;
};

static Boolean valid(AUGraph g) {
    return g != NULL && g->magic == kGraphMagic;
}

static Node *findNode(AUGraph g, AUNode id) {
    for (UInt32 i = 0; i < g->nodeCount; i++)
        if (g->nodes[i].id == id)
            return &g->nodes[i];
    return NULL;
}

OSStatus NewAUGraph(AUGraph *outGraph) {
    AUGraph g;
    if (outGraph == NULL)
        return kAudio_ParamError;
    g = calloc(1, sizeof(*g));
    if (g == NULL)
        return kAudio_MemFullError;
    g->magic = kGraphMagic;
    g->nextID = 1;
    pthread_mutex_init(&g->lock, NULL);
    *outGraph = g;
    return noErr;
}

OSStatus DisposeAUGraph(AUGraph g) {
    if (!valid(g))
        return kAudio_ParamError;
    AUGraphStop(g);
    AUGraphUninitialize(g);
    AUGraphClose(g);
    g->magic = 0;
    free(g->nodes);
    free(g->interactions);
    pthread_mutex_destroy(&g->lock);
    free(g);
    return noErr;
}

/* ---- Interactions on the units ---- */

static OSStatus applyInteraction(AUGraph g, const AUNodeInteraction *in) {
    if (in->nodeInteractionType == kAUNodeInteraction_Connection) {
        const AUNodeConnection *c = &in->nodeInteraction.connection;
        Node *src = findNode(g, c->sourceNode), *dst = findNode(g, c->destNode);
        if (src == NULL || dst == NULL || src->unit == NULL || dst->unit == NULL)
            return kAUGraphErr_NodeNotFound;
        AudioUnitConnection conn = { src->unit, c->sourceOutputNumber, c->destInputNumber };
        return AudioUnitSetProperty(dst->unit, kAudioUnitProperty_MakeConnection, kAudioUnitScope_Input,
                                    c->destInputNumber, &conn, sizeof(conn));
    }
    const AUNodeRenderCallback *cb = &in->nodeInteraction.inputCallback;
    Node *dst = findNode(g, cb->destNode);
    if (dst == NULL || dst->unit == NULL)
        return kAUGraphErr_NodeNotFound;
    return AudioUnitSetProperty(dst->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                cb->destInputNumber, &cb->cback, sizeof(cb->cback));
}

/* The destination's input as it was before: nothing feeding it. */
static void clearInput(AUGraph g, AUNode dest, UInt32 input) {
    Node *dst = findNode(g, dest);
    if (dst == NULL || dst->unit == NULL)
        return;
    AudioUnitConnection none = { NULL, 0, input };
    AudioUnitSetProperty(dst->unit, kAudioUnitProperty_MakeConnection, kAudioUnitScope_Input, input, &none,
                         sizeof(none));
}

static AUNode destOf(const AUNodeInteraction *in) {
    return in->nodeInteractionType == kAUNodeInteraction_Connection ? in->nodeInteraction.connection.destNode
                                                                     : in->nodeInteraction.inputCallback.destNode;
}

static UInt32 inputOf(const AUNodeInteraction *in) {
    return in->nodeInteractionType == kAUNodeInteraction_Connection
               ? in->nodeInteraction.connection.destInputNumber
               : in->nodeInteraction.inputCallback.destInputNumber;
}

/* Removes what feeds the input (one interaction per input). */
static void removeInteraction(AUGraph g, AUNode dest, UInt32 input) {
    for (UInt32 i = 0; i < g->interactionCount; i++)
        if (destOf(&g->interactions[i]) == dest && inputOf(&g->interactions[i]) == input) {
            g->interactions[i] = g->interactions[--g->interactionCount];
            if (g->open)
                clearInput(g, dest, input);
            return;
        }
}

static OSStatus addInteraction(AUGraph g, const AUNodeInteraction *in) {
    AUNodeInteraction *grown;
    OSStatus status = noErr;
    removeInteraction(g, destOf(in), inputOf(in));
    grown = realloc(g->interactions, (g->interactionCount + 1) * sizeof(*grown));
    if (grown == NULL)
        return kAudio_MemFullError;
    g->interactions = grown;
    g->interactions[g->interactionCount++] = *in;
    if (g->open)
        status = applyInteraction(g, in);
    return status;
}

/* ---- Nodes ---- */

static OSStatus openNode(Node *n) {
    AudioComponent c = AudioComponentFindNext(NULL, &n->description);
    if (c == NULL)
        return kAUGraphErr_InvalidAudioUnit;
    return AudioComponentInstanceNew(c, &n->unit);
}

OSStatus AUGraphAddNode(AUGraph g, const AudioComponentDescription *description, AUNode *outNode) {
    Node *grown;
    OSStatus status = noErr;
    if (!valid(g) || description == NULL || outNode == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    if (description->componentType == kAudioUnitType_Output && g->head != 0) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_OutputNodeErr;
    }
    grown = realloc(g->nodes, (g->nodeCount + 1) * sizeof(*grown));
    if (grown == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAudio_MemFullError;
    }
    g->nodes = grown;
    Node *n = &g->nodes[g->nodeCount];
    memset(n, 0, sizeof(*n));
    n->id = g->nextID;
    n->description = *description;
    if (g->open && (status = openNode(n)) != noErr) {
        pthread_mutex_unlock(&g->lock);
        return status;
    }
    if (g->initialized && n->unit != NULL)
        AudioUnitInitialize(n->unit);
    g->nodeCount++;
    g->nextID++;
    if (description->componentType == kAudioUnitType_Output)
        g->head = n->id;
    *outNode = n->id;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphRemoveNode(AUGraph g, AUNode node) {
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    Node *n = findNode(g, node);
    if (n == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_NodeNotFound;
    }
    for (UInt32 i = 0; i < g->interactionCount;) {
        AUNodeInteraction *in = &g->interactions[i];
        Boolean involves = destOf(in) == node || (in->nodeInteractionType == kAUNodeInteraction_Connection &&
                                                  in->nodeInteraction.connection.sourceNode == node);
        if (involves)
            removeInteraction(g, destOf(in), inputOf(in));
        else
            i++;
    }
    if (n->unit != NULL)
        AudioComponentInstanceDispose(n->unit);
    if (g->head == node)
        g->head = 0;
    *n = g->nodes[--g->nodeCount];
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphGetNodeCount(AUGraph g, UInt32 *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = g->nodeCount;
    return noErr;
}

OSStatus AUGraphGetIndNode(AUGraph g, UInt32 index, AUNode *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    if (index >= g->nodeCount)
        return kAUGraphErr_NodeNotFound;
    *out = g->nodes[index].id;
    return noErr;
}

OSStatus AUGraphNodeInfo(AUGraph g, AUNode node, AudioComponentDescription *outDescription, AudioUnit *outUnit) {
    if (!valid(g))
        return kAudio_ParamError;
    Node *n = findNode(g, node);
    if (n == NULL)
        return kAUGraphErr_NodeNotFound;
    if (outDescription != NULL)
        *outDescription = n->description;
    if (outUnit != NULL)
        *outUnit = n->unit;
    return noErr;
}

/* ---- Connections ---- */

OSStatus AUGraphConnectNodeInput(AUGraph g, AUNode source, UInt32 sourceOutput, AUNode dest, UInt32 destInput) {
    AUNodeInteraction in;
    OSStatus status;
    if (!valid(g))
        return kAudio_ParamError;
    if (source == dest)
        return kAUGraphErr_InvalidConnection;
    pthread_mutex_lock(&g->lock);
    if (findNode(g, source) == NULL || findNode(g, dest) == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_NodeNotFound;
    }
    memset(&in, 0, sizeof(in));
    in.nodeInteractionType = kAUNodeInteraction_Connection;
    in.nodeInteraction.connection = (AUNodeConnection){ source, sourceOutput, dest, destInput };
    status = addInteraction(g, &in);
    pthread_mutex_unlock(&g->lock);
    return status;
}

OSStatus AUGraphSetNodeInputCallback(AUGraph g, AUNode dest, UInt32 destInput, const AURenderCallbackStruct *cb) {
    AUNodeInteraction in;
    OSStatus status;
    if (!valid(g) || cb == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    if (findNode(g, dest) == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_NodeNotFound;
    }
    memset(&in, 0, sizeof(in));
    in.nodeInteractionType = kAUNodeInteraction_InputCallback;
    in.nodeInteraction.inputCallback.destNode = dest;
    in.nodeInteraction.inputCallback.destInputNumber = destInput;
    in.nodeInteraction.inputCallback.cback = *cb;
    status = addInteraction(g, &in);
    pthread_mutex_unlock(&g->lock);
    return status;
}

OSStatus AUGraphDisconnectNodeInput(AUGraph g, AUNode dest, UInt32 destInput) {
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    if (findNode(g, dest) == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_NodeNotFound;
    }
    removeInteraction(g, dest, destInput);
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphClearConnections(AUGraph g) {
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    while (g->interactionCount > 0)
        removeInteraction(g, destOf(&g->interactions[0]), inputOf(&g->interactions[0]));
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphGetNumberOfInteractions(AUGraph g, UInt32 *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = g->interactionCount;
    return noErr;
}

OSStatus AUGraphGetInteractionInfo(AUGraph g, UInt32 index, AUNodeInteraction *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    if (index >= g->interactionCount)
        return kAUGraphErr_InvalidConnection;
    *out = g->interactions[index];
    return noErr;
}

static Boolean involves(const AUNodeInteraction *in, AUNode node) {
    return destOf(in) == node ||
           (in->nodeInteractionType == kAUNodeInteraction_Connection && in->nodeInteraction.connection.sourceNode == node);
}

OSStatus AUGraphCountNodeInteractions(AUGraph g, AUNode node, UInt32 *out) {
    UInt32 n = 0;
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    if (findNode(g, node) == NULL)
        return kAUGraphErr_NodeNotFound;
    for (UInt32 i = 0; i < g->interactionCount; i++)
        n += involves(&g->interactions[i], node);
    *out = n;
    return noErr;
}

OSStatus AUGraphGetNodeInteractions(AUGraph g, AUNode node, UInt32 *ioCount, AUNodeInteraction *out) {
    UInt32 n = 0;
    if (!valid(g) || ioCount == NULL || out == NULL)
        return kAudio_ParamError;
    if (findNode(g, node) == NULL)
        return kAUGraphErr_NodeNotFound;
    for (UInt32 i = 0; i < g->interactionCount && n < *ioCount; i++)
        if (involves(&g->interactions[i], node))
            out[n++] = g->interactions[i];
    *ioCount = n;
    return noErr;
}

OSStatus AUGraphUpdate(AUGraph g, Boolean *outIsUpdated) {
    if (!valid(g))
        return kAudio_ParamError;
    if (outIsUpdated != NULL)
        *outIsUpdated = true;
    return noErr;
}

/* ---- State ---- */

OSStatus AUGraphOpen(AUGraph g) {
    OSStatus status = noErr;
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    if (!g->open) {
        for (UInt32 i = 0; i < g->nodeCount && status == noErr; i++)
            if (g->nodes[i].unit == NULL)
                status = openNode(&g->nodes[i]);
        if (status == noErr) {
            g->open = true;
            for (UInt32 i = 0; i < g->interactionCount && status == noErr; i++)
                status = applyInteraction(g, &g->interactions[i]);
        }
    }
    pthread_mutex_unlock(&g->lock);
    return status;
}

OSStatus AUGraphClose(AUGraph g) {
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    for (UInt32 i = 0; i < g->nodeCount; i++)
        if (g->nodes[i].unit != NULL) {
            AudioComponentInstanceDispose(g->nodes[i].unit);
            g->nodes[i].unit = NULL;
        }
    g->open = g->initialized = g->running = false;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

static OSStatus graphNotify(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                            UInt32 bus, UInt32 frames, AudioBufferList *data) {
    AUGraph g = refCon;
    for (UInt32 i = 0; i < g->notifyCount; i++)
        g->notifies[i].proc(g->notifies[i].refCon, flags, time, bus, frames, data);
    return noErr;
}

/* Sources before the units they feed (each unit's inputs are set up when
   it initializes; the order matters to no unit here, but hosts expect
   it). */
OSStatus AUGraphInitialize(AUGraph g) {
    OSStatus status = noErr;
    if (!valid(g))
        return kAudio_ParamError;
    if (!g->open && (status = AUGraphOpen(g)) != noErr)
        return status;
    pthread_mutex_lock(&g->lock);
    if (!g->initialized) {
        for (UInt32 i = 0; i < g->nodeCount && status == noErr; i++)
            if (g->nodes[i].id != g->head)
                status = AudioUnitInitialize(g->nodes[i].unit);
        Node *head = findNode(g, g->head);
        if (status == noErr && head != NULL) {
            status = AudioUnitInitialize(head->unit);
            if (status == noErr)
                AudioUnitAddRenderNotify(head->unit, graphNotify, g);
        }
        g->initialized = status == noErr;
    }
    pthread_mutex_unlock(&g->lock);
    return status;
}

OSStatus AUGraphUninitialize(AUGraph g) {
    if (!valid(g))
        return kAudio_ParamError;
    AUGraphStop(g);
    pthread_mutex_lock(&g->lock);
    if (g->initialized) {
        Node *head = findNode(g, g->head);
        if (head != NULL && head->unit != NULL)
            AudioUnitRemoveRenderNotify(head->unit, graphNotify, g);
        for (UInt32 i = 0; i < g->nodeCount; i++)
            if (g->nodes[i].unit != NULL)
                AudioUnitUninitialize(g->nodes[i].unit);
        g->initialized = false;
    }
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphStart(AUGraph g) {
    OSStatus status;
    if (!valid(g))
        return kAudio_ParamError;
    if (!g->initialized)
        return kAudioUnitErr_Uninitialized;
    Node *head = findNode(g, g->head);
    if (head == NULL || head->unit == NULL)
        return kAUGraphErr_OutputNodeErr;
    status = AudioOutputUnitStart(head->unit);
    if (status == noErr)
        g->running = true;
    return status;
}

OSStatus AUGraphStop(AUGraph g) {
    if (!valid(g))
        return kAudio_ParamError;
    Node *head = findNode(g, g->head);
    if (g->running && head != NULL && head->unit != NULL)
        AudioOutputUnitStop(head->unit);
    g->running = false;
    return noErr;
}

OSStatus AUGraphIsOpen(AUGraph g, Boolean *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = g->open;
    return noErr;
}

OSStatus AUGraphIsInitialized(AUGraph g, Boolean *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = g->initialized;
    return noErr;
}

OSStatus AUGraphIsRunning(AUGraph g, Boolean *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = g->running;
    return noErr;
}

/* Not measured: no load reported. */
OSStatus AUGraphGetCPULoad(AUGraph g, Float32 *out) {
    if (!valid(g) || out == NULL)
        return kAudio_ParamError;
    *out = 0;
    return noErr;
}

OSStatus AUGraphGetMaxCPULoad(AUGraph g, Float32 *out) {
    return AUGraphGetCPULoad(g, out);
}

OSStatus AUGraphAddRenderNotify(AUGraph g, AURenderCallback proc, void *refCon) {
    if (!valid(g) || proc == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    if (g->notifyCount >= sizeof(g->notifies) / sizeof(g->notifies[0])) {
        pthread_mutex_unlock(&g->lock);
        return kAudio_MemFullError;
    }
    g->notifies[g->notifyCount].proc = proc;
    g->notifies[g->notifyCount++].refCon = refCon;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphRemoveRenderNotify(AUGraph g, AURenderCallback proc, void *refCon) {
    if (!valid(g))
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    for (UInt32 i = 0; i < g->notifyCount; i++)
        if (g->notifies[i].proc == proc && g->notifies[i].refCon == refCon) {
            g->notifies[i] = g->notifies[--g->notifyCount];
            break;
        }
    pthread_mutex_unlock(&g->lock);
    return noErr;
}
