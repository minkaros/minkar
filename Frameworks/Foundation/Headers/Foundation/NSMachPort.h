/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/NSPort.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSMachPortOptions) {
    NSMachPortDeallocateNone = 0,
    NSMachPortDeallocateSendRight = (1UL << 0),
    NSMachPortDeallocateReceiveRight = (1UL << 1)
};

@protocol NSMachPortDelegate <NSPortDelegate>
@optional
- (void)handleMachMessage:(void *)msg;
@end

@interface NSMachPort : NSPort {
    uint32_t _machPort;
    NSMachPortOptions _options;
    id _delegate;
    void *_source;
    BOOL _valid;
}

+ (NSPort *)portWithMachPort:(uint32_t)machPort;
+ (NSPort *)portWithMachPort:(uint32_t)machPort options:(NSMachPortOptions)options;
- (instancetype)initWithMachPort:(uint32_t)machPort;
- (instancetype)initWithMachPort:(uint32_t)machPort options:(NSMachPortOptions)options;

- (void)setDelegate:(nullable id<NSMachPortDelegate>)anObject;
- (nullable id<NSMachPortDelegate>)delegate;

@property (readonly) uint32_t machPort;

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;

@end

NS_ASSUME_NONNULL_END
