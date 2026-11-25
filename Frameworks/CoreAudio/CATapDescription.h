/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A process tap's description (AudioHardwareCreateProcessTap): which
 * processes' output it takes (process objects, or every process but
 * those), mixed down to stereo or mono or not, from one device's stream
 * or all, private to its process or not, and whether the tapped audio is
 * still heard. As the public documentation describes it.
 */

#ifndef __CATAPDESCRIPTION__
#define __CATAPDESCRIPTION__

#ifdef __OBJC__
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, CATapMuteBehavior) {
    CATapUnmuted = 0,
    CATapMuted = 1,
    CATapMutedWhenTapped = 2,
};

@interface CATapDescription : NSObject <NSSecureCoding, NSCopying>

@property (copy) NSString *name;
/* AudioObjectIDs of process objects (kAudioHardwarePropertyTranslatePIDToProcessObject). */
@property (copy) NSArray<NSNumber *> *processes;
@property (getter=isPrivate) BOOL privateTap;
@property CATapMuteBehavior muteBehavior;
@property (getter=isMixdown) BOOL mixdown;
@property (getter=isMono) BOOL mono;
/* YES: every process but processes. */
@property (getter=isExclusive) BOOL exclusive;
@property (copy) NSString *deviceUID;
@property (copy) NSNumber *stream;
@property (copy) NSUUID *UUID;

- (instancetype)init;
- (instancetype)initStereoMixdownOfProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initStereoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initMonoMixdownOfProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initMonoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initWithProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID
                       withStream:(NSInteger)stream;
- (instancetype)initExcludingProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID
                            withStream:(NSInteger)stream;

@end

#endif

#endif
