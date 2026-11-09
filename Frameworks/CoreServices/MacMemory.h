/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Handles and pointers of the classic Memory Manager. Handles never move;
 * locking and purging only keep their state bits.
 */

#ifndef __MACMEMORY__
#define __MACMEMORY__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char *Ptr;
typedef Ptr *Handle;
typedef SInt8 SignedByte;

OSErr MemError(void);
Handle NewHandle(Size byteCount);
Handle NewHandleClear(Size byteCount);
Handle NewEmptyHandle(void);
void DisposeHandle(Handle h);
Size GetHandleSize(Handle h);
void SetHandleSize(Handle h, Size newSize);
void EmptyHandle(Handle h);
void ReallocateHandle(Handle h, Size byteCount);
void HLock(Handle h);
void HLockHi(Handle h);
void HUnlock(Handle h);
void HPurge(Handle h);
void HNoPurge(Handle h);
void MoveHHi(Handle h);
SInt8 HGetState(Handle h);
void HSetState(Handle h, SInt8 flags);
OSErr HandToHand(Handle *theHndl);
OSErr PtrToHand(const void *srcPtr, Handle *dstHndl, long size);
OSErr PtrToXHand(const void *srcPtr, Handle dstHndl, long size);
OSErr HandAndHand(Handle hand1, Handle hand2);
OSErr PtrAndHand(const void *ptr1, Handle hand2, long size);
Ptr NewPtr(Size byteCount);
Ptr NewPtrClear(Size byteCount);
void DisposePtr(Ptr p);
Size GetPtrSize(Ptr p);
void SetPtrSize(Ptr p, Size newSize);
/* Programs that define these as macros (for their own copies) keep them. */
#ifndef BlockMove
void BlockMove(const void *srcPtr, void *destPtr, Size byteCount);
#endif
#ifndef BlockMoveData
void BlockMoveData(const void *srcPtr, void *destPtr, Size byteCount);
#endif
#ifndef BlockZero
void BlockZero(void *destPtr, Size byteCount);
#endif

#ifdef __cplusplus
}
#endif

#endif
