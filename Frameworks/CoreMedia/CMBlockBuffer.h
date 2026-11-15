/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Block buffers: a run of bytes made of pieces of memory blocks, each
 * the caller's, allocated (now or when first needed), or a reference to
 * part of another block buffer.
 */

#ifndef __CMBLOCKBUFFER__
#define __CMBLOCKBUFFER__

#include <CoreMedia/CMBase.h>

enum {
    kCMBlockBufferNoErr = 0,
    kCMBlockBufferStructureAllocationFailedErr = -12700,
    kCMBlockBufferBlockAllocationFailedErr = -12701,
    kCMBlockBufferBadCustomBlockSourceErr = -12702,
    kCMBlockBufferBadOffsetParameterErr = -12703,
    kCMBlockBufferBadLengthParameterErr = -12704,
    kCMBlockBufferBadPointerParameterErr = -12705,
    kCMBlockBufferEmptyBBufErr = -12706,
    kCMBlockBufferUnallocatedBlockErr = -12707,
    kCMBlockBufferInsufficientSpaceErr = -12708,
};

typedef uint32_t CMBlockBufferFlags;
enum {
    kCMBlockBufferAssureMemoryNowFlag = (1L << 0),
    kCMBlockBufferAlwaysCopyDataFlag = (1L << 1),
    kCMBlockBufferDontOptimizeDepthFlag = (1L << 2),
    kCMBlockBufferPermitEmptyReferenceFlag = (1L << 3),
};

typedef struct OpaqueCMBlockBuffer *CMBlockBufferRef;

typedef struct {
    uint32_t version;
    void *(*AllocateBlock)(void *refCon, size_t sizeInBytes);
    void (*FreeBlock)(void *refCon, void *doomedMemoryBlock, size_t sizeInBytes);
    void *refCon;
} CMBlockBufferCustomBlockSource;

enum {
    kCMBlockBufferCustomBlockSourceVersion = 0,
};

CM_EXPORT CFTypeID CMBlockBufferGetTypeID(void);

CM_EXPORT OSStatus CMBlockBufferCreateEmpty(CFAllocatorRef structureAllocator, uint32_t subBlockCapacity,
                                            CMBlockBufferFlags flags, CMBlockBufferRef *blockBufferOut);
CM_EXPORT OSStatus CMBlockBufferCreateWithMemoryBlock(CFAllocatorRef structureAllocator, void *memoryBlock,
                                                      size_t blockLength, CFAllocatorRef blockAllocator,
                                                      const CMBlockBufferCustomBlockSource *customBlockSource,
                                                      size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags,
                                                      CMBlockBufferRef *blockBufferOut);
CM_EXPORT OSStatus CMBlockBufferCreateWithBufferReference(CFAllocatorRef structureAllocator,
                                                          CMBlockBufferRef bufferReference, size_t offsetToData,
                                                          size_t dataLength, CMBlockBufferFlags flags,
                                                          CMBlockBufferRef *blockBufferOut);
CM_EXPORT OSStatus CMBlockBufferCreateContiguous(CFAllocatorRef structureAllocator, CMBlockBufferRef sourceBuffer,
                                                 CFAllocatorRef blockAllocator,
                                                 const CMBlockBufferCustomBlockSource *customBlockSource,
                                                 size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags,
                                                 CMBlockBufferRef *blockBufferOut);
CM_EXPORT OSStatus CMBlockBufferAppendMemoryBlock(CMBlockBufferRef theBuffer, void *memoryBlock, size_t blockLength,
                                                  CFAllocatorRef blockAllocator,
                                                  const CMBlockBufferCustomBlockSource *customBlockSource,
                                                  size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags);
CM_EXPORT OSStatus CMBlockBufferAppendBufferReference(CMBlockBufferRef theBuffer, CMBlockBufferRef targetBBuf,
                                                      size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags);
CM_EXPORT OSStatus CMBlockBufferAssureBlockMemory(CMBlockBufferRef theBuffer);
CM_EXPORT OSStatus CMBlockBufferAccessDataBytes(CMBlockBufferRef theBuffer, size_t offset, size_t length,
                                                void *temporaryBlock, char **returnedPointerOut);
CM_EXPORT OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef theSourceBuffer, size_t offsetToData,
                                              size_t dataLength, void *destination);
CM_EXPORT OSStatus CMBlockBufferReplaceDataBytes(const void *sourceBytes, CMBlockBufferRef destinationBuffer,
                                                 size_t offsetIntoDestination, size_t dataLength);
CM_EXPORT OSStatus CMBlockBufferFillDataBytes(char fillByte, CMBlockBufferRef destinationBuffer,
                                              size_t offsetIntoDestination, size_t dataLength);
CM_EXPORT OSStatus CMBlockBufferGetDataPointer(CMBlockBufferRef theBuffer, size_t offset, size_t *lengthAtOffsetOut,
                                               size_t *totalLengthOut, char **dataPointerOut);
CM_EXPORT size_t CMBlockBufferGetDataLength(CMBlockBufferRef theBuffer);
CM_EXPORT Boolean CMBlockBufferIsRangeContiguous(CMBlockBufferRef theBuffer, size_t offset, size_t length);
CM_EXPORT Boolean CMBlockBufferIsEmpty(CMBlockBufferRef theBuffer);

#endif
