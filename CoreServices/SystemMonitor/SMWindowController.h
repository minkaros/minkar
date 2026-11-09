/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "SMSampler.h"

typedef NS_ENUM(NSInteger, SMTab) {
    SMTabCPU = 0,
    SMTabMemory,
    SMTabDisk,
    SMTabNetwork,
};

/* System Monitor's window: a tab for CPU, memory, disk and network, the
 * processes (or, for the network, the interfaces) in a table that sorts
 * by any column and filters by name, and the system's totals with their
 * history below. It samples every two seconds. */
@interface SMWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate>
@property (readonly) SMSampler *sampler;
@property (nonatomic) SMTab tab;
@property (nonatomic, copy) NSString *searchString;
/* The rows shown: SMProcess objects (SMInterface on the network tab). */
@property (readonly, copy) NSArray *rows;
/* Sorts by a column's key (as "cpuPercent", "memory", "name"). */
- (void)sortByKey:(NSString *)key ascending:(BOOL)ascending;
- (void)refresh;
/* Ends a process: SIGTERM, or SIGKILL when forced (no question asked). */
- (BOOL)stopProcess:(pid_t)pid force:(BOOL)force;
- (IBAction)stopSelectedProcess:(id)sender;
- (IBAction)showProcessInfo:(id)sender;
- (IBAction)chooseTab:(id)sender;
@end
