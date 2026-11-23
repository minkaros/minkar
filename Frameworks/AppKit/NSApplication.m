/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
 * Copyright (C) 2022-2026 Zoe Knox
 * Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <objc/runtime.h>
#import <sys/param.h>
#import <mach/mach.h>
#import <servers/bootstrap.h>
#import <Foundation/NSSocket_bsd.h>
#import <Foundation/NSSelectInputSource.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSDraggingManager.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#include <SkyLight/SkyLight.h>
#include <LaunchServices/LaunchServices.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSModalSessionX.h>
#import <AppKit/NSNibLoading.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSColorPanel.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSPageLayout.h>
#import <AppKit/NSDocumentController.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSSheetContext.h>
#import "NSAboutPanel.h"
#import <AppKit/NSHelpManager.h>

@interface NSHelpManager (NSApplicationHelp)
-(BOOL)_showApplicationHelp;
@end
#import <AppKit/NSAlert.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSDockTile.h>
#import <AppKit/NSRunningApplication.h>
#import "NSRunningApplication_private.h"
#import "NSMenu.subproj/NSMenuBarWindow.h"

/* NSWindow's: sends the server what it does not have of the window. */
@interface NSWindow (SkyLightState)
-(BOOL)_updateWSState;
@end
#import <CoreGraphics/CGWindow.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSSpellChecker.h>
#import <AppKit/NSStatusItem.h>
#import <objc/message.h>
#import <pthread.h>

#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>


/* Minkar: API 19's release, as the reference's last one reports it. */
const NSAppKitVersion NSAppKitVersionNumber=1894.7;
NSString * const NSModalPanelRunLoopMode=@"NSModalPanelRunLoopMode";
NSString * const NSEventTrackingRunLoopMode=@"NSEventTrackingRunLoopMode";

NSString * const NSApplicationWillFinishLaunchingNotification=@"NSApplicationWillFinishLaunchingNotification";
NSString * const NSApplicationDidFinishLaunchingNotification=@"NSApplicationDidFinishLaunchingNotification";
NSString * const NSApplicationLaunchUserNotificationKey=@"NSApplicationLaunchUserNotificationKey";
NSString * const NSApplicationLaunchIsDefaultLaunchKey=@"NSApplicationLaunchIsDefaultLaunchKey";

NSString * const NSApplicationWillBecomeActiveNotification=@"NSApplicationWillBecomeActiveNotification";
NSString * const NSApplicationDidBecomeActiveNotification=@"NSApplicationDidBecomeActiveNotification";
NSString * const NSApplicationWillResignActiveNotification=@"NSApplicationWillResignActiveNotification";
NSString * const NSApplicationDidResignActiveNotification=@"NSApplicationDidResignActiveNotification";

NSString * const NSApplicationWillUpdateNotification=@"NSApplicationWillUpdateNotification";
NSString * const NSApplicationDidUpdateNotification=@"NSApplicationDidUpdateNotification";

NSString * const NSApplicationWillHideNotification=@"NSApplicationWillHideNotification";
NSString * const NSApplicationDidHideNotification=@"NSApplicationDidHideNotification";
NSString * const NSApplicationWillUnhideNotification=@"NSApplicationWillUnhideNotification";
NSString * const NSApplicationDidUnhideNotification=@"NSApplicationDidUnhideNotification";

NSString * const NSApplicationWillTerminateNotification=@"NSApplicationWillTerminateNotification";

NSString * const NSApplicationDidChangeScreenParametersNotification=@"NSApplicationDidChangeScreenParametersNotification";

@interface NSDocumentController(forward)
-(void)_updateRecentDocumentsMenu;
@end

@interface NSMenu(private)
-(NSMenu *)_menuWithName:(NSString *)name;
@end

@interface NSDockTile(private)
-initWithOwner:owner;
@end

/* NSDocumentController's, for the launch's documents. */
@interface NSDocumentController (NSApplicationLaunch)
-(void)_noteTransientDocument:(NSDocument *)document;
-(void)_openDocumentPresentingError:(NSURL *)url;
@end

@implementation NSApplication

static NSAppearance *applicationAppearance;

-(NSAppearance *)appearance {
   return applicationAppearance;
}

-(void)setAppearance:(NSAppearance *)appearance {
   [self willChangeValueForKey:@"effectiveAppearance"];
   appearance=[appearance retain];
   [applicationAppearance release];
   applicationAppearance=appearance;
   [self didChangeValueForKey:@"effectiveAppearance"];
}

BOOL _NSSystemAppearanceIsDark(void);

/* The one set, or the system's: dark or light as General in System
   Preferences says. */
-(NSAppearance *)effectiveAppearance {
   if(applicationAppearance!=nil)
    return applicationAppearance;
   return [NSAppearance appearanceNamed:_NSSystemAppearanceIsDark()?NSAppearanceNameDarkAqua:NSAppearanceNameAqua];
}

void _NSWatchSystemAccentColors(void);

__kindof NSApplication *NSApp=nil;

+(NSApplication *)sharedApplication {

   if(NSApp==nil){
      [[self alloc] init]; // NSApp must be nil inside init
   }

   return NSApp;
}

+(void)detachDrawingThread:(SEL)selector toTarget:target withObject:object {
   NSUnimplementedMethod();
}

-(void)_showSplashImage {
   NSImage *image=[NSImage imageNamed:@"splash"];

   if(image!=nil){
    NSSize    imageSize=[image size];
    NSWindow *splash=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,imageSize.width,imageSize.height) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [splash setLevel:NSFloatingWindowLevel];

    NSImageView *view=[[NSImageView alloc] initWithFrame:NSMakeRect(0,0,imageSize.width,imageSize.height)];

    [view setImage:image];
    [splash setContentView:view];
    [view release];
    [splash setReleasedWhenClosed:YES];
    [splash center];
    [splash orderFront:nil];
    [splash display];
   }
}

-(void)_closeSplashImage {
   int i;

   for(i=0;i<[_windows count];i++){
    NSWindow *check=[_windows objectAtIndex:i];
    NSView   *contentView=[check contentView];

    if([contentView isKindOfClass:[NSImageView class]])
     if([[[(NSImageView *)contentView image] name] isEqual:@"splash"]){
      [check close];
      return;
     }
   }
}

/*
 * Events from the window server (WindowServer plan, WS3): a thread waits
 * for them through SkyLight, makes NSEvents and posts them.
 */
extern int _CGUsesSkyLight(void);
extern NSWindow *_NSWindowForSLSWindow(SLSWindowID window);

/* Control-F2 (the server's kSLSHotKeyMenuBarFocus) as AppKit posts it to
   itself: the menu bar takes the keyboard. A display that changed
   (kSLSEventDisplayReconfigured) goes to the main thread the same way:
   data1 the display, data2 the CGDisplayChangeSummaryFlags. */
enum { kNSMenuBarFocusEventSubtype = 0x4d42, kNSNextWindowEventSubtype = 0x4e57,
       kNSDisplayReconfiguredEventSubtype = 0x4443,
       kNSWindowRestoredEventSubtype = 0x5752 };
extern void _CGDisplayReconfigured(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags);

-(void)_postSkyLightEvent:(SLSEvent *)e {
    NSWindow *window = _NSWindowForSLSWindow(e->window);
    NSEvent *event = nil;

    if(e->type == kSLSEventDisplayReconfigured) {
        [_display postEvent:[NSEvent otherEventWithType:NSAppKitDefined location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil
            subtype:kNSDisplayReconfiguredEventSubtype data1:e->buttonNumber data2:e->flags]
            atStart:NO];
        return;
    }

    /* A drag between applications: the dragging manager, on the main
       thread, in the modes a drag's own tracking loop runs too. */
    if(e->type == kSLSEventDrag) {
        [[NSDraggingManager draggingManager] performSelectorOnMainThread:@selector(_skyLightDragEvent:)
            withObject:[NSData dataWithBytes:e length:sizeof(*e)] waitUntilDone:NO
            modes:[NSArray arrayWithObjects:NSDefaultRunLoopMode, NSEventTrackingRunLoopMode,
                   NSModalPanelRunLoopMode, nil]];
        return;
    }

    if(e->type == kSLSEventSymbolicHotKey) {
        if(e->buttonNumber == kSLSHotKeyMenuBarFocus || e->buttonNumber == kSLSHotKeyNextWindow)
            [_display postEvent:[NSEvent otherEventWithType:NSAppKitDefined location:NSZeroPoint
                modifierFlags:0 timestamp:0 windowNumber:0 context:nil
                subtype:e->buttonNumber == kSLSHotKeyNextWindow ? kNSNextWindowEventSubtype
                                                               : kNSMenuBarFocusEventSubtype
                data1:0 data2:0] atStart:NO];
        else {
            /* A hotkey the app claimed (the Dock's Command-Tab): a
               notification on the main thread, in every run loop mode. */
            NSNotification *note = [NSNotification notificationWithName:@"_NSSkyLightSymbolicHotKeyNotification"
                object:nil userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithUnsignedInt:e->buttonNumber], @"hotKey",
                    [NSNumber numberWithUnsignedInt:e->flags], @"flags", nil]];
            [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:)
                withObject:note waitUntilDone:NO
                modes:[NSArray arrayWithObjects:NSDefaultRunLoopMode, NSEventTrackingRunLoopMode,
                       NSModalPanelRunLoopMode, nil]];
        }
        return;
    }

    /* Another app's window minimized or restored (to a connection that
       asked, the Dock): a notification on the main thread, with the
       server's window and its frame's top left. */
    if(window == nil && (e->type == kSLSEventWindowMinimized || e->type == kSLSEventWindowRestored)) {
        NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithUnsignedInt:e->window], @"window",
            [NSNumber numberWithDouble:e->x], @"x", [NSNumber numberWithDouble:e->y], @"y", nil];
        NSNotification *note = [NSNotification notificationWithName:e->type == kSLSEventWindowMinimized ?
            @"_NSSkyLightWindowMinimizedNotification" : @"_NSSkyLightWindowRestoredNotification"
            object:nil userInfo:info];
        [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:)
            withObject:note waitUntilDone:NO];
        return;
    }
    if(window == nil)
        return;
    /* Global, from the top left, to the window's own coordinates (the
       main display's height read each time: a mode change changes it). */
    CGFloat height = CGDisplayBounds(CGMainDisplayID()).size.height;
    NSRect frame = [window frame];
    NSPoint location = NSMakePoint(e->x - frame.origin.x, (height - e->y) - frame.origin.y);

    switch(e->type) {
        case kSLSEventKeyDown:
        case kSLSEventKeyUp:
        case kSLSEventFlagsChanged:
            event = [NSEvent keyEventWithType:e->type location:location
                modifierFlags:e->flags window:window
                characters:[NSString stringWithUTF8String:e->characters]
                charactersIgnoringModifiers:[NSString stringWithUTF8String:e->charactersIgnoringModifiers]
                isARepeat:e->isRepeat keyCode:e->keyCode];
            break;
        case kSLSEventLeftMouseDown: case kSLSEventLeftMouseUp:
        case kSLSEventRightMouseDown: case kSLSEventRightMouseUp:
        case kSLSEventOtherMouseDown: case kSLSEventOtherMouseUp:
        case kSLSEventMouseMoved:
        case kSLSEventLeftMouseDragged: case kSLSEventRightMouseDragged:
        case kSLSEventOtherMouseDragged:
            event = [NSEvent mouseEventWithType:e->type location:location
                modifierFlags:e->flags window:window clickCount:e->clickCount
                deltaX:e->dx deltaY:e->dy];
            break;
        case kSLSEventScrollWheel:
            event = [NSEvent mouseEventWithType:NSScrollWheel location:location
                modifierFlags:e->flags window:window deltaY:e->scrollY];
            break;
        case kSLSEventWindowRestored:
            /* The Dock restored a window of ours it kept. */
            event = [NSEvent otherEventWithType:NSAppKitDefined location:NSZeroPoint
                modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil
                subtype:kNSWindowRestoredEventSubtype data1:0 data2:0];
            break;
        case kSLSEventWindowMoved:
            event = [NSEvent otherEventWithType:NSAppKitDefined location:NSZeroPoint
                modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil
                subtype:NSWindowMovedEventType data1:(NSInteger)e->x data2:(NSInteger)e->y];
            break;
        default:
            return;
    }
    [_display postEvent:event atStart:NO];
}

/*
 * A display changed: the reconfiguration callbacks run, the screens and
 * the menu bars follow the new configuration, and each window keeps its
 * place from the top of the main screen (where the server keeps it) and
 * is brought back onto a screen if the change left it off; then
 * NSApplicationDidChangeScreenParametersNotification, as on the reference
 * platform.
 */
-(void)_displayReconfigured:(CGDirectDisplayID)display flags:(CGDisplayChangeSummaryFlags)flags {
    /* No flags: only the visible part changed (the Dock moved, resized or
       hid itself). The screens are read again and apps told, but windows
       stay where they are, as on the reference platform. */
    if(flags == 0) {
        [[NSDisplay currentDisplay] _reloadScreens];
        [[NSNotificationCenter defaultCenter]
            postNotificationName:NSApplicationDidChangeScreenParametersNotification object:self];
        return;
    }
    NSRect was = [[[NSScreen screens] objectAtIndex:0] frame];

    _CGDisplayReconfigured(display, flags);
    [[NSDisplay currentDisplay] _reloadScreens];
    [NSMenuBarWindow _screensChanged];

    NSScreen *main = [[NSScreen screens] objectAtIndex:0];
    CGFloat top = NSMaxY([main frame]);
    for(NSWindow *window in [[_windows copy] autorelease]) {
        if([window isKindOfClass:[NSMenuBarWindow class]])
            continue;
        NSRect frame = [window frame];
        frame.origin.y = top - (NSMaxY(was) - NSMaxY(frame)) - frame.size.height;
        NSScreen *screen = [window screen];
        frame = [window constrainFrameRect:frame toScreen:screen != nil ? screen : main];
        if(!NSEqualRects(frame, [window frame]))
            [window setFrame:frame display:[window isVisible]];
        else
            /* The same AppKit frame is another place on the server's
               desktop when the main screen's height changed. */
            [window _updateWSState];
    }
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSApplicationDidChangeScreenParametersNotification object:self];
}

-(void)skyLightEventLoop:(id)object {
    SLSEvent events[kSLSMaxEvents];

    for(;;) {
        uint32_t n = 0;

        int err = SLSReceiveEvents(events, kSLSMaxEvents, &n, 1);
        /* The window server went away: as on the reference platform, the
         * app ends with it (loginwindow starts the session's apps again). */
        /* Minkar: unless the app closed it itself (a browser's content
         * process, CGSShutdownServerConnections): then no events come. */
        if(err == kSLSErrorInvalidConnection && SLSServerConnectionsDenied())
            return;
        if(err == kSLSErrorInvalidConnection) {
            fprintf(stderr, "%s: the window server went away; exiting\n", getprogname());
            _exit(1);
        }
        if(err != kSLSErrorSuccess) {
            sleep(1);
            continue;
        }
        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        for(uint32_t i = 0; i < n; i++)
            [self _postSkyLightEvent:&events[i]];
        [pool release];
        if(n > 0 && [self _wakeUp] < 0)
            NSLog(@"Error waking runloop - write: %s", strerror(errno));
    }
}

/* Each event notification is 2 chars. Don't read more! */
-(NSInteger)_drainPipe {
    char junk[2];
    fd_set fds;
    struct timeval tv = {0};

    FD_ZERO(&fds);
    FD_SET(_machEventPipe[0], &fds);
    if(select(_machEventPipe[0] + 1, &fds, NULL, NULL, &tv) == 1)
        if(FD_ISSET(_machEventPipe[0], &fds))
            return read(_machEventPipe[0], junk, sizeof(junk));
    return 0;
}

-(NSInteger)_wakeUp {
    return(write(_machEventPipe[1], "\n", 2));
}

-init {
    if(NSApp)
        NSAssert(!NSApp, @"NSApplication is a singleton");
    NSApp=[self retain];

    if(pipe(_machEventPipe) != 0) {
        NSLog(@"pipe: %s", strerror(errno));
        exit(-1);
    }
    _inputSource = [[NSSelectInputSource socketInputSourceWithSocket:
            [[NSSocket_bsd socketWithDescriptor:_machEventPipe[0]] retain]] retain];
    [_inputSource setSelectEventMask:NSSelectReadEvent];

    //[NSRunLoop mainRunLoop];
    [NSRunLoop currentRunLoop];

   _display=[[NSDisplay currentDisplay] retain];

   _windows=[[NSMutableArray new] retain];
   _mainMenu=nil;

    NSBundle *mainBundle = [NSBundle mainBundle];

   // don't try to find the service if this is the app that provides it...
   bundleID = [mainBundle bundleIdentifier];
    if(bundleID == nil)
        bundleID = [NSString stringWithFormat:@"unix.%u", getpid()];
   /* SkyLight is the window server's connection, and its events come
    * through skyLightEventLoop:. */
   [NSThread detachNewThreadSelector:@selector(skyLightEventLoop:) toTarget:self withObject:nil];
   /* As LaunchServices reads it: an agent (LSUIElement) has no Dock icon
    * or menu bar, a background-only app (LSBackgroundOnly) no windows. */
   _activationPolicy=NSApplicationActivationPolicyRegular;
   if([[mainBundle objectForInfoDictionaryKey:@"LSBackgroundOnly"] boolValue])
       _activationPolicy=NSApplicationActivationPolicyProhibited;
   else if([[mainBundle objectForInfoDictionaryKey:@"LSUIElement"] boolValue])
       _activationPolicy=NSApplicationActivationPolicyAccessory;

   /* The accent and highlight colors chosen in System Preferences, and
      their changes. */
   _NSWatchSystemAccentColors();

   _dockTile=[[NSDockTile alloc] initWithOwner:self];
   _modalStack=[NSMutableArray new];

   _lock=NSZoneMalloc(NULL,sizeof(pthread_mutex_t));

   pthread_mutex_init(_lock,NULL);

   // NSDisplay asked WindowServer for the displays when it was created
   // (CGGetActiveDisplayList), so they are there already.
    [self _showSplashImage];

   return NSApp;
}

-(NSSelectInputSource *)inputSource {
    return _inputSource;
}

-(void)dealloc {
    [super dealloc];
}

-(NSGraphicsContext *)context {
   NSUnimplementedMethod();
   return nil;
}

-delegate {
   return _delegate;
}

-(NSArray *)windows {
   /* A copy, as the reference gives it: callers change the list while
      going through it (Transmission takes items out of a menu so). */
   return [[_windows copy] autorelease];
}

-(NSWindow *)windowWithWindowNumber:(NSInteger)number {
   int i,count=[_windows count];

   for(i=0;i<count;i++){
    NSWindow *check=[_windows objectAtIndex:i];

    if((uint32_t)[check windowNumber]==(uint32_t)number)
     return check;
   }

   return nil;
}

-(NSMenu *)mainMenu {
   return _mainMenu;
}

-(NSMenu *)menu {
  return [self mainMenu];
}

@synthesize helpMenu=_helpMenu;

/* The Help menu: the one set, else the main menu's menu titled Help (in
   the app's language), where the Help search lives (MB2). */
-(NSMenu *)helpMenu {
   if(_helpMenu!=nil)
    return _helpMenu;
   NSString *localized=[[NSBundle mainBundle] localizedStringForKey:@"Help" value:@"Help" table:nil];
   for(NSMenuItem *item in [[self mainMenu] itemArray])
    if([item hasSubmenu] && ([[item title] isEqualToString:@"Help"] || [[item title] isEqualToString:localized]))
     return [item submenu];
   return nil;
}

-(NSMenu *)windowsMenu {
   if(_windowsMenu==nil) {
    _windowsMenu=[[NSApp mainMenu] _menuWithName:@"_NSWindowsMenu"];
    if (_windowsMenu && ![[[_windowsMenu itemArray] lastObject] isSeparatorItem])
     [_windowsMenu addItem:[NSMenuItem separatorItem]];
   }

   return _windowsMenu;
}

-(NSWindow *)mainWindow {
   return _mainWindow;
}

-(void)_setMainWindow:(NSWindow *)window {
   _mainWindow=window;
}

-(NSWindow *)keyWindow {
   return _keyWindow;
}

/* The frontmost visible window of the app that can be key (and main) other
   than the one given: key and main, as the reference does when the key
   window goes away or none was made key. */
-(void)_makeFrontWindowKeyExcept:(NSWindow *)except {
   NSWindow *key=nil,*main=nil;

   for(NSWindow *window in [self orderedWindows]){
    /* Document and panel windows at the normal level only: not a status
       item's, a menu's or a banner's (SystemUIServer's panel going away
       made its status item's window key). */
    if(window==except || ![window isVisible] || [window isMiniaturized] || [window level]!=NSNormalWindowLevel)
     continue;
    if(key==nil && [window canBecomeKeyWindow])
     key=window;
    if(main==nil && [window canBecomeMainWindow])
     main=window;
   }
   if(main!=nil && _mainWindow==nil)
    [main makeMainWindow];
   if(key!=nil && _keyWindow==nil)
    [key makeKeyWindow];
}

-(void)_setKeyWindow:(NSWindow *)window {
   _keyWindow=window;
}

/* An app without an icon of its own has the generic one, as on the
   reference platform. */
-(NSImage *)applicationIconImage {
   if(_applicationIconImage==nil)
    return [NSImage imageNamed:@"NSApplicationIcon"];
   return _applicationIconImage;
}

-(BOOL)isActiveExcludingWindow:(NSWindow *)exclude {
   int count=[_windows count];

   while(--count>=0){
    NSWindow *check=[_windows objectAtIndex:count];

    if(check==exclude)
     continue;

    if([check _isActive])
     return YES;
   }

   return _isActive;
}

-(BOOL)isActive {
   return [self isActiveExcludingWindow:nil];
}

-(BOOL)isHidden {
    return _isHidden;
}

-(BOOL)isRunning {
   return _isRunning;
}

-(NSWindow *)makeWindowsPerform:(SEL)selector inOrder:(BOOL)inOrder {
   NSUnimplementedMethod();
   return nil;
}

-(void)miniaturizeAll:sender {
   int count=[_windows count];

   while(--count>=0)
    [[_windows objectAtIndex:count] miniaturize:sender];
}

-(NSArray *)orderedDocuments {
   NSMutableArray *result=[NSMutableArray array];
   NSArray        *orderedWindows=[self orderedWindows];

   for(NSWindow *checkWindow in orderedWindows){
    NSDocument *checkDocument=[[checkWindow windowController] document];

    if(checkDocument!=nil)
     [result addObject:checkDocument];
   }

   return result;
}

/* Command-` (the server's kSLSHotKeyNextWindow): the front window goes
   behind the app's others and the next one is key, so the app's windows
   take turns, as on the reference. */
-(void)_moveFocusToNextWindow {
   NSMutableArray *windows=[NSMutableArray array];

   for(NSWindow *window in [self orderedWindows])
    if([window isVisible] && ![window isMiniaturized] && [window level]==NSNormalWindowLevel &&
       ![window isKindOfClass:[NSPanel class]] && [window canBecomeKeyWindow])
     [windows addObject:window];
   if([windows count]<2)
    return;
   NSWindow *front=[windows objectAtIndex:0], *next=[windows objectAtIndex:1];
   [next makeKeyAndOrderFront:nil];
   [front orderWindow:NSWindowBelow relativeTo:[[windows lastObject] windowNumber]];
}

-(NSArray *)orderedWindows {
  NSMutableArray *result=[NSMutableArray array];
  // Front-to-back order from WindowServer, including off-screen windows.
  CFArrayRef info=CGWindowListCopyWindowInfo(kCGWindowListOptionAll,kCGNullWindowID);

  if(info==NULL){
   // WindowServer unreachable: fall back to creation order.
   for(NSWindow *window in [self windows])
    if(![window isKindOfClass:[NSPanel class]] && ![window _isInternalWindow])
     [result addObject:window];
   return result;
  }

  for(NSNumber *number in _NSWindowNumbersFromWindowInfo(info,NO)){
   NSWindow *window=[self windowWithWindowNumber:[number integerValue]];

   if(window!=nil && ![window isKindOfClass:[NSPanel class]] && ![window _isInternalWindow])
    [result addObject:window];
  }
  CFRelease(info);

  return result;
}

-(void)preventWindowOrdering {
   NSUnimplementedMethod();
}

-(void)unregisterDelegate {
    if([_delegate respondsToSelector:@selector(applicationWillFinishLaunching:)]){
        [[NSNotificationCenter defaultCenter] removeObserver:_delegate
                                                     name:NSApplicationWillFinishLaunchingNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationDidFinishLaunching:)]){
        [[NSNotificationCenter defaultCenter] removeObserver:_delegate
                                                     name:NSApplicationDidFinishLaunchingNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationDidBecomeActive:)]){
        [[NSNotificationCenter defaultCenter] removeObserver:_delegate
                                                     name: NSApplicationDidBecomeActiveNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationWillTerminate:)]){
        [[NSNotificationCenter defaultCenter] removeObserver:_delegate
                                                     name: NSApplicationWillTerminateNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationDidChangeScreenParameters:)]){
        [[NSNotificationCenter defaultCenter] removeObserver:_delegate
            name:NSApplicationDidChangeScreenParametersNotification object:self];
    }
}

-(void)registerDelegate {
    if([_delegate respondsToSelector:@selector(applicationWillFinishLaunching:)]){
     [[NSNotificationCenter defaultCenter] addObserver:_delegate
       selector:@selector(applicationWillFinishLaunching:)
        name:NSApplicationWillFinishLaunchingNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationDidFinishLaunching:)]){
     [[NSNotificationCenter defaultCenter] addObserver:_delegate
       selector:@selector(applicationDidFinishLaunching:)
        name:NSApplicationDidFinishLaunchingNotification object:self];
    }
    if([_delegate respondsToSelector:@selector(applicationDidBecomeActive:)]){
     [[NSNotificationCenter defaultCenter] addObserver:_delegate
       selector:@selector(applicationDidBecomeActive:)
        name: NSApplicationDidBecomeActiveNotification object:self];
    }
   if([_delegate respondsToSelector:@selector(applicationWillTerminate:)]){
      [[NSNotificationCenter defaultCenter] addObserver:_delegate
                                               selector:@selector(applicationWillTerminate:)
                                                   name: NSApplicationWillTerminateNotification object:self];
   }
    if([_delegate respondsToSelector:@selector(applicationDidChangeScreenParameters:)]){
     [[NSNotificationCenter defaultCenter] addObserver:_delegate
       selector:@selector(applicationDidChangeScreenParameters:)
        name:NSApplicationDidChangeScreenParametersNotification object:self];
    }

}

-(void)setDelegate:delegate {
    if (delegate != _delegate) {
        [self unregisterDelegate];
        _delegate=delegate;
        [self registerDelegate];
    }
}

/* The app's name as the reference platform shows it: its bundle's
   CFBundleName (localized), else the process name. */
static NSString *applicationMenuTitle(void) {
   NSBundle *main = [NSBundle mainBundle];
   NSString *appName = [main objectForInfoDictionaryKey:@"CFBundleName"];

   if([appName length] == 0)
    return [[NSProcessInfo processInfo] processName];
   return [main localizedStringForKey:@"CFBundleName" value:appName table:@"InfoPlist"];
}

/* Marks the app menu AppKit adds to a main menu that has none. */
static NSString * const NSAutomaticApplicationMenu = @"NSAutomaticApplicationMenu";

/* The main menu's first item is the app menu, whatever its title, shown
   as the app's name. A main menu the app sets with no items gets one with
   Quit; when the app then adds its own items (a menu made in code, set
   while still empty), that one goes and the app's first item takes its
   place. The main menu AppKit makes for an app with none keeps its app
   menu: the app's items come after it. */
-(void)_ensureApplicationMenu {
   static BOOL busy;
   NSMenuItem *first;

   if(busy || _mainMenu == nil)
    return;
   busy = YES;

   first = ([_mainMenu numberOfItems] > 0) ? [_mainMenu itemAtIndex:0] : nil;
   if(first != nil && [first representedObject] == NSAutomaticApplicationMenu && [_mainMenu numberOfItems] > 1) {
    [_mainMenu removeItemAtIndex:0];
    first = [_mainMenu itemAtIndex:0];
   }

   if(first != nil && [[[first submenu] _name] isEqualToString:@"NSAppleMenu"]) {
    if([first representedObject] != NSAutomaticApplicationMenu)
     [first setTitle:applicationMenuTitle()];
   }
   else if([first hasSubmenu] && [[first submenu] _name] == nil) {
    [[first submenu] setValue:@"NSAppleMenu" forKey:@"name"];
    [first setTitle:applicationMenuTitle()];
   }
   else if([_mainMenu _menuWithName:@"NSAppleMenu"] == nil) {
    NSString   *appName = applicationMenuTitle();
    NSMenuItem *appleMenuItem = [[NSMenuItem alloc] init];
    NSMenu     *appleMenu = [[NSMenu alloc] initApplicationMenu:appName];

    [appleMenuItem setTitle:appName];
    [appleMenuItem setSubmenu:appleMenu];
    [appleMenuItem setRepresentedObject:NSAutomaticApplicationMenu];
    [_mainMenu insertItem:appleMenuItem atIndex:0];
    [appleMenu release];
    [appleMenuItem release];
   }

   busy = NO;
}

-(void)setMainMenu:(NSMenu *)menu {
   int i,count=[_windows count];

   [_mainMenu autorelease];
   _mainMenu=[menu retain];

   [self _ensureApplicationMenu];

   for(i=0;i<count;i++){
    NSWindow *window=[_windows objectAtIndex:i];

    if(![window isKindOfClass:[NSPanel class]])
     [window setMenu:_mainMenu];
   }

    [NSMenuBarWindow _mainMenuChanged];
}

/* NSDocumentController writes the recent items to LaunchServices' shared
   file lists itself; nothing more to do here. */
- (void)addRecentItem:(NSURL *)url {
}

-(void)setMenu:(NSMenu *)menu {
   [self setMainMenu:menu];
}

-(void)setApplicationIconImage:(NSImage *)image {
   image=[image retain];
   [_applicationIconImage release];
   _applicationIconImage=image;

    [image setName: @"NSApplicationIcon"];
}

-(void)setWindowsMenu:(NSMenu *)menu {
   [_windowsMenu autorelease];
   _windowsMenu=[menu retain];
    [NSMenuBarWindow _mainMenuChanged];
}


-(void)addWindowsItem:(NSWindow *)window title:(NSString *)title filename:(BOOL)isFilename {
    NSMenuItem *item;

    if ([[self windowsMenu] indexOfItemWithTarget:window andAction:@selector(makeKeyAndOrderFront:)] != -1)
        return;

    if (isFilename)
        title = [NSString stringWithFormat:@"%@  --  %@", [title lastPathComponent],[title stringByDeletingLastPathComponent]];

    item = [[[NSMenuItem alloc] initWithTitle:title action:@selector(makeKeyAndOrderFront:) keyEquivalent:@""] autorelease];
    [item setTarget:window];

    [[self windowsMenu] addItem:item];
    [NSMenuBarWindow _mainMenuChanged];
}

-(void)changeWindowsItem:(NSWindow *)window title:(NSString *)title filename:(BOOL)isFilename {

    if ([title length] == 0) {
    // Windows with no name aren't in the Windows menu
        [self removeWindowsItem:window];
    } else {
        int itemIndex = [[self windowsMenu] indexOfItemWithTarget:window andAction:@selector(makeKeyAndOrderFront:)];

        if (itemIndex != -1) {
            NSMenuItem *item = [[self windowsMenu] itemAtIndex:itemIndex];

            if (isFilename)
                title = [NSString stringWithFormat:@"%@  --  %@",[title lastPathComponent], [title stringByDeletingLastPathComponent]];

            [item setTitle:title];
            [[self windowsMenu] itemChanged:item];
        }
        else
            [self addWindowsItem:window title:title filename:isFilename];
    }
    [NSMenuBarWindow _mainMenuChanged];
}

-(void)removeWindowsItem:(NSWindow *)window {
    int itemIndex = [[self windowsMenu] indexOfItemWithTarget:window andAction:@selector(makeKeyAndOrderFront:)];

    if (itemIndex != -1) {
        [[self windowsMenu] removeItemAtIndex:itemIndex];

        if ([[[[self windowsMenu] itemArray] lastObject] isSeparatorItem]){
            [[self windowsMenu] removeItem:[[[self windowsMenu] itemArray] lastObject]];
          }
    }
    [NSMenuBarWindow _mainMenuChanged];
}

-(void)updateWindowsItem:(NSWindow *)window {
   NSMenu *menu=[self windowsMenu];
   int     itemIndex=[[self windowsMenu] indexOfItemWithTarget:window andAction:@selector(makeKeyAndOrderFront:)];

   if(itemIndex!=-1){
    NSMenuItem *item=[menu itemAtIndex:itemIndex];

   }
    [NSMenuBarWindow _mainMenuChanged];
}

/* Open requests that came with the check-in (sent while the app was
 * launching), delivered as it finishes launching. */
static NSMutableArray *pendingOpenRequests;

/* An open request's URLs: absolute strings, each with its NUL. */
static NSArray *urlsFromOpenMessage(const LSSessionOpenMessage *msg, NSUInteger size) {
   NSMutableArray *urls=[NSMutableArray array];
   NSUInteger at=0, length=msg->length;

   if(size<offsetof(LSSessionOpenMessage, urls))
      return urls;
   if(length>size-offsetof(LSSessionOpenMessage, urls))
      length=size-offsetof(LSSessionOpenMessage, urls);
   while(at<length) {
      const char *s=msg->urls+at;
      size_t n=strnlen(s, length-at);
      if(n==length-at)
         break;	/* no NUL: cut off */
      NSString *string=[[[NSString alloc] initWithBytes:s length:n encoding:NSUTF8StringEncoding] autorelease];
      NSURL *url=string ? [NSURL URLWithString:string] : nil;
      if(url!=nil)
         [urls addObject:url];
      at+=n+1;
   }
   return urls;
}

/* URLs from LaunchServices: to the delegate as the reference platform
 * does (openURLs:, else openFiles: or openFile:, else the document
 * controller); YES when something took them. */
-(BOOL)_openURLs:(NSArray *)urls reveal:(BOOL)reveal {
   if([urls count]==0)
      return NO;
   if(reveal) {
      if([_delegate respondsToSelector:@selector(application:revealURLs:)]) {
         [_delegate application:self revealURLs:urls];
         return YES;
      }
      return NO;
   }
   if([_delegate respondsToSelector:@selector(application:openURLs:)]) {
      [_delegate application:self openURLs:urls];
      return YES;
   }
   NSMutableArray *paths=[NSMutableArray array];
   for(NSURL *url in urls)
      if([url isFileURL])
         [paths addObject:[url path]];
   if([paths count]==0)
      return NO;
   if([_delegate respondsToSelector:@selector(application:openFiles:)]) {
      [_delegate application:self openFiles:paths];
      return YES;
   }
   if([_delegate respondsToSelector:@selector(application:openFile:)]) {
      BOOL opened=NO;
      for(NSString *path in paths)
         opened|=[_delegate application:self openFile:path];
      return opened;
   }
   id types=[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleDocumentTypes"];
   if([types count]>0) {
      /* Through the newer method, as the reference opens them: a document
         controller that overrides it is called. */
      for(NSString *path in paths)
         [[NSDocumentController sharedDocumentController] _openDocumentPresentingError:[NSURL fileURLWithPath:path]];
      return YES;
   }
   return NO;
}

-(BOOL)openFiles
{
   BOOL opened = NO;

   /* What LaunchServices sent as the app launched. */
   for(NSDictionary *request in pendingOpenRequests)
      opened|=[self _openURLs:[request objectForKey:@"urls"] reveal:[[request objectForKey:@"reveal"] boolValue]];
   [pendingOpenRequests release];
   pendingOpenRequests=nil;
   if(_delegate) {
      id nsOpen = [[NSUserDefaults standardUserDefaults] objectForKey:@"NSOpen"];
      NSArray *openFiles = nil;
      if ([nsOpen isKindOfClass:[NSString class]] && [nsOpen length]) {
         openFiles = [NSArray arrayWithObject:nsOpen];
      } else if ([nsOpen isKindOfClass:[NSArray class]]) {
         openFiles = nsOpen;
      }
      if ([openFiles count] > 0) {
         if ([openFiles count] == 1 && [_delegate respondsToSelector: @selector(application:openFile:)]) {

            if([_delegate application: self openFile: [openFiles lastObject]]) {
               opened = YES;
            }
         } else {
            if ([_delegate respondsToSelector: @selector(application:openFiles:)]) {
               [_delegate application: self openFiles: openFiles];
               opened = YES;

            } else if ([_delegate respondsToSelector: @selector(application:openFile:)]) {
               for (NSString *aFile in openFiles) {
                  opened |= [_delegate application: self openFile: aFile];
               }
            }
         }
      }
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"NSOpen"];
   }
   return opened;
}

/* Distributed notifications wait while a regular app is inactive (each
   observer's suspension behavior), as on the reference platform; an agent
   or accessory app, seldom active, gets them as they come. */
-(void)_activeForDistributedNotifications:(NSNotification *)note {
   if(_activationPolicy!=NSApplicationActivationPolicyRegular)
    return;
   [[NSDistributedNotificationCenter defaultCenter] setSuspended:
    [[note name] isEqualToString:NSApplicationDidResignActiveNotification]];
}

-(void)finishLaunching {
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_activeForDistributedNotifications:)
      name:NSApplicationDidBecomeActiveNotification object:self];
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_activeForDistributedNotifications:)
      name:NSApplicationDidResignActiveNotification object:self];
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   BOOL               needsUntitled=YES;

   [self _checkInWithLaunchServices];

   NS_DURING
    [[NSNotificationCenter defaultCenter] postNotificationName: NSApplicationWillFinishLaunchingNotification object:self];
   NS_HANDLER
    [self reportException:localException];
   NS_ENDHANDLER

    // Load the application icon if we have one
    NSString* iconName = [[[NSBundle mainBundle] infoDictionary]
                          objectForKey:@"CFBundleIconFile"];
    if (iconName) {
        // Minkar: the name may already have its .icns.
        if ([[iconName pathExtension] length] == 0)
            iconName = [iconName stringByAppendingPathExtension: @"icns"];
        NSImage* image = [NSImage imageNamed: iconName];
        [self setApplicationIconImage: image];
    }

    /* If there are no menus loaded (e.g. we have no nib), make sure we at least have
     * the application menu
     */
    if([self mainMenu] == nil) {
        NSMenu *menu = [[NSMenu alloc] init];

        [self setMenu:menu];
        [menu release];
        /* AppKit's own: the app adds its menus after this app menu. */
        if([_mainMenu numberOfItems] > 0)
            [[_mainMenu itemAtIndex:0] setRepresentedObject:nil];
    }

// Give us a first event
   [NSTimer scheduledTimerWithTimeInterval:0.1 target:nil
     selector:NULL userInfo:nil repeats:NO];

   [self _closeSplashImage];

   NSDocumentController *controller = nil;
   id types=[[[NSBundle mainBundle]
         infoDictionary]
        objectForKey:@"CFBundleDocumentTypes"];
   if([types count] > 0)
       controller = [NSDocumentController sharedDocumentController];

   if ([self openFiles]) {
      needsUntitled = NO;
   }

   if(needsUntitled && _delegate &&
      [_delegate respondsToSelector: @selector(applicationShouldOpenUntitledFile:)]) {
       needsUntitled = [_delegate applicationShouldOpenUntitledFile: self];
   }

   if(needsUntitled && _delegate && [_delegate respondsToSelector: @selector(applicationOpenUntitledFile:)]) {
     needsUntitled = ![_delegate applicationOpenUntitledFile: self];
   }

   if(needsUntitled && controller && ![controller documentClassForType:[controller defaultType]]) {
       needsUntitled = NO;
   }

   if(needsUntitled && controller) {
       [controller _updateRecentDocumentsMenu];
       [controller newDocument: self];
       /* The first document opened takes its place while untouched. */
       if([[controller documents] count]==1)
           [controller _noteTransientDocument:[[controller documents] lastObject]];
   }

   NS_DURING
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidFinishLaunchingNotification object:self];
   NS_HANDLER
    [self reportException:localException];
   NS_ENDHANDLER

   [self _addFullScreenItem];
   [self _addTabItems];
   [self _addWindowAlternates];
   [self _noteRecentApplication];
   /* A window shown at launch with none made key (a nib's window visible
      at launch): the frontmost one becomes key and main, as on the
      reference. */
   if(_keyWindow==nil)
    [self _makeFrontWindowKeyExcept:nil];
   [pool release];
}

/* The app among the system's recent applications (the system menu's
   Recent Items): a regular app in a bundle, not one of the system's own
   services or an agent. */
-(void)_noteRecentApplication {
   NSBundle *bundle=[NSBundle mainBundle];
   NSString *path=[bundle bundlePath];

   if(![[path pathExtension] isEqualToString:@"app"] || [path hasPrefix:@"/System/Library/CoreServices/"] ||
      [[bundle objectForInfoDictionaryKey:@"LSUIElement"] boolValue] ||
      [[bundle objectForInfoDictionaryKey:@"LSBackgroundOnly"] boolValue])
    return;
   LSSharedFileListRef list=LSSharedFileListCreate(NULL,kLSSharedFileListRecentApplicationItems,NULL);
   if(list==NULL)
    return;
   LSSharedFileListItemRef item=LSSharedFileListInsertItemURL(list,kLSSharedFileListItemBeforeFirst,NULL,NULL,
      (CFURLRef)[NSURL fileURLWithPath:path],NULL,NULL);
   if(item!=NULL)
    CFRelease(item);
   CFRelease(list);
}

/* The View menu gets Enter Full Screen (Control-Command-F) unless the app
   has an item for it, as AppKit adds it on the reference platform; its
   title follows the key window (NSWindow's validateMenuItem:). */
-(void)_addFullScreenItem {
   NSString *localized=[[NSBundle mainBundle] localizedStringForKey:@"View" value:@"View" table:nil];
   NSMenu   *view=nil;

   for(NSMenuItem *item in [[self mainMenu] itemArray])
    if([item hasSubmenu] && ([[item title] isEqualToString:@"View"] || [[item title] isEqualToString:localized]))
     view=[item submenu];
   if(view==nil || [view indexOfItemWithTarget:nil andAction:@selector(toggleFullScreen:)]>=0)
    return;
   for(NSMenuItem *item in [view itemArray])
    if([item action]==@selector(toggleFullScreen:))
     return;
   NSString *title=[[NSBundle bundleForClass:[NSApplication class]] localizedStringForKey:@"Enter Full Screen"
      value:@"Enter Full Screen" table:nil];
   NSMenuItem *full=[[[NSMenuItem alloc] initWithTitle:title action:@selector(toggleFullScreen:) keyEquivalent:@"f"] autorelease];

   [full setKeyEquivalentModifierMask:NSCommandKeyMask|NSControlKeyMask];
   if([view numberOfItems]>0 && ![[[view itemArray] lastObject] isSeparatorItem])
    [view addItem:[NSMenuItem separatorItem]];
   [view addItem:full];
}

/* Window tabs' items, as AppKit adds them when the app allows automatic
   tabbing: Show Tab Bar in the View menu (in the Window menu when there
   is none), and in the Window menu, before Bring All to Front, Show
   Previous Tab (Control-Shift-Tab), Show Next Tab (Control-Tab), Move Tab
   to New Window and Merge All Windows. NSWindow validates them. */
-(void)_addTabItems {
   NSBundle *appKit=[NSBundle bundleForClass:[NSApplication class]];
   NSMenu   *window=[self windowsMenu],*view=nil;
   NSString *localized=[[NSBundle mainBundle] localizedStringForKey:@"View" value:@"View" table:nil];

   if(![NSWindow allowsAutomaticWindowTabbing] || window==nil ||
      [window indexOfItemWithTarget:nil andAction:@selector(mergeAllWindows:)]>=0)
    return;
   for(NSMenuItem *item in [[self mainMenu] itemArray])
    if([item hasSubmenu] && ([[item title] isEqualToString:@"View"] || [[item title] isEqualToString:localized]))
     view=[item submenu];

   NSInteger at=[window indexOfItemWithTarget:nil andAction:@selector(arrangeInFront:)];
   if(at<0){
    /* Before the separator AppKit keeps above the windows' list. */
    at=[window numberOfItems];
    if(at>0 && [[window itemAtIndex:at-1] isSeparatorItem])
     at--;
   }
   else if(at>0 && [[window itemAtIndex:at-1] isSeparatorItem])
    at--;

   struct { NSString *title; SEL action; NSString *key; NSUInteger modifiers; } items[]={
    { @"Show Previous Tab", @selector(selectPreviousTab:), @"\t", NSControlKeyMask|NSShiftKeyMask },
    { @"Show Next Tab", @selector(selectNextTab:), @"\t", NSControlKeyMask },
    { @"Move Tab to New Window", @selector(moveTabToNewWindow:), @"", 0 },
    { @"Merge All Windows", @selector(mergeAllWindows:), @"", 0 },
   };
   NSMutableArray *added=[NSMutableArray array];

   if(view==nil)
    [added addObject:[[[NSMenuItem alloc] initWithTitle:[appKit localizedStringForKey:@"Show Tab Bar" value:@"Show Tab Bar" table:nil]
       action:@selector(toggleTabBar:) keyEquivalent:@""] autorelease]];
   for(size_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
    NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:[appKit localizedStringForKey:items[i].title value:items[i].title table:nil]
       action:items[i].action keyEquivalent:items[i].key] autorelease];

    [item setKeyEquivalentModifierMask:items[i].modifiers];
    [added addObject:item];
   }
   if(at>0 && ![[window itemAtIndex:at-1] isSeparatorItem])
    [window insertItem:[NSMenuItem separatorItem] atIndex:at++];
   for(NSMenuItem *item in added)
    [window insertItem:item atIndex:at++];
   /* A separator after them (before Bring All to Front, or the windows'
      list). */
   if(at>=[window numberOfItems] || ![[window itemAtIndex:at] isSeparatorItem])
    [window insertItem:[NSMenuItem separatorItem] atIndex:at];

   if(view!=nil && [view indexOfItemWithTarget:nil andAction:@selector(toggleTabBar:)]<0){
    NSInteger full=[view indexOfItemWithTarget:nil andAction:@selector(toggleFullScreen:)];
    NSMenuItem *bar=[[[NSMenuItem alloc] initWithTitle:[appKit localizedStringForKey:@"Show Tab Bar" value:@"Show Tab Bar" table:nil]
       action:@selector(toggleTabBar:) keyEquivalent:@""] autorelease];

    if(full<0){
     if([view numberOfItems]>0 && ![[[view itemArray] lastObject] isSeparatorItem])
      [view addItem:[NSMenuItem separatorItem]];
     [view addItem:bar];
    }
    else
     [view insertItem:bar atIndex:full];
   }
}

/* The alternates AppKit adds after an app's menu items, as the reference
   platform's does: with Option, Close becomes Close All, Minimize
   Minimize All, Zoom Zoom All, Bring All to Front Arrange in Front. */
-(void)_addWindowAlternates {
   struct { SEL base; SEL alternate; NSString *title; } pairs[]={
    { @selector(performClose:), @selector(_closeAllWindows:), @"Close All" },
    { @selector(performMiniaturize:), @selector(miniaturizeAll:), @"Minimize All" },
    { @selector(performZoom:), @selector(_zoomAllWindows:), @"Zoom All" },
    { @selector(arrangeInFront:), @selector(_arrangeInFrontCascading:), @"Arrange in Front" },
   };
   NSBundle *appKit=[NSBundle bundleForClass:[NSApplication class]];

   for(NSMenuItem *top in [[self mainMenu] itemArray]){
    NSMenu *menu=[top submenu];

    for(NSInteger i=0;i<[menu numberOfItems];i++){
     NSMenuItem *item=[menu itemAtIndex:i];

     for(size_t p=0;p<sizeof(pairs)/sizeof(pairs[0]);p++){
      if([item action]!=pairs[p].base || [item isAlternate])
       continue;
      /* Not twice: the app may have its own. */
      NSMenuItem *next=(i+1<[menu numberOfItems])?[menu itemAtIndex:i+1]:nil;
      if([next isAlternate] || [menu indexOfItemWithTarget:nil andAction:pairs[p].alternate]>=0)
       continue;
      NSString   *title=[appKit localizedStringForKey:pairs[p].title value:pairs[p].title table:nil];
      NSMenuItem *alt=[[[NSMenuItem alloc] initWithTitle:title action:pairs[p].alternate
       keyEquivalent:[item keyEquivalent]] autorelease];

      [alt setKeyEquivalentModifierMask:[item keyEquivalentModifierMask]|NSAlternateKeyMask];
      [alt setAlternate:YES];
      [menu insertItem:alt atIndex:i+1];
      i++;
     }
    }
   }
}

-(void)_checkForReleasedWindows {
   int  count=[_windows count];

   while(--count>=0){
    NSWindow *check=[_windows objectAtIndex:count];

    if([check retainCount]==1){

        // Use the setters here - give a chance to the observer to notice something happened
        if(check==_keyWindow) {
            [self _setKeyWindow:nil];
        }

        if(check==_mainWindow) {
            [self _setMainWindow:nil];
        }

     [_windows removeObjectAtIndex:count];
   }
}
}

/* As on the reference platform: when the last window closes, the app
   quits only if its delegate's applicationShouldTerminateAfterLastWindowClosed:
   says so. An app with no window (an agent with status items) runs on. */
-(void)_checkForTerminate {
   static BOOL hadWindow=NO;
   int  count=[_windows count];

   while(--count>=0){
    NSWindow *check=[_windows objectAtIndex:count];

    if(![check isKindOfClass:[NSPanel class]] && [check isVisible]){
     hadWindow=YES;
     return;
    }
   }

   if(!hadWindow)
      return;
   hadWindow=NO;
   if([_delegate respondsToSelector:@selector(applicationShouldTerminateAfterLastWindowClosed:)] &&
      [_delegate applicationShouldTerminateAfterLastWindowClosed:self])
      [self terminate:self];
}

-(void)_checkForAppActivation {
    if([self isActive])
        [_windows makeObjectsPerformSelector:@selector(_showForActivation)];
    else
        [_windows makeObjectsPerformSelector:@selector(_hideForDeactivation)];
}

-(void)run {

  static BOOL didlaunch = NO;
  NSAutoreleasePool *pool;

  _isRunning=YES;

  if (!didlaunch) {
    didlaunch = YES;
    [self finishLaunching];
  }

   do {
    // There is another pool inside nextEventMatchingMask. Do we really need this one?
    pool = [NSAutoreleasePool new];
    NSEvent           *event;

    event=[self nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture] inMode:NSDefaultRunLoopMode dequeue:YES];

    NS_DURING
     [self sendEvent:event];

    NS_HANDLER
     [self reportException:localException];
    NS_ENDHANDLER

    [self _checkForReleasedWindows];
    [self _checkForTerminate];

    [pool release];
   }while(_isRunning);
}

-(BOOL)_performKeyEquivalent:(NSEvent *)event {
    if (event.charactersIgnoringModifiers.length > 0) {
    /* order is important here, views may want to handle the event before menu*/

        if([[self keyWindow] performKeyEquivalent:event])
            return YES;
        if([[self mainWindow] performKeyEquivalent:event])
            return YES;
        if([[self mainMenu] performKeyEquivalent:event])
            return YES;
        /* The system menu's, after the app's: Control-Command-Q locks
           the screen, Shift-Command-Q logs out. Not in an app without a
           menu bar of its own (an agent), as on the reference. */
        if([self activationPolicy]==NSApplicationActivationPolicyRegular &&
           [NSMenuBarWindow _performSystemMenuKeyEquivalent:event])
            return YES;
    }
// documentation says to send it to all windows
   return NO;
}

extern void NSEventNoteMouseButtons(NSEvent *event);
extern NSEvent *NSEventRunLocalMonitors(NSEvent *event);

-(void)sendEvent:(NSEvent *)event {
    if(event == nil)
        return;
    NSEventNoteMouseButtons(event);
    /* Local monitors see what is sent, and may change or drop it. */
    event = NSEventRunLocalMonitors(event);
    if(event == nil)
        return;

   if([event type]==NSKeyDown){
    NSEventModifierFlags modifierFlags=[event modifierFlags];

    if(modifierFlags&(NSCommandKeyMask|NSAlternateKeyMask|NSControlKeyMask)){
     if([self _performKeyEquivalent:event])
      return;
    }
    /* A function key (F8 alone) is the main menu's key equivalent before
       it is typed, as on the reference; other keys alone are typed. */
    else if((modifierFlags&NSFunctionKeyMask) && [[event charactersIgnoringModifiers] length]>0)
     if([[self mainMenu] performKeyEquivalent:event])
      return;
   }
   if([event type]==NSAppKitDefined && [event subtype]==kNSMenuBarFocusEventSubtype){
    [NSMenuBarWindow _focusMenuBar];
    return;
   }
   if([event type]==NSAppKitDefined && [event subtype]==kNSNextWindowEventSubtype){
    [self _moveFocusToNextWindow];
    return;
   }
   if([event type]==NSAppKitDefined && [event subtype]==kNSDisplayReconfiguredEventSubtype){
    [self _displayReconfigured:(CGDirectDisplayID)[event data1]
        flags:(CGDisplayChangeSummaryFlags)[event data2]];
    return;
   }

   [[event window] sendEvent:event];
}

// This method is used by NSWindow
-(void)_displayAllWindowsIfNeeded {
   [[NSApp windows] makeObjectsPerformSelector:@selector(displayIfNeeded)];
}

-(NSEvent *)nextEventMatchingMask:(NSEventMask)mask untilDate:(NSDate *)untilDate inMode:(NSString *)mode dequeue:(BOOL)dequeue {
   NSEvent *nextEvent=nil;

   do {
   NSAutoreleasePool *pool=[NSAutoreleasePool new];

   NS_DURING

    // This should happen before _makeSureIsOnAScreen so we don't reposition done windows
    [self _checkForReleasedWindows];

    [[NSApp windows] makeObjectsPerformSelector:@selector(_makeSureIsOnAScreen)];

    [self _checkForAppActivation];
     [self _displayAllWindowsIfNeeded];

     nextEvent=[_display nextEventMatchingMask:mask untilDate:untilDate inMode:mode dequeue:dequeue];

     /* NSDisplay's placeholder for "no event": autoreleased, so it is
      * only dropped here (releasing it too freed it twice). A real event
      * is retained now: it is autoreleased in this loop's pool, released
      * below, and the display's queue no longer holds it. */
     if([nextEvent type]==NSAppKitSystem)
      nextEvent=nil;
     else
      [nextEvent retain];

   NS_HANDLER
    [self reportException:localException];
   NS_ENDHANDLER

   [pool release];
   }while(nextEvent==nil && [untilDate timeIntervalSinceNow]>0);

   if(nextEvent!=nil){
    /* The loop's reference is the one autoreleased below; this one is
     * _currentEvent's. */
    nextEvent=[nextEvent retain];

    pthread_mutex_lock(_lock);
     [_currentEvent release];
     _currentEvent=nextEvent;
    pthread_mutex_unlock(_lock);
   }

   return [nextEvent autorelease];
}

-(NSEvent *)currentEvent {
   /* Apps do use currentEvent from secondary threads and it doesn't crash on OS X, so we need to be safe here too. */
   NSEvent *result;

    pthread_mutex_lock(_lock);
     result=[_currentEvent retain];
    pthread_mutex_unlock(_lock);

   return [result autorelease];
}

-(void)discardEventsMatchingMask:(NSEventMask)mask beforeEvent:(NSEvent *)event {
   [_display discardEventsMatchingMask:mask beforeEvent:event];
}

-(void)postEvent:(NSEvent *)event atStart:(BOOL)atStart {
   [_display postEvent:event atStart:atStart];
}

-_searchForAction:(SEL)action responder:target {
  // Search a responder chain

   while (target != nil) {

    if ([target respondsToSelector:action])
     return target;

    if([target respondsToSelector:@selector(nextResponder)])
     target = [target nextResponder];
    else
     break;
   }

   return nil;
}

-_searchForAction:(SEL)action window:(NSWindow *)window {
 // Search a windows responder chain and window
 // The window check is done seperately from the responder chain
 // in case the responder chain is broken

// FIXME: should a windows delegate and windowController be checked if a window is found in a responder chain too ?
// Document based facts:
//  An NSWindow's next responder should be the window controller
//  An NSWindow's delegate should be the document
// - This probably means the windowController check is duplicative, but need to make the next responder is window controller

   id check=[self _searchForAction:action responder:[window firstResponder]];

   if(check!=nil)
    return check;

   if ([[window delegate] respondsToSelector:action])
    return [window delegate];

   if ([[window windowController] respondsToSelector:action])
    return [window windowController];

   return nil;
}

-targetForAction:(SEL)action {
  return [self targetForAction:action to:nil from:nil];
}

-targetForAction:(SEL)action to:target from:sender {
  if (target == nil)
    {
      target = [self _searchForAction:action window:[self keyWindow]];
      if (target)
        return target;

      if ([self mainWindow] != [self keyWindow])
        {
          target = [self _searchForAction:action window:[self mainWindow]];
          if (target)
            return target;
        }
    }
  else
    {
      target = [self _searchForAction:action responder:target];
      if (target)
        return target;
    }

  NSDocumentController *documentController = [NSDocumentController sharedDocumentController];
  if ([[documentController currentDocument] respondsToSelector:action])
    return [documentController currentDocument];

  if([self respondsToSelector:action])
    return self;

  if([[self delegate] respondsToSelector:action])
    return [self delegate];

  if([documentController respondsToSelector:action])
    return documentController;

  return nil;
}

-(BOOL)sendAction:(SEL)action to:target from:sender {
  if([target respondsToSelector:action])
    {
      [target performSelector:action withObject:sender];
      return YES;
    }

  target=[self targetForAction:action to:target from:sender];
  if (target != nil)
    {
      [target performSelector:action withObject:sender];
      return YES;
    }

  return NO;
}

-(BOOL)tryToPerform:(SEL)selector with:object {
  if ([self respondsToSelector:selector])
    {
      [self performSelector:selector withObject:object];
      return YES;
    }

  if ([[self delegate] respondsToSelector:selector])
    {
      [[self delegate] performSelector:selector withObject:object];
      return YES;
    }

  return NO;
}

-(void)setWindowsNeedUpdate:(BOOL)value {
   _windowsNeedUpdate=value;
   NSUnimplementedMethod();
}

-(void)updateWindows {
   [_windows makeObjectsPerformSelector:@selector(update)];
}

/*
 * Activating: the window server makes this the front app (with the new
 * server), and a window comes forward and becomes key. LaunchServices'
 * session, told by the server, then makes the app active
 * (_launchServicesMessage:).
 */
/* API 23, ahead of Minkar's API for the apps: the reference platform lets
   the active app decline; on Minkar the app comes to the front. */
-(void)activate {
   [self activateIgnoringOtherApps:YES];
}

-(void)activateIgnoringOtherApps:(BOOL)flag {
   NSWindow *window=_keyWindow ? _keyWindow : _mainWindow;

   SLSSetFrontProcess();
   if(window==nil || ![window isVisible] || ![window canBecomeKeyWindow]) {
       window=nil;
       for(NSWindow *check in [self orderedWindows])
           if([check isVisible] && [check canBecomeKeyWindow]) {
               window=check;
               break;
           }
   }
   [window makeKeyAndOrderFront:self];
}

-(NSApplicationActivationPolicy)activationPolicy {
   return _activationPolicy;
}

/* Presentation options (API 10.6): checked as the reference checks them
   (an invalid combination raises), kept, and told to the session's Dock
   and menu bar, which hide or show while the app is in front. */
static char presentationOptionsKey;
NSString * const _NSApplicationPresentationOptionsDidChangeNotification=@"org.minkar.presentationOptionsChanged";

static BOOL presentationOptionsValid(NSApplicationPresentationOptions o) {
   BOOL dock=(o&(NSApplicationPresentationAutoHideDock|NSApplicationPresentationHideDock))!=0;

   if((o&NSApplicationPresentationAutoHideDock) && (o&NSApplicationPresentationHideDock))
    return NO;
   if((o&NSApplicationPresentationAutoHideMenuBar) && (o&NSApplicationPresentationHideMenuBar))
    return NO;
   if((o&NSApplicationPresentationHideMenuBar) && !(o&NSApplicationPresentationHideDock))
    return NO;
   if((o&NSApplicationPresentationAutoHideMenuBar) && !dock)
    return NO;
   if((o&(NSApplicationPresentationDisableProcessSwitching|NSApplicationPresentationDisableForceQuit|
          NSApplicationPresentationDisableSessionTermination|NSApplicationPresentationDisableHideApplication)) && !dock)
    return NO;
   if((o&NSApplicationPresentationDisableAppleMenu) &&
      !(o&(NSApplicationPresentationAutoHideMenuBar|NSApplicationPresentationHideMenuBar)))
    return NO;
   if((o&NSApplicationPresentationAutoHideToolbar) && !(o&NSApplicationPresentationFullScreen))
    return NO;
   return YES;
}

-(NSApplicationPresentationOptions)presentationOptions {
   return [objc_getAssociatedObject(self,&presentationOptionsKey) unsignedIntegerValue];
}

-(void)setPresentationOptions:(NSApplicationPresentationOptions)options {
   if(!presentationOptionsValid(options))
    [NSException raise:NSInvalidArgumentException format:@"-[NSApplication setPresentationOptions:] called with NSApplicationPresentationOptions = %lu, an invalid combination",(unsigned long)options];
   objc_setAssociatedObject(self,&presentationOptionsKey,[NSNumber numberWithUnsignedInteger:options],OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   /* The app's own menu bar, while it is in front; the Dock hides its
      part of the bar and itself when it hears of it. */
   if(_isActive && _activationPolicy==NSApplicationActivationPolicyRegular){
    if([self _presentationHidesMenuBar])
     [NSMenuBarWindow _hideMenuBars];
    else
     [NSMenuBarWindow _showMenuBars];
   }
   [[NSDistributedNotificationCenter defaultCenter] postNotificationName:_NSApplicationPresentationOptionsDidChangeNotification
     object:[NSString stringWithFormat:@"%d",getpid()]
     userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedInteger:options] forKey:@"options"]
     deliverImmediately:YES];
}

-(BOOL)_presentationHidesMenuBar {
   return ([self presentationOptions]&(NSApplicationPresentationHideMenuBar|NSApplicationPresentationAutoHideMenuBar))!=0;
}

/* What is in effect: the front app's options (this app's while it is). */
-(NSApplicationPresentationOptions)currentSystemPresentationOptions {
   return [self isActive]?[self presentationOptions]:NSApplicationPresentationDefault;
}

-(BOOL)setActivationPolicy:(NSApplicationActivationPolicy)policy {
   if(policy<NSApplicationActivationPolicyRegular || policy>NSApplicationActivationPolicyProhibited)
       return NO;
   _activationPolicy=policy;
   if(_lsPort!=MACH_PORT_NULL)
       [self _checkInWithLaunchServices];
   return YES;
}

/*
 * LaunchServices' session (WindowServer plan, WS4a): the app checks in
 * with launchservicesd as it finishes launching, then hears on its own
 * port about the other apps (NSWorkspace's notifications), about being
 * made the front app or not (active), and requests to activate.
 */
-(void)_checkInWithLaunchServices {
   LSSessionApp app;
   BOOL first=(_lsPort==MACH_PORT_NULL);

   _NSRunningApplicationDescribeSelf(&app, _activationPolicy);
   if(LSSessionCheckIn(&app, &_lsPort, NULL)!=kLSSessionSuccess)
       return;
   if(first) {
       /* Documents sent for this launch are on the port already (the
        * daemon sends them as it answers the check-in): kept for
        * finishLaunching, so no untitled document opens with them. */
       for(;;) {
           NSData *data=[self _receiveLaunchServicesMessage:YES];
           if(data==nil)
               break;
           const LSSessionMessage *msg=[data bytes];
           if(msg->kind==kLSSessionOpenRequest) {
               const LSSessionOpenMessage *open=[data bytes];
               if(pendingOpenRequests==nil)
                   pendingOpenRequests=[NSMutableArray new];
               [pendingOpenRequests addObject:[NSDictionary dictionaryWithObjectsAndKeys:
                   urlsFromOpenMessage(open, [data length]), @"urls",
                   [NSNumber numberWithBool:(open->flags & kLSSessionOpenReveal)!=0], @"reveal", nil]];
           } else
               [self performSelectorOnMainThread:@selector(_launchServicesMessage:) withObject:data
                   waitUntilDone:NO];
       }
       [NSThread detachNewThreadSelector:@selector(launchServicesLoop:) toTarget:self withObject:nil];
   }
}

/* One message from the daemon, or nil (none waiting when poll is set). */
-(NSData *)_receiveLaunchServicesMessage:(BOOL)poll {
   union {
       LSSessionMessage msg;
       LSSessionOpenMessage open;
       uint8_t bytes[sizeof(LSSessionOpenMessage) + MAX_TRAILER_SIZE];
   } *buf=malloc(sizeof(*buf));
   NSData *data=nil;

   while(buf!=NULL) {
       if(mach_msg(&buf->msg.header, MACH_RCV_MSG | (poll ? MACH_RCV_TIMEOUT : 0), 0, sizeof(*buf),
           _lsPort, poll ? 0 : MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL)!=MACH_MSG_SUCCESS)
           break;
       if(buf->msg.header.msgh_id!=kLSSessionMessageID)
           continue;
       mach_msg_size_t size=buf->msg.header.msgh_size;
       if(size<sizeof(mach_msg_header_t)+sizeof(uint32_t))
           continue;
       if(buf->msg.kind!=kLSSessionOpenRequest && size<sizeof(LSSessionMessage))
           continue;
       data=[NSData dataWithBytes:buf length:size];
       break;
   }
   free(buf);
   return data;
}

-(void)launchServicesLoop:(id)object {
   for(;;) {
       NSAutoreleasePool *pool=[NSAutoreleasePool new];
       NSData *data=[self _receiveLaunchServicesMessage:NO];
       if(data==nil) {
           [pool release];
           return;
       }
       [self performSelectorOnMainThread:@selector(_launchServicesMessage:) withObject:data
           waitUntilDone:NO];
       [pool release];
   }
}

-(void)_setActiveFromLaunchServices:(BOOL)active {
   if(active==_isActive)
       return;
   /* The front app's menu bar is on the screen (MB0). It stays when the
      app goes behind an agent (a Menu Extra's panel, which has no menu
      bar): it goes when another regular app comes to the front
      (_launchServicesMessage:). */
   if(_activationPolicy==NSApplicationActivationPolicyRegular && active && ![self _presentationHidesMenuBar])
       [NSMenuBarWindow _showMenuBars];
   [[NSNotificationCenter defaultCenter] postNotificationName:
       active ? NSApplicationWillBecomeActiveNotification : NSApplicationWillResignActiveNotification
       object:self];
   _isActive=active;
   [self _checkForAppActivation];
   [[NSNotificationCenter defaultCenter] postNotificationName:
       active ? NSApplicationDidBecomeActiveNotification : NSApplicationDidResignActiveNotification
       object:self];
}

-(void)_launchServicesMessage:(NSData *)data {
   const LSSessionMessage *msg=[data bytes];

   if(msg->kind==kLSSessionOpenRequest) {
       const LSSessionOpenMessage *open=[data bytes];
       [self _openURLs:urlsFromOpenMessage(open, [data length]) reveal:(open->flags & kLSSessionOpenReveal)!=0];
       return;
   }

   /* Another regular app in front: its menu bar replaces this one's. */
   if(msg->kind==kLSSessionAppActivated && msg->app.pid!=getpid() &&
      msg->app.activationPolicy==NSApplicationActivationPolicyRegular &&
      _activationPolicy==NSApplicationActivationPolicyRegular)
       [NSMenuBarWindow _hideMenuBars];

   if(msg->app.pid==getpid()) {
       switch(msg->kind) {
           case kLSSessionAppActivated:
               [self _setActiveFromLaunchServices:YES];
               break;
           case kLSSessionAppDeactivated:
               [self _setActiveFromLaunchServices:NO];
               break;
           case kLSSessionActivateRequest:
               [self activateIgnoringOtherApps:(msg->options & NSApplicationActivateIgnoringOtherApps)!=0];
               break;
           case kLSSessionQuitRequest:
               /* As the Quit item: the delegate may refuse. */
               [self terminate:self];
               break;
           case kLSSessionHideRequest:
               [self hide:self];
               break;
           case kLSSessionUnhideRequest:
               [self unhide:self];
               break;
       }
   }
   _NSRunningApplicationSessionMessage(msg);
}

-(void)deactivate {
   NSUnimplementedMethod();
}

/* Visible while the app is not hidden and a window of it is on screen. */
-(NSApplicationOcclusionState)occlusionState {
   if(_isHidden)
    return 0;
   for(NSWindow *window in [self windows])
    if([window isVisible] && ![window isMiniaturized])
     return NSApplicationOcclusionStateVisible;
   return 0;
}

/* Not public API, but apps call it (Firefox): whether an app-modal session
   (runModalForWindow:, not a sheet) runs; sheets do not use the stack. */
-(BOOL)_isRunningAppModal {
   return [_modalStack count]>0;
}

/* Not public API, but apps call it (Firefox): whether a modal session runs. */
/* Minkar: the direction the first preferred language is written in. */
-(NSUserInterfaceLayoutDirection)userInterfaceLayoutDirection {
   NSArray  *languages=[NSLocale preferredLanguages];
   NSString *language=([languages count]>0)?[languages objectAtIndex:0]:@"en";

   return ([NSLocale characterDirectionForLanguage:language]==NSLocaleLanguageDirectionRightToLeft)?
      NSUserInterfaceLayoutDirectionRightToLeft:NSUserInterfaceLayoutDirectionLeftToRight;
}

-(BOOL)_isRunningModal {
   return [_modalStack count]>0;
}

-(NSWindow *)modalWindow {
   return [[_modalStack lastObject] modalWindow];
}

-(NSModalSession)beginModalSessionForWindow:(NSWindow *)window {
   NSModalSessionX *session=[NSModalSessionX sessionWithWindow:window];

   [_modalStack addObject:session];

   if(![window isVisible]){
    [window center];
   }
   [window makeKeyAndOrderFront:self];

   return session;
}

-(NSInteger)runModalSession:(NSModalSession)session {
    while([session stopCode]==NSRunContinuesResponse) {
        NSAutoreleasePool *pool=[NSAutoreleasePool new];
        NSEvent           *event=[self nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate date] inMode:NSModalPanelRunLoopMode dequeue:YES];

        if(event==nil){
            [pool release];
            break;
        }

        NSWindow          *window=[event window];


        // in theory this could get weird, but all we want is the ESC-cancel keybinding, afaik NSApp doesn't respond to any other doCommandBySelectors...
        if([event type]==NSKeyDown && window == [session modalWindow])
            [self interpretKeyEvents:[NSArray arrayWithObject:event]];

        if(window==[session modalWindow] || [window worksWhenModal])
            [self sendEvent:event];
        else if([event type]==NSLeftMouseDown)
            [[session modalWindow] makeKeyAndOrderFront:self];
        else {
            // We need to preserve some events which are not processed in the modal loop and requeue them.
            // The particular case we need to handle is mouse down. run modal. then actually receive the mouse up when the modal is done.
            // So we know this works in Cocoa, save the mouse up here.
            // We don't want to save mouse moved or such.
            // There is kind of adhoc, probably a better way to do it, find out which combinations should work (e.g. mouse enter, do we get mouse exit?)
            if([[session unprocessedEvents] count]==0){

                switch([event type]){

                    case NSLeftMouseUp:
                    case NSRightMouseUp:
                        [session addUnprocessedEvent: event];
                        break;

                    default:
                        // don't save
                        break;
                }
            }
        }
        [pool release];
    }


    return [session stopCode];
}

-(void)endModalSession:(NSModalSession)session {
    if(session!=[_modalStack lastObject])
        [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] modal session %@ is not the current one %@",[self class],sel_getName(_cmd),session,[_modalStack lastObject]];

    for(NSEvent *requeue in [session unprocessedEvents]){
        [self postEvent:requeue atStart:YES];
    }

    [_modalStack removeLastObject];
}

-(void)stopModalWithCode:(NSInteger)code {
    // This should silently ignore any attempt to end a session when there is none.
   [[_modalStack lastObject] stopModalWithCode:code];
}

-(void)_mainThreadRunModalForWindow:(NSMutableDictionary *)values {
   NSWindow *window=[values objectForKey:@"NSWindow"];

   NSModalSession session=[self beginModalSessionForWindow:window];
   int result;

   while((result=[NSApp runModalSession:session])==NSRunContinuesResponse){
    /* Waits for the next event with the app's event source in the run
       loop (a plain runMode: here never woke for one). */
    [self nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture]
        inMode:NSModalPanelRunLoopMode dequeue:NO];
   }

   [self endModalSession:session];

   [values setObject:[NSNumber numberWithInteger:result] forKey:@"result"];
}

-(NSInteger)runModalForWindow:(NSWindow *)window {
   NSMutableDictionary *values=[NSMutableDictionary dictionary];

   [values setObject:window forKey:@"NSWindow"];

   [self performSelectorOnMainThread:@selector(_mainThreadRunModalForWindow:) withObject:values waitUntilDone:YES modes:[NSArray arrayWithObjects:NSDefaultRunLoopMode,NSModalPanelRunLoopMode,nil]];

   NSNumber *result=[values objectForKey:@"result"];

   return [result integerValue];
}


-(void)stopModal {
   [self stopModalWithCode:NSRunStoppedResponse];
}

-(void)abortModal {
   [self stopModalWithCode:NSRunAbortedResponse];
}

// cancel modal windows
-(void)cancel:sender {
    if ([self modalWindow] != nil)
        [self abortModal];
}

-(void)beginSheet:(NSWindow *)sheet modalForWindow:(NSWindow *)window modalDelegate:modalDelegate didEndSelector:(SEL)didEndSelector contextInfo:(void *)contextInfo {
    NSSheetContext *context=[NSSheetContext sheetContextWithSheet:sheet modalDelegate:modalDelegate didEndSelector:didEndSelector contextInfo:contextInfo frame:[sheet frame]];

    if ([[NSUserDefaults standardUserDefaults] boolForKey: @"NSRunAllSheetsAsModalPanel"]) {
        // Center the sheet on the window
        NSPoint windowCenter = NSMakePoint(NSMidX([window frame]), NSMidY([window frame]));
        NSPoint sheetCenter = NSMakePoint(NSMidX([sheet frame]), NSMidY([sheet frame]));
        NSPoint origin = [sheet frame].origin;
        origin.x += windowCenter.x - sheetCenter.x;
        origin.y += windowCenter.y - sheetCenter.y;
        [sheet setFrameOrigin:origin];

        [sheet _setSheetContext: context];
        [sheet setLevel: NSModalPanelWindowLevel];
        NSModalSession session = [self beginModalSessionForWindow: sheet];
        [context setModalSession: session];
        while([NSApp runModalSession:session] == NSRunContinuesResponse){
            [self nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture]
                inMode:NSModalPanelRunLoopMode dequeue:NO];
        }
        [self endModalSession:session];
    } else {
        [sheet _setSheetParent:window];
        [window _attachSheetContextOrderFrontAndAnimate:context];
    }
}

typedef void (*endfunction_t)(id, SEL, id, NSInteger, void *);
-(void)endSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode {

    if ([[NSUserDefaults standardUserDefaults] boolForKey: @"NSRunAllSheetsAsModalPanel"]) {
        NSSheetContext* context = [sheet _sheetContext];
        NSModalSession session = [context modalSession];
        [session stopModalWithCode: NSRunStoppedResponse];
        IMP function=[[context modalDelegate] methodForSelector:[context didEndSelector]];

        if(function!=NULL) {
            ((endfunction_t)function)([context modalDelegate],[context didEndSelector],sheet,returnCode,[context contextInfo]);
        }
        [sheet _setSheetContext: nil];

    } else {

   int count=[_windows count];

   while(--count>=0){
    NSWindow       *check=[_windows objectAtIndex:count];
    NSSheetContext *context=[check _sheetContext];
    IMP             function;

    if([context sheet]==sheet){
     [[context retain] autorelease];
     [[sheet retain] autorelease];

     /* The sheet goes, then its delegate (its completion handler) is
        told, then NSWindowDidEndSheetNotification: the handler first,
        as on the reference, with the window already free of the sheet
        (a handler closing the window or beginning another sheet). */
     [check _detachSheetContextAnimateAndOrderOutNotifying:NO];
     [sheet _setSheetParent:nil];

     function=[[context modalDelegate] methodForSelector:[context didEndSelector]];

     if(function!=NULL)
      ((endfunction_t)function)([context modalDelegate],[context didEndSelector],sheet,returnCode,[context contextInfo]);

     [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidEndSheetNotification object:check];
     return;
    }
   }
    }
}

-(void)endSheet:(NSWindow *)sheet {
   [self endSheet:sheet returnCode:0];
}

-(void)reportException:(NSException *)exception {
   NSLog(@"NSApplication got exception: %@",exception);
}

-(void)_attentionTimer:(NSTimer *)timer {
   [_windows makeObjectsPerformSelector:@selector(_flashWindow)];
}

/* The Dock bounces the app's tile (D3): once for an informational request,
   until the app is active or the request cancelled for a critical one. An
   active app asks for nothing, as on the reference platform. */
-(NSInteger)requestUserAttention:(NSRequestUserAttentionType)attentionType {
   static NSInteger requests;

   if([self isActive])
    return 0;
   NSInteger request=++requests;
   [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"org.minkar.dock.attention"
      object:[[NSBundle mainBundle] bundleIdentifier]
      userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
         [NSNumber numberWithInt:getpid()],@"pid",
         [NSNumber numberWithBool:attentionType==NSCriticalRequest],@"critical",
         [NSNumber numberWithInteger:request],@"request",nil]
      deliverImmediately:YES];
   return request;
}

-(void)cancelUserAttentionRequest:(NSInteger)requestNumber {
   [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"org.minkar.dock.attention.cancel"
      object:[[NSBundle mainBundle] bundleIdentifier]
      userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
         [NSNumber numberWithInt:getpid()],@"pid",[NSNumber numberWithInteger:requestNumber],@"request",nil]
      deliverImmediately:YES];
}

-(void)runPageLayout:sender {
   [[NSPageLayout pageLayout] runModal];
}

-(void)orderFrontColorPanel:(id)sender {
   [[NSColorPanel sharedColorPanel] orderFront:sender];
}

-(void)orderFrontCharacterPalette:sender {
   NSUnimplementedMethod();
}

-(void)hide:sender {//deactivates the application and hides all windows
    if (!_isHidden)
    {
        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationWillHideNotification object:self];
        [_windows makeObjectsPerformSelector:@selector(_forcedHideForDeactivation)];//do no use orderOut here ist causes the application to quit if no window is visible
        _isHidden=YES;
        LSSessionSetHidden(getpid(), 1);	/* NSRunningApplication's isHidden */
        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationDidHideNotification object:self];
    }
    _isHidden=YES;

}

-(void)hideOtherApplications:sender {
   NSUnimplementedMethod();
}

-(void)unhide:sender
{

    if (_isHidden)
    {
        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationWillUnhideNotification object:self];
        [_windows makeObjectsPerformSelector:@selector(_showAfterHide)];//only shows the windows Hide took off
        _isHidden=NO;
        LSSessionSetHidden(getpid(), 0);
        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationDidUnhideNotification object:self];
    }
    _isHidden=NO;
    //[self activateIgnoringOtherApps:NO]

}

-(void)unhideAllApplications:sender {
   NSUnimplementedMethod();
}

-(void)unhideWithoutActivation {
    if (_isHidden)
    {

        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationWillUnhideNotification object:self];
        [_windows makeObjectsPerformSelector:@selector(_showAfterHide)];//only shows the windows Hide took off
        _isHidden=NO;
        LSSessionSetHidden(getpid(), 0);
        [[NSNotificationCenter defaultCenter]postNotificationName:NSApplicationDidUnhideNotification object:self];
    }
    _isHidden=NO;
}

-(void)stop:sender {
   if([_modalStack lastObject]!=nil){
    [self stopModal];
    return;
   }

   _isRunning=NO;
}

-(void)terminate:sender
{
  [[NSDocumentController sharedDocumentController] closeAllDocumentsWithDelegate:self
                                                             didCloseAllSelector:@selector(_documentController:didCloseAll:contextInfo:)
                                                                     contextInfo:NULL];
}

-(void)_documentController:(NSDocumentController *)docController didCloseAll:(BOOL)didCloseAll contextInfo:(void *)info
{
  // callback method for terminate:
  if (didCloseAll)
    {
      if ([_delegate respondsToSelector:@selector(applicationShouldTerminate:)])
        [self replyToApplicationShouldTerminate: [_delegate applicationShouldTerminate:self] == NSTerminateNow];
      else
        [self replyToApplicationShouldTerminate:YES];
    }
}

-(void)replyToApplicationShouldTerminate:(BOOL)terminate
{
  if (terminate == YES)
    {
      [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillTerminateNotification object:self];


      exit(0);
    }
}

-(void)replyToOpenOrPrint:(NSApplicationDelegateReply)reply {
   NSUnimplementedMethod();
}

/* The app's windows in the Window menu, in its order. */
-(NSArray *)_windowsInWindowsMenu {
    NSMutableArray *windows=[NSMutableArray array];

    for(NSMenuItem *item in [[self windowsMenu] itemArray]){
        id target=[item target];

        if([target isKindOfClass:[NSWindow class]] && ![windows containsObject:target])
            [windows addObject:target];
    }
    return windows;
}

/* Bring All to Front: the app's windows come in front of other apps',
   where they are (as on the reference); the key window first. */
-(void)arrangeInFront:sender {
    NSWindow *key=[self keyWindow];

    for(NSWindow *window in [[self _windowsInWindowsMenu] reverseObjectEnumerator])
        if(window!=key && ![window isMiniaturized])
            [window orderFront:sender];
    if(key!=nil && ![key isMiniaturized])
        [key orderFront:sender];
}

/* Close All: each window as its close button would (documents ask to be
   saved). */
-(void)_closeAllWindows:sender {
    for(NSWindow *window in [[[self windows] copy] autorelease])
        if([window isVisible] && ([window styleMask]&NSWindowStyleMaskClosable))
            [window performClose:sender];
}

/* Zoom All: each visible window as its zoom button would. */
-(void)_zoomAllWindows:sender {
    for(NSWindow *window in [[[self windows] copy] autorelease])
        if([window isVisible] && ([window styleMask]&NSWindowStyleMaskResizable))
            [window performZoom:sender];
}

/* Arrange in Front (Bring All to Front with Option): the windows in front,
   cascaded from the screen's middle. */
-(void)_arrangeInFrontCascading:sender {
#define CASCADE_DELTA   20      // ? isn't there a call for this?
    NSArray *visibleWindows = [self _windowsInWindowsMenu];
    NSRect rect=[[[NSScreen screens] objectAtIndex:0] frame], winRect;
    int i, count;

    count = [visibleWindows count];
    if (count == 0)
        return;

    // find screen center.
    // subtract window w,h /2
    winRect = [[visibleWindows objectAtIndex:0] frame];
    rect.origin.x = (rect.size.width/2) - (winRect.size.width/2);
    rect.origin.x -= count*CASCADE_DELTA/2;
    rect.origin.x=floor(rect.origin.x);

    rect.origin.y = (rect.size.height/2) + (winRect.size.height/2);
    rect.origin.y += count*CASCADE_DELTA/2;
    rect.origin.y=floor(rect.origin.y);

    for (i = 0; i < count; ++i) {
        [[visibleWindows objectAtIndex:i] setFrameTopLeftPoint:rect.origin];
        [[visibleWindows objectAtIndex:i] orderFront:nil];

        rect.origin.x += CASCADE_DELTA;
        rect.origin.y -= CASCADE_DELTA;
    }
}

-(NSMenu *)servicesMenu {
   return [[NSApp mainMenu] _menuWithName:@"_NSServicesMenu"];
}

-(void)setServicesMenu:(NSMenu *)menu {
   NSUnimplementedMethod();
}

-servicesProvider {
   return nil;
}

-(void)setServicesProvider:provider {
}

-(void)registerServicesMenuSendTypes:(NSArray *)sendTypes returnTypes:(NSArray *)returnTypes {
   //tiredofthesewarnings NSUnsupportedMethod();
}

-validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType {
   NSUnimplementedMethod();
   return nil;
}


-(void)orderFrontStandardAboutPanel:sender {
   [self orderFrontStandardAboutPanelWithOptions:nil];
}

-(void)orderFrontStandardAboutPanelWithOptions:(NSDictionary *)options {
   [[NSAboutPanel sharedAboutPanel] orderFrontWithOptions:options];
}

-(void)activateContextHelpMode:sender {
   NSUnimplementedMethod();
}

-(void)showGuessPanel:sender {
    [[[NSSpellChecker sharedSpellChecker] spellingPanel] makeKeyAndOrderFront: self];
}

/* The app's help book from its start page (NSHelpManager.m), or an
   alert that it has none. */
-(void)showHelp:sender
{
    if ([[NSHelpManager sharedHelpManager] _showApplicationHelp])
        return;

   /* No help book: say so, with the name the app shows. */
   NSAlert *alert = [[NSAlert alloc] init];
   [alert setMessageText:[NSString stringWithFormat: NSLocalizedStringFromTableInBundle(@"Help isn't available for %@.", nil, [NSBundle bundleForClass: [NSApplication class]], @""), applicationMenuTitle()]];
   [alert setInformativeText:@""];
   [alert runModal];
   [alert release];
}

-(NSDockTile *)dockTile {
   return _dockTile;
}

- (void)doCommandBySelector:(SEL)selector {
    if ([_delegate respondsToSelector:selector])
        [_delegate performSelector:selector withObject:nil];
    else
        [super doCommandBySelector:selector];
}

-(void)_addWindow:(NSWindow *)window {
   [_windows addObject:window];
}

-(void)_removeWindow:(NSWindow *)window {
    [_windows removeObject:window];
}

-(void)_windowWillBecomeActive:(NSWindow *)window {
   [_attentionTimer invalidate];
   _attentionTimer=nil;

   if(![self isActive]){
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillBecomeActiveNotification object:self];
   }
}

-(void)_windowDidBecomeActive:(NSWindow *)window {
    if(![self isActiveExcludingWindow:window]){
        [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:self];
   }
}

-(void)_windowWillBecomeDeactive:(NSWindow *)window {
   if(![self isActiveExcludingWindow:window]){
       [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillResignActiveNotification object:self];
   }
}

-(void)_windowDidBecomeDeactive:(NSWindow *)window {
    if(![self isActive]){
        // Exposed menus are running tight event tracking loops and would remain visible when the app deactivates (making
        // the UI less than community minded) - unfortunately because they're in these tracking loops they're waiting
        // on events and even though they could receive the notification sent here they can't deal with it until an event is
        // received to let them proceed. This special event type was added to help them get unstuck and remove the menu on
        // deactivation
        NSEvent* appKitEvent = [NSEvent otherEventWithType: NSAppKitDefined location: NSZeroPoint modifierFlags: 0 timestamp: 0 windowNumber: 0 context: nil subtype: NSApplicationDeactivated data1: 0 data2: 0];
        [self postEvent: appKitEvent atStart: YES];

        [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidResignActiveNotification object:self];
    }
}

//private method called when the application is reopened
-(void)_reopen {
    BOOL doReopen=YES;
    if ([_delegate respondsToSelector:@selector(applicationShouldHandleReopen:hasVisibleWindows:)])
        doReopen=[_delegate applicationShouldHandleReopen:self hasVisibleWindows:!_isHidden];
    if(!doReopen)
        return;
    if(_isHidden)
        [self unhide:nil];
}


/* Full keyboard access: the AppleKeyboardUIMode default's bit 2. */
-(BOOL)isFullKeyboardAccessEnabled {
   return ([[NSUserDefaults standardUserDefaults] integerForKey:@"AppleKeyboardUIMode"]&2)!=0;
}

static char customizeTouchBarKey;
-(BOOL)isAutomaticCustomizeTouchBarMenuItemEnabled {
   id value=objc_getAssociatedObject(self,&customizeTouchBarKey);
   return (value!=nil)?[value boolValue]:NO;
}
-(void)setAutomaticCustomizeTouchBarMenuItemEnabled:(BOOL)value {
   objc_setAssociatedObject(self,&customizeTouchBarKey,[NSNumber numberWithBool:value],OBJC_ASSOCIATION_RETAIN);
}

@end


/* Minkar: readies AppKit for code that is not a Cocoa application (a
   crash reporter in a plain process, a plug-in): the shared application,
   which loads the rest. */
BOOL NSApplicationLoad(void) {
   [NSApplication sharedApplication];
   return YES;
}

int NSApplicationMain(int argc, const char *argv[]) {
    __NSInitializeProcess(argc, argv);

    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    NSBundle *bundle=[[NSBundle mainBundle] retain];
    Class     class=[bundle principalClass];
    NSString *nibFile=[[bundle infoDictionary] objectForKey:@"NSMainNibFile"];

    if (argc > 1) {
        NSMutableArray *arguments = [NSMutableArray arrayWithCapacity:argc-1];
        for (int i = 1; i < argc; i++)
            if (argv[i][0] != '-')
                [arguments addObject:[NSString stringWithUTF8String:argv[i]]];
            else if (argv[i][1] == '-' && argv[i][2] == '\0')
                break;
            else // (argv[i][0] == '-' && argv[i] != "--")
                if (*(int64_t *)argv[i] != *(int64_t *)"-NSOpen")
                    i++;

        if (argc = [arguments count])
            [[NSUserDefaults standardUserDefaults] setObject:((argc == 1) ? [arguments lastObject] : arguments) forKey:@"NSOpen"];
    }


    if(class==Nil)
        class=[NSApplication class];

    [[class sharedApplication] retain];

    nibFile=[nibFile stringByDeletingPathExtension];

    if(![NSBundle loadNibNamed:nibFile owner:NSApp])
        NSLog(@"Unable to load main nib file %@",nibFile);

    [pool release];

    [NSApp run];

    [NSApp release];
    [bundle release];

    return 0;
}

void NSUpdateDynamicServices(void) {
    NSUnimplementedFunction();
}

BOOL NSPerformService(NSString *itemName, NSPasteboard *pasteboard) {
    NSUnimplementedFunction();
    return NO;
}
