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

/*
 * NSFileHandle's methods that report errors (API 19), its readability and
 * writeability handlers (dispatch sources on its descriptor), and the URL
 * forms of its constructors.
 */

#import <Foundation/NSFileHandle.h>
#import <Foundation/NSData.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSURL.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <objc/runtime.h>
#include <unistd.h>

static NSError *posixError(void) {
    return [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
}

static char readabilityKey, writeabilityKey, readSourceKey, writeSourceKey;

@implementation NSFileHandle (NSFileHandleErrors)

+ (instancetype)_fileHandleForURL:(NSURL *)url flags:(int)flags error:(NSError **)error {
    int fd = open([[url path] fileSystemRepresentation], flags | O_CLOEXEC);
    if (fd < 0) {
        if (error) *error = posixError();
        return nil;
    }
    return [[self alloc] initWithFileDescriptor:fd closeOnDealloc:YES];
}

+ (instancetype)fileHandleForReadingFromURL:(NSURL *)url error:(NSError **)error {
    return [self _fileHandleForURL:url flags:O_RDONLY error:error];
}

+ (instancetype)fileHandleForWritingToURL:(NSURL *)url error:(NSError **)error {
    return [self _fileHandleForURL:url flags:O_WRONLY error:error];
}

+ (instancetype)fileHandleForUpdatingURL:(NSURL *)url error:(NSError **)error {
    return [self _fileHandleForURL:url flags:O_RDWR error:error];
}

- (NSData *)readDataToEndOfFileAndReturnError:(NSError **)error {
    NSMutableData *data = [NSMutableData data];
    uint8_t buffer[65536];
    for (;;) {
        ssize_t n = read([self fileDescriptor], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            if (error) *error = posixError();
            return nil;
        }
        if (n == 0)
            return data;
        [data appendBytes:buffer length:(NSUInteger)n];
    }
}

// Up to count bytes: fewer at the end of the file (or the pipe).
- (NSData *)readDataUpToLength:(NSUInteger)count error:(NSError **)error {
    NSMutableData *data = [NSMutableData dataWithLength:count];
    NSUInteger got = 0;
    while (got < count) {
        ssize_t n = read([self fileDescriptor], (uint8_t *)[data mutableBytes] + got, count - got);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            if (error) *error = posixError();
            return nil;
        }
        if (n == 0)
            break;
        got += (NSUInteger)n;
    }
    [data setLength:got];
    return data;
}

- (BOOL)writeData:(NSData *)data error:(NSError **)error {
    const uint8_t *bytes = [data bytes];
    NSUInteger left = [data length];
    while (left > 0) {
        ssize_t n = write([self fileDescriptor], bytes, left);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            if (error) *error = posixError();
            return NO;
        }
        bytes += n;
        left -= (NSUInteger)n;
    }
    return YES;
}

- (BOOL)getOffset:(unsigned long long *)offsetInFile error:(NSError **)error {
    off_t offset = lseek([self fileDescriptor], 0, SEEK_CUR);
    if (offset < 0) {
        if (error) *error = posixError();
        return NO;
    }
    if (offsetInFile) *offsetInFile = (unsigned long long)offset;
    return YES;
}

- (BOOL)seekToEndReturningOffset:(unsigned long long *)offsetInFile error:(NSError **)error {
    off_t offset = lseek([self fileDescriptor], 0, SEEK_END);
    if (offset < 0) {
        if (error) *error = posixError();
        return NO;
    }
    if (offsetInFile) *offsetInFile = (unsigned long long)offset;
    return YES;
}

- (BOOL)seekToOffset:(unsigned long long)offset error:(NSError **)error {
    if (lseek([self fileDescriptor], (off_t)offset, SEEK_SET) < 0) {
        if (error) *error = posixError();
        return NO;
    }
    return YES;
}

- (BOOL)truncateAtOffset:(unsigned long long)offset error:(NSError **)error {
    if (ftruncate([self fileDescriptor], (off_t)offset) != 0 ||
        lseek([self fileDescriptor], (off_t)offset, SEEK_SET) < 0) {
        if (error) *error = posixError();
        return NO;
    }
    return YES;
}

- (BOOL)synchronizeAndReturnError:(NSError **)error {
    if (fsync([self fileDescriptor]) != 0) {
        if (error) *error = posixError();
        return NO;
    }
    return YES;
}

- (BOOL)closeAndReturnError:(NSError **)error {
    [self setReadabilityHandler:nil];
    [self setWriteabilityHandler:nil];
    [self closeFile];
    return YES;
}

#pragma mark Handlers

// The handler runs on a dispatch queue each time the descriptor can be read
// (or written); nil stops it.
- (void)_setHandler:(void (^)(NSFileHandle *))handler key:(char *)key sourceKey:(char *)sourceKey
               type:(dispatch_source_type_t)type {
    dispatch_source_t old = objc_getAssociatedObject(self, sourceKey);
    if (old != nil)
        dispatch_source_cancel(old);
    objc_setAssociatedObject(self, key, handler, OBJC_ASSOCIATION_COPY);
    objc_setAssociatedObject(self, sourceKey, nil, OBJC_ASSOCIATION_RETAIN);
    if (handler == nil)
        return;
    dispatch_source_t source = dispatch_source_create(type, (uintptr_t)[self fileDescriptor], 0,
                                                      dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
    __weak NSFileHandle *weakSelf = self;
    void (^copy)(NSFileHandle *) = [handler copy];
    dispatch_source_set_event_handler(source, ^{
        NSFileHandle *strongSelf = weakSelf;
        if (strongSelf != nil)
            copy(strongSelf);
    });
    objc_setAssociatedObject(self, sourceKey, source, OBJC_ASSOCIATION_RETAIN);
    dispatch_resume(source);
}

- (void (^)(NSFileHandle *))readabilityHandler {
    return objc_getAssociatedObject(self, &readabilityKey);
}

- (void)setReadabilityHandler:(void (^)(NSFileHandle *))handler {
    [self _setHandler:handler key:&readabilityKey sourceKey:&readSourceKey type:DISPATCH_SOURCE_TYPE_READ];
}

- (void (^)(NSFileHandle *))writeabilityHandler {
    return objc_getAssociatedObject(self, &writeabilityKey);
}

- (void)setWriteabilityHandler:(void (^)(NSFileHandle *))handler {
    [self _setHandler:handler key:&writeabilityKey sourceKey:&writeSourceKey type:DISPATCH_SOURCE_TYPE_WRITE];
}

@end
