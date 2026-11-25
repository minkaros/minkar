/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio processing graphs: nodes (audio units by description), their
 * connections and input callbacks, opened, initialized and started
 * together; the graph's output node (one unit of type 'auou') is its head.
 */

#ifndef __AUGRAPH__
#define __AUGRAPH__

#include <AudioUnit/AudioUnit.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef SInt32 AUNode;
typedef struct OpaqueAUGraph *AUGraph;

enum {
    kAUGraphErr_NodeNotFound = -10860,
    kAUGraphErr_InvalidConnection = -10861,
    kAUGraphErr_OutputNodeErr = -10862,
    kAUGraphErr_CannotDoInCurrentContext = -10863,
    kAUGraphErr_InvalidAudioUnit = -10864,
};

enum {
    kAUNodeInteraction_Connection = 1,
    kAUNodeInteraction_InputCallback = 2,
};

typedef struct AUNodeConnection {
    AUNode sourceNode;
    UInt32 sourceOutputNumber;
    AUNode destNode;
    UInt32 destInputNumber;
} AUNodeConnection;

typedef struct AUNodeRenderCallback {
    AUNode destNode;
    AudioUnitElement destInputNumber;
    AURenderCallbackStruct cback;
} AUNodeRenderCallback;

typedef struct AUNodeInteraction {
    UInt32 nodeInteractionType;
    union {
        AUNodeConnection connection;
        AUNodeRenderCallback inputCallback;
    } nodeInteraction;
} AUNodeInteraction;

OSStatus NewAUGraph(AUGraph *outGraph);
OSStatus DisposeAUGraph(AUGraph inGraph);

OSStatus AUGraphAddNode(AUGraph inGraph, const AudioComponentDescription *inDescription, AUNode *outNode);
OSStatus AUGraphRemoveNode(AUGraph inGraph, AUNode inNode);
OSStatus AUGraphGetNodeCount(AUGraph inGraph, UInt32 *outNumberOfNodes);
OSStatus AUGraphGetIndNode(AUGraph inGraph, UInt32 inIndex, AUNode *outNode);
OSStatus AUGraphNodeInfo(AUGraph inGraph, AUNode inNode, AudioComponentDescription *outDescription,
                         AudioUnit *outAudioUnit);

OSStatus AUGraphConnectNodeInput(AUGraph inGraph, AUNode inSourceNode, UInt32 inSourceOutputNumber,
                                 AUNode inDestNode, UInt32 inDestInputNumber);
OSStatus AUGraphSetNodeInputCallback(AUGraph inGraph, AUNode inDestNode, UInt32 inDestInputNumber,
                                     const AURenderCallbackStruct *inInputCallback);
OSStatus AUGraphDisconnectNodeInput(AUGraph inGraph, AUNode inDestNode, UInt32 inDestInputNumber);
OSStatus AUGraphClearConnections(AUGraph inGraph);
OSStatus AUGraphGetNumberOfInteractions(AUGraph inGraph, UInt32 *outNumInteractions);
OSStatus AUGraphGetInteractionInfo(AUGraph inGraph, UInt32 inInteractionIndex, AUNodeInteraction *outInteraction);
OSStatus AUGraphCountNodeInteractions(AUGraph inGraph, AUNode inNode, UInt32 *outNumInteractions);
OSStatus AUGraphGetNodeInteractions(AUGraph inGraph, AUNode inNode, UInt32 *ioNumInteractions,
                                    AUNodeInteraction *outInteractions);
OSStatus AUGraphUpdate(AUGraph inGraph, Boolean *outIsUpdated);

OSStatus AUGraphOpen(AUGraph inGraph);
OSStatus AUGraphClose(AUGraph inGraph);
OSStatus AUGraphInitialize(AUGraph inGraph);
OSStatus AUGraphUninitialize(AUGraph inGraph);
OSStatus AUGraphStart(AUGraph inGraph);
OSStatus AUGraphStop(AUGraph inGraph);
OSStatus AUGraphIsOpen(AUGraph inGraph, Boolean *outIsOpen);
OSStatus AUGraphIsInitialized(AUGraph inGraph, Boolean *outIsInitialized);
OSStatus AUGraphIsRunning(AUGraph inGraph, Boolean *outIsRunning);

OSStatus AUGraphGetCPULoad(AUGraph inGraph, Float32 *outAverageCPULoad);
OSStatus AUGraphGetMaxCPULoad(AUGraph inGraph, Float32 *outMaxLoad);
OSStatus AUGraphAddRenderNotify(AUGraph inGraph, AURenderCallback inCallback, void *inRefCon);
OSStatus AUGraphRemoveRenderNotify(AUGraph inGraph, AURenderCallback inCallback, void *inRefCon);

#ifdef __cplusplus
}
#endif

#endif
