/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * os_log's messages for Logs, read from the log command: the last hour
 * (log show), then as they come (log stream), one JSON object a line
 * (--style ndjson). logd decides who sees what: a user their own, root
 * all of them.
 */

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, LGLevel) {
    LGLevelDefault = 0,
    LGLevelInfo,
    LGLevelDebug,
};

@interface LGMessage : NSObject
@property (copy) NSString *timestamp;
@property (copy) NSString *type;        /* Default, Info, Debug, Error, Fault */
@property pid_t pid;
@property (copy) NSString *process;
@property (copy) NSString *subsystem;
@property (copy) NSString *category;
@property (copy) NSString *message;
+ (instancetype)messageWithJSONLine:(NSData *)line;
@end

@interface LGLogReader : NSObject
/* Called on the main thread with each group of messages read. */
@property (copy) void (^handler)(NSArray<LGMessage *> *messages);
@property (readonly) LGLevel level;
@property (readonly, getter=isStreaming) BOOL streaming;
/* The last hour, then the stream, at this level (info and debug too). */
- (void)startAtLevel:(LGLevel)level;
- (void)pause;
- (void)resume;
- (void)stop;
@end
