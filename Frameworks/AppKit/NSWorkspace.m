/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSRunningApplication.h>
#import "NSRunningApplication_private.h"
#import <AppKit/NSImage.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSCell.h>
#include <notify.h>
#include <spawn.h>
#include <crt_externs.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <LaunchServices/LaunchServices.h>
#include <DiskArbitration/DiskArbitration.h>

NSString * const NSWorkspaceWillPowerOffNotification=@"NSWorkspaceWillPowerOffNotification";
NSString * const NSWorkspaceDidLaunchApplicationNotification=@"NSWorkspaceDidLaunchApplicationNotification";
NSString * const NSWorkspaceDidTerminateApplicationNotification=@"NSWorkspaceDidTerminateApplicationNotification";
NSString * const NSWorkspaceDidActivateApplicationNotification=@"NSWorkspaceDidActivateApplicationNotification";
NSString * const NSWorkspaceDidDeactivateApplicationNotification=@"NSWorkspaceDidDeactivateApplicationNotification";
NSString * const NSWorkspaceDidHideApplicationNotification=@"NSWorkspaceDidHideApplicationNotification";
NSString * const NSWorkspaceDidUnhideApplicationNotification=@"NSWorkspaceDidUnhideApplicationNotification";
NSString * const NSWorkspaceApplicationKey=@"NSWorkspaceApplicationKey";

NSString * const NSWorkspaceRecycleOperation=@"NSWorkspaceRecycleOperation";

NSString * const NSWorkspaceDesktopImageScalingKey=@"NSWorkspaceDesktopImageScalingKey";
NSString * const NSWorkspaceDesktopImageAllowClippingKey=@"NSWorkspaceDesktopImageAllowClippingKey";
NSString * const NSWorkspaceDesktopImageFillColorKey=@"NSWorkspaceDesktopImageFillColorKey";


NSString * const NSWorkspaceWillUnmountNotification=@"NSWorkspaceWillUnmountNotification";
NSString * const NSWorkspaceDidMountNotification=@"NSWorkspaceDidMountNotification";
NSString * const NSWorkspaceDidUnmountNotification=@"NSWorkspaceDidUnmountNotification";
NSString * const NSWorkspaceDidRenameVolumeNotification=@"NSWorkspaceDidRenameVolumeNotification";
NSString * const NSWorkspaceVolumeLocalizedNameKey=@"NSWorkspaceVolumeLocalizedNameKey";
NSString * const NSWorkspaceVolumeURLKey=@"NSWorkspaceVolumeURLKey";
NSString * const NSWorkspaceVolumeOldLocalizedNameKey=@"NSWorkspaceVolumeOldLocalizedNameKey";
NSString * const NSWorkspaceVolumeOldURLKey=@"NSWorkspaceVolumeOldURLKey";
NSString * const NSWorkspaceDidWakeNotification=@"NSWorkspaceDidWakeNotification";
NSString * const NSWorkspaceWillSleepNotification=@"NSWorkspaceWillSleepNotification";
NSString * const NSWorkspaceScreensDidSleepNotification=@"NSWorkspaceScreensDidSleepNotification";
NSString * const NSWorkspaceScreensDidWakeNotification=@"NSWorkspaceScreensDidWakeNotification";
NSString * const NSWorkspaceSessionDidBecomeActiveNotification=@"NSWorkspaceSessionDidBecomeActiveNotification";
NSString * const NSWorkspaceSessionDidResignActiveNotification=@"NSWorkspaceSessionDidResignActiveNotification";
NSString * const NSWorkspaceDidChangeFileLabelsNotification=@"NSWorkspaceDidChangeFileLabelsNotification";
NSString * const NSWorkspaceActiveSpaceDidChangeNotification=@"NSWorkspaceActiveSpaceDidChangeNotification";
NSString * const NSWorkspaceDidPerformFileOperationNotification=@"NSWorkspaceDidPerformFileOperationNotification";

NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationAppleEvent=@"NSWorkspaceLaunchConfigurationAppleEvent";
NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationArguments=@"NSWorkspaceLaunchConfigurationArguments";
NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationEnvironment=@"NSWorkspaceLaunchConfigurationEnvironment";
NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationArchitecture=@"NSWorkspaceLaunchConfigurationArchitecture";

@implementation NSWorkspaceOpenConfiguration

+(instancetype)configuration {
   return [[[self alloc] init] autorelease];
}

-init {
   _promptsUserIfNeeded=YES;
   _addsToRecentItems=YES;
   _activates=YES;
   _allowsRunningApplicationSubstitution=YES;
   return self;
}

-(void)dealloc {
   [_arguments release];
   [_environment release];
   [_appleEvent release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSWorkspaceOpenConfiguration *copy=[[[self class] allocWithZone:zone] init];

   copy->_promptsUserIfNeeded=_promptsUserIfNeeded;
   copy->_addsToRecentItems=_addsToRecentItems;
   copy->_activates=_activates;
   copy->_hides=_hides;
   copy->_hidesOthers=_hidesOthers;
   copy->_forPrinting=_forPrinting;
   copy->_createsNewApplicationInstance=_createsNewApplicationInstance;
   copy->_allowsRunningApplicationSubstitution=_allowsRunningApplicationSubstitution;
   copy->_requiresUniversalLinks=_requiresUniversalLinks;
   copy->_arguments=[_arguments copy];
   copy->_environment=[_environment copy];
   copy->_appleEvent=[_appleEvent retain];
   copy->_architecture=_architecture;
   return copy;
}

-(BOOL)promptsUserIfNeeded { return _promptsUserIfNeeded; }
-(void)setPromptsUserIfNeeded:(BOOL)flag { _promptsUserIfNeeded=flag; }
-(BOOL)addsToRecentItems { return _addsToRecentItems; }
-(void)setAddsToRecentItems:(BOOL)flag { _addsToRecentItems=flag; }
-(BOOL)activates { return _activates; }
-(void)setActivates:(BOOL)flag { _activates=flag; }
-(BOOL)hides { return _hides; }
-(void)setHides:(BOOL)flag { _hides=flag; }
-(BOOL)hidesOthers { return _hidesOthers; }
-(void)setHidesOthers:(BOOL)flag { _hidesOthers=flag; }
-(BOOL)isForPrinting { return _forPrinting; }
-(void)setForPrinting:(BOOL)flag { _forPrinting=flag; }
-(BOOL)createsNewApplicationInstance { return _createsNewApplicationInstance; }
-(void)setCreatesNewApplicationInstance:(BOOL)flag { _createsNewApplicationInstance=flag; }
-(BOOL)allowsRunningApplicationSubstitution { return _allowsRunningApplicationSubstitution; }
-(void)setAllowsRunningApplicationSubstitution:(BOOL)flag { _allowsRunningApplicationSubstitution=flag; }
-(BOOL)requiresUniversalLinks { return _requiresUniversalLinks; }
-(void)setRequiresUniversalLinks:(BOOL)flag { _requiresUniversalLinks=flag; }
-(NSArray *)arguments { return _arguments; }
-(void)setArguments:(NSArray *)arguments { arguments=[arguments copy]; [_arguments release]; _arguments=arguments; }
-(NSDictionary *)environment { return _environment; }
-(void)setEnvironment:(NSDictionary *)environment { environment=[environment copy]; [_environment release]; _environment=environment; }
-(id)appleEvent { return _appleEvent; }
-(void)setAppleEvent:(id)event { event=[event retain]; [_appleEvent release]; _appleEvent=event; }
-(int)architecture { return _architecture; }
-(void)setArchitecture:(int)value { _architecture=value; }

@end

@implementation NSWorkspace

/* One per process: its notification center gets the session's notices
 * whichever thread asks for it. */
+(NSWorkspace *)sharedWorkspace {
   static NSWorkspace *shared;
   static dispatch_once_t once;

   dispatch_once(&once, ^{
      shared=[NSWorkspace new];
   });
   return shared;
}

/* Volumes mounted, unmounted or renamed, from DiskArbitration on the
   main run loop, as notifications of the workspace's center. */
static void volumePosted(NSString *name,NSDictionary *info){
   [[[NSWorkspace sharedWorkspace] notificationCenter] postNotificationName:name object:[NSWorkspace sharedWorkspace] userInfo:info];
}

static NSDictionary *volumeInfo(NSURL *url,NSString *name){
   NSMutableDictionary *info=[NSMutableDictionary dictionary];

   if(url!=nil){
    [info setObject:url forKey:NSWorkspaceVolumeURLKey];
    [info setObject:[url path] forKey:@"NSDevicePath"];
   }
   if(name!=nil)
    [info setObject:name forKey:NSWorkspaceVolumeLocalizedNameKey];
   return info;
}

static NSMutableDictionary *volumesByDisk(void){
   static NSMutableDictionary *volumes;

   if(volumes==nil)
    volumes=[[NSMutableDictionary alloc] init];
   return volumes;
}

static void diskChanged(DADiskRef disk,CFArrayRef keys,void *context){
   NSDictionary *description=[(NSDictionary *)DADiskCopyDescription(disk) autorelease];
   NSString     *bsd=[NSString stringWithUTF8String:DADiskGetBSDName(disk)?:""];
   NSURL        *url=[description objectForKey:(NSString *)kDADiskDescriptionVolumePathKey];
   NSString     *name=[description objectForKey:(NSString *)kDADiskDescriptionVolumeNameKey];
   NSDictionary *old=[volumesByDisk() objectForKey:bsd];

   if(url!=nil && old==nil){
    [volumesByDisk() setObject:volumeInfo(url,name) forKey:bsd];
    volumePosted(NSWorkspaceDidMountNotification,volumeInfo(url,name));
   }
   else if(url==nil && old!=nil){
    [volumesByDisk() removeObjectForKey:bsd];
    volumePosted(NSWorkspaceDidUnmountNotification,old);
   }
   else if(url!=nil && old!=nil && (![url isEqual:[old objectForKey:NSWorkspaceVolumeURLKey]] || (name!=nil && ![name isEqual:[old objectForKey:NSWorkspaceVolumeLocalizedNameKey]]))){
    NSMutableDictionary *info=[[volumeInfo(url,name) mutableCopy] autorelease];

    [info setObject:[old objectForKey:NSWorkspaceVolumeURLKey] forKey:NSWorkspaceVolumeOldURLKey];
    if([old objectForKey:NSWorkspaceVolumeLocalizedNameKey]!=nil)
     [info setObject:[old objectForKey:NSWorkspaceVolumeLocalizedNameKey] forKey:NSWorkspaceVolumeOldLocalizedNameKey];
    [volumesByDisk() setObject:volumeInfo(url,name) forKey:bsd];
    volumePosted(NSWorkspaceDidRenameVolumeNotification,info);
   }
}

static void diskAppeared(DADiskRef disk,void *context){
   NSDictionary *description=[(NSDictionary *)DADiskCopyDescription(disk) autorelease];
   NSURL        *url=[description objectForKey:(NSString *)kDADiskDescriptionVolumePathKey];

   /* Volumes there when the process starts are known, not announced. */
   if(url!=nil)
    [volumesByDisk() setObject:volumeInfo(url,[description objectForKey:(NSString *)kDADiskDescriptionVolumeNameKey])
       forKey:[NSString stringWithUTF8String:DADiskGetBSDName(disk)?:""]];
}

static void diskDisappeared(DADiskRef disk,void *context){
   NSString     *bsd=[NSString stringWithUTF8String:DADiskGetBSDName(disk)?:""];
   NSDictionary *old=[[[volumesByDisk() objectForKey:bsd] retain] autorelease];

   if(old!=nil){
    [volumesByDisk() removeObjectForKey:bsd];
    volumePosted(NSWorkspaceDidUnmountNotification,old);
   }
}

-init {
   _notificationCenter=[[NSNotificationCenter alloc] init];
   return self;
}

/* DiskArbitration is asked about volumes once something asks for the
   center (that is, may observe it). */
-(void)_watchVolumes {
   static dispatch_once_t once;

   dispatch_once(&once,^{
      dispatch_async(dispatch_get_main_queue(),^{
         DASessionRef session=DASessionCreate(kCFAllocatorDefault);

         if(session==NULL)
          return;
         DARegisterDiskAppearedCallback(session,NULL,diskAppeared,NULL);
         DARegisterDiskDisappearedCallback(session,NULL,diskDisappeared,NULL);
         DARegisterDiskDescriptionChangedCallback(session,NULL,kDADiskDescriptionWatchVolumePath,diskChanged,NULL);
         DARegisterDiskDescriptionChangedCallback(session,NULL,kDADiskDescriptionWatchVolumeName,diskChanged,NULL);
         DASessionScheduleWithRunLoop(session,CFRunLoopGetMain(),kCFRunLoopCommonModes);
      });
   });
}

-(NSNotificationCenter *)notificationCenter {
   [self _watchVolumes];
   return _notificationCenter;
}

/* An application's icon: CFBundleIconFile in its Resources, tried as
 * named and with the image types NSImage reads. Others get the generic
 * application icon (applications) or nil. */
static NSImage *bundleIcon(NSString *path) {
   NSBundle *bundle=[NSBundle bundleWithPath:path];
   NSString *name=[[bundle infoDictionary] objectForKey:@"CFBundleIconFile"];

   if([name length]==0)
      return nil;
   NSString *resources=[bundle resourcePath];
   NSArray *candidates=[NSArray arrayWithObjects:name,
      [[name stringByDeletingPathExtension] stringByAppendingPathExtension:@"png"],
      [[name stringByDeletingPathExtension] stringByAppendingPathExtension:@"tiff"],nil];

   for(NSString *candidate in candidates) {
      NSString *file=[resources stringByAppendingPathComponent:candidate];
      if([[NSFileManager defaultManager] fileExistsAtPath:file]) {
         NSImage *image=[[NSImage alloc] initWithContentsOfFile:file];
         if(image!=nil)
            return [image autorelease];
      }
   }
   return nil;
}

/* An icon of the system's set (/System/Library/Icons). */
static NSImage *systemIcon(NSString *relative) {
   NSString *path=[@"/System/Library/Icons" stringByAppendingPathComponent:relative];

   return [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
}

/* An app's own icon; the startup disk's for "/"; a folder's; a file's
   for its type (never nil, as on the reference platform). */
-(NSImage *)iconForFile:(NSString *)path {
   BOOL isDirectory=NO;

   if([[path pathExtension] isEqualToString:@"app"]) {
      NSImage *icon=bundleIcon(path);
      return icon!=nil ? icon : [NSImage imageNamed:@"NSApplicationIcon"];
   }
   if([[path stringByStandardizingPath] isEqualToString:@"/"]) {
      NSImage *icon=systemIcon(@"devices/drive-harddisk.png");
      if(icon!=nil)
         return icon;
   }
   if([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory) {
      NSImage *icon=[NSImage imageNamed:@"NSFolder"];
      if(icon!=nil)
         return icon;
   }
   return [self iconForFileType:[path pathExtension]];
}

-(NSImage *)iconForFiles:(NSArray *)array {
   NSInvalidAbstractInvocation();
   return 0;
}

/* A file type's icon by its extension (a few kinds; the generic document
   for the rest). It used to raise. */
-(NSImage *)iconForFileType:(NSString *)type {
   static NSDictionary *kinds;
   NSString *extension=[[type pathExtension] length] ? [type pathExtension] : type;
   NSString *name;
   NSImage  *icon;

   if(kinds==nil)
      kinds=[[NSDictionary alloc] initWithObjectsAndKeys:
         @"text-plain",@"txt", @"text-x-csrc",@"c", @"text-x-csrc",@"m", @"text-x-chdr",@"h",
         @"text-x-script",@"sh", @"text-x-script",@"command", @"text-html",@"html", @"text-html",@"htm",
         @"application-xml",@"xml", @"application-xml",@"plist", @"application-zip",@"zip",
         @"application-x-archive",@"tar", @"application-x-archive",@"gz", @"application-x-archive",@"xz",
         @"application-x-cd-image",@"iso", @"application-x-cd-image",@"dmg",
         @"image-x-generic",@"png", @"image-x-generic",@"jpg", @"image-x-generic",@"jpeg", @"image-x-generic",@"gif",
         @"image-x-generic",@"tiff", @"image-x-generic",@"webp", @"audio-x-generic",@"mp3", @"audio-x-generic",@"wav",
         @"audio-x-generic",@"aiff", @"video-x-generic",@"mp4", @"video-x-generic",@"mov",
         @"font-x-generic",@"ttf", @"font-x-generic",@"otf", @"x-office-document",@"rtf", @"x-office-document",@"pdf",
         @"package-x-generic",@"pkg", nil];
   name=[kinds objectForKey:[extension lowercaseString]];
   if([extension length]==0)
      name=@"text-x-generic";
   icon=systemIcon([NSString stringWithFormat:@"mimetypes/%@.png",name!=nil?name:@"text-x-generic"]);
   return icon!=nil ? icon : [NSImage imageNamed:@"NSApplicationIcon"];
}

-(NSString *)localizedDescriptionForType:(NSString *)type {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)filenameExtension:(NSString *)extension isValidForType:(NSString *)type {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSString *)preferredFilenameExtensionForType:(NSString *)type {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)type:(NSString *)type conformsToType:(NSString *)conformsToType {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSString *)typeOfFile:(NSString *)path error:(NSError **)error {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)openFile:(NSString *)path {
   return [self openURL:[NSURL fileURLWithPath:path]];
}

/* CF objects for LaunchServices: Foundation's are not CF ones here (the
 * bridge goes from CF to Objective-C only). */
static CFURLRef copyCFURL(NSURL *url) {
   const char *path=[[url path] fileSystemRepresentation];

   if(![url isFileURL] || path==NULL)
      return NULL;
   return CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8 *)path,strlen(path),url.hasDirectoryPath);
}

static CFArrayRef copyCFURLs(NSArray *urls) {
   CFMutableArrayRef list=CFArrayCreateMutable(NULL,[urls count],&kCFTypeArrayCallBacks);

   for(NSURL *url in urls) {
      CFURLRef cf=copyCFURL(url);
      if(cf!=NULL) {
         CFArrayAppendValue(list,cf);
         CFRelease(cf);
      }
   }
   return list;
}

/* Opening goes through LaunchServices: each item's application, which
 * gets it on its session port (application:openURLs: and the like). */
-(BOOL)openFile:(NSString *)path withApplication:(NSString *)application {
   if(application==nil)
      return [self openFile:path];
   NSString *app=[self pathForApplication:application];
   if(app==nil || path==nil)
      return NO;
   CFArrayRef items=copyCFURLs([NSArray arrayWithObject:[NSURL fileURLWithPath:path]]);
   CFURLRef appURL=copyCFURL([NSURL fileURLWithPath:app]);
   LSLaunchURLSpec spec={ appURL, items, NULL, kLSLaunchDefaults, NULL };
   OSStatus status=LSOpenFromURLSpec(&spec, NULL);
   CFRelease(items);
   if(appURL!=NULL)
      CFRelease(appURL);
   return status==noErr;
}

-(BOOL)openTempFile:(NSString *)path {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)openFile:(NSString *)path fromImage:(NSImage *)image at:(NSPoint)point inView:(NSView *)view {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)openFile:(NSString *)path withApplication:(NSString *)application andDeactivate:(BOOL)deactivate {
   return [self openFile:path withApplication:application];
}

-(BOOL)openURL:(NSURL *)url {
   CFURLRef cf=url ? copyCFURL(url) : NULL;	/* other schemes: with URL handlers */

   if(cf==NULL)
      return NO;
   OSStatus status=LSOpenCFURLRef(cf, NULL);
   CFRelease(cf);
   return status==noErr;
}

/* The file manager shows the file selected (the folder alone with no file). */
-(BOOL)selectFile:(NSString *)path inFileViewerRootedAtPath:(NSString *)rootedAtPath {
   NSString *item=[path length] ? path : rootedAtPath;

   if([item length]==0)
      return NO;
   CFArrayRef items=copyCFURLs([NSArray arrayWithObject:[NSURL fileURLWithPath:item]]);
   OSStatus status=LSRevealInFiles(items);
   CFRelease(items);
   return status==noErr;
}

-(void)activateFileViewerSelectingURLs:(NSArray *)fileURLs {
   if([fileURLs count]>0) {
      CFArrayRef items=copyCFURLs(fileURLs);
      LSRevealInFiles(items);
      CFRelease(items);
   }
}

-(NSURL *)URLForApplicationToOpenURL:(NSURL *)url {
   if(url==nil)
      return nil;
   CFURLRef cf=copyCFURL(url);
   CFURLRef app=cf ? LSCopyDefaultApplicationURLForURL(cf, kLSRolesAll, NULL) : NULL;
   if(cf!=NULL)
      CFRelease(cf);
   NSURL *result=nil;
   char path[1024];
   if(app!=NULL && CFURLGetFileSystemRepresentation(app,true,(UInt8 *)path,sizeof(path)))
      result=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
   if(app!=NULL)
      CFRelease(app);
   return result;
}

-(NSURL *)URLForApplicationWithBundleIdentifier:(NSString *)bundleIdentifier {
   if(bundleIdentifier==nil)
      return nil;
   CFStringRef id=CFStringCreateWithCString(NULL,[bundleIdentifier UTF8String],kCFStringEncodingUTF8);
   CFArrayRef apps=id ? LSCopyApplicationURLsForBundleIdentifier(id, NULL) : NULL;
   NSURL *app=nil;
   if(apps!=NULL && CFArrayGetCount(apps)>0) {
      char path[1024];
      if(CFURLGetFileSystemRepresentation(CFArrayGetValueAtIndex(apps,0),true,(UInt8 *)path,sizeof(path)))
         app=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
   }
   if(apps!=NULL)
      CFRelease(apps);
   if(id!=NULL)
      CFRelease(id);
   return app;
}

-(void)slideImage:(NSImage *)image from:(NSPoint)from to:(NSPoint)to {
   NSInvalidAbstractInvocation();
}

-(BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(NSInteger *)tag {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)getFileSystemInfoForPath:(NSString *)path isRemovable:(BOOL *)isRemovable isWritable:(BOOL *)isWritable isUnmountable:(BOOL *)isUnmountable description:(NSString **)description type:(NSString **)type {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)getInfoForFile:(NSString *)path application:(NSString **)application type:(NSString **)type {
   NSInvalidAbstractInvocation();
   return 0;
}

-(void)checkForRemovableMedia {
   NSInvalidAbstractInvocation();
}

-(NSArray *)mountNewRemovableMedia {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSArray *)mountedRemovableMedia {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSArray *)mountedLocalVolumePaths {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)unmountAndEjectDeviceAtPath:(NSString *)path {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)fileSystemChanged {
   NSInvalidAbstractInvocation();
   return 0;
}

-(BOOL)userDefaultsChanged {
   NSInvalidAbstractInvocation();
   return 0;
}

-(void)noteFileSystemChanged {
   NSInvalidAbstractInvocation();
}

-(void)noteFileSystemChanged:(NSString *)path {
   NSInvalidAbstractInvocation();
}

-(void)noteUserDefaultsChanged {
   NSInvalidAbstractInvocation();
}

-(BOOL)isFilePackageAtPath:(NSString *)path {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSString *)absolutePathForAppBundleWithIdentifier:(NSString *)identifier {
   return [[self URLForApplicationWithBundleIdentifier:identifier] path];
}

-(NSString *)fullPathForApplication:(NSString *)application {
   return [self pathForApplication:application];
}

-(NSString *)pathForApplication:(NSString *)application {
   NSFileManager *fm=[NSFileManager defaultManager];
   NSString *name=[application lastPathComponent];

   if([application isAbsolutePath])
      return [fm fileExistsAtPath:application] ? application : nil;
   if(![[name pathExtension] isEqualToString:@"app"])
      name=[name stringByAppendingPathExtension:@"app"];

   NSArray *dirs=[NSArray arrayWithObjects:[@"~/Applications" stringByExpandingTildeInPath],
      @"/Applications",@"/Applications/Utilities",@"/System/Applications",
      @"/System/Applications/Utilities",@"/System/Library/CoreServices",nil];

   for(NSString *dir in dirs) {
      NSString *candidate=[dir stringByAppendingPathComponent:name];
      if([fm fileExistsAtPath:candidate])
         return candidate;
   }
   return nil;
}

/*
 * Desktop pictures: in the domain org.minkar.desktop, key "screens", one
 * entry per display (its NSScreenNumber) with the picture's URL and the
 * options; notify(3) posts org.minkar.desktop.changed, and the Dock, which
 * draws the desktop, follows. The fill color is kept as calibrated RGBA.
 */
#define kDesktopDomain CFSTR("org.minkar.desktop")

static NSString *screenKey(NSScreen *screen) {
   NSNumber *number=[[screen deviceDescription] objectForKey:@"NSScreenNumber"];

   return number ? [number stringValue] : @"0";
}

static NSDictionary *desktopEntries(void) {
   CFPreferencesAppSynchronize(kDesktopDomain);
   id screens=[(id)CFPreferencesCopyAppValue(CFSTR("screens"),kDesktopDomain) autorelease];

   return [screens isKindOfClass:[NSDictionary class]] ? screens : [NSDictionary dictionary];
}

-(BOOL)setDesktopImageURL:(NSURL *)url forScreen:(NSScreen *)screen options:(NSDictionary *)options error:(NSError **)error {
   if(url==nil || ![url isFileURL] || screen==nil ||
      ![[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
      if(error!=NULL)
         *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileNoSuchFileError
            userInfo:url ? [NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey] : nil];
      return NO;
   }

   NSMutableDictionary *entry=[NSMutableDictionary dictionaryWithObject:[url absoluteString] forKey:@"url"];
   NSNumber *scaling=[options objectForKey:NSWorkspaceDesktopImageScalingKey];
   NSNumber *clipping=[options objectForKey:NSWorkspaceDesktopImageAllowClippingKey];
   NSColor *color=[[options objectForKey:NSWorkspaceDesktopImageFillColorKey]
      colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

   if(scaling!=nil)
      [entry setObject:scaling forKey:@"scaling"];
   if(clipping!=nil)
      [entry setObject:clipping forKey:@"clipping"];
   if(color!=nil) {
      CGFloat r,g,b,a;
      [color getRed:&r green:&g blue:&b alpha:&a];
      [entry setObject:[NSArray arrayWithObjects:[NSNumber numberWithDouble:r],
         [NSNumber numberWithDouble:g],[NSNumber numberWithDouble:b],
         [NSNumber numberWithDouble:a],nil] forKey:@"color"];
   }

   NSMutableDictionary *screens=[[desktopEntries() mutableCopy] autorelease];
   [screens setObject:entry forKey:screenKey(screen)];
   CFPreferencesSetAppValue(CFSTR("screens"),(CFPropertyListRef)screens,kDesktopDomain);
   if(!CFPreferencesAppSynchronize(kDesktopDomain)) {
      if(error!=NULL)
         *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
      return NO;
   }
   notify_post("org.minkar.desktop.changed");
   return YES;
}

-(NSURL *)desktopImageURLForScreen:(NSScreen *)screen {
   NSString *string=[[desktopEntries() objectForKey:screenKey(screen)] objectForKey:@"url"];

   return [string isKindOfClass:[NSString class]] ? [NSURL URLWithString:string] : nil;
}

-(NSDictionary *)desktopImageOptionsForScreen:(NSScreen *)screen {
   NSDictionary *entry=[desktopEntries() objectForKey:screenKey(screen)];
   NSMutableDictionary *result=[NSMutableDictionary dictionary];
   NSArray *rgba=[entry objectForKey:@"color"];

   [result setObject:[entry objectForKey:@"scaling"] ? [entry objectForKey:@"scaling"] :
      [NSNumber numberWithInteger:NSImageScaleProportionallyUpOrDown] forKey:NSWorkspaceDesktopImageScalingKey];
   [result setObject:[entry objectForKey:@"clipping"] ? [entry objectForKey:@"clipping"] :
      [NSNumber numberWithBool:YES] forKey:NSWorkspaceDesktopImageAllowClippingKey];
   if([rgba isKindOfClass:[NSArray class]] && [rgba count]==4)
      [result setObject:[NSColor colorWithCalibratedRed:[[rgba objectAtIndex:0] doubleValue]
         green:[[rgba objectAtIndex:1] doubleValue] blue:[[rgba objectAtIndex:2] doubleValue]
         alpha:[[rgba objectAtIndex:3] doubleValue]] forKey:NSWorkspaceDesktopImageFillColorKey];
   return result;
}

-(NSArray *)runningApplications {
   return [NSRunningApplication _runningApplications];
}

-(NSRunningApplication *)frontmostApplication {
   for(NSRunningApplication *app in [self runningApplications])
      if([app isActive])
         return app;
   return nil;
}

/* The dictionaries from before NSRunningApplication. */
static NSDictionary *applicationDictionary(NSRunningApplication *app) {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];
   NSURL *url=[app bundleURL] ? [app bundleURL] : [app executableURL];

   [result setObject:[NSNumber numberWithInt:[app processIdentifier]]
      forKey:@"NSApplicationProcessIdentifier"];
   if([app localizedName]!=nil)
      [result setObject:[app localizedName] forKey:@"NSApplicationName"];
   if([app bundleIdentifier]!=nil)
      [result setObject:[app bundleIdentifier] forKey:@"NSApplicationBundleIdentifier"];
   if(url!=nil)
      [result setObject:[url path] forKey:@"NSApplicationPath"];
   return result;
}

-(NSArray *)launchedApplications {
   NSMutableArray *result=[NSMutableArray array];

   for(NSRunningApplication *app in [self runningApplications])
      if([app activationPolicy]==NSApplicationActivationPolicyRegular)
         [result addObject:applicationDictionary(app)];
   return result;
}

-(BOOL)launchApplication:(NSString *)application {
   NSString *path=[self pathForApplication:application];

   return path!=nil && [self openFile:path];
}

-(BOOL)launchApplication:(NSString *)application showIcon:(BOOL)showIcon autolaunch:(BOOL)autolaunch {
   return [self launchApplication:application];
}

/* The application at url once it runs: a launch may take a moment to
   register it. */
static NSRunningApplication *runningApplicationAtURL(NSURL *url,BOOL wait){
   NSString *identifier=[[NSBundle bundleWithURL:url] bundleIdentifier];
   int       tries=wait?40:1;

   while(tries-->0){
    for(NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications])
     if((identifier!=nil && [[app bundleIdentifier] isEqualToString:identifier]) ||
        [[[app bundleURL] URLByStandardizingPath] isEqual:[url URLByStandardizingPath]])
      return app;
    if(tries>0)
     usleep(50000);
   }
   return nil;
}

-(NSRunningApplication *)openURLs:(NSArray *)urls withApplicationAtURL:(NSURL *)applicationURL options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary *)configuration error:(NSError **)error {
   LSLaunchFlags flags=kLSLaunchDefaults;
   CFArrayRef    items=([urls count]>0)?copyCFURLs(urls):NULL;
   CFURLRef      appURL=(applicationURL!=nil)?copyCFURL(applicationURL):NULL;
   NSArray      *arguments=[configuration objectForKey:NSWorkspaceLaunchConfigurationArguments];
   OSStatus      status;

   if(options&NSWorkspaceLaunchAndHide) flags|=kLSLaunchAndHide;
   if(options&NSWorkspaceLaunchAndHideOthers) flags|=kLSLaunchAndHideOthers;
   if(options&NSWorkspaceLaunchNewInstance) flags|=kLSLaunchNewInstance;
   if(options&NSWorkspaceLaunchWithoutActivation) flags|=kLSLaunchDontSwitch;
   if(options&NSWorkspaceLaunchWithoutAddingToRecents) flags|=kLSLaunchDontAddToRecents;
   if(options&NSWorkspaceLaunchAndPrint) flags|=kLSLaunchAndPrint;
   if(options&NSWorkspaceLaunchAsync) flags|=kLSLaunchAsync;

   LSLaunchURLSpec spec={ appURL, items, NULL, flags, NULL };

   status=LSOpenFromURLSpecWithArguments(&spec,(CFArrayRef)arguments,NULL);
   if(items!=NULL)
    CFRelease(items);
   if(appURL!=NULL)
    CFRelease(appURL);
   if(status!=noErr){
    if(error!=NULL)
     *error=[NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:[NSDictionary dictionaryWithObject:
        (applicationURL!=nil)?(id)applicationURL:(id)[urls firstObject] forKey:NSURLErrorKey]];
    return nil;
   }
   if(applicationURL==nil && [urls count]>0)
    applicationURL=[self URLForApplicationToOpenURL:[urls objectAtIndex:0]];
   return (applicationURL!=nil)?runningApplicationAtURL(applicationURL,YES):nil;
}

-(NSRunningApplication *)launchApplicationAtURL:(NSURL *)url options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary *)configuration error:(NSError **)error {
   return [self openURLs:[NSArray array] withApplicationAtURL:url options:options configuration:configuration error:error];
}

-(NSRunningApplication *)openURL:(NSURL *)url options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary *)configuration error:(NSError **)error {
   if([url isFileURL] && [[[url path] pathExtension] isEqualToString:@"app"])
    return [self launchApplicationAtURL:url options:options configuration:configuration error:error];
   return [self openURLs:[NSArray arrayWithObject:url] withApplicationAtURL:[self URLForApplicationToOpenURL:url] options:options configuration:configuration error:error];
}

-(BOOL)launchAppWithBundleIdentifier:(NSString *)bundleIdentifier options:(NSWorkspaceLaunchOptions)options additionalEventParamDescriptor:(id)descriptor launchIdentifier:(NSNumber **)identifier {
   NSURL *url=[self URLForApplicationWithBundleIdentifier:bundleIdentifier];
   NSRunningApplication *app=(url!=nil)?[self launchApplicationAtURL:url options:options configuration:[NSDictionary dictionary] error:NULL]:nil;

   if(identifier!=NULL)
    *identifier=(app!=nil)?[NSNumber numberWithInt:[app processIdentifier]]:nil;
   return app!=nil;
}

static NSWorkspaceLaunchOptions launchOptionsOfConfiguration(NSWorkspaceOpenConfiguration *configuration){
   NSWorkspaceLaunchOptions options=NSWorkspaceLaunchAsync;

   if(![configuration activates]) options|=NSWorkspaceLaunchWithoutActivation;
   if([configuration hides]) options|=NSWorkspaceLaunchAndHide;
   if([configuration hidesOthers]) options|=NSWorkspaceLaunchAndHideOthers;
   if([configuration createsNewApplicationInstance]) options|=NSWorkspaceLaunchNewInstance;
   if(![configuration addsToRecentItems]) options|=NSWorkspaceLaunchWithoutAddingToRecents;
   if([configuration isForPrinting]) options|=NSWorkspaceLaunchAndPrint;
   return options;
}

static NSDictionary *launchConfigurationOfConfiguration(NSWorkspaceOpenConfiguration *configuration){
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   if([configuration arguments]!=nil)
    [result setObject:[configuration arguments] forKey:NSWorkspaceLaunchConfigurationArguments];
   if([configuration environment]!=nil)
    [result setObject:[configuration environment] forKey:NSWorkspaceLaunchConfigurationEnvironment];
   return result;
}

-(void)openURLs:(NSArray *)urls withApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler {
   NSWorkspaceOpenConfiguration *copy=[[(configuration!=nil)?configuration:[NSWorkspaceOpenConfiguration configuration] copy] autorelease];
   void (^handler)(NSRunningApplication *,NSError *)=[[completionHandler copy] autorelease];

   [urls retain];
   [applicationURL retain];
   [copy retain];
   [handler retain];
   dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0),^{
      NSAutoreleasePool    *pool=[NSAutoreleasePool new];
      NSError              *error=nil;
      NSRunningApplication *app=[self openURLs:urls withApplicationAtURL:applicationURL options:launchOptionsOfConfiguration(copy)
         configuration:launchConfigurationOfConfiguration(copy) error:&error];

      if(handler!=NULL)
       handler(app,error);
      [urls release];
      [applicationURL release];
      [copy release];
      [handler release];
      [pool release];
   });
}

-(void)openApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler {
   [self openURLs:[NSArray array] withApplicationAtURL:applicationURL configuration:configuration completionHandler:completionHandler];
}

-(void)openURL:(NSURL *)url configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler {
   if([url isFileURL] && [[[url path] pathExtension] isEqualToString:@"app"])
    [self openApplicationAtURL:url configuration:configuration completionHandler:completionHandler];
   else
    [self openURLs:[NSArray arrayWithObject:url] withApplicationAtURL:[self URLForApplicationToOpenURL:url] configuration:configuration completionHandler:completionHandler];
}

-(void)findApplications {
   NSInvalidAbstractInvocation();
}

-(NSDictionary *)activeApplication {
   NSRunningApplication *app=[self frontmostApplication];

   return app ? applicationDictionary(app) : nil;
}

-(void)hideOtherApplications {
   NSInvalidAbstractInvocation();
}

-(NSInteger)extendPowerOffBy:(NSInteger)milliseconds {
   NSInvalidAbstractInvocation();
   return 0;
}

@end

@implementation NSWorkspace (CocotronAdditions)

- (BOOL)isFileHiddenAtPath:(NSString*)path
{
	return NO;
}

@end

