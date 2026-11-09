/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>

@class NSImage, NSView, NSRunningApplication, NSScreen;

APPKIT_EXPORT NSString *const NSWorkspaceWillPowerOffNotification;

/* On the workspace's notification center; their NSWorkspaceApplicationKey
 * is the NSRunningApplication. */
APPKIT_EXPORT NSString *const NSWorkspaceDidLaunchApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidTerminateApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidActivateApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidDeactivateApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidHideApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidUnhideApplicationNotification;
APPKIT_EXPORT NSString *const NSWorkspaceApplicationKey;

APPKIT_EXPORT NSString *const NSWorkspaceRecycleOperation;

/* Volumes mounted, unmounted and renamed, on the workspace's notification
   center: NSDevicePath and the keys below. */
APPKIT_EXPORT NSString *const NSWorkspaceWillUnmountNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidMountNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidUnmountNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidRenameVolumeNotification;
APPKIT_EXPORT NSString *const NSWorkspaceVolumeLocalizedNameKey;
APPKIT_EXPORT NSString *const NSWorkspaceVolumeURLKey;
APPKIT_EXPORT NSString *const NSWorkspaceVolumeOldLocalizedNameKey;
APPKIT_EXPORT NSString *const NSWorkspaceVolumeOldURLKey;
APPKIT_EXPORT NSString *const NSWorkspaceDidWakeNotification;
APPKIT_EXPORT NSString *const NSWorkspaceWillSleepNotification;
APPKIT_EXPORT NSString *const NSWorkspaceScreensDidSleepNotification;
APPKIT_EXPORT NSString *const NSWorkspaceScreensDidWakeNotification;
APPKIT_EXPORT NSString *const NSWorkspaceSessionDidBecomeActiveNotification;
APPKIT_EXPORT NSString *const NSWorkspaceSessionDidResignActiveNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidChangeFileLabelsNotification;
APPKIT_EXPORT NSString *const NSWorkspaceActiveSpaceDidChangeNotification;
APPKIT_EXPORT NSString *const NSWorkspaceDidPerformFileOperationNotification;

typedef NS_OPTIONS(NSUInteger, NSWorkspaceLaunchOptions) {
    NSWorkspaceLaunchAndPrint = 0x00000002,
    NSWorkspaceLaunchWithErrorPresentation = 0x00000040,
    NSWorkspaceLaunchInhibitingBackgroundOnly = 0x00000080,
    NSWorkspaceLaunchWithoutAddingToRecents = 0x00000100,
    NSWorkspaceLaunchWithoutActivation = 0x00000200,
    NSWorkspaceLaunchAsync = 0x00010000,
    NSWorkspaceLaunchNewInstance = 0x00080000,
    NSWorkspaceLaunchAndHide = 0x00100000,
    NSWorkspaceLaunchAndHideOthers = 0x00200000,
    NSWorkspaceLaunchDefault = NSWorkspaceLaunchAsync,
    NSWorkspaceLaunchAllowingClassicStartup = 0x00020000,
    NSWorkspaceLaunchPreferringClassic = 0x00040000,
};

typedef NSString *NSWorkspaceLaunchConfigurationKey;
APPKIT_EXPORT NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationAppleEvent;
APPKIT_EXPORT NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationArguments;
APPKIT_EXPORT NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationEnvironment;
APPKIT_EXPORT NSWorkspaceLaunchConfigurationKey const NSWorkspaceLaunchConfigurationArchitecture;

/* How an application or URL is opened (API 19). */
@interface NSWorkspaceOpenConfiguration : NSObject <NSCopying> {
    BOOL _promptsUserIfNeeded;
    BOOL _addsToRecentItems;
    BOOL _activates;
    BOOL _hides;
    BOOL _hidesOthers;
    BOOL _forPrinting;
    BOOL _createsNewApplicationInstance;
    BOOL _allowsRunningApplicationSubstitution;
    BOOL _requiresUniversalLinks;
    NSArray *_arguments;
    NSDictionary *_environment;
    id _appleEvent;
    int _architecture;
}
+ (instancetype)configuration;
@property BOOL promptsUserIfNeeded;
@property BOOL addsToRecentItems;
@property BOOL activates;
@property BOOL hides;
@property BOOL hidesOthers;
@property (getter=isForPrinting) BOOL forPrinting;
@property BOOL createsNewApplicationInstance;
@property BOOL allowsRunningApplicationSubstitution;
@property BOOL requiresUniversalLinks;
@property (copy) NSArray<NSString *> *arguments;
@property (copy) NSDictionary<NSString *, NSString *> *environment;
@property (strong) id appleEvent;
@property int architecture;
@end

/* The desktop picture's options (API 19). */
APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageScalingKey;      /* NSNumber of NSImageScaling */
APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageAllowClippingKey; /* NSNumber of BOOL */
APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageFillColorKey;    /* NSColor */

@interface NSWorkspace : NSObject {
    NSNotificationCenter *_notificationCenter;
}

@property (class, readonly, strong) NSWorkspace *sharedWorkspace;

- (NSNotificationCenter *)notificationCenter;

- (NSImage *)iconForFile:(NSString *)path;
- (NSImage *)iconForFiles:(NSArray *)array;
- (NSImage *)iconForFileType:(NSString *)type;
- (NSString *)localizedDescriptionForType:(NSString *)type;
- (BOOL)filenameExtension:(NSString *)extension isValidForType:(NSString *)type;
- (NSString *)preferredFilenameExtensionForType:(NSString *)type;
- (BOOL)type:(NSString *)type conformsToType:(NSString *)conformsToType;
- (NSString *)typeOfFile:(NSString *)path error:(NSError **)error;

- (BOOL)openFile:(NSString *)path;
- (BOOL)openFile:(NSString *)path withApplication:(NSString *)application;
- (BOOL)openTempFile:(NSString *)path;
- (BOOL)openFile:(NSString *)path fromImage:(NSImage *)image at:(NSPoint)point inView:(NSView *)view;
- (BOOL)openFile:(NSString *)path withApplication:(NSString *)application andDeactivate:(BOOL)deactivate;
- (BOOL)openURL:(NSURL *)url;

- (BOOL)selectFile:(NSString *)path inFileViewerRootedAtPath:(NSString *)rootedAtPath;
- (void)activateFileViewerSelectingURLs:(NSArray<NSURL *> *)fileURLs;
- (NSURL *)URLForApplicationToOpenURL:(NSURL *)url;
- (NSURL *)URLForApplicationWithBundleIdentifier:(NSString *)bundleIdentifier;
- (void)slideImage:(NSImage *)image from:(NSPoint)from to:(NSPoint)to;
- (BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(NSInteger *)tag;

- (BOOL)getFileSystemInfoForPath:(NSString *)path isRemovable:(BOOL *)isRemovable isWritable:(BOOL *)isWritable isUnmountable:(BOOL *)isUnmountable description:(NSString **)description type:(NSString **)type;

- (BOOL)getInfoForFile:(NSString *)path application:(NSString **)application type:(NSString **)type;

- (void)checkForRemovableMedia;
- (NSArray *)mountNewRemovableMedia;
- (NSArray *)mountedRemovableMedia;
- (NSArray *)mountedLocalVolumePaths;

- (BOOL)unmountAndEjectDeviceAtPath:(NSString *)path;

- (BOOL)fileSystemChanged;
- (BOOL)userDefaultsChanged;

- (void)noteFileSystemChanged;
- (void)noteFileSystemChanged:(NSString *)path;
- (void)noteUserDefaultsChanged;

- (BOOL)isFilePackageAtPath:(NSString *)path;
- (NSString *)absolutePathForAppBundleWithIdentifier:(NSString *)identifier;
- (NSString *)pathForApplication:(NSString *)application;
- (NSString *)fullPathForApplication:(NSString *)application;
- (NSArray *)launchedApplications;
- (NSArray *)runningApplications;

/* The desktop picture of each screen (API 19). */
-(BOOL)setDesktopImageURL:(NSURL *)url forScreen:(NSScreen *)screen options:(NSDictionary *)options error:(NSError **)error;
-(NSURL *)desktopImageURLForScreen:(NSScreen *)screen;
-(NSDictionary *)desktopImageOptionsForScreen:(NSScreen *)screen;
- (NSRunningApplication *)frontmostApplication;
- (BOOL)launchApplication:(NSString *)application;
- (BOOL)launchApplication:(NSString *)application showIcon:(BOOL)showIcon autolaunch:(BOOL)autolaunch;
- (BOOL)launchAppWithBundleIdentifier:(NSString *)bundleIdentifier options:(NSWorkspaceLaunchOptions)options additionalEventParamDescriptor:(id)descriptor launchIdentifier:(NSNumber **)identifier;
/* Launches (or finds, when it runs and no new instance is asked for) the
   application, with the configuration's arguments; the running
   application, or nil and the error. */
- (NSRunningApplication *)launchApplicationAtURL:(NSURL *)url options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary<NSWorkspaceLaunchConfigurationKey, id> *)configuration error:(NSError **)error;
- (NSRunningApplication *)openURL:(NSURL *)url options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary<NSWorkspaceLaunchConfigurationKey, id> *)configuration error:(NSError **)error;
- (NSRunningApplication *)openURLs:(NSArray<NSURL *> *)urls withApplicationAtURL:(NSURL *)applicationURL options:(NSWorkspaceLaunchOptions)options configuration:(NSDictionary<NSWorkspaceLaunchConfigurationKey, id> *)configuration error:(NSError **)error;
/* The same with an open configuration; the handler runs on a background
   queue. */
- (void)openApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;
- (void)openURL:(NSURL *)url configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;
- (void)openURLs:(NSArray<NSURL *> *)urls withApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;

- (void)findApplications;
- (NSDictionary *)activeApplication;
- (void)hideOtherApplications;

- (NSInteger)extendPowerOffBy:(NSInteger)milliseconds;

@end

@interface NSWorkspace (CocotronAdditions)

// Cocoa should provide a simple method for this - but it doesn't. You have to build it with Launch Services
// But so many files are typically hidden from users in GUI browsers that this just makes life better for everyone...
- (BOOL)isFileHiddenAtPath:(NSString *)path;

@end
