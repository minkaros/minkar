/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/NSDateInterval.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>

@implementation NSDateInterval {
    NSDate *_startDate;
    NSTimeInterval _duration;
}

- (instancetype)init {
    return [self initWithStartDate:[NSDate date] duration:0];
}

- (instancetype)initWithStartDate:(NSDate *)startDate duration:(NSTimeInterval)duration {
    if (duration < 0)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSDateInterval initWithStartDate:duration:]: negative duration"];
    if ((self = [super init]) != nil) {
        _startDate = [startDate copy] ?: [NSDate date];
        _duration = duration;
    }
    return self;
}

- (instancetype)initWithStartDate:(NSDate *)startDate endDate:(NSDate *)endDate {
    if ([endDate compare:startDate] == NSOrderedAscending)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSDateInterval initWithStartDate:endDate:]: end before start"];
    return [self initWithStartDate:startDate duration:[endDate timeIntervalSinceDate:startDate]];
}

- (NSDate *)startDate { return _startDate; }
- (NSDate *)endDate { return [_startDate dateByAddingTimeInterval:_duration]; }
- (NSTimeInterval)duration { return _duration; }

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithStartDate:[coder decodeObjectOfClass:[NSDate class] forKey:@"NS.startDate"]
                          duration:[coder decodeDoubleForKey:@"NS.duration"]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_startDate forKey:@"NS.startDate"];
    [coder encodeDouble:_duration forKey:@"NS.duration"];
}

// By start date, then by duration.
- (NSComparisonResult)compare:(NSDateInterval *)other {
    NSComparisonResult result = [_startDate compare:[other startDate]];
    if (result != NSOrderedSame)
        return result;
    if (_duration < [other duration]) return NSOrderedAscending;
    if (_duration > [other duration]) return NSOrderedDescending;
    return NSOrderedSame;
}

- (BOOL)isEqualToDateInterval:(NSDateInterval *)other {
    return other != nil && [self compare:other] == NSOrderedSame;
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[NSDateInterval class]] && [self isEqualToDateInterval:other];
}

- (NSUInteger)hash {
    return [_startDate hash] ^ (NSUInteger)_duration;
}

- (BOOL)intersectsDateInterval:(NSDateInterval *)other {
    return [self intersectionWithDateInterval:other] != nil;
}

- (NSDateInterval *)intersectionWithDateInterval:(NSDateInterval *)other {
    NSDate *start = [[_startDate laterDate:[other startDate]] copy];
    NSDate *end = [[self endDate] earlierDate:[other endDate]];
    if ([end compare:start] == NSOrderedAscending)
        return nil;
    return [[NSDateInterval alloc] initWithStartDate:start endDate:end];
}

- (BOOL)containsDate:(NSDate *)date {
    return [date compare:_startDate] != NSOrderedAscending && [date compare:[self endDate]] != NSOrderedDescending;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ to %@", _startDate, [self endDate]];
}

@end
