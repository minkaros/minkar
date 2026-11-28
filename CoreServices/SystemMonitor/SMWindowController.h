/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "SMSampler.h"

/* Which processes the table lists (View menu). */
typedef NS_ENUM(NSInteger, SMProcessFilter) {
    SMFilterAll = 0,
    SMFilterMine,          /* the user's own */
    SMFilterSystem,        /* root's and the system's accounts' */
    SMFilterWindowed,      /* applications with windows (in the Dock) */
};

typedef NS_ENUM(NSInteger, SMTab) {
    SMTabCPU = 0,
    SMTabMemory,
    SMTabDisk,
    SMTabNetwork,
};

/* System Monitor's window: a tab for CPU, memory, disk and network, the
 * processes (or, for the network, the interfaces) in a table that sorts
 * by any column and filters by name, and the system's totals with their
 * history below (one per tab, kept while another is shown). It samples
 * every one, two (the default) or five seconds, whatever the user does. */
@interface SMWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@property (readonly) SMSampler *sampler;
@property (nonatomic) SMTab tab;
@property (nonatomic, copy) NSString *searchString;
@property (nonatomic) SMProcessFilter processFilter;      /* kept in the defaults */
@property (nonatomic) NSTimeInterval updateInterval;     /* 1, 2 or 5 s; kept in the defaults */
/* A tab's history: one point a sample, the newest last. */
- (NSArray *)historyForTab:(SMTab)tab;
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
/* From the View menu: the item's tag is the filter, or the seconds. */
- (IBAction)chooseProcessFilter:(id)sender;
- (IBAction)chooseUpdateInterval:(id)sender;
@end
