/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSLock;

typedef struct {
    NSInteger majorVersion;
    NSInteger minorVersion;
    NSInteger patchVersion;
} NSOperatingSystemVersion;

enum {
    NSWindowsNTOperatingSystem = 1,
    NSWindows95OperatingSystem,
    NSSolarisOperatingSystem,
    NSMACHOperatingSystem,
};

NS_ASSUME_NONNULL_BEGIN

/* What an activity needs kept going (API 19). */
typedef NS_OPTIONS(uint64_t, NSActivityOptions) {
    NSActivityIdleDisplaySleepDisabled = (1ULL << 40),
    NSActivityIdleSystemSleepDisabled = (1ULL << 20),
    NSActivitySuddenTerminationDisabled = (1ULL << 14),
    NSActivityAutomaticTerminationDisabled = (1ULL << 15),
    NSActivityUserInitiated = (0x00FFFFFFULL | NSActivityIdleSystemSleepDisabled),
    NSActivityUserInitiatedAllowingIdleSystemSleep = (NSActivityUserInitiated & ~NSActivityIdleSystemSleepDisabled),
    NSActivityBackground = 0x000000FFULL,
    NSActivityLatencyCritical = 0xFF00000000ULL,
};

typedef NS_ENUM(NSInteger, NSProcessInfoThermalState) {
    NSProcessInfoThermalStateNominal,
    NSProcessInfoThermalStateFair,
    NSProcessInfoThermalStateSerious,
    NSProcessInfoThermalStateCritical,
};

FOUNDATION_EXPORT NSString *const NSProcessInfoThermalStateDidChangeNotification;
FOUNDATION_EXPORT NSString *const NSProcessInfoPowerStateDidChangeNotification;

@interface NSProcessInfo : NSObject {
    NSDictionary *_environment;
    NSArray *_arguments;
    NSString *_hostName;
    NSString *_processName;
    NSInteger _counter;
    NSLock *_counterLock;
}

@property(class, readonly, strong) NSProcessInfo *processInfo;

@property(readonly, copy) NSDictionary<NSString *, NSString *> *environment;
@property(readonly, copy) NSArray<NSString *> *arguments;
@property(readonly, copy) NSString *hostName;
@property(copy) NSString *processName;
@property(readonly) int processIdentifier;
@property(readonly, copy) NSString *globallyUniqueString;

- (NSUInteger)operatingSystem;
- (NSString *)operatingSystemName;
@property(readonly, copy) NSString *operatingSystemVersionString;
@property(readonly) NSOperatingSystemVersion operatingSystemVersion;
- (BOOL)isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion)version;

@property(readonly) NSUInteger processorCount;
@property(readonly) NSUInteger activeProcessorCount;
@property(readonly) unsigned long long physicalMemory;
@property(readonly) NSTimeInterval systemUptime;

@property(readonly, copy) NSString *userName;
@property(readonly, copy) NSString *fullUserName;

/* Activities: the token is kept until endActivity:. Sudden and automatic
   termination are counted: an application is not ended by the system
   while they are disabled. */
- (id<NSObject>)beginActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason;
- (void)endActivity:(id<NSObject>)activity;
- (void)performActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason usingBlock:(void (^)(void))block;
- (void)disableSuddenTermination;
- (void)enableSuddenTermination;
- (void)disableAutomaticTermination:(NSString *)reason;
- (void)enableAutomaticTermination:(NSString *)reason;
@property BOOL automaticTerminationSupportEnabled;

@property(readonly) NSProcessInfoThermalState thermalState;
@property(readonly, getter=isLowPowerModeEnabled) BOOL lowPowerModeEnabled;
@property(readonly, getter=isMacCatalystApp) BOOL macCatalystApp;
@property(readonly, getter=isiOSAppOnMac) BOOL iOSAppOnMac;

@end

NS_ASSUME_NONNULL_END

#ifdef __cplusplus
extern "C" {
#endif
void __NSInitializeProcess(int argc, const char *argv[]);
#ifdef __cplusplus
}
#endif
