/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What System Monitor shows, sampled from the system: the processes
 * (libproc) with their CPU, memory and disk use since the last sample,
 * and the system's CPU load, memory and network traffic (Mach host
 * statistics, sysctl, the interfaces' counters).
 */

#import <Foundation/Foundation.h>

@interface SMProcess : NSObject
@property (readonly) pid_t pid;
@property (readonly) pid_t parentPID;
@property (readonly, copy) NSString *name;
@property (readonly, copy) NSString *path;
@property (readonly, copy) NSString *user;
@property (readonly) double cpuPercent;          /* of one processor, since the last sample */
@property (readonly) double cpuTime;             /* seconds */
@property (readonly) uint64_t memory;            /* resident bytes */
@property (readonly) int threads;
@property (readonly) uint64_t bytesRead;         /* since it started */
@property (readonly) uint64_t bytesWritten;
@property (readonly) double readRate;            /* bytes a second, since the last sample */
@property (readonly) double writeRate;
@property (readonly, copy) NSDate *startDate;
@end

@interface SMInterface : NSObject
@property (readonly, copy) NSString *name;
@property (readonly) uint64_t bytesIn, bytesOut;
@property (readonly) double inRate, outRate;     /* bytes a second */
@end

@interface SMSampler : NSObject
/* Takes a sample: the rates are against the one before. */
- (void)sample;
@property (readonly, copy) NSArray<SMProcess *> *processes;
@property (readonly, copy) NSArray<SMInterface *> *interfaces;
@property (readonly) double userPercent, systemPercent, idlePercent;   /* of the whole machine */
@property (readonly) uint64_t physicalMemory, usedMemory, wiredMemory, freeMemory;
@property (readonly) double readRate, writeRate;   /* all processes */
@property (readonly) double inRate, outRate;       /* all interfaces but loopback */
@property (readonly) int processors;
- (SMProcess *)processWithPID:(pid_t)pid;
@end
