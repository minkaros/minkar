/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "LGLogReader.h"

static NSString *const kLog = @"/usr/bin/log";

@implementation LGMessage

+ (instancetype)messageWithJSONLine:(NSData *)line
{
    NSDictionary *d = [NSJSONSerialization JSONObjectWithData:line options:0 error:NULL];
    LGMessage *m;

    if (![d isKindOfClass:[NSDictionary class]])
        return nil;
    m = [LGMessage new];
    m.timestamp = d[@"timestamp"] ?: @"";
    m.type = d[@"messageType"] ?: @"Default";
    m.pid = [d[@"processID"] intValue];
    m.process = d[@"process"] ?: @"";
    m.subsystem = d[@"subsystem"] ?: @"";
    m.category = d[@"category"] ?: @"";
    m.message = d[@"eventMessage"] ?: @"";
    return m;
}

@end

@implementation LGLogReader {
    NSTask *_task;
    NSMutableData *_pending;
    BOOL _paused;
}

static NSArray *levelArguments(LGLevel level)
{
    switch (level) {
    case LGLevelInfo:
        return @[ @"--info" ];
    case LGLevelDebug:
        return @[ @"--info", @"--debug" ];
    default:
        return @[];
    }
}

static NSString *levelName(LGLevel level)
{
    return level == LGLevelDebug ? @"debug" : level == LGLevelInfo ? @"info" : @"default";
}

/* The complete lines read so far, as messages; a part line waits. */
- (NSArray *)takeMessages:(NSData *)data
{
    NSMutableArray *messages = [NSMutableArray array];
    const char *bytes;
    NSUInteger start = 0, length;

    [_pending appendData:data];
    bytes = [_pending bytes];
    length = [_pending length];
    for (NSUInteger i = 0; i < length; i++)
        if (bytes[i] == '\n') {
            if (i > start) {
                LGMessage *m = [LGMessage messageWithJSONLine:[_pending subdataWithRange:NSMakeRange(start, i - start)]];
                if (m != nil)
                    [messages addObject:m];
            }
            start = i + 1;
        }
    [_pending replaceBytesInRange:NSMakeRange(0, start) withBytes:NULL length:0];
    return messages;
}

- (void)deliver:(NSArray *)messages
{
    if ([messages count] == 0 || self.handler == nil)
        return;
    if ([NSThread isMainThread])
        self.handler(messages);
    else
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.handler != nil)
                self.handler(messages);
        });
}

- (void)startAtLevel:(LGLevel)level
{
    [self stop];
    _level = level;
    _pending = [NSMutableData data];

    /* The last hour, read whole. */
    NSTask *show = [NSTask new];
    NSPipe *pipe = [NSPipe pipe];
    [show setLaunchPath:kLog];
    [show setArguments:[@[ @"show", @"--last", @"1h", @"--style", @"ndjson" ]
        arrayByAddingObjectsFromArray:levelArguments(level)]];
    [show setStandardOutput:pipe];
    [show setStandardError:[NSFileHandle fileHandleWithNullDevice]];
    @try {
        [show launch];
        NSData *all = [[pipe fileHandleForReading] readDataToEndOfFile];
        [show waitUntilExit];
        [self deliver:[self takeMessages:all]];
    } @catch (NSException *e) {
    }
    _pending = [NSMutableData data];
    [self startStream];
}

- (void)startStream
{
    NSPipe *pipe = [NSPipe pipe];

    _task = [NSTask new];
    [_task setLaunchPath:kLog];
    [_task setArguments:@[ @"stream", @"--style", @"ndjson", @"--level", levelName(_level) ]];
    [_task setStandardOutput:pipe];
    [_task setStandardError:[NSFileHandle fileHandleWithNullDevice]];
    __weak LGLogReader *weakSelf = self;
    [[pipe fileHandleForReading] setReadabilityHandler:^(NSFileHandle *handle) {
        NSData *data = [handle availableData];
        LGLogReader *reader = weakSelf;

        if ([data length] == 0) {
            [handle setReadabilityHandler:nil];
            return;
        }
        if (reader == nil)
            return;
        @synchronized (reader) {
            NSArray *messages = [reader takeMessages:data];
            if (!reader->_paused)
                [reader deliver:messages];
        }
    }];
    @try {
        [_task launch];
        _streaming = YES;
    } @catch (NSException *e) {
        _task = nil;
    }
}

- (void)pause
{
    @synchronized (self) {
        _paused = YES;
    }
}

- (void)resume
{
    @synchronized (self) {
        _paused = NO;
    }
}

- (void)stop
{
    if (_task != nil) {
        [[[_task standardOutput] fileHandleForReading] setReadabilityHandler:nil];
        if ([_task isRunning])
            [_task terminate];
        _task = nil;
    }
    _streaming = NO;
}

- (void)dealloc
{
    [self stop];
}

@end
