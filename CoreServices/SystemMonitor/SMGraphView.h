/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

/* A history of up to two series, newest on the right, scaled to the
 * largest value shown (or to a fixed maximum, as 100 for percentages). */
@interface SMGraphView : NSView
@property double maximum;                 /* 0: the largest value shown */
@property (copy) NSArray<NSColor *> *colors;
- (void)addValues:(NSArray<NSNumber *> *)values;
- (void)clear;
/* The points shown, oldest first (each an array of the series' values):
   a window keeps one history per tab and shows the one asked for. */
@property (nonatomic, strong) NSMutableArray<NSArray<NSNumber *> *> *history;
/* The most points a history keeps. */
+ (NSUInteger)capacity;
@end
