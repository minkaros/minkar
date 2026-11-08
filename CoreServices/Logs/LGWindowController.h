/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "LGLogReader.h"

typedef NS_ENUM(NSInteger, LGSource) {
    LGSourceMessages = 0,      /* os_log's, through log show and log stream */
    LGSourceCrashReports,      /* DiagnosticReports, the system's and the user's */
    LGSourceSystemLogs,        /* /var/log */
    LGSourceUserLogs,          /* ~/Library/Logs */
};

typedef NS_ENUM(NSInteger, LGLevelChoice) {
    LGChoiceDefault = 0,
    LGChoiceInfo,
    LGChoiceDebug,
    LGChoiceErrors,            /* default level, errors and faults only */
};

/* Logs's window: the sources on the left; os_log's messages, filtered by
 * level, process, subsystem and text, with the selected one in full
 * below; or a source's files, newest first, the selected one's text. */
@interface LGWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate>
@property (readonly) LGLogReader *reader;
@property (nonatomic) LGSource source;
@property (nonatomic) LGLevelChoice level;
@property (nonatomic, copy) NSString *processFilter, *subsystemFilter, *searchString;
@property (readonly, copy) NSArray<LGMessage *> *messages;          /* all read */
@property (readonly, copy) NSArray<LGMessage *> *visibleMessages;   /* after the filters */
@property (readonly, copy) NSArray<NSString *> *files;              /* the source's, newest first */
@property (readonly, copy) NSString *shownText;                     /* the detail or the file's text */
- (void)showFile:(NSString *)path;
- (IBAction)togglePause:(id)sender;
- (IBAction)clear:(id)sender;
- (IBAction)reveal:(id)sender;
@end
