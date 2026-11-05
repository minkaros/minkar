/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRunLoop.h>

@class NSData, NSArray<ObjectType>;

FOUNDATION_EXPORT NSString *const NSFileHandleConnectionAcceptedNotification;
FOUNDATION_EXPORT NSString *const NSFileHandleDataAvailableNotification;
FOUNDATION_EXPORT NSString *const NSFileHandleReadCompletionNotification;
FOUNDATION_EXPORT NSString *const NSFileHandleReadToEndOfFileCompletionNotification;

FOUNDATION_EXPORT NSString *const NSFileHandleNotificationDataItem;
FOUNDATION_EXPORT NSString *const NSFileHandleNotificationFileHandleItem;

FOUNDATION_EXPORT NSString *const NSFileHandleNotificationMonitorModes;

FOUNDATION_EXPORT NSString *const NSFileHandleOperationException;

NS_ASSUME_NONNULL_BEGIN

@interface NSFileHandle : NSObject

+ (nullable instancetype)fileHandleForReadingAtPath:(NSString *)path;
+ (nullable instancetype)fileHandleForWritingAtPath:(NSString *)path;
+ (nullable instancetype)fileHandleForUpdatingAtPath:(NSString *)path;

@property(class, readonly, strong) NSFileHandle *fileHandleWithStandardInput;
@property(class, readonly, strong) NSFileHandle *fileHandleWithStandardOutput;
@property(class, readonly, strong) NSFileHandle *fileHandleWithStandardError;
@property(class, readonly, strong) NSFileHandle *fileHandleWithNullDevice;

- (instancetype)initWithFileDescriptor:(int)descriptor;
- (instancetype)initWithFileDescriptor:(int)descriptor closeOnDealloc:(BOOL)closeOnDealloc;

@property(readonly) int fileDescriptor;

- (void)closeFile;
- (void)synchronizeFile;

@property(readonly) unsigned long long offsetInFile;
- (void)seekToFileOffset:(unsigned long long)offset;
- (unsigned long long)seekToEndOfFile;

- (NSData *)readDataOfLength:(NSUInteger)length;
- (NSData *)readDataToEndOfFile;
@property(readonly, copy) NSData *availableData;

- (void)writeData:(NSData *)data;

- (void)truncateFileAtOffset:(unsigned long long)offset;

- (void)readInBackgroundAndNotifyForModes:(nullable NSArray<NSRunLoopMode> *)modes;
- (void)readInBackgroundAndNotify;

- (void)readToEndOfFileInBackgroundAndNotifyForModes:(nullable NSArray<NSRunLoopMode> *)modes;
- (void)readToEndOfFileInBackgroundAndNotify;

- (void)acceptConnectionInBackgroundAndNotifyForModes:(nullable NSArray<NSRunLoopMode> *)modes;
- (void)acceptConnectionInBackgroundAndNotify;

- (void)waitForDataInBackgroundAndNotifyForModes:(nullable NSArray<NSRunLoopMode> *)modes;
- (void)waitForDataInBackgroundAndNotify;

@end

@class NSURL, NSError;

@interface NSFileHandle (NSFileHandleErrors)
+ (nullable instancetype)fileHandleForReadingFromURL:(NSURL *)url error:(NSError **)error;
+ (nullable instancetype)fileHandleForWritingToURL:(NSURL *)url error:(NSError **)error;
+ (nullable instancetype)fileHandleForUpdatingURL:(NSURL *)url error:(NSError **)error;
- (nullable NSData *)readDataToEndOfFileAndReturnError:(NSError **)error;
- (nullable NSData *)readDataUpToLength:(NSUInteger)length error:(NSError **)error;
- (BOOL)writeData:(NSData *)data error:(NSError **)error;
- (BOOL)getOffset:(out unsigned long long *)offsetInFile error:(NSError **)error;
- (BOOL)seekToEndReturningOffset:(out unsigned long long *_Nullable)offsetInFile error:(NSError **)error;
- (BOOL)seekToOffset:(unsigned long long)offset error:(NSError **)error;
- (BOOL)truncateAtOffset:(unsigned long long)offset error:(NSError **)error;
- (BOOL)synchronizeAndReturnError:(NSError **)error;
- (BOOL)closeAndReturnError:(NSError **)error;
@property(nullable, copy) void (^readabilityHandler)(NSFileHandle *);
@property(nullable, copy) void (^writeabilityHandler)(NSFileHandle *);
@end

NS_ASSUME_NONNULL_END
