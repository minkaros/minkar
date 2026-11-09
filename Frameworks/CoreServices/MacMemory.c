/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A handle points at the first field of its block: the data pointer,
 * followed by the size and state. Pointers carry their size in front.
 */

#include "MacMemory.h"
#include "MacErrors.h"
#include <stdlib.h>
#include <string.h>

typedef struct HandleBlock {
    char *data;
    Size size;
    SInt8 state;
} HandleBlock;

enum { kLocked = 0x80, kPurgeable = 0x40, kResource = 0x20 };

static __thread OSErr sMemError;

#define BLOCK(h) ((HandleBlock *)(void *)(h))

OSErr MemError(void) {
    return sMemError;
}

static Handle makeHandle(Size byteCount, Boolean clear) {
    HandleBlock *b;
    if (byteCount < 0) {
        sMemError = paramErr;
        return NULL;
    }
    b = calloc(1, sizeof(HandleBlock));
    if (b == NULL) {
        sMemError = memFullErr;
        return NULL;
    }
    if (byteCount > 0) {
        b->data = clear ? calloc(1, (size_t)byteCount) : malloc((size_t)byteCount);
        if (b->data == NULL) {
            free(b);
            sMemError = memFullErr;
            return NULL;
        }
    }
    b->size = byteCount;
    sMemError = noErr;
    return (Handle)(void *)b;
}

Handle NewHandle(Size byteCount) { return makeHandle(byteCount, false); }
Handle NewHandleClear(Size byteCount) { return makeHandle(byteCount, true); }
Handle NewEmptyHandle(void) { return makeHandle(0, false); }

void DisposeHandle(Handle h) {
    if (h == NULL) {
        sMemError = nilHandleErr;
        return;
    }
    free(BLOCK(h)->data);
    free(BLOCK(h));
    sMemError = noErr;
}

Size GetHandleSize(Handle h) {
    if (h == NULL) {
        sMemError = nilHandleErr;
        return 0;
    }
    sMemError = noErr;
    return BLOCK(h)->size;
}

void SetHandleSize(Handle h, Size newSize) {
    char *grown;
    if (h == NULL) {
        sMemError = nilHandleErr;
        return;
    }
    if (newSize < 0) {
        sMemError = paramErr;
        return;
    }
    grown = realloc(BLOCK(h)->data, newSize > 0 ? (size_t)newSize : 1);
    if (grown == NULL) {
        sMemError = memFullErr;
        return;
    }
    BLOCK(h)->data = grown;
    BLOCK(h)->size = newSize;
    sMemError = noErr;
}

void EmptyHandle(Handle h) {
    if (h == NULL) {
        sMemError = nilHandleErr;
        return;
    }
    free(BLOCK(h)->data);
    BLOCK(h)->data = NULL;
    BLOCK(h)->size = 0;
    sMemError = noErr;
}

void ReallocateHandle(Handle h, Size byteCount) {
    EmptyHandle(h);
    if (sMemError == noErr && byteCount > 0)
        SetHandleSize(h, byteCount);
}

#define STATE_OP(name, expr) \
    void name(Handle h) { if (h == NULL) { sMemError = nilHandleErr; return; } expr; sMemError = noErr; }

STATE_OP(HLock, BLOCK(h)->state |= kLocked)
STATE_OP(HLockHi, BLOCK(h)->state |= kLocked)
STATE_OP(HUnlock, BLOCK(h)->state &= ~kLocked)
STATE_OP(HPurge, BLOCK(h)->state |= kPurgeable)
STATE_OP(HNoPurge, BLOCK(h)->state &= ~kPurgeable)
STATE_OP(MoveHHi, (void)0)

SInt8 HGetState(Handle h) {
    if (h == NULL) {
        sMemError = nilHandleErr;
        return 0;
    }
    sMemError = noErr;
    return BLOCK(h)->state;
}

void HSetState(Handle h, SInt8 flags) {
    if (h == NULL) {
        sMemError = nilHandleErr;
        return;
    }
    BLOCK(h)->state = flags;
    sMemError = noErr;
}

OSErr PtrToHand(const void *srcPtr, Handle *dstHndl, long size) {
    Handle h = NewHandle(size);
    if (h == NULL)
        return sMemError;
    if (size > 0)
        memcpy(*h, srcPtr, (size_t)size);
    *dstHndl = h;
    return noErr;
}

OSErr HandToHand(Handle *theHndl) {
    if (theHndl == NULL || *theHndl == NULL)
        return nilHandleErr;
    return PtrToHand(**theHndl, theHndl, GetHandleSize(*theHndl));
}

OSErr PtrToXHand(const void *srcPtr, Handle dstHndl, long size) {
    SetHandleSize(dstHndl, size);
    if (sMemError == noErr && size > 0)
        memcpy(*dstHndl, srcPtr, (size_t)size);
    return sMemError;
}

OSErr PtrAndHand(const void *ptr1, Handle hand2, long size) {
    Size old = GetHandleSize(hand2);
    if (sMemError != noErr)
        return sMemError;
    SetHandleSize(hand2, old + size);
    if (sMemError == noErr && size > 0)
        memcpy(*hand2 + old, ptr1, (size_t)size);
    return sMemError;
}

OSErr HandAndHand(Handle hand1, Handle hand2) {
    if (hand1 == NULL)
        return nilHandleErr;
    return PtrAndHand(*hand1, hand2, GetHandleSize(hand1));
}

/* Pointers: the size in a 16-byte header, so the data stays aligned. */
#define PTR_HEADER 16

static Ptr makePtr(Size byteCount, Boolean clear) {
    char *block;
    if (byteCount < 0) {
        sMemError = paramErr;
        return NULL;
    }
    block = clear ? calloc(1, PTR_HEADER + (size_t)byteCount) : malloc(PTR_HEADER + (size_t)byteCount);
    if (block == NULL) {
        sMemError = memFullErr;
        return NULL;
    }
    *(Size *)(void *)block = byteCount;
    sMemError = noErr;
    return block + PTR_HEADER;
}

Ptr NewPtr(Size byteCount) { return makePtr(byteCount, false); }
Ptr NewPtrClear(Size byteCount) { return makePtr(byteCount, true); }

void DisposePtr(Ptr p) {
    if (p != NULL)
        free(p - PTR_HEADER);
    sMemError = noErr;
}

Size GetPtrSize(Ptr p) {
    if (p == NULL) {
        sMemError = paramErr;
        return 0;
    }
    sMemError = noErr;
    return *(Size *)(void *)(p - PTR_HEADER);
}

/* Pointers do not move: they shrink, and grow only within their block. */
void SetPtrSize(Ptr p, Size newSize) {
    if (p == NULL || newSize < 0) {
        sMemError = paramErr;
        return;
    }
    if (newSize > GetPtrSize(p)) {
        sMemError = memFullErr;
        return;
    }
    *(Size *)(void *)(p - PTR_HEADER) = newSize;
    sMemError = noErr;
}

void BlockMove(const void *srcPtr, void *destPtr, Size byteCount) {
    if (byteCount > 0)
        memmove(destPtr, srcPtr, (size_t)byteCount);
}

void BlockMoveData(const void *srcPtr, void *destPtr, Size byteCount) {
    BlockMove(srcPtr, destPtr, byteCount);
}

void BlockZero(void *destPtr, Size byteCount) {
    if (byteCount > 0)
        memset(destPtr, 0, (size_t)byteCount);
}
