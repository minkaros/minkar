/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSStringFormatter.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSThread-Private.h>
#import <Foundation/NSPlatform.h>
#ifdef __WINDOWS__
#import <Foundation/NSPlatform_win32.h>
#endif
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
#ifdef __WINDOWS__
	OSVERSIONINFOEX osVersion;
	int systemVersion;
	NSString* versionString;
	NSString* servicePack;

	osVersion.dwOSVersionInfoSize=sizeof(osVersion);
	GetVersionEx((OSVERSIONINFO *)&osVersion);

	// Switches aren't float-friendly, so let's get our major/minor version in
	// some kind of combination that'll be easier for it to handle.  dwMajorVersion
	// can live in the 10s digit, with dwMinorVersion living in the 1s digit.
	// We'll also want to negate in the case we're not an NT_WORKSTATION productType - 
	// this only matters for Vista and up
	systemVersion =  osVersion.dwMajorVersion * 10 + osVersion.dwMinorVersion;
	if(systemVersion >= 60 && osVersion.wProductType != VER_NT_WORKSTATION) {
		systemVersion = -systemVersion;
	}
	
	if (osVersion.szCSDVersion != '\0') {
		servicePack = [NSString stringWithCString:osVersion.szCSDVersion];
	} else {
		servicePack = @"";
	}
		
	switch (systemVersion) {
		case 62:
			return [NSString stringWithFormat: @"Windows 8 %@", servicePack];
			break;
		case -62:
			return [NSString stringWithFormat: @"Windows Server 2012 %@", servicePack];
			break;
		case 61:
			return [NSString stringWithFormat: @"Windows 7 %@", servicePack];
			break;
		case -61:
			return [NSString stringWithFormat: @"Windows Server 2008 R2 %@", servicePack];
			break;
		case 60:
			return [NSString stringWithFormat: @"Windows Vista %@", servicePack];
			break;
		case -60:
			return [NSString stringWithFormat: @"Windows Server 2003 R2 %@", servicePack];
			break;
		case 52:
			return [NSString stringWithFormat: @"Windows XP Professional x64 Edition %@", servicePack];
			break;
		case 51:
			return [NSString stringWithFormat: @"Windows XP %@", servicePack];
			break;
		case 50:
			return [NSString stringWithFormat: @"Windows 2000 %@", servicePack];
			break;
		default:
			return [NSString stringWithFormat: @"%d.%d %d %d", osVersion.dwMajorVersion, osVersion.dwMinorVersion, osVersion.wServicePackMajor, osVersion.wServicePackMinor ];
			break;
	}
#else
   NSOperatingSystemVersion v=[self operatingSystemVersion];

   /* As the reference system writes it: "Version 10.15 (Build 19G2021)". */
   if(v.patchVersion!=0)
    return [NSString stringWithFormat:@"Version %ld.%ld.%ld (Build %@)",(long)v.majorVersion,(long)v.minorVersion,(long)v.patchVersion,system_build?system_build:@""];
   return [NSString stringWithFormat:@"Version %ld.%ld (Build %@)",(long)v.majorVersion,(long)v.minorVersion,system_build?system_build:@""];
#endif
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

