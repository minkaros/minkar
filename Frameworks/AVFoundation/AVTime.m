/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Times kept in NSValues by their bytes; archived as CoreMedia's own
 * dictionaries (CMTimeCopyAsDictionary), so they read back exactly.
 */

#import <AVFoundation/AVTime.h>

@implementation NSValue (NSValueCMTimeExtensions)

+ (NSValue *)valueWithCMTime:(CMTime)time {
    return [NSValue valueWithBytes:&time objCType:@encode(CMTime)];
}

- (CMTime)CMTimeValue {
    CMTime t = kCMTimeInvalid;
    if (strcmp(self.objCType, @encode(CMTime)) == 0)
        [self getValue:&t];
    return t;
}

+ (NSValue *)valueWithCMTimeRange:(CMTimeRange)range {
    return [NSValue valueWithBytes:&range objCType:@encode(CMTimeRange)];
}

- (CMTimeRange)CMTimeRangeValue {
    CMTimeRange r = kCMTimeRangeInvalid;
    if (strcmp(self.objCType, @encode(CMTimeRange)) == 0)
        [self getValue:&r];
    return r;
}

+ (NSValue *)valueWithCMTimeMapping:(CMTimeMapping)mapping {
    return [NSValue valueWithBytes:&mapping objCType:@encode(CMTimeMapping)];
}

- (CMTimeMapping)CMTimeMappingValue {
    CMTimeMapping m = kCMTimeMappingInvalid;
    if (strcmp(self.objCType, @encode(CMTimeMapping)) == 0)
        [self getValue:&m];
    return m;
}

@end

@implementation NSCoder (AVTimeCoding)

- (void)encodeCMTime:(CMTime)time forKey:(NSString *)key {
    NSDictionary *d = CFBridgingRelease(CMTimeCopyAsDictionary(time, NULL));
    [self encodeObject:d forKey:key];
}

- (CMTime)decodeCMTimeForKey:(NSString *)key {
    NSDictionary *d = [self decodeObjectForKey:key];
    return d != nil ? CMTimeMakeFromDictionary((__bridge CFDictionaryRef)d) : kCMTimeInvalid;
}

- (void)encodeCMTimeRange:(CMTimeRange)range forKey:(NSString *)key {
    NSDictionary *d = CFBridgingRelease(CMTimeRangeCopyAsDictionary(range, NULL));
    [self encodeObject:d forKey:key];
}

- (CMTimeRange)decodeCMTimeRangeForKey:(NSString *)key {
    NSDictionary *d = [self decodeObjectForKey:key];
    return d != nil ? CMTimeRangeMakeFromDictionary((__bridge CFDictionaryRef)d) : kCMTimeRangeInvalid;
}

- (void)encodeCMTimeMapping:(CMTimeMapping)mapping forKey:(NSString *)key {
    NSDictionary *d = CFBridgingRelease(CMTimeMappingCopyAsDictionary(mapping, NULL));
    [self encodeObject:d forKey:key];
}

- (CMTimeMapping)decodeCMTimeMappingForKey:(NSString *)key {
    NSDictionary *d = [self decodeObjectForKey:key];
    return d != nil ? CMTimeMappingMakeFromDictionary((__bridge CFDictionaryRef)d) : kCMTimeMappingInvalid;
}

@end
