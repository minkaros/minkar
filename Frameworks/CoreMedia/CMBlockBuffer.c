/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A block buffer is a list of pieces, each a range of a memory block.
 * Blocks are counted: a reference to another buffer shares its blocks (it
 * takes their pieces, not the buffer), and a block is freed with its last
 * piece, through its allocator or its custom source. A block given no
 * memory gets it when first needed (AssureBlockMemory, or the
 * kCMBlockBufferAssureMemoryNowFlag); until then its bytes cannot be read
 * or written.
 */

#include "CMBlockBuffer.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int refs;
    void *memory;
    size_t size;
    CFAllocatorRef allocator;   /* retained, NULL for the default; kCFAllocatorNull: not freed */
    Boolean hasCustom;
    CMBlockBufferCustomBlockSource custom;
} Block;

typedef struct {
    Block *block;
    size_t offset;              /* into the block */
    size_t length;
} Piece;

struct OpaqueCMBlockBuffer {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    Piece *pieces;
    uint32_t count, capacity;
    size_t length;
};

static CFTypeID typeID;

static void releaseBlock(Block *b) {
    if (__atomic_sub_fetch(&b->refs, 1, __ATOMIC_ACQ_REL) != 0)
        return;
    if (b->memory != NULL) {
        if (b->hasCustom) {
            if (b->custom.FreeBlock != NULL)
                b->custom.FreeBlock(b->custom.refCon, b->memory, b->size);
        } else if (b->allocator != kCFAllocatorNull)
            CFAllocatorDeallocate(b->allocator, b->memory);
    }
    if (b->allocator != NULL)
        CFRelease(b->allocator);
    free(b);
}

static void finalize(CFTypeRef cf) {
    CMBlockBufferRef bb = (CMBlockBufferRef)cf;
    for (uint32_t i = 0; i < bb->count; i++)
        releaseBlock(bb->pieces[i].block);
    free(bb->pieces);
    pthread_mutex_destroy(&bb->lock);
}

static CFStringRef describe(CFTypeRef cf) {
    CMBlockBufferRef bb = (CMBlockBufferRef)cf;
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CMBlockBuffer %p length=%zu pieces=%u>"), cf, bb->length,
                                    bb->count);
}

static const CFRuntimeClass blockBufferClass = { 0, "CMBlockBuffer", NULL, NULL, finalize, NULL, NULL, NULL, describe };

CFTypeID CMBlockBufferGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&blockBufferClass); });
    return typeID;
}

static CMBlockBufferRef create(CFAllocatorRef allocator, uint32_t capacity) {
    size_t extra = sizeof(struct OpaqueCMBlockBuffer) - sizeof(CFRuntimeBase);
    CMBlockBufferRef bb = (CMBlockBufferRef)_CFRuntimeCreateInstance(allocator, CMBlockBufferGetTypeID(), extra, NULL);
    if (bb == NULL)
        return NULL;
    pthread_mutex_init(&bb->lock, NULL);
    bb->capacity = capacity > 0 ? capacity : 4;
    bb->pieces = calloc(bb->capacity, sizeof(Piece));
    if (bb->pieces == NULL) {
        CFRelease(bb);
        return NULL;
    }
    return bb;
}

static Boolean addPiece(CMBlockBufferRef bb, Block *block, size_t offset, size_t length) {
    if (bb->count == bb->capacity) {
        Piece *p = realloc(bb->pieces, bb->capacity * 2 * sizeof(Piece));
        if (p == NULL)
            return false;
        bb->pieces = p;
        bb->capacity *= 2;
    }
    __atomic_add_fetch(&block->refs, 1, __ATOMIC_ACQ_REL);
    bb->pieces[bb->count++] = (Piece){ block, offset, length };
    bb->length += length;
    return true;
}

static OSStatus assure(Block *b) {
    if (b->memory != NULL)
        return kCMBlockBufferNoErr;
    if (b->hasCustom)
        b->memory = b->custom.AllocateBlock != NULL ? b->custom.AllocateBlock(b->custom.refCon, b->size) : NULL;
    else
        b->memory = CFAllocatorAllocate(b->allocator != NULL ? b->allocator : kCFAllocatorDefault, (CFIndex)b->size, 0);
    return b->memory != NULL ? kCMBlockBufferNoErr : kCMBlockBufferBlockAllocationFailedErr;
}

static OSStatus appendBlock(CMBlockBufferRef bb, void *memory, size_t blockLength, CFAllocatorRef blockAllocator,
                            const CMBlockBufferCustomBlockSource *custom, size_t offset, size_t length,
                            CMBlockBufferFlags flags) {
    if (blockLength == 0 || offset > blockLength || length > blockLength - offset)
        return blockLength == 0 ? kCMBlockBufferBadLengthParameterErr
                                : (offset > blockLength ? kCMBlockBufferBadOffsetParameterErr
                                                        : kCMBlockBufferBadLengthParameterErr);
    if (custom != NULL && custom->version != kCMBlockBufferCustomBlockSourceVersion)
        return kCMBlockBufferBadCustomBlockSourceErr;
    Block *b = calloc(1, sizeof(Block));
    if (b == NULL)
        return kCMBlockBufferStructureAllocationFailedErr;
    b->memory = memory;
    b->size = blockLength;
    if (custom != NULL) {
        b->hasCustom = true;
        b->custom = *custom;
    } else
        b->allocator = blockAllocator != NULL ? CFRetain(blockAllocator) : NULL;  /* NULL: the default */
    if (flags & kCMBlockBufferAssureMemoryNowFlag) {
        OSStatus err = assure(b);
        if (err != kCMBlockBufferNoErr) {
            b->refs = 1;
            releaseBlock(b);
            return err;
        }
    }
    pthread_mutex_lock(&bb->lock);
    Boolean added = addPiece(bb, b, offset, length);
    pthread_mutex_unlock(&bb->lock);
    if (!added) {
        b->refs = 1;
        releaseBlock(b);
        return kCMBlockBufferStructureAllocationFailedErr;
    }
    return kCMBlockBufferNoErr;
}

/* The target's pieces over [offset, offset + length), sharing their blocks. */
static OSStatus appendReference(CMBlockBufferRef bb, CMBlockBufferRef target, size_t offset, size_t length,
                                CMBlockBufferFlags flags) {
    if (target == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&target->lock);
    if (length == 0 && !(flags & kCMBlockBufferPermitEmptyReferenceFlag))
        length = offset <= target->length ? target->length - offset : 0;
    if (offset > target->length || length > target->length - offset) {
        pthread_mutex_unlock(&target->lock);
        return offset > target->length ? kCMBlockBufferBadOffsetParameterErr : kCMBlockBufferBadLengthParameterErr;
    }
    Piece copies[target->count > 0 ? target->count : 1];
    uint32_t n = 0;
    size_t at = 0;
    for (uint32_t i = 0; i < target->count && length > 0; i++) {
        Piece p = target->pieces[i];
        if (at + p.length <= offset) {
            at += p.length;
            continue;
        }
        size_t skip = offset > at ? offset - at : 0;
        size_t take = p.length - skip < length ? p.length - skip : length;
        copies[n++] = (Piece){ p.block, p.offset + skip, take };
        __atomic_add_fetch(&p.block->refs, 1, __ATOMIC_ACQ_REL);
        length -= take;
        offset += take;
        at += p.length;
    }
    pthread_mutex_unlock(&target->lock);
    OSStatus err = kCMBlockBufferNoErr;
    pthread_mutex_lock(&bb->lock);
    for (uint32_t i = 0; i < n; i++) {
        if (err == kCMBlockBufferNoErr && !addPiece(bb, copies[i].block, copies[i].offset, copies[i].length))
            err = kCMBlockBufferStructureAllocationFailedErr;
        releaseBlock(copies[i].block);      /* addPiece holds its own */
    }
    pthread_mutex_unlock(&bb->lock);
    return err;
}

OSStatus CMBlockBufferCreateEmpty(CFAllocatorRef allocator, uint32_t capacity, CMBlockBufferFlags flags,
                                  CMBlockBufferRef *out) {
    if (out == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    *out = create(allocator, capacity);
    return *out != NULL ? kCMBlockBufferNoErr : kCMBlockBufferStructureAllocationFailedErr;
}

OSStatus CMBlockBufferCreateWithMemoryBlock(CFAllocatorRef allocator, void *memoryBlock, size_t blockLength,
                                            CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource *custom,
                                            size_t offset, size_t length, CMBlockBufferFlags flags,
                                            CMBlockBufferRef *out) {
    if (out == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    *out = NULL;
    CMBlockBufferRef bb = create(allocator, 1);
    if (bb == NULL)
        return kCMBlockBufferStructureAllocationFailedErr;
    OSStatus err = appendBlock(bb, memoryBlock, blockLength, blockAllocator, custom, offset, length, flags);
    if (err != kCMBlockBufferNoErr) {
        CFRelease(bb);
        return err;
    }
    *out = bb;
    return kCMBlockBufferNoErr;
}

OSStatus CMBlockBufferCreateWithBufferReference(CFAllocatorRef allocator, CMBlockBufferRef reference, size_t offset,
                                                size_t length, CMBlockBufferFlags flags, CMBlockBufferRef *out) {
    if (out == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    *out = NULL;
    CMBlockBufferRef bb = create(allocator, 1);
    if (bb == NULL)
        return kCMBlockBufferStructureAllocationFailedErr;
    OSStatus err = appendReference(bb, reference, offset, length, flags);
    if (err != kCMBlockBufferNoErr) {
        CFRelease(bb);
        return err;
    }
    *out = bb;
    return kCMBlockBufferNoErr;
}

OSStatus CMBlockBufferAppendMemoryBlock(CMBlockBufferRef bb, void *memoryBlock, size_t blockLength,
                                        CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource *custom,
                                        size_t offset, size_t length, CMBlockBufferFlags flags) {
    if (bb == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    return appendBlock(bb, memoryBlock, blockLength, blockAllocator, custom, offset, length, flags);
}

OSStatus CMBlockBufferAppendBufferReference(CMBlockBufferRef bb, CMBlockBufferRef target, size_t offset,
                                            size_t length, CMBlockBufferFlags flags) {
    if (bb == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    return appendReference(bb, target, offset, length, flags);
}

OSStatus CMBlockBufferAssureBlockMemory(CMBlockBufferRef bb) {
    if (bb == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    OSStatus err = kCMBlockBufferNoErr;
    pthread_mutex_lock(&bb->lock);
    for (uint32_t i = 0; i < bb->count && err == kCMBlockBufferNoErr; i++)
        err = assure(bb->pieces[i].block);
    pthread_mutex_unlock(&bb->lock);
    return err;
}

size_t CMBlockBufferGetDataLength(CMBlockBufferRef bb) {
    if (bb == NULL)
        return 0;
    pthread_mutex_lock(&bb->lock);
    size_t n = bb->length;
    pthread_mutex_unlock(&bb->lock);
    return n;
}

Boolean CMBlockBufferIsEmpty(CMBlockBufferRef bb) {
    return CMBlockBufferGetDataLength(bb) == 0;
}

/* The piece holding byte offset, and where in it; the lock is held. */
static Piece *pieceAt(CMBlockBufferRef bb, size_t offset, size_t *into) {
    size_t at = 0;
    for (uint32_t i = 0; i < bb->count; i++) {
        if (offset < at + bb->pieces[i].length) {
            *into = offset - at;
            return &bb->pieces[i];
        }
        at += bb->pieces[i].length;
    }
    return NULL;
}

Boolean CMBlockBufferIsRangeContiguous(CMBlockBufferRef bb, size_t offset, size_t length) {
    if (bb == NULL)
        return false;
    pthread_mutex_lock(&bb->lock);
    if (length == 0)
        length = offset < bb->length ? bb->length - offset : 0;
    size_t into = 0;
    Piece *p = offset + length <= bb->length && length > 0 ? pieceAt(bb, offset, &into) : NULL;
    Boolean contiguous = p != NULL && p->length - into >= length;
    pthread_mutex_unlock(&bb->lock);
    return contiguous;
}

OSStatus CMBlockBufferGetDataPointer(CMBlockBufferRef bb, size_t offset, size_t *lengthAtOffset, size_t *totalLength,
                                     char **dataPointer) {
    if (bb == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&bb->lock);
    if (bb->length == 0) {
        pthread_mutex_unlock(&bb->lock);
        return kCMBlockBufferEmptyBBufErr;
    }
    size_t into = 0;
    Piece *p = pieceAt(bb, offset, &into);
    OSStatus err = kCMBlockBufferNoErr;
    if (p == NULL)
        err = kCMBlockBufferBadOffsetParameterErr;
    else if (p->block->memory == NULL)
        err = kCMBlockBufferUnallocatedBlockErr;
    else {
        if (lengthAtOffset != NULL)
            *lengthAtOffset = p->length - into;
        if (totalLength != NULL)
            *totalLength = bb->length;
        if (dataPointer != NULL)
            *dataPointer = (char *)p->block->memory + p->offset + into;
    }
    pthread_mutex_unlock(&bb->lock);
    return err;
}

/* Over [offset, offset + length): to bytes (copy out), from bytes (copy
   in), or a fill byte; the lock is held. */
typedef enum { kCopyOut, kCopyIn, kFill } Transfer;

static OSStatus transfer(CMBlockBufferRef bb, size_t offset, size_t length, Transfer how, void *bytes, char fill) {
    if (offset > bb->length)
        return kCMBlockBufferBadOffsetParameterErr;
    if (length > bb->length - offset)
        return kCMBlockBufferBadLengthParameterErr;
    size_t at = 0, done = 0;
    for (uint32_t i = 0; i < bb->count && done < length; i++) {
        Piece *p = &bb->pieces[i];
        if (offset + done >= at + p->length) {
            at += p->length;
            continue;
        }
        size_t into = offset + done - at;
        size_t n = p->length - into < length - done ? p->length - into : length - done;
        if (p->block->memory == NULL)
            return kCMBlockBufferUnallocatedBlockErr;
        char *mem = (char *)p->block->memory + p->offset + into;
        if (how == kCopyOut)
            memcpy((char *)bytes + done, mem, n);
        else if (how == kCopyIn)
            memcpy(mem, (const char *)bytes + done, n);
        else
            memset(mem, fill, n);
        done += n;
        at += p->length;
    }
    return kCMBlockBufferNoErr;
}

OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef bb, size_t offset, size_t length, void *destination) {
    if (bb == NULL || destination == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&bb->lock);
    OSStatus err = transfer(bb, offset, length, kCopyOut, destination, 0);
    pthread_mutex_unlock(&bb->lock);
    return err;
}

OSStatus CMBlockBufferReplaceDataBytes(const void *source, CMBlockBufferRef bb, size_t offset, size_t length) {
    if (bb == NULL || source == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&bb->lock);
    OSStatus err = transfer(bb, offset, length, kCopyIn, (void *)source, 0);
    pthread_mutex_unlock(&bb->lock);
    return err;
}

OSStatus CMBlockBufferFillDataBytes(char fillByte, CMBlockBufferRef bb, size_t offset, size_t length) {
    if (bb == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&bb->lock);
    if (length == 0)
        length = offset < bb->length ? bb->length - offset : 0;
    OSStatus err = transfer(bb, offset, length, kFill, NULL, fillByte);
    pthread_mutex_unlock(&bb->lock);
    return err;
}

/* A pointer to the bytes where they are contiguous, else a copy into the
   caller's temporary block. */
OSStatus CMBlockBufferAccessDataBytes(CMBlockBufferRef bb, size_t offset, size_t length, void *temporaryBlock,
                                      char **returned) {
    if (bb == NULL || returned == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    pthread_mutex_lock(&bb->lock);
    OSStatus err = kCMBlockBufferNoErr;
    size_t into = 0;
    Piece *p = offset < bb->length ? pieceAt(bb, offset, &into) : NULL;
    if (p == NULL)
        err = kCMBlockBufferBadOffsetParameterErr;
    else if (length > bb->length - offset)
        err = kCMBlockBufferBadLengthParameterErr;
    else if (p->length - into >= length && p->block->memory != NULL)
        *returned = (char *)p->block->memory + p->offset + into;
    else if (temporaryBlock == NULL)
        err = kCMBlockBufferBadPointerParameterErr;
    else if ((err = transfer(bb, offset, length, kCopyOut, temporaryBlock, 0)) == kCMBlockBufferNoErr)
        *returned = temporaryBlock;
    pthread_mutex_unlock(&bb->lock);
    return err;
}

/* The source's bytes in one piece: a reference when they already are
   (unless a copy is asked for), else a new block with a copy. */
OSStatus CMBlockBufferCreateContiguous(CFAllocatorRef allocator, CMBlockBufferRef source, CFAllocatorRef blockAllocator,
                                       const CMBlockBufferCustomBlockSource *custom, size_t offset, size_t length,
                                       CMBlockBufferFlags flags, CMBlockBufferRef *out) {
    if (out == NULL || source == NULL)
        return kCMBlockBufferBadPointerParameterErr;
    *out = NULL;
    size_t total = CMBlockBufferGetDataLength(source);
    if (total == 0)
        return kCMBlockBufferEmptyBBufErr;
    if (length == 0)
        length = offset < total ? total - offset : 0;
    if (offset >= total)
        return kCMBlockBufferBadOffsetParameterErr;
    if (length > total - offset)
        return kCMBlockBufferBadLengthParameterErr;
    if (!(flags & kCMBlockBufferAlwaysCopyDataFlag) && CMBlockBufferIsRangeContiguous(source, offset, length)) {
        size_t at = 0;
        char *p = NULL;
        if (CMBlockBufferGetDataPointer(source, offset, &at, NULL, &p) == kCMBlockBufferNoErr)
            return CMBlockBufferCreateWithBufferReference(allocator, source, offset, length, 0, out);
    }
    CMBlockBufferRef bb = NULL;
    OSStatus err = CMBlockBufferCreateWithMemoryBlock(allocator, NULL, length, blockAllocator, custom, 0, length,
                                                      kCMBlockBufferAssureMemoryNowFlag, &bb);
    if (err != kCMBlockBufferNoErr)
        return err;
    char *dest = NULL;
    CMBlockBufferGetDataPointer(bb, 0, NULL, NULL, &dest);
    err = CMBlockBufferCopyDataBytes(source, offset, length, dest);
    if (err != kCMBlockBufferNoErr) {
        CFRelease(bb);
        return err;
    }
    *out = bb;
    return kCMBlockBufferNoErr;
}
