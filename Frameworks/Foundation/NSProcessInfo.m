/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSProcessInfo.h>
#include <pwd.h>
#include <unistd.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSStringFormatter.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSThread-Private.h>
#import <Foundation/NSPlatform.h>
#import <objc/runtime.h>
#import <Foundation/NSString.h>
#import <sys/sysctl.h>
#import <sys/time.h>
#import <mach/mach_time.h>
#import <stdlib.h>
#import <string.h>

#import "NSCFString.h"
#import "NSDarwinString.h"
#import "NSString_cString.h"

NSString *const NSProcessInfoThermalStateDidChangeNotification=@"NSProcessInfoThermalStateDidChangeNotification";
NSString *const NSProcessInfoPowerStateDidChangeNotification=@"NSProcessInfoPowerStateDidChangeNotification";

@implementation NSProcessInfo

int                 NSProcessInfoArgc=0;
const char * const *NSProcessInfoArgv=NULL;

-(NSInteger)incrementCounter {
   NSInteger result;

   [_counterLock lock];
   _counter++;
   result=_counter;
   [_counterLock unlock];

   return result;
}

+(NSProcessInfo *)processInfo {
   return NSThreadSharedInstance(@"NSProcessInfo");
}

-init {
   _environment=nil;
   _arguments=nil;
   _hostName=nil;
   _processName=nil;
   _counter=0;
   _counterLock=[NSLock new];
   return self;
}

/* Minkar: the system's numbers, from sysctl. */
static uint64_t sysctl_number(const char *name) {
   uint64_t value=0;
   size_t size=sizeof(value);

   if(sysctlbyname(name,&value,&size,NULL,0)!=0)
    return 0;
   if(size==sizeof(uint32_t))
    return *(uint32_t *)&value;
   return value;
}

-(NSUInteger)processorCount {
   return (NSUInteger)sysctl_number("hw.ncpu");
}

-(NSUInteger)activeProcessorCount {
   return (NSUInteger)sysctl_number("hw.activecpu");
}

-(uint64_t)physicalMemory {
   return sysctl_number("hw.memsize");
}

-(NSUInteger)operatingSystem {
   return NSMACHOperatingSystem;
}

-(NSString *)operatingSystemName {
   return @"NSMACHOperatingSystem";
}

/*
 * Minkar: the version programs compare against is ProductVersion in
 * SystemVersion.plist (the Cocoa API version: 10.15 for API 19), not the
 * release. Read once.
 */
static NSOperatingSystemVersion system_version;
static NSString *system_build;

static void read_system_version(void) {
   static dispatch_once_t once;

   dispatch_once(&once, ^{
    NSDictionary *plist=[NSDictionary dictionaryWithContentsOfFile:
      @"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *version=[plist objectForKey:@"ProductVersion"];
    NSArray *parts=[version componentsSeparatedByString:@"."];

    if([parts count]>0)
     system_version.majorVersion=[[parts objectAtIndex:0] integerValue];
    if([parts count]>1)
     system_version.minorVersion=[[parts objectAtIndex:1] integerValue];
    if([parts count]>2)
     system_version.patchVersion=[[parts objectAtIndex:2] integerValue];
    system_build=[[plist objectForKey:@"ProductBuildVersion"] copy];
   });
}

-(NSOperatingSystemVersion)operatingSystemVersion {
   read_system_version();
   return system_version;
}

-(BOOL)isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion)version {
   NSOperatingSystemVersion current=[self operatingSystemVersion];

   if(current.majorVersion!=version.majorVersion)
    return current.majorVersion>version.majorVersion;
   if(current.minorVersion!=version.minorVersion)
    return current.minorVersion>version.minorVersion;
   return current.patchVersion>=version.patchVersion;
}

-(NSTimeInterval)systemUptime {
   static mach_timebase_info_data_t timebase;

   if(timebase.denom==0)
    mach_timebase_info(&timebase);
   return (NSTimeInterval)mach_absolute_time()*timebase.numer/timebase.denom/1e9;
}

-(NSString *)operatingSystemVersionString {
   NSOperatingSystemVersion v=[self operatingSystemVersion];

   /* As the reference system writes it: "Version 10.15 (Build 19G2021)". */
   if(v.patchVersion!=0)
    return [NSString stringWithFormat:@"Version %ld.%ld.%ld (Build %@)",(long)v.majorVersion,(long)v.minorVersion,(long)v.patchVersion,system_build?system_build:@""];
   return [NSString stringWithFormat:@"Version %ld.%ld (Build %@)",(long)v.majorVersion,(long)v.minorVersion,system_build?system_build:@""];
}

-(NSString *)hostName {
   if(_hostName==nil){
    _hostName=[[[NSPlatform currentPlatform] hostName] retain];

    if(_hostName==nil)
     _hostName=@"HOSTNAME";
   }

   return _hostName;
}

-(NSString *)processName {
   if(_processName==nil){
    NSArray *arguments=[self arguments];

    if([arguments count]>0)
     _processName=[[[[[self arguments] objectAtIndex:0]
       lastPathComponent] stringByDeletingPathExtension] retain];

    if(_processName==nil){
     _processName=@"";
    }
   }

   return _processName;
}

-(void)setProcessName:(NSString *)name {
   [_processName release];
   _processName=[name copy];
}

-(int)processIdentifier {
   return NSPlatformProcessID();
}

// Minkar: this sent -arguments to the NSPlatform_posix class, which has
// no such class method: arguments, processName and so NSLog crashed.
-(NSArray *)arguments {
   if(_arguments==nil){
    _arguments=[[[NSPlatform currentPlatform] arguments] retain];
   }

   return _arguments;
}

-(NSDictionary *)environment {
   if(_environment==nil)
    _environment=[[[NSPlatform currentPlatform] environment] retain];

   return _environment;
}

-(NSString *)globallyUniqueString {
   return NSStringWithFormat(@"%@_%d_%d_%d_%d",[self hostName],
     [self processIdentifier],0,0,[self incrementCounter]);
}

-(NSString *)userName {
   struct passwd *pw=getpwuid(getuid());

   return (pw!=NULL)?[NSString stringWithUTF8String:pw->pw_name]:@"";
}

/* The account's full name (its GECOS field up to the first comma). */
-(NSString *)fullUserName {
   struct passwd *pw=getpwuid(getuid());
   NSString      *gecos;

   if(pw==NULL)
    return @"";
   gecos=[NSString stringWithUTF8String:(pw->pw_gecos!=NULL)?pw->pw_gecos:""];
   gecos=[[gecos componentsSeparatedByString:@","] objectAtIndex:0];
   return ([gecos length]>0)?gecos:[NSString stringWithUTF8String:pw->pw_name];
}

static NSInteger suddenTerminationDisabled,automaticTerminationDisabled;
static BOOL      automaticTerminationSupport;

-(id<NSObject>)beginActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason {
   NSDictionary *activity=[NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedLongLong:options],@"options",
      (reason!=nil)?reason:@"",@"reason",nil];

   @synchronized([NSProcessInfo class]){
    if(options&NSActivitySuddenTerminationDisabled)
     suddenTerminationDisabled++;
    if(options&NSActivityAutomaticTerminationDisabled)
     automaticTerminationDisabled++;
   }
   return activity;
}

-(void)endActivity:(id<NSObject>)activity {
   NSActivityOptions options=[[(NSDictionary *)activity objectForKey:@"options"] unsignedLongLongValue];

   @synchronized([NSProcessInfo class]){
    if((options&NSActivitySuddenTerminationDisabled) && suddenTerminationDisabled>0)
     suddenTerminationDisabled--;
    if((options&NSActivityAutomaticTerminationDisabled) && automaticTerminationDisabled>0)
     automaticTerminationDisabled--;
   }
}

-(void)performActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason usingBlock:(void (^)(void))block {
   id activity=[self beginActivityWithOptions:options reason:reason];

   block();
   [self endActivity:activity];
}

-(void)disableSuddenTermination {
   @synchronized([NSProcessInfo class]){ suddenTerminationDisabled++; }
}

-(void)enableSuddenTermination {
   @synchronized([NSProcessInfo class]){ if(suddenTerminationDisabled>0) suddenTerminationDisabled--; }
}

-(void)disableAutomaticTermination:(NSString *)reason {
   @synchronized([NSProcessInfo class]){ automaticTerminationDisabled++; }
}

-(void)enableAutomaticTermination:(NSString *)reason {
   @synchronized([NSProcessInfo class]){ if(automaticTerminationDisabled>0) automaticTerminationDisabled--; }
}

-(BOOL)automaticTerminationSupportEnabled {
   return automaticTerminationSupport;
}

-(void)setAutomaticTerminationSupportEnabled:(BOOL)flag {
   automaticTerminationSupport=flag;
}

/* Minkar does not watch the processor's temperature or a battery yet. */
-(NSProcessInfoThermalState)thermalState {
   return NSProcessInfoThermalStateNominal;
}

-(BOOL)isLowPowerModeEnabled {
   return NO;
}

-(BOOL)isMacCatalystApp {
   return NO;
}

-(BOOL)isiOSAppOnMac {
   return NO;
}

@end

int __NSConstantStringClassReference[24];

// objc4's in-place class construction (objc-internal.h).
OBJC_EXPORT Class objc_initializeClassPair(Class superclass, const char *name,
                                          Class cls, Class metacls);

FOUNDATION_EXPORT void __NSInitializeProcess(int argc,const char *argv[]) {
   NSProcessInfoArgc=argc;
   NSProcessInfoArgv=argv;
#if !defined(APPLE_RUNTIME_4)
    OBJCInitializeProcess();
#endif
#if defined(__APPLE__)
    Class cls = objc_getClass("__builtin_NSString");
    memcpy(&__NSConstantStringClassReference, cls, sizeof(__NSConstantStringClassReference));

#if __LP64__
    extern int __CFConstantStringClassReference[64];
#else
    extern int __CFConstantStringClassReference[32];
#endif

    // The class of constant strings (@"...", CFSTR): a real subclass of
    // NSCFString, built in place where the compiler's strings point.
    // Minkar: this was a byte copy of NSCFString, which shared its method
    // cache: once the runtime grew and freed that cache, a message to a
    // constant string (ARC's retain, for one) jumped to freed memory.
    static uintptr_t constantStringMetaclass[32];   // OBJC_MAX_CLASS_SIZE
    Class constantString = (Class)(void *)__CFConstantStringClassReference;
    if (objc_lookUpClass("__NSCFConstantString") == Nil &&
        objc_initializeClassPair(objc_getClass("NSCFString"), "__NSCFConstantString",
                                 constantString, (Class)(void *)constantStringMetaclass) != Nil)
        objc_registerClassPair(constantString);
    
    // Override the compiler version of the class
    //objc_addClass(&_NSConstantStringClassReference);
#if TOLL_FREE_BRIDGING
    CFSetupFoundationBridging();
#endif
#endif

}

