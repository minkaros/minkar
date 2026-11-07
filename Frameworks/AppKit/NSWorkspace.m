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
#include <LaunchServices/LaunchServices.h>

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

-init {
   _notificationCenter=[[NSNotificationCenter alloc] init];
   return self;
}

-(NSNotificationCenter *)notificationCenter {
   return _notificationCenter;
}

/* Starts an application bundle's executable. SHORTCUT(LS-open): until
 * LaunchServices' open code is built (Phase 3, item 4), only application
 * bundles open, by running their executable; documents and other URLs
 * do not. An app that is already running is activated instead. */
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

-(NSImage *)iconForFile:(NSString *)path {
   if([[path pathExtension] isEqualToString:@"app"]) {
      NSImage *icon=bundleIcon(path);
      return icon!=nil ? icon : [NSImage imageNamed:@"NSApplicationIcon"];
   }
   return nil;
}

-(NSImage *)iconForFiles:(NSArray *)array {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSImage *)iconForFileType:(NSString *)type {
   NSInvalidAbstractInvocation();
   return 0;
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

-(BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(int *)tag {
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

-(int)extendPowerOffBy:(int)milliseconds {
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

