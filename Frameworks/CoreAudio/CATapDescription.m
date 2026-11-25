/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CATapDescription (CATapDescription.h): the description's values, kept
 * and coded; the tap itself is the audio service's.
 */

#import "CATapDescription.h"

@implementation CATapDescription

@synthesize name = _name, processes = _processes, privateTap = _privateTap, muteBehavior = _muteBehavior,
            mixdown = _mixdown, mono = _mono, exclusive = _exclusive, deviceUID = _deviceUID, stream = _stream,
            UUID = _UUID;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _name = [@"" copy];
        _processes = [[NSArray alloc] init];
        _UUID = [[NSUUID alloc] init];
        _mixdown = YES;
        _muteBehavior = CATapUnmuted;
    }
    return self;
}

- (instancetype)initWithProcesses:(NSArray *)processes exclusive:(BOOL)exclusive mono:(BOOL)mono {
    if ((self = [self init]) != nil) {
        [_processes release];
        _processes = [processes copy];
        _exclusive = exclusive;
        _mono = mono;
        _mixdown = YES;
    }
    return self;
}

- (instancetype)initStereoMixdownOfProcesses:(NSArray *)processes {
    return [self initWithProcesses:processes exclusive:NO mono:NO];
}

- (instancetype)initStereoGlobalTapButExcludeProcesses:(NSArray *)processes {
    return [self initWithProcesses:processes exclusive:YES mono:NO];
}

- (instancetype)initMonoMixdownOfProcesses:(NSArray *)processes {
    return [self initWithProcesses:processes exclusive:NO mono:YES];
}

- (instancetype)initMonoGlobalTapButExcludeProcesses:(NSArray *)processes {
    return [self initWithProcesses:processes exclusive:YES mono:YES];
}

/* One device's stream, not mixed down. */
- (instancetype)initWithProcesses:(NSArray *)processes andDeviceUID:(NSString *)deviceUID withStream:(NSInteger)stream {
    if ((self = [self initWithProcesses:processes exclusive:NO mono:NO]) != nil) {
        _mixdown = NO;
        _deviceUID = [deviceUID copy];
        _stream = [[NSNumber alloc] initWithInteger:stream];
    }
    return self;
}

- (instancetype)initExcludingProcesses:(NSArray *)processes andDeviceUID:(NSString *)deviceUID
                            withStream:(NSInteger)stream {
    if ((self = [self initWithProcesses:processes andDeviceUID:deviceUID withStream:stream]) != nil)
        _exclusive = YES;
    return self;
}

- (void)dealloc {
    [_name release];
    [_processes release];
    [_deviceUID release];
    [_stream release];
    [_UUID release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    CATapDescription *copy = [[[self class] allocWithZone:zone] init];
    copy.name = _name;
    copy.processes = _processes;
    copy.privateTap = _privateTap;
    copy.muteBehavior = _muteBehavior;
    copy.mixdown = _mixdown;
    copy.mono = _mono;
    copy.exclusive = _exclusive;
    copy.deviceUID = _deviceUID;
    copy.stream = _stream;
    copy.UUID = _UUID;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_name forKey:@"name"];
    [coder encodeObject:_processes forKey:@"processes"];
    [coder encodeBool:_privateTap forKey:@"private"];
    [coder encodeInteger:_muteBehavior forKey:@"mute"];
    [coder encodeBool:_mixdown forKey:@"mixdown"];
    [coder encodeBool:_mono forKey:@"mono"];
    [coder encodeBool:_exclusive forKey:@"exclusive"];
    [coder encodeObject:_deviceUID forKey:@"device"];
    [coder encodeObject:_stream forKey:@"stream"];
    [coder encodeObject:_UUID forKey:@"uuid"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) != nil) {
        NSSet *numbers = [NSSet setWithObjects:[NSArray class], [NSNumber class], nil];
        self.name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"] ?: @"";
        self.processes = [coder decodeObjectOfClasses:numbers forKey:@"processes"] ?: @[];
        _privateTap = [coder decodeBoolForKey:@"private"];
        _muteBehavior = [coder decodeIntegerForKey:@"mute"];
        _mixdown = [coder decodeBoolForKey:@"mixdown"];
        _mono = [coder decodeBoolForKey:@"mono"];
        _exclusive = [coder decodeBoolForKey:@"exclusive"];
        self.deviceUID = [coder decodeObjectOfClass:[NSString class] forKey:@"device"];
        self.stream = [coder decodeObjectOfClass:[NSNumber class] forKey:@"stream"];
        NSUUID *uuid = [coder decodeObjectOfClass:[NSUUID class] forKey:@"uuid"];
        if (uuid != nil)
            self.UUID = uuid;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<CATapDescription %@ \"%@\" processes %@%@%@%@>", _UUID, _name, _processes,
            _exclusive ? @" (excluded)" : @"", _mono ? @" mono" : @"", _mixdown ? @" mixdown" : @""];
}

@end
