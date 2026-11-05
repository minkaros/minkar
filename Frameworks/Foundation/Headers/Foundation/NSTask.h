/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>

@class NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSTaskDidTerminateNotification;

@class NSURL, NSError;

typedef NS_ENUM(NSInteger, NSTaskTerminationReason) {
    NSTaskTerminationReasonExit = 1,
    NSTaskTerminationReasonUncaughtSignal = 2
};

@interface NSTask : NSObject {
    NSString *launchPath;
    NSArray *arguments;
    NSString *currentDirectoryPath;
    NSDictionary *environment;

    id standardInput;
    id standardOutput;
    id standardError;
}

- (instancetype)init;

@property (nullable, copy) NSURL *executableURL;
@property (nullable, copy) NSArray<NSString *> *arguments;
@property (nullable, copy) NSDictionary<NSString *, NSString *> *environment;
@property (nullable, copy) NSURL *currentDirectoryURL;

@property (nullable, retain) id standardInput;
@property (nullable, retain) id standardOutput;
@property (nullable, retain) id standardError;

- (BOOL)launchAndReturnError:(out NSError **_Nullable)error NS_SWIFT_NAME(run());

- (void)interrupt;
- (void)terminate;

- (BOOL)suspend;
- (BOOL)resume;

@property (readonly) int processIdentifier;
@property (readonly, getter=isRunning) BOOL running;

@property (readonly) int terminationStatus;
@property (readonly) NSTaskTerminationReason terminationReason;

@property (nullable, copy) void (^terminationHandler)(NSTask *);

@property NSQualityOfService qualityOfService;

+ (nullable NSTask *)launchedTaskWithExecutableURL:(NSURL *)url arguments:(NSArray<NSString *> *)arguments error:(out NSError **_Nullable)error
                                terminationHandler:(void (^_Nullable)(NSTask *))terminationHandler
    NS_SWIFT_NAME(run(_:arguments:terminationHandler:));

- (void)waitUntilExit;
- (void)blockAndWaitUntilExit;

@end

@interface NSTask (NSDeprecated)
@property (nullable, copy) NSString *launchPath;
@property (copy) NSString *currentDirectoryPath;
- (void)launch;
+ (NSTask *)launchedTaskWithLaunchPath:(NSString *)path arguments:(NSArray<NSString *> *)arguments;
@end

NS_ASSUME_NONNULL_END
