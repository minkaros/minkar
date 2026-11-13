/*
 * Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
 *               2009 Markus Hitter <mah@jump-ing.de>
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>

#import <CoreGraphics/CGDirectDisplay.h>
#import <objc/runtime.h>
#import <CoreGraphics/CGWindow.h>
#import <QuartzCore/CAMetalLayerPrivate.h>
#import <Onyx2D/O2Surface.h>
#import <Onyx2D/O2GraphicsState.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSThemeFrame.h>
#import <AppKit/NSSheetContext.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSEvent_CoreGraphics.h>
#import <AppKit/NSColor.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSDraggingManager.h>
#import <AppKit/NSCursor.h>
#import <AppKit/NSTextView.h>
#import <AppKit/NSTrackingArea.h>
#import <AppKit/NSToolbar.h>
#import <AppKit/NSWindowAnimationContext.h>
#import <AppKit/NSToolTipWindow.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSControl.h>
#import <AppKit/NSOpenGLView.h>
#import <AppKit/NSDocument.h>
#include <SkyLight/SkyLight.h>
#import "O2Context_builtin_FT.h"

NSString * const NSWindowDidBecomeKeyNotification=@"NSWindowDidBecomeKeyNotification";
NSString * const NSWindowDidExposeNotification=@"NSWindowDidExposeNotification";
NSString * const NSWindowDidResignKeyNotification=@"NSWindowDidResignKeyNotification";
NSString * const NSWindowDidBecomeMainNotification=@"NSWindowDidBecomeMainNotification";
NSString * const NSWindowDidResignMainNotification=@"NSWindowDidResignMainNotification";
NSString * const NSWindowWillMiniaturizeNotification=@"NSWindowWillMiniaturizeNotification";
NSString * const NSWindowDidMiniaturizeNotification=@"NSWindowDidMiniaturizeNotification";
NSString * const NSWindowDidDeminiaturizeNotification=@"NSWindowDidDeminiaturizeNotification";
NSString * const NSWindowDidMoveNotification=@"NSWindowDidMoveNotification";
NSString * const NSWindowDidResizeNotification=@"NSWindowDidResizeNotification";
NSString * const NSWindowDidUpdateNotification=@"NSWindowDidUpdateNotification";
NSString * const NSWindowWillCloseNotification=@"NSWindowWillCloseNotification";
NSString * const NSWindowWillMoveNotification=@"NSWindowWillMoveNotification";
NSString * const NSWindowWillStartLiveResizeNotification=@"NSWindowWillStartLiveResizeNotification";
NSString * const NSWindowDidEndLiveResizeNotification=@"NSWindowDidEndLiveResizeNotification";

NSString * const NSWindowWillAnimateNotification=@"NSWindowWillAnimateNotification";
NSString * const NSWindowAnimatingNotification=@"NSWindowAnimatingNotification";
NSString * const NSWindowDidAnimateNotification=@"NSWindowDidAnimateNotification";

// All measurements in pixel. Keep in sync with WSWindowRecord!!
const float WSWindowTitleHeight = 32;
const float WSWindowEdgePad = 2;

@interface NSToolbar (NSToolbar_privateForWindow)
- (void)_setWindow:(NSWindow *)window;
- (NSView *)_view;
-(CGFloat)visibleHeight;
-(void)layoutFrameSizeWithWidth:(CGFloat)width;
@end

@interface NSWindow ()

- (NSRect) zoomedFrame;

@end

@interface NSApplication(private)
-(void)_setMainWindow:(NSWindow *)window;
-(void)_setKeyWindow:(NSWindow *)window;
@end

@interface _NSKeyViewPosition : NSObject {
    NSView *_view;
    NSRect  _rect;
}

+(NSArray *)sortedKeyViewPositionsWithView:(NSView *)view;

-initWithView:(NSView *)view;

-(NSView *)view;

-(NSComparisonResult)compareKeyViewPosition:(_NSKeyViewPosition *)other;

@end

@implementation _NSKeyViewPosition

+(void)addKeyViewPositionsWithView:(NSView *)view toArray:(NSMutableArray *)array {
    [array addObject:[[[_NSKeyViewPosition alloc] initWithView:view] autorelease]];

    for(NSView *child in [view subviews])
        [self addKeyViewPositionsWithView:child toArray:array];
}

+(NSArray *)sortedKeyViewPositionsWithView:(NSView *)view {
    NSMutableArray *result=[NSMutableArray array];

    [self addKeyViewPositionsWithView:view toArray:result];
    [result sortUsingSelector:@selector(compareKeyViewPosition:)];

    return result;
}

-initWithView:(NSView *)view {
    _view=view;
    _rect=[[_view superview] convertRect:[_view frame] toView:nil];
    return self;
}

-(NSView *)view {
    return _view;
}

-(NSComparisonResult)compareKeyViewPosition:(_NSKeyViewPosition *)other {

    // Sort by larger Y (cartesian coordinates)
    if(NSMaxY(_rect)<NSMaxY(other->_rect))
        return NSOrderedDescending;
    else {
        // Then sort by smaller X
        if(NSMinX(_rect)<NSMinX(other->_rect))
            return NSOrderedAscending;
        else
            return NSOrderedDescending;
    }
}

@end

/*
 * The window server (WindowServer plan): each window is a SkyLight
 * window. AppKit draws into its own pixels and flushes them into the
 * window's surface; frame, level, alpha and order go to the server.
 */
extern int _CGUsesSkyLight(void);

static NSMutableDictionary *slsWindows;	/* window number -> SLSWindowID */
static NSMutableSet *slsShown;		/* window numbers ordered in */
static NSMutableDictionary *slsDragRegions;	/* window number -> last region sent */
/* window number -> the frame the server has (sent, or told us by a move) */
static NSMutableDictionary *slsKnownFrames;

/* Whether frame is the one the server has; records it as such. */
static BOOL slsFrameIsKnown(int number, SLSRect frame) {
    NSNumber *key = [NSNumber numberWithInt:number];
    NSData *value = [NSData dataWithBytes:&frame length:sizeof(frame)];
    @synchronized([NSWindow class]) {
        if([[slsKnownFrames objectForKey:key] isEqualToData:value])
            return YES;
        if(slsKnownFrames == nil)
            slsKnownFrames = [[NSMutableDictionary alloc] init];
        [slsKnownFrames setObject:value forKey:key];
    }
    return NO;
}

static SLSWindowID slsWindowFor(int number) {
    @synchronized([NSWindow class]) {
        return [[slsWindows objectForKey:[NSNumber numberWithInt:number]] unsignedIntValue];
    }
}

/* The NSWindow of a SkyLight window, for events (any thread). */
NSWindow *_NSWindowForSLSWindow(SLSWindowID window) {
    int number = 0;

    @synchronized([NSWindow class]) {
        for(NSNumber *key in slsWindows)
            if([[slsWindows objectForKey:key] unsignedIntValue] == window)
                number = [key intValue];
    }
    return number ? [NSApp windowWithWindowNumber:number] : nil;
}

/* AppKit's frames start at the main screen's bottom left; the server's at
 * its top left. */
static SLSRect slsFrame(NSRect frame) {
    CGFloat height = CGDisplayBounds(CGMainDisplayID()).size.height;
    return (SLSRect){ frame.origin.x, height - frame.origin.y - frame.size.height,
        frame.size.width, frame.size.height };
}

/* Runs the sheet's completion block and lets the sheet go. */
static char windowAppearanceKey;

@interface NSSheetCompletion : NSObject {
@public
   void (^_handler)(NSModalResponse);
}
@end

@implementation NSSheetCompletion
-(void)sheetDidEnd:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo {
   if(_handler!=NULL)
    _handler(returnCode);
   [_handler release];
   _handler=NULL;
   [self release];
}
@end

static char contentTitleContext;

@implementation NSWindow

static char hidesWindowControlsKey;

-(void)_setHidesWindowControls:(BOOL)hides {
   objc_setAssociatedObject(self,&hidesWindowControlsKey,hides?(id)kCFBooleanTrue:nil,OBJC_ASSOCIATION_RETAIN);
   [[self _backgroundView] setNeedsDisplay:YES];
}

-(BOOL)_hidesWindowControls {
   return objc_getAssociatedObject(self,&hidesWindowControlsKey)!=nil;
}

+(NSWindowDepth)defaultDepthLimit {
   return 0;
}

+(NSRect)frameRectForContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)styleMask {
   return CGOutsetRectForNativeWindowBorder(contentRect,styleMask);
}

+(NSRect)contentRectForFrameRect:(NSRect)frameRect styleMask:(NSWindowStyleMask)styleMask {
   return CGInsetRectForNativeWindowBorder(frameRect,styleMask);
}

+(CGFloat)minFrameWidthWithTitle:(NSString *)title styleMask:(NSWindowStyleMask)styleMask {
   NSUnimplementedMethod();
   return 0;
}

/* The frontmost window on screen under the point (screen coordinates,
   from the bottom left), below window when it is not 0; 0 for none. */
+(NSInteger)windowNumberAtPoint:(NSPoint)point belowWindowWithWindowNumber:(NSInteger)window {
   CFArrayRef info=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly,kCGNullWindowID);
   CGFloat    height=CGDisplayBounds(CGMainDisplayID()).size.height;
   CGPoint    top=CGPointMake(point.x,height-point.y);
   BOOL       below=(window==0);
   NSInteger  result=0;
   CFIndex    i,count;

   if(info==NULL)
    return 0;
   count=CFArrayGetCount(info);
   for(i=0;i<count && result==0;i++){
    CFDictionaryRef entry=CFArrayGetValueAtIndex(info,i);
    CFNumberRef     number=CFDictionaryGetValue(entry,kCGWindowNumber);
    CFDictionaryRef boundsDict=CFDictionaryGetValue(entry,kCGWindowBounds);
    int32_t         windowNumber=0;
    CGRect          bounds;

    if(number==NULL || boundsDict==NULL || !CGRectMakeWithDictionaryRepresentation(boundsDict,&bounds))
     continue;
    CFNumberGetValue(number,kCFNumberSInt32Type,&windowNumber);
    if(!below){
     below=(_NSWindowNumberForServerWindow(windowNumber)==window);
     continue;
    }
    if(CGRectContainsPoint(bounds,top))
     result=_NSWindowNumberForServerWindow(windowNumber);
   }
   CFRelease(info);
   return result;
}

/* The window server numbers windows its own way: this application's
   windows have their AppKit numbers, others keep the server's. */
NSInteger _NSWindowNumberForServerWindow(int32_t serverNumber) {
   NSWindow *window=_NSWindowForSLSWindow((SLSWindowID)serverNumber);

   return (window!=nil)?[window windowNumber]:serverNumber;
}

NSArray *_NSWindowNumbersFromWindowInfo(CFArrayRef info,BOOL allApps) {
   NSMutableArray *result=[NSMutableArray array];
   pid_t           pid=getpid();
   CFIndex         i,count=CFArrayGetCount(info);

   for(i=0;i<count;i++){
    CFDictionaryRef entry=CFArrayGetValueAtIndex(info,i);
    CFNumberRef     owner=CFDictionaryGetValue(entry,kCGWindowOwnerPID);
    CFNumberRef     number=CFDictionaryGetValue(entry,kCGWindowNumber);
    int32_t         ownerPID=0,windowNumber=0;

    if(owner==NULL || number==NULL)
     continue;
    CFNumberGetValue(owner,kCFNumberSInt32Type,&ownerPID);
    CFNumberGetValue(number,kCFNumberSInt32Type,&windowNumber);
    if(allApps || ownerPID==pid)
     [result addObject:[NSNumber numberWithInteger:_NSWindowNumberForServerWindow(windowNumber)]];
   }
   return result;
}

+(NSArray *)windowNumbersWithOptions:(NSWindowNumberListOptions)options {
   // Visible windows only, as on macOS. Minkar has a single space, so
   // NSWindowNumberListAllSpaces changes nothing.
   CFArrayRef info=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly,kCGNullWindowID);
   NSArray   *result;

   if(info==NULL)
    return nil;
   result=_NSWindowNumbersFromWindowInfo(info,(options&NSWindowNumberListAllApplications)!=0);
   CFRelease(info);
   return result;
}

+(void)removeFrameUsingName:(NSString *)name {
   NSUnimplementedMethod();
}

+(NSButton *)standardWindowButton:(NSWindowButton)button forStyleMask:(NSWindowStyleMask)styleMask {
   NSUnimplementedMethod();
   return nil;
}

+(void)menuChanged:(NSMenu *)menu {
   NSUnimplementedMethod();
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}

// This is Apple private API
+(Class)frameViewClassForStyleMask:(NSUInteger)styleMask {
   return [NSThemeFrame class];
}

-init {
    return [self initWithContentRect:NSMakeRect(100,100,100,100) styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
}

-initWithCoder:(NSCoder *)coder {
  [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] is not implemented for coder %@",[self class],sel_getName(_cmd),coder];
   return self;
}

-initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)styleMask backing:(NSBackingStoreType)backing defer:(BOOL)defer screen:(NSScreen *)screen {
    NSRect backgroundFrame;
    NSRect contentViewFrame;

    _number = (int)self;
    _styleMask=styleMask;
    _frame=[self frameRectForContentRect:contentRect];
    /* Only a titled window is kept to the visible frame: a borderless one
       (the desktop, a panel, an overlay) may cover the menu bar and the
       Dock, as on the reference platform. */
    if(styleMask&NSWindowStyleMaskTitled)
     _frame=[self constrainFrameRect: _frame toScreen: [NSScreen mainScreen]];
    _level=NSNormalWindowLevel;
    _hasShadow=YES;
    SLSWindowID window = 0;
    if(SLSNewWindow(CGWindowLevelForKey(_level), slsFrame(_frame), &window) != kSLSErrorSuccess)
        return nil;
    @synchronized([NSWindow class]) {
        if(slsWindows == nil) {
            slsWindows = [NSMutableDictionary new];
            slsShown = [NSMutableSet new];
        }
        [slsWindows setObject:[NSNumber numberWithUnsignedInt:window]
            forKey:[NSNumber numberWithInt:_number]];
    }
    /* Windows have shadows by default, as on the reference platform. */
    SLSSetWindowShadow(window, 1);

    _preferredScreen = screen;

   backgroundFrame.origin=NSMakePoint(0,0);
   backgroundFrame.size=_frame.size;
   contentViewFrame=[self contentRectForFrameRect:backgroundFrame];

   _savedFrame = _frame;

   _backingType=backing;
   _minSize=NSMakeSize(0,0);
    // "The default maximum size of a window is {FLT_MAX, FLT_MAX}"
   _maxSize=NSMakeSize(FLT_MAX,FLT_MAX);

   _title=@"";
   _miniwindowTitle=@"";

   _menu=nil;

   _backgroundView=[[[[self class] frameViewClassForStyleMask:styleMask] alloc] initWithFrame:backgroundFrame];
   [_backgroundView setAutoresizesSubviews:YES];
   [_backgroundView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
   [_backgroundView _setWindow:self];
   [_backgroundView setNextResponder:self];

   _contentView=[[NSView alloc] initWithFrame:contentViewFrame];
   [_contentView setAutoresizesSubviews:YES];
   [_contentView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

   _backgroundColor=[[NSColor windowBackgroundColor] copy];

   _delegate=nil;
   _firstResponder=self;
   _sharedFieldEditor=nil;
   _currentFieldEditor=nil;
   _draggedTypes=nil;

   _trackingAreas=nil;
   _allowsToolTipsWhenApplicationIsInactive=NO;

   _sheetContext=nil;

   _isVisible=NO;
   _isDocumentEdited=NO;

   _makeSureIsOnAScreen=YES;

   _acceptsMouseMovedEvents=NO;
   _excludedFromWindowsMenu=NO;
   _isDeferred=defer;
   _isOneShot=NO;
   _useOptimizedDrawing=NO;
   _releaseWhenClosed=YES;
   _hidesOnDeactivate=NO;
   _viewsNeedDisplay=YES;
   _flushNeeded=YES;

   _isInLiveResize=NO;

   _resizeIncrements=NSMakeSize(1,1);
   _contentResizeIncrements=NSMakeSize(1,1);

   _autosaveFrameName=nil;

   _threadToContext=[[NSMutableDictionary alloc] init];

   [_backgroundView addSubview:_contentView];
   [_backgroundView setNeedsDisplay:YES];
    if (!(_styleMask & NSAppKitPrivateWindow)) {
        [[NSApplication sharedApplication] _addWindow:self];
    }

    _deviceDictionary = [NSMutableDictionary new];
    //_cglContext = NULL;
    //_caContext = NULL;
    _display = [NSDisplay currentDisplay];
    _isZoomed = NO;
    _isMiniaturized = NO;

    buffer = NULL;

    _context = [self cgContext];
    [[NSDraggingManager draggingManager] registerWindow:self dragTypes:nil];
    return self;
}

// FIX, relocate contentRect
-initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)styleMask backing:(NSBackingStoreType)backing defer:(BOOL)defer {
   return [self initWithContentRect:contentRect styleMask:styleMask backing:backing defer:defer screen:nil];
}

-(NSWindow *)initWithWindowRef:(void *)carbonRef {
   NSUnimplementedMethod();
   return nil;
}

-(void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_contentViewController removeObserver:self forKeyPath:@"title" context:&contentTitleContext];
    [_contentViewController release];
    if(_context)
        [_context release];
    if(buffer && bufsize) {
        free(buffer);
    }
    //if(_cglContext)
    //    [_cglContext release];
    //if(_caContext)
    //    [_caContext release];
    [_deviceDictionary release];
    [_childWindows release];
    [_title release];
    [_miniwindowTitle release];
    [_miniwindowImage release];
    [_backgroundView _setWindow:nil];
    [_backgroundView release];
    [_menu release];
    [_contentView release];
    [_backgroundColor release];
    [_sharedFieldEditor release];
    [_draggedTypes release];
    [_trackingAreas release];
    [_autosaveFrameName release];
    [_threadToContext release];
    [_undoManager release];
    [NSApp _removeWindow:self];

    [super dealloc];
}

-(O2Context *) createCGContextIfNeeded
{
    if(_context == nil) {
        if(buffer != NULL && bufsize > 0) {
            free(buffer);
            buffer = NULL;
        }

        /* Whole pixels, as flushWindow copies them: a fractional frame
           (an alert sized to its text) gave a stride a pixel off. */
        size_t pixelsWide = (size_t)_frame.size.width, pixelsHigh = (size_t)_frame.size.height;
        int depth = [_display depth] / 8;
        bufsize = depth * pixelsWide * pixelsHigh;
        /* A new buffer: all of it goes to the server at the next flush. */
        _damageAll = YES;

        /* The window's own pixels; flushWindow copies them to its surface. */
        buffer = calloc(1, bufsize);

        O2ColorSpaceRef colorSpace = O2ColorSpaceCreateDeviceRGB();
        O2Surface *surface = [[O2Surface alloc] initWithBytes:buffer
                width:pixelsWide height:pixelsHigh
                bitsPerComponent:8 bytesPerRow:4*pixelsWide colorSpace:colorSpace
                bitmapInfo:kO2BitmapByteOrder32Little|kCGImageAlphaPremultipliedFirst];
        _context = [[O2Context_builtin_FT alloc] initWithSurface:surface flipped:NO];

        /* The current context is left alone: it may be another window's,
         * one drawing while this one is made (a menu bar or a pop-up
         * button opening a menu), which restores it afterwards; lockFocus
         * makes this one current when it draws. */
        NSGraphicsContext *gc = [NSGraphicsContext graphicsContextWithGraphicsPort:_context flipped:NO];
        NSValue *key = [NSValue valueWithPointer:[NSThread currentThread]];
        [_threadToContext setObject:gc forKey:key];
    }
    return _context;
}

-(CGContextRef)cgContext {
    return [self createCGContextIfNeeded];
}

-(void)setStyleMask:(NSUInteger)mask {
    _styleMask=mask;
    [_backgroundView resizeSubviewsWithOldSize:[_backgroundView frame].size];
    [_backgroundView setNeedsDisplay:YES]; // FIXME: verify this is done
    // FIXME: do we need to tell WS about the new style?
}

-(void)postNotificationName:(NSString *)name {
   [[NSNotificationCenter defaultCenter] postNotificationName:name
     object:self];
}

/* FIXME: I have no idea why this is using a different store for the context
 * than is used in NSGraphicsContext.m. _threadToContext only seems to be
 * used here. Leave it for now but also set the other one, because NSThemeFrame
 * and others use that.
 */
-(NSGraphicsContext *)graphicsContext {
   NSValue           *key=[NSValue valueWithPointer:[NSThread currentThread]];
   NSGraphicsContext *result=[_threadToContext objectForKey:key];

   if(result==nil){
    result=[NSGraphicsContext graphicsContextWithWindow:self];
    [_threadToContext setObject:result forKey:key];
   }

   return result;
}

-(NSDictionary *)deviceDescription {
   NSValue *resolution=[NSValue valueWithSize:NSMakeSize(96.0,96.0)];
   NSValue *size=[NSValue valueWithSize:[self frame].size];

   return [NSDictionary dictionaryWithObjectsAndKeys:
    resolution,NSDeviceResolution,
    NSDeviceRGBColorSpace,NSDeviceColorSpaceName,
    [NSNumber numberWithInt:8],NSDeviceBitsPerSample,
    [NSNumber numberWithBool:YES],NSDeviceIsScreen,
    size,NSDeviceSize,
    nil];
}

-(void *)windowRef {
   NSUnimplementedMethod();
   return NULL;
}

-(BOOL)allowsConcurrentViewDrawing {
   NSUnimplementedMethod();
   return NO;
}

-(void)setAllowsConcurrentViewDrawing:(BOOL)allows {
   NSUnimplementedMethod();
}

-(NSView *)contentView {
   return _contentView;
}

-(id)delegate {
   return _delegate;
}

-(NSString *)title {
   return _title;
}

-(NSString *)representedFilename {
   return _representedFilename;
}

-(NSURL *)representedURL {
   NSUnimplementedMethod();
   return nil;
}

-(NSInteger)level {
   return _level;
}
-(void)setLevel:(NSInteger)value {
    _level = value;
    [self _updateWSState];
}

-(NSRect)frame {
   return _frame;
}

-(NSWindowStyleMask)styleMask {
   return _styleMask;
}

-(NSBackingStoreType)backingType {
   return _backingType;
}

-(NSWindowBackingLocation)preferredBackingLocation {
   NSUnimplementedMethod();
   return 0;
}

-(void)setPreferredBackingLocation:(NSWindowBackingLocation)location {
   NSUnimplementedMethod();
}

-(NSWindowBackingLocation)backingLocation {
   NSUnimplementedMethod();
   return 0;
}

-(NSSize)minSize {
   return _minSize;
}

-(NSSize)maxSize {
   return _maxSize;
}

-(NSSize)contentMinSize {
   return _contentMinSize;
}

-(NSSize)contentMaxSize {
   return _contentMaxSize;
}

-(BOOL)isOneShot {
   return _isOneShot;
}

-(BOOL)isOpaque {
   return _isOpaque;
}

-(BOOL)hasDynamicDepthLimit {
   return _dynamicDepthLimit;
}

-(BOOL)isReleasedWhenClosed {
   return _releaseWhenClosed;
}

-(BOOL)preventsApplicationTerminationWhenModal {
   NSUnimplementedMethod();
   return NO;
}

-(void)setPreventsApplicationTerminationWhenModal:(BOOL)prevents {
   NSUnimplementedMethod();
}

-(BOOL)hidesOnDeactivate {
   return _hidesOnDeactivate;
}

-(BOOL)worksWhenModal {
    // We do work when we're running a modal session
    return (_sheetContext && [_sheetContext modalSession] != nil);
}

-(BOOL)acceptsMouseMovedEvents {
   return _acceptsMouseMovedEvents;
}

-(BOOL)isExcludedFromWindowsMenu {
   return _excludedFromWindowsMenu;
}

-(BOOL)isAutodisplay {
   return _isAutodisplay;
}

-(BOOL)isFlushWindowDisabled {
   return _isFlushWindowDisabled;
}

-(NSString *)frameAutosaveName {
   return _autosaveFrameName;
}

-(BOOL)hasShadow {
   return _hasShadow;
}

-(BOOL)ignoresMouseEvents {
   return _ignoresMouseEvents;
}

-(NSSize)aspectRatio {
   return NSMakeSize(1.0,_resizeIncrements.height/_resizeIncrements.width);
}

-(NSSize)contentAspectRatio {
   return NSMakeSize(1.0,_contentResizeIncrements.height/_contentResizeIncrements.width);
}

-(BOOL)autorecalculatesKeyViewLoop {
    return _autorecalculatesKeyViewLoop;
}

-(BOOL)canHide {
   return _canHide;
}

-(BOOL)canStoreColor {
   return _canStoreColor;
}

-(BOOL)showsResizeIndicator {
   return _showsResizeIndicator;
}

-(BOOL)showsToolbarButton {
   return _showsToolbarButton;
}

-(BOOL)displaysWhenScreenProfileChanges {
   return _displaysWhenScreenProfileChanges;
}

-(BOOL)isMovableByWindowBackground {
   return _isMovableByWindowBackground;
}

-(BOOL)allowsToolTipsWhenApplicationIsInactive {
   return _allowsToolTipsWhenApplicationIsInactive;
}

-(NSImage *)miniwindowImage {
   return _miniwindowImage;
}

-(NSString *)miniwindowTitle {
   return _miniwindowTitle;
}

-(NSDockTile *)dockTile {
   NSUnimplementedMethod();
   return nil;
}

-(NSColor *)backgroundColor {
   return _backgroundColor;
}

-(CGFloat)alphaValue {
   return _alphaValue;
}

-(NSWindowDepth)depthLimit {
   return 0;
}

-(NSSize)resizeIncrements {
   return _resizeIncrements;
}

-(NSSize)contentResizeIncrements {
   return _contentResizeIncrements;
}

-(BOOL)preservesContentDuringLiveResize {
   return NO;
}

-(NSToolbar *)toolbar {
    return _toolbar;
}

-(NSView *)initialFirstResponder {
   return _initialFirstResponder;
}

-(void)removeObserver:(NSString *)name selector:(SEL)selector {
    if([_delegate respondsToSelector:selector]){
     [[NSNotificationCenter defaultCenter] removeObserver:_delegate
       name:name object:self];
    }
}

-(void)addObserver:(NSString *)name selector:(SEL)selector {
    if([_delegate respondsToSelector:selector]){
     [[NSNotificationCenter defaultCenter] addObserver:_delegate
       selector:selector name:name object:self];
    }
}

-(void)setDelegate:delegate {
   struct {
    NSString *name;
    SEL       selector;
   } notes[]={
    { NSWindowDidBecomeKeyNotification,@selector(windowDidBecomeKey:) },
    { NSWindowDidBecomeMainNotification,@selector(windowDidBecomeMain:) },
    { NSWindowDidDeminiaturizeNotification,@selector(windowDidDeminiaturize:) },
    { NSWindowDidMiniaturizeNotification,@selector(windowDidMiniaturize:) },
    { NSWindowDidMoveNotification,@selector(windowDidMove:) },
    { NSWindowDidResignKeyNotification,@selector(windowDidResignKey:) },
    { NSWindowDidResignMainNotification,@selector(windowDidResignMain:) },
    { NSWindowDidResizeNotification,@selector(windowDidResize:) },
    { NSWindowWillStartLiveResizeNotification,@selector(windowWillStartLiveResize:) },
    { NSWindowDidEndLiveResizeNotification,@selector(windowDidEndLiveResize:) },
    { NSWindowDidUpdateNotification,@selector(windowDidUpdate:) },
    { NSWindowWillCloseNotification,@selector(windowWillClose:) },
    { NSWindowWillMiniaturizeNotification,@selector(windowWillMiniaturize:) },
    { NSWindowWillMoveNotification,@selector(windowWillMove:) },
    { nil, NULL }
   };
   int i;

   if(_delegate!=nil)
    for(i=0;notes[i].name!=nil;i++)
     [self removeObserver:notes[i].name selector:notes[i].selector];

   _delegate=delegate;

   for(i=0;notes[i].name!=nil;i++)
    [self addObserver:notes[i].name selector:notes[i].selector];
}

/* A titled window keeps 20 points on a screen, so its title bar can be
   reached; a borderless one (the Dock, its edge, menus) stays where its
   app puts it, as on the reference platform. */
-(void)_makeSureIsOnAScreen {
   if(_makeSureIsOnAScreen && (_styleMask & NSTitledWindowMask) && [self isVisible] && ![self isMiniaturized]){
    NSRect   frame=_frame;
    NSArray *screens=[NSScreen screens];
    int      i,count=[screens count];
    BOOL     changed=NO;

    BOOL     tooFarLeft=YES,tooFarRight=YES,tooFarUp=YES,tooFarDown=YES;
    float    leastX=0,maxX=0,leastY=0,maxY=0;

    for(i=0;i<count;i++){
     NSRect check=[[screens objectAtIndex:i] frame];

     if(NSMaxX(frame)>check.origin.x+20)
      tooFarLeft=NO;
     if(frame.origin.x<NSMaxX(check)-20)
      tooFarRight=NO;
     if(frame.origin.y<NSMaxY(check)-20)
      tooFarUp=NO;
     if(NSMaxY(frame)>check.origin.y+20)
      tooFarDown=NO;

     if(check.origin.x<leastX)
      leastX=check.origin.x;
     if(check.origin.y<leastY)
      leastY=check.origin.y;
     if(NSMaxX(check)>maxX)
      maxX=NSMaxX(check);
     if(NSMaxY(check)>maxY)
      maxY=NSMaxY(check);
    }

    if(tooFarLeft){
     frame.origin.x=(leastX+20)-frame.size.width;
     changed=YES;
    }
    if(tooFarRight){
     frame.origin.x=maxX-20;
     changed=YES;
    }
    if(tooFarUp){
     frame.origin.y=(maxY-20)-frame.size.height;
     changed=YES;
    }
    if(tooFarDown){
     changed=YES;
     frame.origin.y=(leastY+20)-frame.size.height;
    }

       if(changed){
        [self setFrame:frame display:YES];
       }

    _makeSureIsOnAScreen=NO;
   }
}

-(void)setFrame:(NSRect)frame display:(BOOL)display {
   [self setFrame:frame display:display animate:NO];
}

- (void)_animateWithContext:(NSWindowAnimationContext *)context
{
    NSRect frame = [self frame];
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:context, @"NSWindowAnimationContext", nil];

    if (_animationContext == nil)
        _animationContext = [context retain];

    if (_animationContext != context)
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: attempt to animate frame while animation still in progress",
            [self class], NSStringFromSelector(_cmd)];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowWillAnimateNotification object:self userInfo:userInfo];

    [context decrement];

    if ([context stepCount] > 0) {
        frame.origin.x += [context stepRect].origin.x;
        frame.origin.y += [context stepRect].origin.y;
        frame.size.width += [context stepRect].size.width;
        frame.size.height += [context stepRect].size.height;
    }
    else
        frame = [context targetRect];

    [self setFrame:frame display:[context display]];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowAnimatingNotification object:self userInfo:userInfo];

    if ([context stepCount] > 0) {
        [self performSelector:_cmd withObject:context afterDelay:[context stepInterval]];
    }
    else {
        [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidAnimateNotification object:self userInfo:userInfo];

        [_animationContext release];
        _animationContext = nil;
#if 0
        if ([_backgroundView cachesImageForAnimation])
            [_backgroundView invalidateCachedImage];
#endif
    }
}

- (NSWindowAnimationContext *)_animationContext
{
    return _animationContext;
}

-(void)invalidateContextsWithNewSize:(NSSize)size forceRebuild:(BOOL)forceRebuild {
#if 0
    O2Image *snapshot = nil;
    if(_context)
        snapshot = O2BitmapContextCreateImage(_context);
    NSSize oldSize = _frame.size;
#endif

    if(!NSEqualSizes(_frame.size,size) || forceRebuild) {
        _frame.size = size;
        [self _updateWSState];

        [_context release];
        _context = nil;
        [self cgContext];
        //[_caContext release];
        //_caContext = NULL;
        //CGLReleaseContext(_cglContext);
        //_cglContext = NULL;
        //[self createCGLContextObjIfNeeded];
    }

    [self cgContext];

#if 0
    if(snapshot) {
        [_context drawImage:snapshot inRect:NSMakeRect(0,0,oldSize.width,oldSize.height)];
        [snapshot release];
    }
#endif

    //CGLSurfaceResize(_cglContext, size.width, size.height);
}

-(void)invalidateContextsWithNewSize:(NSSize)size {
    [self invalidateContextsWithNewSize:size forceRebuild:NO];
}

-(void)invalidate {
    [_threadToContext removeAllObjects];
}


-(void)setFrame:(NSRect)newFrame display:(BOOL)display animate:(BOOL)animate  {
    [self setFrame:newFrame display:display animate:animate tellWS:YES];
}

-(void)setFrame:(NSRect)newFrame display:(BOOL)display animate:(BOOL)animate tellWS:(BOOL)tellWS {
    if (NSEqualSizes([self minSize], NSMakeSize(0, 0)) == NO) {
       newFrame.size.width = MAX(NSWidth(newFrame), [self minSize].width);
       newFrame.size.height = MAX(NSHeight(newFrame), [self minSize].height);
    }

    if (NSEqualSizes([self maxSize], NSMakeSize(FLT_MAX, FLT_MAX)) == NO) {
       newFrame.size.width = MIN(NSWidth(newFrame), [self maxSize].width);
       newFrame.size.height = MIN(NSHeight(newFrame), [self maxSize].height);
    }

    BOOL didSize=NSEqualSizes(newFrame.size,_frame.size)?NO:YES;
    BOOL didMove=NSEqualPoints(newFrame.origin,_frame.origin)?NO:YES;

    _frame=newFrame;
    _makeSureIsOnAScreen=YES;

    if(didSize) {
        NSSize oldSize = [_backgroundView frame].size;
        [_backgroundView setFrameSize:_frame.size];
        [_backgroundView resizeSubviewsWithOldSize:oldSize];
        [self invalidateContextsWithNewSize:_frame.size forceRebuild:YES];
        [self resetCursorRects];
        [self saveFrameUsingName:_autosaveFrameName];
        [self postNotificationName:NSWindowDidResizeNotification];
    }

    if(didMove) {
        [self saveFrameUsingName:_autosaveFrameName];
        [self postNotificationName:NSWindowDidMoveNotification];
    }

    // Sync WS to our new geometry & position before redisplaying
    if(tellWS)
        [self _updateWSState];

    // If you setFrame:display:YES before rearranging views with only setFrame:
    // calls (which do not mark the view for display) Cocoa will properly
    // redisplay the views So, doing a hard display right here is not the right
    // thing to do, delay it

    if(display)
        [_backgroundView setNeedsDisplay:YES];

    if(animate) {
        NSWindowAnimationContext *context;

        context = [NSWindowAnimationContext contextToTransformWindow:self startRect:[self frame] targetRect:newFrame resizeTime:    [self animationResizeTime:newFrame] display:display];

        [self _animateWithContext:context];
    }

    [self _setSheetOriginAndFront];
    [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidChangeFrame:) withObject:self];
    [_drawers makeObjectsPerformSelector:@selector(parentWindowDidChangeFrame:) withObject:self];
}

-(void)setContentSize:(NSSize)size {
   NSRect frame,content=[self contentRectForFrameRect:[self frame]];

   content.size=size;

   frame=[self frameRectForContentRect:content];

   [self setFrame:frame display:YES];
}

-(void)setFrameOrigin:(NSPoint)point {
   NSRect frame=[self frame];

   frame.origin=point;
   [self setFrame:frame display:NO];
}

-(void)setFrameTopLeftPoint:(NSPoint)point {
   NSRect frame=[self frame];

   frame.origin.x=point.x;
   frame.origin.y=point.y-frame.size.height;

   [self setFrame:frame display:NO];
}

-(void)setMinSize:(NSSize)size {
   _minSize=size;
}

-(void)setMaxSize:(NSSize)size {
   _maxSize=size;
}

-(void)setContentMinSize:(NSSize)value {
   _contentMinSize=value;
   NSUnimplementedMethod();
}

-(void)setContentMaxSize:(NSSize)value {
   _contentMaxSize=value;
}

-(void)setContentBorderThickness:(CGFloat)thickness forEdge:(NSRectEdge)edge {
// FIXME: should warn, but low priority cosmetic, so we dont, still needs to be implemented
//   NSUnimplementedMethod();
}

-(void)setMovable:(BOOL)movable {
   _isNotMovable = !movable;
}

-(void)setBackingType:(NSBackingStoreType)value {
   _backingType=value;
   NSUnimplementedMethod();
}

-(void)setDynamicDepthLimit:(BOOL)value {
   _dynamicDepthLimit=value;
}

-(void)setOneShot:(BOOL)flag {
   _isOneShot=flag;
}

-(void)setReleasedWhenClosed:(BOOL)flag {
   _releaseWhenClosed=flag;
}

-(void)setHidesOnDeactivate:(BOOL)flag {
   _hidesOnDeactivate=flag;
}

-(void)setAcceptsMouseMovedEvents:(BOOL)flag {
   _acceptsMouseMovedEvents=flag;
}

-(void)setExcludedFromWindowsMenu:(BOOL)value {
   _excludedFromWindowsMenu=value;
}

-(void)setAutodisplay:(BOOL)value {
   _isAutodisplay=value;
}

-(void)setAutorecalculatesContentBorderThickness:(BOOL)automatic forEdge:(NSRectEdge)edge {
// FIXME: should warn, but low priority cosmetic, so we dont, still needs to be implemented
//   NSUnimplementedMethod();
}

-(BOOL)_isApplicationWindow {
   return (![self isKindOfClass:[NSPanel class]] && [self isVisible] && ![self isExcludedFromWindowsMenu])?YES:NO;
}

-(void)setTitle:(NSString *)title {
    title=[title copy];
    [_title release];
    _title=title;

    [_miniwindowTitle release];
    _miniwindowTitle=[title copy];

    [self _updatePlatformWindowTitle];

    if ([self _isApplicationWindow])
        [NSApp changeWindowsItem:self title:title filename:NO];
    [_backgroundView setNeedsDisplay:YES];
}

-(NSWindowTitleVisibility)titleVisibility {
    return _titleVisibility;
}

-(void)setTitleVisibility:(NSWindowTitleVisibility)visibility {
    _titleVisibility=visibility;
    [_backgroundView setNeedsDisplay:YES];
}

/* The title bar's background is the window's own in Atlas's look, so a
   transparent one differs only where the content view covers it
   (NSWindowStyleMaskFullSizeContentView); kept for what asks it. */
-(BOOL)titlebarAppearsTransparent {
    return _titlebarAppearsTransparent;
}

-(void)setTitlebarAppearsTransparent:(BOOL)transparent {
    _titlebarAppearsTransparent=transparent;
    [_backgroundView setNeedsDisplay:YES];
}

-(CGFloat)_titlebarHeight {
    return _titlebarHeight;
}

-(void)_setTitlebarHeight:(CGFloat)height {
    _titlebarHeight=height;
    [_backgroundView setNeedsDisplay:YES];
    /* The bar the window moves by is that tall too. */
    [self _syncSkyLightDragRegion];
}

/* The title (and the edited mark, which the frame draws) to the window
 * server and on the bar again. */
-(void)_updatePlatformWindowTitle {
    [self _updateWSState];
    [_backgroundView setNeedsDisplay:YES];
}

-(void)setTitleWithRepresentedFilename:(NSString *)filename {
   [self setTitle:[NSString stringWithFormat:@"%@  --  %@",
      [filename lastPathComponent],
      [filename stringByDeletingLastPathComponent]]];

   if ([self _isApplicationWindow])
        [NSApp changeWindowsItem:self title:filename filename:YES];
}

-(void)setContentView:(NSView *)view {
   view=[view retain];
   [view setFrame:[_contentView frame]];
   // Minkar: the content view follows the window's size, as on the
   // reference platform (with constraints, a fixed one kept the window from
   // growing to its content).
   [view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

   [_contentView removeFromSuperview];
   [_contentView release];

   _contentView=view;

   [_backgroundView addSubview:_contentView];
}

-(void)setInitialFirstResponder:(NSView *)view {
    _initialFirstResponder = view;
}

-(void)setMiniwindowImage:(NSImage *)image {
   image=[image retain];
   [_miniwindowImage release];
   _miniwindowImage=image;
}

-(void)setMiniwindowTitle:(NSString *)title {
   title=[title copy];
   [_miniwindowTitle release];
   _miniwindowTitle=title;

   [self _updatePlatformWindowTitle];
}

-(void)setBackgroundColor:(NSColor *)color {
   if (color==nil) color = [NSColor windowBackgroundColor];
   color=[color copy];
   [_backgroundColor release];
   _backgroundColor=color;
   [_backgroundView setNeedsDisplay:YES];
}

-(void)setAlphaValue:(CGFloat)value {
   _alphaValue=value;
   SLSSetWindowAlpha(slsWindowFor(_number), value);
}

/* A toolbar set on a window takes its room from the content view, the
   window keeping its frame, as on the reference platform (an app that
   lays out its content for the window's size before adding the toolbar
   sees it so there). */
/* A toolbar set or taken away: as on the reference platform the content
   keeps its size and the frame grows or shrinks by the toolbar, its top
   edge kept. The content gave up the room instead: a window with no
   content yet (TextMate's preferences, sized by their pane before they
   show) lost the toolbar's height and grew past the screen's bottom. */
-(void)_toolbarSetFromOldHeight:(CGFloat)oldHeight {
   [self _toolbarSizeDidChangeFromOldHeight:oldHeight];
   [[self _backgroundView] setNeedsDisplay:YES];
}

-(void)_toolbarSizeDidChangeFromOldHeight:(CGFloat)oldHeight {
   CGFloat    newHeight,contentHeightDelta;
   NSView    *toolbarView=[_toolbar _view];
   NSUInteger mask=[[self contentView] autoresizingMask];
   NSRect     frame=[self frame];

   [_toolbar layoutFrameSizeWithWidth:NSWidth([[self _backgroundView] bounds])];
   newHeight=(_toolbar==nil)?0:[_toolbar visibleHeight];
   contentHeightDelta=newHeight-oldHeight;

   frame.size.height+=contentHeightDelta;
   frame.origin.y-=contentHeightDelta;

   NSPoint toolbarOrigin;
   NSRect backgroundBounds=[self _backgroundView].bounds;
   toolbarOrigin.x=backgroundBounds.origin.x;
   toolbarOrigin.y=NSMaxY([[self contentView] frame])-contentHeightDelta;
   [toolbarView setFrameOrigin:toolbarOrigin];

   [[self contentView] setAutoresizingMask:NSViewNotSizable];
   [self setFrame:frame display:NO animate:NO];

   [[self contentView] setAutoresizingMask:mask];
}

-(void)setToolbar:(NSToolbar *)toolbar {
   if(toolbar!=_toolbar){
    CGFloat oldHeight=0;

    toolbar=[toolbar retain];

    if(_toolbar!=nil){
     oldHeight=[_toolbar visibleHeight];
     [_toolbar _setWindow:nil];
     [[_toolbar _view] removeFromSuperview];
     [_toolbar release];
     [[self _backgroundView] setNeedsDisplay:YES];
    }

    _toolbar = toolbar;

    if(_toolbar!=nil){
     [_toolbar _setWindow:self];
     [[self _backgroundView] addSubview:[_toolbar _view]];
     [[self _backgroundView] setNeedsDisplay:YES];
    }
    [self _toolbarSetFromOldHeight:oldHeight];
   }
}

- (void)setDefaultButtonCell:(NSButtonCell *)buttonCell {
    [_defaultButtonCell autorelease];
    _defaultButtonCell = [buttonCell retain];
    [_defaultButtonCell setKeyEquivalent:@"\r"];
    [[_defaultButtonCell controlView] setNeedsDisplay:YES];
}

-(void)setWindowController:(NSWindowController *)value {
   _windowController=value;
/*
   Cocoa does not setReleasedWhenClosed:NO when setWindowController: is called.
   The NSWindowController class does setReleasedWhenClosed:NO in conjunction with setWindowController:

   However, there is one application (AC), which calls setWindowController: standalone and does
   _something else_ which also does setReleasedWhenClosed:NO. Perhaps some byproduct of NSDOcument, NSWindowController or NSWindow.
   THis hasn't been figured out yet. So, in the meantime we do setReleasedWhenClosed:NO since all cases which do call setWindowCOntroller: also
   want setReleasedWhenClosed:NO.
 */
   [self setReleasedWhenClosed:NO];
}

-(void)setDocumentEdited:(BOOL)flag {
   _isDocumentEdited=flag;
   [self _updatePlatformWindowTitle];
}

-(void)setContentAspectRatio:(NSSize)value {
   _resizeIncrements.width=1.0;
   _resizeIncrements.height=value.height/value.width;
}

-(void)setHasShadow:(BOOL)value {
   _hasShadow=value;
   SLSSetWindowShadow(slsWindowFor(_number), value);
}

-(void)setIgnoresMouseEvents:(BOOL)value {
   _ignoresMouseEvents=value;
}

-(void)setAspectRatio:(NSSize)value {
   _resizeIncrements.width=1.0;
   _resizeIncrements.height=value.height/value.width;
}

-(void)setAutorecalculatesKeyViewLoop:(BOOL)value {
    _autorecalculatesKeyViewLoop=value;
}

-(void)setCanHide:(BOOL)value {
   _canHide=value;
}

-(void)setCanBecomeVisibleWithoutLogin:(BOOL)flag {
//   NSUnimplementedMethod();
}

-(void)setCollectionBehavior:(NSWindowCollectionBehavior)behavior {
   NSUnimplementedMethod();
}

-(void)setOpaque:(BOOL)value {
   _isOpaque=value;
}

-(void)setParentWindow:(NSWindow *)value {
   _parentWindow=value;
}

-(void)setPreservesContentDuringLiveResize:(BOOL)value {
  // _preservesContentDuringLiveResize=value;
}

-(void)setRepresentedFilename:(NSString *)value {
   value=[value copy];
   [_representedFilename release];
   _representedFilename=value;
}

-(void)setRepresentedURL:(NSURL *)newURL {
   NSUnimplementedMethod();
}

-(void)setResizeIncrements:(NSSize)value {
   _resizeIncrements=value;
}

-(void)setShowsResizeIndicator:(BOOL)value {
   _showsResizeIndicator=value;
   NSUnimplementedMethod();
}

-(void)setShowsToolbarButton:(BOOL)value {
  _showsToolbarButton=value;
   NSUnimplementedMethod();
}

-(void)setContentResizeIncrements:(NSSize)value {
   _contentResizeIncrements=value;
}

-(void)setDepthLimit:(NSWindowDepth)value {
   NSUnimplementedMethod();
}

-(void)setDisplaysWhenScreenProfileChanges:(BOOL)value {
   _displaysWhenScreenProfileChanges=value;
}

-(void)setMovableByWindowBackground:(BOOL)value {
   _isMovableByWindowBackground=value;
}

-(void)setAllowsToolTipsWhenApplicationIsInactive:(BOOL)value {
   _allowsToolTipsWhenApplicationIsInactive=value;
}

-(BOOL)autorecalculatesContentBorderThicknessForEdge:(NSRectEdge)edge {
   NSUnimplementedMethod();
   return NO;
}

-(CGFloat)contentBorderThicknessForEdge:(NSRectEdge)edge {
   NSUnimplementedMethod();
   return 0.;
}

-(NSString *)_autosaveFrameKeyWithName:(NSString *)name {
   return [NSString stringWithFormat:@"NSWindow frame %@ %@",name, NSStringFromRect([[self screen] frame])];
}

-(BOOL)setFrameUsingName:(NSString *)name {
   return [self setFrameUsingName:name force:NO];
}

-(BOOL)setFrameUsingName:(NSString *)name force:(BOOL)force {
   NSString *key=[self _autosaveFrameKeyWithName:name];
   NSString *value=[[NSUserDefaults standardUserDefaults] objectForKey:key];

   if([value length]==0)
    return NO;

   [self setFrameFromString:value];

   return YES;
}

-(void)_setFrameAutosaveNameNoIO:(NSString *)name {
   name=[name copy];
   [_autosaveFrameName release];
   _autosaveFrameName=name;
}

-(BOOL)setFrameAutosaveName:(NSString *)name {
   [self _setFrameAutosaveNameNoIO:name];

   [self setFrameUsingName:_autosaveFrameName];
   [self saveFrameUsingName:_autosaveFrameName];
   return YES;
}

-(void)postAwakeFromNib {
/*
  We  do this after awakeFromNib because a saved frame is also post awakeFromNib. If awakeFromNib modifies
  the windows adornments we need to wait until here to actually set it.
 */
   if([_autosaveFrameName length]>0){
    [self setFrameUsingName:_autosaveFrameName];
    [self saveFrameUsingName:_autosaveFrameName];
   }
}

-(void)saveFrameUsingName:(NSString *)name {
   if([name length]>0){
    NSString *key=[self _autosaveFrameKeyWithName:name];
    NSString *value=[self stringWithSavedFrame];

    [[NSUserDefaults standardUserDefaults] setObject:value forKey:key];
   }
}

-(void)setFrameFromString:(NSString *)value {
   NSRect rect=NSRectFromString(value);

   if(!NSIsEmptyRect(rect)){
    [self setFrame:rect display:YES];
   }
}

-(NSString *)stringWithSavedFrame {
   return NSStringFromRect([self frame]);
}

-(NSInteger)resizeFlags {
   NSUnimplementedMethod();
   return 0;
}

-(CGFloat)userSpaceScaleFactor {
   return 1.0;
}

-(NSResponder *)firstResponder {
   if ([_firstResponder isKindOfClass:[NSDrawer class]])
    return [_firstResponder nextResponder];
   else
    return _firstResponder;
}

-(NSButton *)standardWindowButton:(NSWindowButton)value {
   NSUnimplementedMethod();
   return nil;
}

-(NSButtonCell *)defaultButtonCell {
    return _defaultButtonCell;
}

-(NSWindow *)attachedSheet {
   return [_sheetContext sheet];
}

-(id)windowController {
   return _windowController;
}

-(NSArray *)drawers {
    return _drawers;
}

-(NSInteger)windowNumber {
    return _number;
}

-(NSInteger)gState {
   NSUnimplementedMethod();
   return 0;
}

-(NSScreen *)screen {
   NSArray  *screens=[NSScreen screens];
   int       i,count=[screens count];
   NSRect    mostRect=NSZeroRect;
   NSScreen *mostScreen=nil;

   for(i=0;i<count;i++){
    NSScreen *check=[screens objectAtIndex:i];
    NSRect    intersect=NSIntersectionRect([check frame],_frame);

    if(intersect.size.width*intersect.size.height>mostRect.size.width*mostRect.size.height){
     mostRect=intersect;
     mostScreen=check;
    }
   }

   return mostScreen;
}

-(NSScreen *)deepestScreen {
   NSUnimplementedMethod();
   return 0;
}

-(NSColorSpace *)colorSpace {
   NSUnimplementedMethod();
   return nil;
}

-(void)setColorSpace:(NSColorSpace *)newColorSpace {
   NSUnimplementedMethod();
}

-(BOOL)isOnActiveSpace {
   NSUnimplementedMethod();
   return YES;
}

-(NSWindowSharingType)sharingType {
   NSUnimplementedMethod();
   return 0;
}

-(void)setSharingType:(NSWindowSharingType)type {
   NSUnimplementedMethod();
}

-(BOOL)isDocumentEdited {
   return _isDocumentEdited;
}

-(BOOL)isZoomed {
    NSRect zoomedFrame = [self zoomedFrame];
    return NSEqualRects( _frame, zoomedFrame );
}

-(BOOL)isVisible {
   return _isVisible;
}

-(BOOL)isKeyWindow {
   return ([NSApp keyWindow]==self)?YES:NO;
}

-(BOOL)isMainWindow {
   return ([NSApp mainWindow]==self)?YES:NO;
}

-(BOOL)isMiniaturized {
    return _isMiniaturized;
}

/* As documented: a window is movable unless setMovable:NO was called. */
-(BOOL)isMovable {
   return !_isNotMovable;
}

-(BOOL)inLiveResize {
   return _isInLiveResize;
}

-(BOOL)canBecomeKeyWindow {
    // The NSWindow implementation returns YES if the window has a title bar or a resize bar, or NO otherwise;
    // a sheet (no title bar of its own) takes the keys too.
    return (_styleMask & (NSTitledWindowMask|NSResizableWindowMask|NSDocModalWindowMask)) != 0;
}

-(BOOL)canBecomeMainWindow {
    // The NSWindow implementation returns YES if the window is visible and has a title bar or a resize mechanism. Otherwise it returns NO
    return [self isVisible] && (_styleMask & (NSTitledWindowMask|NSResizableWindowMask));
}

-(BOOL)canBecomeVisibleWithoutLogin {
   NSUnimplementedMethod();
   return NO;
}

-(NSWindowCollectionBehavior)collectionBehavior {
   NSUnimplementedMethod();
   return 0;
}

-(NSPoint)convertBaseToScreen:(NSPoint)point {
   NSRect frame=[self frame];

   point.x+=frame.origin.x;
   point.y+=frame.origin.y;

   return point;
}

-(NSPoint)convertScreenToBase:(NSPoint)point {
   NSRect frame=[self frame];

   point.x-=frame.origin.x;
   point.y-=frame.origin.y;

   return point;
}

-(NSRect)frameRectForContentRect:(NSRect)contentRect {
   NSRect result=CGOutsetRectForNativeWindowBorder(contentRect,[self styleMask]);

   if([_toolbar _view]!=nil && ![[_toolbar _view] isHidden])
    result.size.height+=[[_toolbar _view] frame].size.height;

    return result;
}

-(NSRect)contentRectForFrameRect:(NSRect)frameRect {
   NSRect result=CGInsetRectForNativeWindowBorder(frameRect,[self styleMask]);

   if([_toolbar _view]!=nil && ![[_toolbar _view] isHidden])
    result.size.height-=[[_toolbar _view] frame].size.height;

   return result;
}

/* Minkar: the part of the window, in its own coordinates, where content
   is laid out: below the title bar and the toolbar (API 10.10). */
-(NSRect)contentLayoutRect {
   NSRect frame=[self frame];
   NSRect content=[self contentRectForFrameRect:frame];

   content.origin.x-=frame.origin.x;
   content.origin.y-=frame.origin.y;
   return content;
}

-(NSWindowOcclusionState)occlusionState {
   return ([self isVisible] && ![self isMiniaturized] && ![NSApp isHidden])?NSWindowOcclusionStateVisible:0;
}

static char titlebarSeparatorKey;

/* Atlas draws the title bar without a separator line of its own: the
   style is kept for the app (automatic until set). */
-(NSTitlebarSeparatorStyle)titlebarSeparatorStyle {
   return [objc_getAssociatedObject(self,&titlebarSeparatorKey) integerValue];
}

-(void)setTitlebarSeparatorStyle:(NSTitlebarSeparatorStyle)style {
   objc_setAssociatedObject(self,&titlebarSeparatorKey,[NSNumber numberWithInteger:style],OBJC_ASSOCIATION_RETAIN);
}

/* The title bar's buttons sit by the application's reading direction. */
-(NSUserInterfaceLayoutDirection)windowTitlebarLayoutDirection {
   return (NSApp!=nil)?[NSApp userInterfaceLayoutDirection]:NSUserInterfaceLayoutDirectionLeftToRight;
}

-(NSRect)constrainFrameRect:(NSRect)rect toScreen:(NSScreen *)screen {
   if ( !screen) return rect;
   NSRect visRect = [screen visibleFrame];

   /* Minkar: a resizable window taller than the visible area is cut to
      it, so its title bar stays below the menu bar (as documented). */
   if(([self styleMask]&NSWindowStyleMaskResizable) && rect.size.height>visRect.size.height){
    rect.origin.y=NSMaxY(rect)-visRect.size.height;
    rect.size.height=visRect.size.height;
   }

   if (NSMaxX(rect) > NSMaxX(visRect)) {
    rect.origin.x = NSMaxX(visRect) - rect.size.width;
   }
   if (NSMaxY(rect) > NSMaxY(visRect)) {
    rect.origin.y = NSMaxY(visRect) - rect.size.height;
   }
   if (NSMinX(rect) < NSMinX(visRect)) {
    rect.origin.x = NSMinX(visRect);
   }
   if (NSMinY(rect) < NSMinY(visRect)) {
    rect.origin.y = NSMinY(visRect);
   }
   return rect;
}

-(NSWindow *)parentWindow {
   return _parentWindow;
}

-(NSArray *)childWindows {
   return _childWindows;
}

/*
 * With the new server (WS4a), the server keeps a child next to its parent,
 * moves it with the parent and orders it out and in with it
 * (SLSSetWindowParent); a child is ordered, not made key, as on the
 * reference platform, and removing it leaves it where it is.
 */
-(void)addChildWindow:(NSWindow *)child ordered:(NSWindowOrderingMode)ordered {
   if(_childWindows==nil)
    _childWindows=[NSMutableArray new];

   if([_childWindows indexOfObjectIdenticalTo:child]==NSNotFound)
       [_childWindows addObject:child];
   [child setParentWindow:self];
   SLSSetWindowParent(slsWindowFor([child windowNumber]), slsWindowFor(_number),
       ordered==NSWindowBelow ? kSLSOrderBelow : kSLSOrderAbove);
   if([self isVisible])
       [child orderWindow:ordered==NSWindowBelow ? NSWindowBelow : NSWindowAbove
           relativeTo:_number];
   return;
}

-(void)removeChildWindow:(NSWindow *)child {
   SLSSetWindowParent(slsWindowFor([child windowNumber]), 0, 0);
   [child setParentWindow:nil];
   [_childWindows removeObjectIdenticalTo:child];
   return;
}

-(void)_parentWindowDidClose:(NSWindow *)parent {
   [self orderOut:nil];
}

-(void)_parentWindowDidActivate:(NSWindow *)parent {
   [self orderWindow:NSWindowAbove relativeTo:[_parentWindow windowNumber]];
}

-(void)_parentWindowDidDeactivate:(NSWindow *)parent {
   [self orderWindow:NSWindowAbove relativeTo:[_parentWindow windowNumber]];
}

-(void)_parentWindowDidMiniaturize:(NSWindow *)parent {
   [self orderOut:nil];
}

-(void)_parentWindowDidChangeFrame:(NSWindow *)parent {
}

-(void)_parentWindowDidExitMove:(NSWindow *)parent {
   [self orderWindow:NSWindowAbove relativeTo:[_parentWindow windowNumber]];
}

-(BOOL)acceptsFirstResponder {
   return YES;
}

-(BOOL)makeFirstResponder:(NSResponder *)responder {

   if(_firstResponder==responder ||
      ([responder isKindOfClass:[NSControl class]] && _firstResponder==[(NSControl *)responder currentEditor]))
    return YES;

   if(![_firstResponder resignFirstResponder])
    return NO;

   _firstResponder=responder;

   if([_firstResponder becomeFirstResponder])
    return YES;

   _firstResponder=self;

   return NO;
}

-(void)makeKeyWindow {
    /* The new window server sends this window the keys, and AppKit makes
       it its key window (becomeKeyWindow, which calls back here): the
       legacy server's activation callbacks did that, and nothing did with
       the new one, so NSApp had no key window. */
    static BOOL becoming;
    SLSSetKeyWindow(slsWindowFor(_number));
    if(!becoming && ![self isKeyWindow]) {
        becoming=YES;
        [self becomeKeyWindow];
        becoming=NO;
        return;
    }
    if(!_hasBeenOnScreen){
        _hasBeenOnScreen=YES;

        // Ref. http://www.cocoadev.com/index.pl?KeyViewLoopGuidelines

        // If there is an initial first responder there is a manual key view loop and we don't calculate one
        if([self initialFirstResponder]!=nil)
            [self makeFirstResponder:[self initialFirstResponder]];
        else {
            // otherwise calculate one and set the first responder
            if ([self autorecalculatesKeyViewLoop]) {
                [self recalculateKeyViewLoop];
            }
            if([self firstResponder]==self)
                [self makeFirstResponder:[_contentView nextValidKeyView]];
        }
    }
}

-(void)makeMainWindow {
   [self becomeMainWindow];
}

-(void)becomeKeyWindow {

    // The platform should always be told to become key when we want to
    // become key
    [self makeKeyWindow];

   if([self isKeyWindow]) // if we don't return early we may resign ourself
    return;

// Become key window before the previous key window resigns so that the new key window is valid
// before NSWindowDidResignKeyNotification is sent.
   NSWindow *keyWindow=[NSApp keyWindow];

   [NSApp _setKeyWindow:self];

   [keyWindow resignKeyWindow];

   if(_firstResponder!=self && [_firstResponder respondsToSelector:_cmd])
    [_firstResponder performSelector:_cmd];

   [self postNotificationName:NSWindowDidBecomeKeyNotification];
}

-(void)resignKeyWindow {
   if(_firstResponder!=self && [_firstResponder respondsToSelector:_cmd])
    [_firstResponder performSelector:_cmd];

   [self postNotificationName:NSWindowDidResignKeyNotification];
}

-(void)becomeMainWindow {
    if([self isMainWindow])
        return;

    NSWindow *mainWindow=[NSApp mainWindow];
    [NSApp _setMainWindow:self];
    [mainWindow resignMainWindow];

    [self postNotificationName:NSWindowDidBecomeMainNotification];
}

-(void)resignMainWindow {
    [self postNotificationName:NSWindowDidResignMainNotification];
}

- (NSTimeInterval)animationResizeTime:(NSRect)frame {
    return 0.20;
}

-(void)selectNextKeyView:sender {
   if([_firstResponder isKindOfClass:[NSView class]]){
    NSView *view=(NSView *)_firstResponder;

    [self selectKeyViewFollowingView:view];
   }
}

-(void)selectPreviousKeyView:sender {
   if([_firstResponder isKindOfClass:[NSView class]]){
    NSView *view=(NSView *)_firstResponder;

    [self selectKeyViewPrecedingView:view];
   }
}

-(void)selectKeyViewFollowingView:(NSView *)view {
   NSView *next=[view nextValidKeyView];

   [self makeFirstResponder:next];
}

-(void)selectKeyViewPrecedingView:(NSView *)view {
   NSView *next=[view previousValidKeyView];

   [self makeFirstResponder:next];
}

-(void)recalculateKeyViewLoopIfNeeded {
    if(YES){
      //  _needsKeyViewLoop=NO;

        NSArray *sorted=[_NSKeyViewPosition sortedKeyViewPositionsWithView:_contentView];
        NSUInteger i,count=[sorted count];

        for(i=0;i<count;i++){
            _NSKeyViewPosition *position=[sorted objectAtIndex:i];

            if(i+1<count){
                [[position view] setNextKeyView:[[sorted objectAtIndex:i+1] view]];
            }
            else {
                [[position view] setNextKeyView:[[sorted objectAtIndex:0] view]];
            }
        }
    }
}

-(void)recalculateKeyViewLoop {
    //_needsKeyViewLoop=YES;
    // This should be deferred
    [self recalculateKeyViewLoopIfNeeded];
}

-(NSSelectionDirection)keyViewSelectionDirection {
   NSUnimplementedMethod();
   return 0;
}

- (void)disableKeyEquivalentForDefaultButtonCell {
    _defaultButtonCellKeyEquivalentDisabled = YES;
}

- (void)enableKeyEquivalentForDefaultButtonCell {
    _defaultButtonCellKeyEquivalentDisabled = NO;
}

-(NSText *)fieldEditor:(BOOL)create forObject:object {
   NSTextView *newFieldEditor = nil;
   if([_delegate respondsToSelector:@selector(windowWillReturnFieldEditor:toObject:)])
      newFieldEditor = [_delegate windowWillReturnFieldEditor:self toObject:object];

   if(create && newFieldEditor == nil && _sharedFieldEditor == nil)
      newFieldEditor = _sharedFieldEditor = [[NSTextView alloc] init];

   if (newFieldEditor)
      _currentFieldEditor = newFieldEditor;
   else
      _currentFieldEditor = _sharedFieldEditor;

   if (_currentFieldEditor) {
      [_currentFieldEditor setHorizontallyResizable:NO];
      [_currentFieldEditor setVerticallyResizable:NO];
      [_currentFieldEditor setFieldEditor:YES];
      [_currentFieldEditor setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
   }

   return _currentFieldEditor;
}

-(void)endEditingFor:object {
   if (_currentFieldEditor) {
      if ((NSResponder *)_currentFieldEditor == _firstResponder) {
         _firstResponder = object;
         [_currentFieldEditor resignFirstResponder];
      }
      [_currentFieldEditor setDelegate:nil];
      [_currentFieldEditor removeFromSuperview];
      [_currentFieldEditor setString:@""];
      _currentFieldEditor = nil;
   }
}

-(void)disableScreenUpdatesUntilFlush {
   NSUnimplementedMethod();
}

-(void)useOptimizedDrawing:(BOOL)flag {
   // do nothing
}

-(BOOL)viewsNeedDisplay {
   return _viewsNeedDisplay;
}

-(void)setViewsNeedDisplay:(BOOL)flag {
   if(flag && !_viewsNeedDisplay){
    // NSApplication does a _displayAllWindowsIfNeeded before every event, but there are some things which wont generate
    // an event such as performOnMainThread, so we do the callout here too. There is probably a better way to do this
       [[NSRunLoop currentRunLoop] cancelPerformSelector:@selector(_displayAllWindowsIfNeeded) target:NSApp argument:nil]; // Be sure we don't accumulate unneeded perform operations
       [[NSRunLoop currentRunLoop] performSelector:@selector(_displayAllWindowsIfNeeded) target:NSApp argument:nil order:0 modes:[NSArray arrayWithObjects:NSDefaultRunLoopMode, NSModalPanelRunLoopMode, NSEventTrackingRunLoopMode, nil]];
   }
    _viewsNeedDisplay=flag;
}

-(void)disableFlushWindow {
   _flushDisabled++;
}

-(void)enableFlushWindow {
   _flushDisabled--;
}

-(void)flushWindow {
    if(_flushDisabled > 0)
        _flushNeeded=YES;
    else {
        _flushNeeded=NO;

        if(!([self isOpaque] && [_contentView isKindOfClass:[NSOpenGLView class]] && [_contentView isOpaque])) {
            O2ContextFlush(_context);
        }
        if(buffer != NULL) {
            /* What was drawn since the last flush; the whole window when
               nothing was noted (or the buffer is new). */
            int32_t width = (int32_t)_frame.size.width, height = (int32_t)_frame.size.height;
            SLSIntRect all = { 0, 0, width, height };
            [self _syncSkyLightDragRegion];
            if(_damageAll || _damageCount == 0)
                SLSWindowFlushPixels(slsWindowFor(_number), buffer, 4 * width, &all, 1);
            else
                SLSWindowFlushPixels(slsWindowFor(_number), buffer, 4 * width,
                    (const SLSIntRect *)_damage, _damageCount);
            _damageCount = 0;
            _damageAll = NO;
            /* The window's drawing may have covered its Metal layers. */
            _CAMetalLayersWindowFlushed(slsWindowFor(_number));
        }
    }
}

-(void)_addDamageRect:(NSRect)rect {
    int32_t width = (int32_t)_frame.size.width, height = (int32_t)_frame.size.height;
    NSRect r = NSIntegralRect(NSIntersectionRect(rect, NSMakeRect(0, 0, width, height)));

    if(NSIsEmptyRect(r) || _damageAll)
        return;
    /* From the bottom left (AppKit) to the top left (the server). */
    int32_t x = (int32_t)r.origin.x, y = height - (int32_t)NSMaxY(r);
    int32_t w = (int32_t)r.size.width, h = (int32_t)r.size.height;
    for(uint32_t i = 0; i < _damageCount; i++) {
        /* Inside one already noted. */
        if(x >= _damage[i].x && y >= _damage[i].y && x + w <= _damage[i].x + _damage[i].width &&
            y + h <= _damage[i].y + _damage[i].height)
            return;
    }
    if(_damageCount == sizeof(_damage) / sizeof(_damage[0])) {
        /* Too many: their bounding rect. */
        int32_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
        for(uint32_t i = 0; i < _damageCount; i++) {
            x0 = MIN(x0, _damage[i].x);
            y0 = MIN(y0, _damage[i].y);
            x1 = MAX(x1, _damage[i].x + _damage[i].width);
            y1 = MAX(y1, _damage[i].y + _damage[i].height);
        }
        _damageCount = 1;
        x = x0; y = y0; w = x1 - x0; h = y1 - y0;
        _damage[0].x = x; _damage[0].y = y; _damage[0].width = w; _damage[0].height = h;
        return;
    }
    _damage[_damageCount].x = x;
    _damage[_damageCount].y = y;
    _damage[_damageCount].width = w;
    _damage[_damageCount].height = h;
    _damageCount++;
}

-(void)flushWindowIfNeeded {
   if(_flushNeeded)
    [self flushWindow];
}

-(void)displayIfNeeded {
   [self layoutIfNeeded];
   if([self isVisible] && ![self isMiniaturized] && [self viewsNeedDisplay]){
    NSAutoreleasePool *pool=[NSAutoreleasePool new];

    if ([NSGraphicsContext quartzDebuggingIsEnabled] == YES) {

        // Show all the views getting redrawn
       [NSGraphicsContext setQuartzDebugMode: YES];
       [self disableFlushWindow];
       [_backgroundView displayIfNeeded];
       [self enableFlushWindow];
       [self flushWindowIfNeeded];
    }

    [NSGraphicsContext setQuartzDebugMode: NO];

    [self disableFlushWindow];
    [_backgroundView displayIfNeeded];
    [self enableFlushWindow];
    [self flushWindowIfNeeded];
    [self setViewsNeedDisplay:NO];
    [pool release];
   }
}

-(void)display {
/* FIXME: See Issue #405, display when the window is not visible causes layout problems (maybe the underlying Win32 window doesnt exist and we're not getting resize feedback messages?), so there is a problem. The fix is to not display when we aren't visible, displayIfNeeded does this already so it makes sense. The underlying problem should be fixed too though.
 */
   [self layoutIfNeeded];
   if([self isVisible]){
    NSAutoreleasePool *pool=[NSAutoreleasePool new];

    if ([NSGraphicsContext quartzDebuggingIsEnabled] == YES) {

        // Show all the views getting redrawn
       [NSGraphicsContext setQuartzDebugMode: YES];
       [self disableFlushWindow];
       [_backgroundView display];
       [self enableFlushWindow];
       [self flushWindowIfNeeded];
    }

    [NSGraphicsContext setQuartzDebugMode: NO];

    [self disableFlushWindow];
    [_backgroundView display];
    [self enableFlushWindow];
    [self flushWindowIfNeeded];
    [pool release];
   }
   else {
    // If we were asked to display and weren't visible, mark it for display
    [_backgroundView setNeedsDisplay:YES];
}
}

-(void)invalidateShadow {
   // Do nothing
}

-(void)cacheImageInRect:(NSRect)rect {
   NSUnimplementedMethod();
}

-(void)restoreCachedImage {
   NSUnimplementedMethod();
}

-(void)discardCachedImage {
   NSUnimplementedMethod();
}

-(BOOL)areCursorRectsEnabled {
   return (_cursorRectsDisabled<=0)?YES:NO;
}

-(void)disableCursorRects {
   _cursorRectsDisabled++;
   if(_cursorRectsDisabled==1)
    [self _invalidateTrackingAreas];
}

-(void)enableCursorRects {
   _cursorRectsDisabled--;
   if(_cursorRectsDisabled==0)
    [self _invalidateTrackingAreas];
}

-(void)discardCursorRects {
   [[self _backgroundView] discardCursorRects];
   [self _invalidateTrackingAreas];
}

// Apple docs say: "sends -resetCursorRects to every NSView object in the [...] hierarchy",
// and it means that. No [[self _backgroundView] resetCursorRects] and trusting in
// recursion through the view hierarchy.
-(void)_resetCursorRectsInView:(NSView *)view {
   NSArray *subviews=[view subviews];
   int      i,count=[subviews count];

   for(i=0;i<count;i++)
    [self _resetCursorRectsInView:[subviews objectAtIndex:i]];

   [view resetCursorRects];
}

-(void)resetCursorRects {
   [self discardCursorRects];
   [self _resetCursorRectsInView:_backgroundView];
   [self _invalidateTrackingAreas];
}

-(void)invalidateCursorRectsForView:(NSView *)view {
   [view discardCursorRects];
   [self _resetCursorRectsInView:view];
   [self _invalidateTrackingAreas];
}

// This shall be received in case of
// - -[NSWindows areCursorRectsEnabled] changes
// - -[NSApplication isActive] changes
// - the number or TrackingAreas has changed
// - a property of one of the TrackingAreas has changed
// - a frame of this window or one the relevant views has changed.
-(void)_invalidateTrackingAreas {
   // Rebuild it on demand.
   [_trackingAreas release];
   _trackingAreas=nil;
}

// Never send this directly, except you actually need _trackingAreas.
-(void)_resetTrackingAreas {
   if(_trackingAreas==nil){
    NSInteger count;
    BOOL toolTipsAllowed=[[NSApplication sharedApplication] isActive] ||
                         [self allowsToolTipsWhenApplicationIsInactive];

    NSMutableArray *collectedAreas=[[NSMutableArray alloc] init];
    [[self _backgroundView] _collectTrackingAreasForWindowInto:collectedAreas];
    _trackingAreas=collectedAreas;

    count=[_trackingAreas count];
    while(--count>=0){
     NSTrackingArea *area=[_trackingAreas objectAtIndex:count];

     if((_cursorRectsDisabled>0 && [area options]&NSTrackingCursorUpdate) ||
        ([area _isToolTip] && !toolTipsAllowed))
      [_trackingAreas removeObjectAtIndex:count];
    }

    if(!toolTipsAllowed){
     // We have to do this here as Area handling won't even recignize ToolTips now.
     NSToolTipWindow *toolTipWindow=[NSToolTipWindow sharedToolTipWindow];

     [NSObject cancelPreviousPerformRequestsWithTarget:toolTipWindow selector:@selector(orderFront:) object:nil];
     [toolTipWindow orderOut:nil];
    }
   }
}

-(void)close {
    [self orderOut:nil];

    [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidClose:) withObject:self];
        [_drawers makeObjectsPerformSelector:@selector(parentWindowDidClose:) withObject:self];

    [self postNotificationName:NSWindowWillCloseNotification];

    NSNumber *key = [NSNumber numberWithInt:_number];
    SLSReleaseWindow(slsWindowFor(_number));
    @synchronized([NSWindow class]) {
        [slsWindows removeObjectForKey:key];
        [slsDragRegions removeObjectForKey:key];
        [slsKnownFrames removeObjectForKey:key];
    }
    [slsShown removeObject:key];

    if(_releaseWhenClosed)
        [self autorelease];
}

-(void)center {
   NSRect    frame=[self frame];
   NSScreen *screen=[self screen];
   NSRect    screenFrame;

   if(screen==nil)
    screen=[[NSScreen screens] objectAtIndex:0];

   screenFrame=[screen frame];

   frame.origin.x=floor(screenFrame.origin.x+screenFrame.size.width/2-frame.size.width/2);
   frame.origin.y=floor(screenFrame.origin.y+screenFrame.size.height/2-frame.size.height/2);

   [self setFrame:frame display:YES];
}

/* The controllers of the window's views hear them appear and disappear
   as the window comes on the screen and leaves it, as on the reference
   platform (not when a view is put in a window not shown yet: TextMate
   makes its preferences window before the panes it shows). */
static void sendAppearance(NSView *view,SEL selector) {
   NSViewController *controller=_NSViewControllerForView(view);

   if(controller!=nil)
    [controller performSelector:selector];
   for(NSView *subview in [[[view subviews] copy] autorelease])
    sendAppearance(subview,selector);
}

-(void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)relativeTo {
   BOOL appearing=(!_isVisible && place!=NSWindowOut);
   BOOL disappearing=(_isVisible && place==NSWindowOut);

   /* Minkar: a titled window is kept on screen, below the menu bar, when
      it is ordered in (an app may have set any frame before). */
   if(appearing && ([self styleMask]&NSWindowStyleMaskTitled) && _parentWindow==nil){
    NSScreen *screen=[self screen]!=nil?[self screen]:[NSScreen mainScreen];
    NSRect    constrained=[self constrainFrameRect:_frame toScreen:screen];

    if(!NSEqualRects(constrained,_frame))
     [self setFrame:constrained display:NO];
   }
   if(appearing)
    sendAppearance([self _backgroundView],@selector(viewWillAppear));
   if(disappearing)
    sendAppearance([self _backgroundView],@selector(viewWillDisappear));
   [self _orderWindow:place relativeTo:relativeTo];
   if(appearing)
    sendAppearance([self _backgroundView],@selector(viewDidAppear));
   if(disappearing)
    sendAppearance([self _backgroundView],@selector(viewDidDisappear));
}

-(void)_orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)relativeTo {
// The move notifications are sent under unknown conditions around orderFront: in the Apple AppKit, we do them all the time here until it's figured out. I suspect it is a side effect of off-screen windows being at off-screen coordinates (as opposed to just being hidden)

   [self postNotificationName:NSWindowWillMoveNotification];

   switch(place){
    case NSWindowAbove:
     [self update];

     _isVisible=YES;
     [self displayIfNeeded];
     [self _updateWSState];
     // this is here since it would seem that doing this any earlier will not work.
     if(![self isKindOfClass:[NSPanel class]] && ![self isExcludedFromWindowsMenu]) {
         [NSApp changeWindowsItem:self title:_title filename:NO];
     }

     break;

    case NSWindowBelow:
     [self update];

     _isVisible=YES;
     [self displayIfNeeded];
     [self _updateWSState];
     // this is here since it would seem that doing this any earlier will not work.
     if(![self isKindOfClass:[NSPanel class]] && ![self isExcludedFromWindowsMenu]) {
       [NSApp changeWindowsItem:self title:_title filename:NO];
     }
     break;

    case NSWindowOut:
     _isVisible=NO;
     [self _updateWSState];
     if (![self isKindOfClass:[NSPanel class]]) {
       [NSApp removeWindowsItem:self];
     }
     break;
   }

   if(place != NSWindowOut) {
       NSNumber *key = [NSNumber numberWithInt:_number];
       SLSOrderWindow(slsWindowFor(_number), place == NSWindowAbove ? kSLSOrderAbove : kSLSOrderBelow,
           relativeTo ? slsWindowFor(relativeTo) : 0);
       [slsShown addObject:key];
   }

   [self postNotificationName:NSWindowDidMoveNotification];
}

-(void)orderFrontRegardless {
   NSUnimplementedMethod();
}

-(NSPoint)mouseLocationOutsideOfEventStream {
    /* The cursor, from the top left of the main display, in this
       window's coordinates. */
    SLSPoint p;

    if(SLSGetCursorPosition(&p) != kSLSErrorSuccess)
        return NSZeroPoint;
    CGFloat height = CGDisplayBounds(CGMainDisplayID()).size.height;
    return [self convertScreenToBase:NSMakePoint(p.x, height - p.y)];
}

-(NSEvent *)currentEvent {
    return [NSApp currentEvent];
}

-(NSEvent *)nextEventMatchingMask:(NSEventMask)mask {
   return [self nextEventMatchingMask:mask untilDate:[NSDate distantFuture]
      inMode:NSEventTrackingRunLoopMode dequeue:YES];
}

-(void) captureEvents {
}

-(NSEvent *)nextEventMatchingMask:(NSEventMask)mask untilDate:(NSDate *)untilDate inMode:(NSString *)mode dequeue:(BOOL)dequeue {
   NSEvent *event;

// this should get migrated down into event queue

   [self captureEvents]; // what does this do?

   do {
    event=[NSApp nextEventMatchingMask:mask untilDate:untilDate inMode:mode dequeue:dequeue];

   }while(!(mask&NSEventMaskFromType([event type])));

   return event;
}

-(void)discardEventsMatchingMask:(NSEventMask)mask beforeEvent:(NSEvent *)event {
   NSUnimplementedMethod();
}

-(void)sendEvent:(NSEvent *)event {

    // Some events can cause our window to be destroyed
    // So make sure self lives at least through this current run loop...
    [[self retain] autorelease];

    if (_sheetContext != nil) {
        NSView *view = [_backgroundView hitTest:[event locationInWindow]];

        // Pretend that the event goes to the toolbar's view, no matter where it really is.
        // Could cause problems if custom views wanted to do something while the palette is running;
        // however they shouldn't be doing that!
        if ([[self toolbar] customizationPaletteIsRunning] &&
            (view == [[self toolbar] _view] || [[[[self toolbar] _view] subviews] containsObject:view])) {
            switch ([event type]) {
                case NSLeftMouseDown:
                    [[[self toolbar] _view] mouseDown:event];
                    break;

                case NSLeftMouseUp:
                    [[[self toolbar] _view] mouseUp:event];
                    break;

                case NSLeftMouseDragged:
                    [[[self toolbar] _view] mouseDragged:event];
                    break;

                default:
                    break;
            }
            return;
        }
        else if ([event type] == NSPlatformSpecific){
            //[self _setSheetOriginAndFront];
            return;
        }
    }

    BOOL shouldValidateToolbarItems = YES;
    // OK let's see if anyone else wants it
   switch([event type]){

    case NSLeftMouseDown:{
        NSView *view=[_backgroundView hitTest:[event locationInWindow]];

        /* Control-click is a context menu click, as on the reference
           platform, where the view has a menu. */
        if(([event modifierFlags]&NSControlKeyMask) && view!=nil){
            NSMenu *contextMenu=[view menuForEvent:event];

            if(contextMenu!=nil){
                [NSMenu popUpContextMenu:contextMenu withEvent:event forView:view];
                break;
            }
        }

        /* A click in a window that is not key makes it key and brings it
         * to the front (the window server does not reorder windows). A
         * key window comes to the front too: it may be behind another
         * app's windows, each app keeping its own key window. */
        if(![self isKeyWindow] && [self canBecomeKeyWindow])
            [self makeKeyAndOrderFront:self];
        else if([self isVisible])
            [self orderFront:self];

        if([view acceptsFirstResponder]){
            if([view needsPanelToBecomeKey]) {
                [self makeFirstResponder:view];
            }
        }

        // Event goes to view, not first responder
        [view mouseDown:event];
        _mouseDownLocationInWindow=[event locationInWindow];
     }
     break;

    case NSLeftMouseUp:
     [[_backgroundView hitTest:_mouseDownLocationInWindow] mouseUp:event];
     _mouseDownLocationInWindow=NSMakePoint(NAN,NAN);
     break;

    case NSRightMouseDown:
      _mouseDownLocationInWindow=[event locationInWindow];
     [[_backgroundView hitTest:[event locationInWindow]] rightMouseDown:event];
     break;

    case NSRightMouseUp:
     [[_backgroundView hitTest:_mouseDownLocationInWindow] rightMouseUp:event];
     _mouseDownLocationInWindow=NSMakePoint(NAN,NAN);
     break;

    case NSMouseMoved:{
      NSView *hit=[_backgroundView hitTest:[event locationInWindow]];

      if(hit==nil)
       [self mouseMoved:event];
      else
       [hit mouseMoved:event];
      /* With the new server, the moves that reach the window drive its
         tracking areas: cursor rects, entered and exited, tool tips. */
      [self _updateTrackingAreas];
     }
     break;

    case NSLeftMouseDragged:
     [[_backgroundView hitTest:_mouseDownLocationInWindow] mouseDragged:event];
     break;

    case NSRightMouseDragged:
     [[_backgroundView hitTest:_mouseDownLocationInWindow] rightMouseDragged:event];
     break;

    case NSMouseEntered:
     [[_backgroundView hitTest:[event locationInWindow]] mouseEntered:event];
     break;

    case NSMouseExited:
     [[_backgroundView hitTest:[event locationInWindow]] mouseExited:event];
     break;

    case NSKeyDown:
     /* Return, Enter and Escape go to the window's buttons first (the
        default button, a Cancel button), as on the reference platform:
        a text field being edited would take them otherwise. */
     if(!([event modifierFlags]&(NSCommandKeyMask|NSAlternateKeyMask|NSControlKeyMask))) {
      NSString *keys=[event charactersIgnoringModifiers];
      unichar key=[keys length] ? [keys characterAtIndex:0] : 0;
      if((key=='\r' || key==3 || key==27) && [self performKeyEquivalent:event])
       break;
     }
     [_firstResponder keyDown:event];
     break;

    case NSKeyUp:
     [_firstResponder keyUp:event];
     break;

    case NSFlagsChanged:
     [_firstResponder flagsChanged:event];
     break;

    case NSPlatformSpecific:
     break;

    case NSScrollWheel:
     [[_backgroundView hitTest:[event locationInWindow]] scrollWheel:event];
     break;

    case NSAppKitDefined:
     if([event subtype] == NSWindowMovedEventType)
      [self _skyLightMovedToX:[event data1] y:[event data2]];
     break;

    default:
     shouldValidateToolbarItems = NO;
     NSUnimplementedMethod();
     break;
   }
    if (shouldValidateToolbarItems && [self toolbar]) {
        [NSObject cancelPreviousPerformRequestsWithTarget:[self toolbar] selector:@selector(validateVisibleItems) object:nil];
        [[self toolbar] performSelector:@selector(validateVisibleItems) withObject:nil afterDelay:.5];

    }
}

-(void)postEvent:(NSEvent *)event atStart:(BOOL)atStart {
   [NSApp postEvent:event atStart:atStart];
}

-(BOOL)tryToPerform:(SEL)selector with:object {
   if([super tryToPerform:selector with:object])
    return YES;

   if([_delegate respondsToSelector:selector]){
    [_delegate performSelector:selector withObject:object];
    return YES;
   }

   return NO;
}

-(NSPoint)cascadeTopLeftFromPoint:(NSPoint)topLeftPoint {
   BOOL    reposition = NO;
   NSSize  screenSize = [[self screen] frame].size;
   NSRect  frame = [self frame];

   if (frame.origin.x < 0.0 || screenSize.width  <= frame.origin.x + frame.size.width)
   {
      frame.origin.x = 2.0;
      reposition = YES;
   }

   if (frame.origin.y < 0.0 || screenSize.height <= frame.origin.y + frame.size.height)
   {
      frame.origin.y = 2.0;
      reposition = YES;
   }

   if (topLeftPoint.x != 0.0 && topLeftPoint.x + frame.size.width + 20.0 < screenSize.width)
   {
      topLeftPoint.x += 18.0;
      frame.origin.x = topLeftPoint.x;
      reposition = YES;
   }
   else
      topLeftPoint.x = frame.origin.x;

   if (topLeftPoint.y != 0.0 && topLeftPoint.y - frame.size.height - 23.0 >= 0.0)
   {
      topLeftPoint.y -= 21.0;
      frame.origin.y = topLeftPoint.y - frame.size.height;
      reposition = YES;
   }
   else
      topLeftPoint.y = frame.origin.y + frame.size.height;

   if (reposition)
      [self setFrame:frame display:YES];

   return topLeftPoint;
}

-(NSData *)dataWithEPSInsideRect:(NSRect)rect {
   return [_backgroundView dataWithEPSInsideRect:rect];
}

-(NSData *)dataWithPDFInsideRect:(NSRect)rect {
   return [_backgroundView dataWithPDFInsideRect:rect];
}

-(void)registerForDraggedTypes:(NSArray *)types {
   _draggedTypes=[types copy];
}

-(void)unregisterDraggedTypes {
   [_draggedTypes release];
   _draggedTypes=nil;
}

-(void)dragImage:(NSImage *)image at:(NSPoint)location offset:(NSSize)offset event:(NSEvent *)event pasteboard:(NSPasteboard *)pasteboard source:source slideBack:(BOOL)slideBack {
   [[NSDraggingManager draggingManager] dragImage:image at:location offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

-validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType {
   NSUnimplementedMethod();
   return nil;
}

-(void)update {
    [[self toolbar] validateVisibleItems];
   [[NSNotificationCenter defaultCenter]
       postNotificationName:NSWindowDidUpdateNotification
                     object:self];
}

-(void)makeKeyAndOrderFront:sender {
    if ([self isMiniaturized]) {
        [self deminiaturize:self];
    }

// Order window before making it key, per doc.s and behavior

   [self orderWindow:NSWindowAbove relativeTo:0];

    if([self canBecomeKeyWindow])
        [self makeKeyWindow];

   if([self canBecomeMainWindow])
    [self makeMainWindow];
}

-(void)orderFront:sender {
   [self orderWindow:NSWindowAbove relativeTo:0];
}

-(void)orderBack:sender {
   [self orderWindow:NSWindowBelow relativeTo:0];
}

-(void)orderOut:sender {
   [self orderWindow:NSWindowOut relativeTo:0];
}

-(void)performClose:sender
{
  if([_delegate respondsToSelector:@selector(windowShouldClose:)])
    {
      if(![_delegate windowShouldClose:self])
        return;
    }
  else if ([self respondsToSelector:@selector(windowShouldClose:)])
    {
      if (![self windowShouldClose:self])
        return;
    }

  NSDocument * document = [_windowController document];
  if (document)
    {
      [document shouldCloseWindowController:_windowController
                                   delegate:self
                        shouldCloseSelector:@selector(_document:shouldClose:contextInfo:)
                                contextInfo:NULL];
    }
  else
    {
    // Clicking the close button on a Window generates a performClose:, in a non-modal case we just close the window. If the window is a modal window, we abort the session, but do not close the window. So far it looks like we should not close the window too.

        if([NSApp modalWindow]==self)
            [NSApp abortModal];
        else
           [self close];
    }
}

-(void)_document:(NSDocument *)document shouldClose:(BOOL)shouldClose contextInfo:(void *)context
{
  // Callback used by performClose:
  if (shouldClose)
    {
      [self close];
    }
}

-(void)performMiniaturize:sender {
   [self miniaturize:sender];
}

-(void)performZoom:sender {
    [self zoom: sender];
}

- (NSRect) zoomedFrame;
{
    NSScreen *screen = [self screen];
    NSRect zoomedFrame = [screen visibleFrame];

    if (_delegate && [_delegate respondsToSelector: @selector(windowWillUseStandardFrame:defaultFrame:)]) {
        zoomedFrame = [_delegate windowWillUseStandardFrame: self defaultFrame: zoomedFrame];
    } else if ([self respondsToSelector: @selector( windowWillUseStandardFrame:defaultFrame: )]) {
        zoomedFrame = [self windowWillUseStandardFrame: self defaultFrame: zoomedFrame];
    }
    /* Minkar: what the delegate gives is kept on screen, below the menu
       bar (a delegate may give the whole screen). */
    zoomedFrame = [self constrainFrameRect: zoomedFrame toScreen: screen];

    return zoomedFrame;
}

-(void)zoom:sender {
    NSRect zoomedFrame = [self zoomedFrame];
    if (NSEqualRects( _frame, zoomedFrame )) zoomedFrame = _savedFrame;

    // Make sure we obey our minimums
    NSSize minSize = [self minSize];
    if (NSWidth(zoomedFrame) < minSize.width) {
        zoomedFrame.size.width = minSize.width;
    }
    if (NSHeight(zoomedFrame) < minSize.height) {
        zoomedFrame.size.height = minSize.height;
    }

    BOOL shouldZoom = YES;
    if (_delegate && [_delegate respondsToSelector: @selector( windowShouldZoom:toFrame: )]) {
        shouldZoom = [_delegate windowShouldZoom: self toFrame: zoomedFrame];
    } else if ([self respondsToSelector: @selector( windowShouldZoom:toFrame: )]) {
        shouldZoom = [self windowShouldZoom: self toFrame: zoomedFrame];
    }

    if (shouldZoom) {
        _savedFrame = [self frame];
        [self setFrame: zoomedFrame display: YES];
    }
}

/*
 * With the new server, minimizing is the server's (WS4a): it orders the
 * window out, makes the app's next window key and tells the Dock, which
 * keeps it. The Dock's animation and tile come with D2 and D3.
 */
-(void)miniaturize:sender {
    if(_isMiniaturized)
        return;
    [self postNotificationName:NSWindowWillMiniaturizeNotification];
    _isMiniaturized = YES;
    _isZoomed = NO;
    SLSSetWindowMinimized(slsWindowFor(_number), 1);
    [slsShown removeObject:[NSNumber numberWithInt:_number]];
    if([self isKeyWindow])
        [self resignKeyWindow];
    if([self isMainWindow])
        [self resignMainWindow];
    [self postNotificationName:NSWindowDidMiniaturizeNotification];
    return;
}

-(void)deminiaturize:sender {
    if(!_isMiniaturized)
        return;
    /* Ordering it in restores it on the server. */
    _isMiniaturized = NO;
    [self makeKeyAndOrderFront:sender];
    [self postNotificationName:NSWindowDidDeminiaturizeNotification];
    return;
}

-(void)print:sender {
   [_backgroundView print:sender];
}

-(void)toggleToolbarShown:sender {
    [_toolbar setVisible:![_toolbar isVisible]];
    [sender setTitle:[NSString stringWithFormat:@"%@ Toolbar", [_toolbar isVisible] ? @"Hide" : @"Show"]];
}

-(void)runToolbarCustomizationPalette:sender {
    [_toolbar runCustomizationPalette:sender];
}

- (void)keyDown:(NSEvent *)event {
    if ([self performKeyEquivalent:event] == NO)
        [self interpretKeyEvents:[NSArray arrayWithObject:event]];
}

- (void)doCommandBySelector:(SEL)selector {
    if ([_delegate respondsToSelector:selector])
        [_delegate performSelector:selector withObject:nil];
    else
        [super doCommandBySelector:selector];
}

- (void)insertTab:sender {
    [self selectNextKeyView:nil];
}

- (void)insertBacktab:sender {
    [self selectPreviousKeyView:nil];
}

- (void)insertNewline:sender {
    if (_defaultButtonCell != nil)
        [(NSControl *)[_defaultButtonCell controlView] performClick:nil];
}

-(void)_showForActivation {
   if(_hiddenForDeactivate){
    _hiddenForDeactivate=NO;
   }
}

-(void)showWindowWithoutActivation {
    _isVisible = YES;
    [self _updateWSState];
}

-(void)hideWindow {
    _isVisible = NO;
    [self _updateWSState];
}

-(void)_hideForDeactivation {
   if([self hidesOnDeactivate] && [self isVisible] && ![self isMiniaturized]){
    _hiddenForDeactivate=YES;
   }
}

/* Hide and unhide (NSApplication): the windows Hide took off the screen,
 * which Unhide brings back. The menu bar and status items stay (they are
 * not the app's windows to hide). */
static NSMutableSet *hiddenByHide;

-(void)_forcedHideForDeactivation {
    if([self level]==NSMainMenuWindowLevel || [self level]==NSStatusWindowLevel ||
       [self level]==NSPopUpMenuWindowLevel)
        return;
    if([self isVisible]){
        if(hiddenByHide==nil)
            hiddenByHide=[NSMutableSet new];
        [hiddenByHide addObject:[NSNumber numberWithInt:_number]];
        /* Off the screen without orderOut: (the app would end with no
           window visible). */
        [self hideWindow];
    }
}

-(void)_showAfterHide {
    NSNumber *key=[NSNumber numberWithInt:_number];

    if([hiddenByHide containsObject:key]) {
        [hiddenByHide removeObject:key];
        [self orderWindow:NSWindowAbove relativeTo:0];
    }
}

-(BOOL)performKeyEquivalent:(NSEvent *)event {
   return [_backgroundView performKeyEquivalent:event];
}

-(void)setMenu:(NSMenu *)menu {
    [menu retain];
    [_menu release];
    _menu = menu;
}

-(NSMenu *)menu {
   return _menu;
}

-(BOOL)_isActive {
   return _isActive;
}

-(void)_setVisible:(BOOL)visible;
{
    _isVisible = visible;
    if(visible) {
        [self showWindowWithoutActivation];
    } else {
        [self hideWindow];
    }
}

// default NSDraggingDestination
-(NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender {
   return NSDragOperationNone;
}

-(NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender {
   return [sender draggingSourceOperationMask];
}

-(void)draggingExited:(id <NSDraggingInfo>)sender {
   // do nothing
}

-(BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender {
   return YES;
}

-(BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
   return NO;
}

-(void)concludeDragOperation:(id <NSDraggingInfo>)sender {
   // do nothing
}


-(NSArray *)_draggedTypes {
   return _draggedTypes;
}

-(void)_setSheetOrigin {
   NSWindow *sheet=[_sheetContext sheet];
   NSRect    sheetFrame=[sheet frame];
   NSRect    frame=[self frame];
   /* Under the title bar: the content area's top edge. */
   NSRect    content=[self contentRectForFrameRect:frame];
   NSPoint   origin;

   /* The content rect is already below the toolbar. */
   origin.y=NSMaxY(content)-sheetFrame.size.height;
   origin.x=frame.origin.x+floor((frame.size.width-sheetFrame.size.width)/2);

   [sheet setFrameOrigin:origin];
}

-(void)_setSheetOriginAndFront {
   if(_sheetContext!=nil){
    [self _setSheetOrigin];

    [[_sheetContext sheet] orderFront:nil];
   }
}

-(void)_attachSheetContextOrderFrontAndAnimate:(NSSheetContext *)sheetContext {
   NSWindow *sheet = [sheetContext sheet];
   NSRect    sheetFrame;

   if ([sheet styleMask] != NSDocModalWindowMask)
    [sheet setStyleMask:NSDocModalWindowMask];

   [_sheetContext autorelease];
   _sheetContext=[sheetContext retain];

   [(NSThemeFrame *)[sheet _backgroundView] setWindowBorderType:NSWindowSheetBorderType];

   [self _setSheetOrigin];
   sheetFrame = [sheet frame];

   /* A sheet is a child window over the title bar's bottom edge. */
   [sheet orderWindow:NSWindowAbove relativeTo:_number];
   SLSSetWindowParent(slsWindowFor([sheet windowNumber]), slsWindowFor(_number),
       kSLSOrderAbove);
   /* The sheet takes the keys: Return presses its default button. */
   [sheet makeKeyWindow];
   return;
}

- (void)_setSheetContext:(NSSheetContext *)sheetContext
{
    [sheetContext retain];
    [_sheetContext release];
    _sheetContext = sheetContext;
}

-(NSSheetContext *)_sheetContext {
   return _sheetContext;
}

-(void)_detachSheetContextAnimateAndOrderOut {
    NSWindow *sheet = [_sheetContext sheet];
    NSRect sheetFrame = [sheet frame];

    BOOL      wasKey = [sheet isKeyWindow];

    SLSSetWindowParent(slsWindowFor([sheet windowNumber]), 0, 0);
    [sheet orderWindow:NSWindowOut relativeTo:0];
    /* The keys go back to the window the sheet was on. */
    if(wasKey && [self isVisible])
     [self makeKeyWindow];
    [_sheetContext release];
    _sheetContext=nil;
    return;
}

-(void)_flashWindow {
    if([self _isApplicationWindow])
        NSUnimplementedMethod();
}

#if 0 // This seems all backwards for WindowServer
-(void)platformWindowActivated:(CGWindow *)window displayIfNeeded:(BOOL)displayIfNeeded {
   [NSApp _windowWillBecomeActive:self];

   [self _setSheetOriginAndFront];
   [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidActivate:) withObject:self];
   [_drawers makeObjectsPerformSelector:@selector(parentWindowDidActivate:) withObject:self];

   _isActive=YES;
   if([self canBecomeKeyWindow])
    [self becomeKeyWindow];
   if([self canBecomeMainWindow] && ![self isMainWindow])
    [self becomeMainWindow];

   [_menuView setNeedsDisplay:YES];
   if(displayIfNeeded)
    [self displayIfNeeded];

   [NSApp _windowDidBecomeActive:self];

   [NSApp updateWindows];
}

-(void)platformWindowDeactivated:(CGWindow *)window checkForAppDeactivation:(BOOL)checkForAppDeactivation {
   [NSApp _windowWillBecomeDeactive:self];

   [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidDeactivate:) withObject:self];
   [_drawers makeObjectsPerformSelector:@selector(parentWindowDidDeactivate:) withObject:self];

   _isActive=NO;

   [_menuView setNeedsDisplay:YES];
   [self displayIfNeeded];

   if(checkForAppDeactivation)
    [NSApp performSelector:@selector(_checkForAppActivation)];

   [NSApp _windowDidBecomeDeactive:self];

   [NSApp updateWindows];
}

-(void)platformWindowDeminiaturized:(CGWindow *)window {
   [self _updatePlatformWindowTitle];
   if(_sheetContext!=nil){
    [[_sheetContext sheet] orderWindow:NSWindowAbove relativeTo:[self windowNumber]];
   }
   [self postNotificationName:NSWindowDidDeminiaturizeNotification];
   [NSApp updateWindows];
}

-(void)platformWindowMiniaturized:(CGWindow *)window {
    _isActive=NO;

   [self _updatePlatformWindowTitle];
   if(_sheetContext!=nil){
    [[_sheetContext sheet] orderWindow:NSWindowOut relativeTo:0];
   }

   [self postNotificationName:NSWindowDidMiniaturizeNotification];

   if([self isKeyWindow])
    [self resignKeyWindow];

   if([self isMainWindow])
    [self resignMainWindow];

   [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidMiniaturize:) withObject:self];
   [_drawers makeObjectsPerformSelector:@selector(parentWindowDidMiniaturize:) withObject:self];

   [NSApp updateWindows];
}

-(void)platformWindowExitMove:(CGWindow *)window {
   [self _setSheetOriginAndFront];
   [_childWindows makeObjectsPerformSelector:@selector(_parentWindowDidExitMove:) withObject:self];
   [_drawers makeObjectsPerformSelector:@selector(parentWindowDidExitMove:) withObject:self];
}

-(NSSize)platformWindow:(CGWindow *)window frameSizeWillChange:(NSSize)size {
   if(_resizeIncrements.width!=1 || _resizeIncrements.height!=1){
    NSSize vertical=size;
    NSSize horizontal=size;

    vertical.width=vertical.height*(_resizeIncrements.width/_resizeIncrements.height);
    horizontal.height=horizontal.width*(_resizeIncrements.height/_resizeIncrements.width);
    if(vertical.width*vertical.height>horizontal.width*horizontal.height)
     size=vertical;
    else
     size=horizontal;
   }


   if([_delegate respondsToSelector:@selector(windowWillResize:toSize:)])
    size=[_delegate windowWillResize:self toSize:size];

   return size;
}

-(void)platformWindowWillBeginSizing:(CGWindow *)window {
   [self postNotificationName:NSWindowWillStartLiveResizeNotification];
    _isInLiveResize=YES;
   [_backgroundView viewWillStartLiveResize];
}

-(void)platformWindowDidEndSizing:(CGWindow *)window {
   _isInLiveResize=NO;
   [_backgroundView viewDidEndLiveResize];
   [self postNotificationName:NSWindowDidEndLiveResizeNotification];
}

-(void)platformWindow:(CGWindow *)window needsDisplayInRect:(NSRect)rect {
    [self display];
}

-(void)platformWindowStyleChanged:(CGWindow *)window {
    [self display];
}

-(void)platformWindowWillClose:(CGWindow *)window {
   [self performClose:nil];
}

-(BOOL)platformWindowIgnoreModalMessages:(CGWindow *)window {
   if([NSApp modalWindow]==nil)
    return NO;

   if([self worksWhenModal])
    return NO;

   return ([NSApp modalWindow]==self)?NO:YES;
}

#endif // 0

/*
 * The tracking areas (cursor rects, entered and exited, mouse moved, tool
 * tips) for where the mouse is now. Moved out of the platform window
 * callbacks, which nothing calls: with the new server, sendEvent: calls it
 * for each mouse move (WS4a).
 */
-(BOOL)_updateTrackingAreas {
   NSMutableArray *exited=[NSMutableArray array];
   NSMutableArray *entered=[NSMutableArray array];
   NSMutableArray *moved=[NSMutableArray array];
   NSMutableArray *update=[NSMutableArray array];

   BOOL        cursorIsSet=NO;
   BOOL        raiseToolTipWindow=NO;
   NSUInteger  i,count;
   NSPoint     mousePoint=[self mouseLocationOutsideOfEventStream];


   // This collects only the active ones.
   [self _resetTrackingAreas];

   count=[_trackingAreas count];
   for(i=0;i<count;i++){
    NSTrackingArea *area=[_trackingAreas objectAtIndex:i];
    BOOL mouseWasInside=[area _mouseInside];
    BOOL mouseIsInside=NSPointInRect(mousePoint,[area _rectInWindow]);
    id owner=[area owner];

       if([area _isToolTip]==YES){
           NSToolTipWindow *toolTipWindow=[NSToolTipWindow sharedToolTipWindow];

           if([self isKeyWindow]==NO || [self _sheetContext]!=nil)
               mouseIsInside=NO;

           if(mouseWasInside==YES && mouseIsInside==NO && [toolTipWindow _trackingArea]==area){
               [NSObject cancelPreviousPerformRequestsWithTarget:toolTipWindow selector:@selector(orderFront:) object:nil];
               [toolTipWindow orderOut:nil];
           }
           if(mouseWasInside==NO && mouseIsInside==YES){ // AllowsToolTipsWhenApplicationIsInactive
               // is handled when rebuilding areas.
               [NSObject cancelPreviousPerformRequestsWithTarget:toolTipWindow selector:@selector(orderFront:) object:nil];
               [toolTipWindow orderOut:nil];
               NSString *tooltip = nil;

               if([owner respondsToSelector:@selector(view:stringForToolTip:point:userData:)]==YES) {
                   NSPoint pt =[[area _view] convertPoint:mousePoint fromView:nil];
                   tooltip = [owner view:[area _view] stringForToolTip:area point:pt userData:[area userInfo]];
               } else {
                   tooltip = [owner description];
               }

               if (tooltip) {
                   [toolTipWindow setToolTip:tooltip];

                   // This gives us some protection when ToolTip areas overlap:
                   [toolTipWindow _setTrackingArea:area];

                   raiseToolTipWindow=YES;
               }
           }
       }
       else{ // not ToolTip
     NSTrackingAreaOptions options=[area options];

     // Options by view activation.
     if(options&NSTrackingActiveAlways){
     }
     else if(options&NSTrackingActiveInActiveApp && [NSApp isActive]==NO){
      mouseIsInside=NO;
     }
     else if(options&NSTrackingActiveInKeyWindow && ([self isKeyWindow]==NO || [self _sheetContext]!=nil)){
      mouseIsInside=NO;
     }
     else if(options&NSTrackingActiveWhenFirstResponder && [area _view]!=[self firstResponder]){
      mouseIsInside=NO;
     }
     if(options&NSTrackingInVisibleRect){
      // This does not do hit testing, it just checks if it's inside the visible rect,
      // child views will cause the test to fail if they aren't tracking anything
      NSPoint check=[[area _view] convertPoint:mousePoint fromView:nil];

      if(!NSMouseInRect(check,[[area _view] visibleRect],[[area _view] isFlipped]))
       mouseIsInside=NO;
     }

//FIXME:
     if(options&NSTrackingEnabledDuringMouseDrag){
      // NSLog(@"NSTrackingEnabledDuringMouseDrag handling unimplemented.");
     }

     // Send appropriate events.
     if(options&NSTrackingMouseEnteredAndExited && mouseWasInside==NO && mouseIsInside==YES){
      [entered addObject:area];
     }
     if(options&(NSTrackingMouseEnteredAndExited|NSTrackingCursorUpdate) && mouseWasInside==YES && mouseIsInside==NO){
      [exited addObject:area];
     }
     if(options&NSTrackingMouseMoved && [self acceptsMouseMovedEvents]==YES){
      [moved addObject:area];
     }
     if(options&NSTrackingCursorUpdate && mouseWasInside==NO && mouseIsInside==YES && !(options&NSTrackingActiveAlways)){
      cursorIsSet=YES;
      [update addObject:area];
     }
#if 0
     if(options&NSTrackingCursorUpdate && mouseIsInside==YES)
      cursorIsSet=YES;
#endif
    } // (not) ToolTip

    [area _setMouseInside:mouseIsInside];
   }

// Exited events need to be sent before entered events
// The order of the other two is not specific at this time

   for(NSTrackingArea *check in exited){
    id owner=[check owner];

       if([check options]&NSTrackingCursorUpdate){
           [[NSCursor arrowCursor] set];
       }

    if([owner respondsToSelector:@selector(mouseExited:)]){
      NSEvent *event=[NSEvent enterExitEventWithType:NSMouseExited
                                            location:mousePoint
                                       modifierFlags:[NSEvent modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[self windowNumber]
                                             context:[self graphicsContext]
                                         eventNumber:0 // NSEvent currently ignores this.
                                      trackingNumber:(NSInteger)check
                                            userData:[check userInfo]];
      [owner mouseExited:event];
     }
   }

   for(NSTrackingArea *check in entered){
    id owner=[check owner];

    if([owner respondsToSelector:@selector(mouseEntered:)]){
      NSEvent *event=[NSEvent enterExitEventWithType:NSMouseEntered
                                            location:mousePoint
                                       modifierFlags:[NSEvent modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[self windowNumber]
                                             context:[self graphicsContext]
                                         eventNumber:0 // NSEvent currently ignores this.
                                      trackingNumber:(NSInteger)check
                                            userData:[check userInfo]];
      [owner mouseEntered:event];
     }
   }

   for(NSTrackingArea *check in moved){
    id owner=[check owner];

    if([owner respondsToSelector:@selector(mouseMoved:)]){
      NSEvent *event=[NSEvent mouseEventWithType:NSMouseMoved
                                        location:mousePoint
                                   modifierFlags:[NSEvent modifierFlags]
                                       timestamp:[NSDate timeIntervalSinceReferenceDate]
                                    windowNumber:[self windowNumber]
                                         context:[self graphicsContext]
                                     eventNumber:0 // NSEvent currently ignores this.
                                      clickCount:0
                                        pressure:0.];
      [owner mouseMoved:event];
     }
   }

   for(NSTrackingArea *check in update){
    id owner=[check owner];

    if([owner respondsToSelector:@selector(cursorUpdate:)]){
      NSEvent *event=[NSEvent enterExitEventWithType:NSCursorUpdate
                                            location:mousePoint
                                       modifierFlags:[NSEvent modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[self windowNumber]
                                             context:[self graphicsContext]
                                         eventNumber:0 // NSEvent currently ignores this.
                                      trackingNumber:(NSInteger)check
                                            userData:[check userInfo]];
      [owner cursorUpdate:event];
     }
   }

   if(raiseToolTipWindow==YES){
    NSTimeInterval delay=((NSTimeInterval)[[NSUserDefaults standardUserDefaults] integerForKey:@"NSInitialToolTipDelay"])/1000.;

    if(delay<=0.)
     delay=2.;
    [[NSToolTipWindow sharedToolTipWindow] performSelector:@selector(orderFront:) withObject:nil afterDelay:delay];
   }

   if(!cursorIsSet){
    NSPoint check=[_contentView convertPoint:mousePoint fromView:nil];

    // we set the cursor to the current cursor if it is inside the content area, this will need to be changed
    // if we're drawing out own window frame
    if(NSMouseInRect(check,[_contentView bounds],[_contentView isFlipped])){
     if([NSCursor currentCursor]==nil)
         [[NSCursor arrowCursor] set];
    else
        [[NSCursor currentCursor] set];
     cursorIsSet=YES;
    }
   }

   return cursorIsSet;
}

-(NSUndoManager *)undoManager {
    if ([_delegate respondsToSelector:@selector(windowWillReturnUndoManager:)])
        return [_delegate windowWillReturnUndoManager:self];

    // If this window is associated with a document, return the document's undo manager.
    // Apple's documentation says this is the delegate's responsibility, but that's not how it works in real life.
    if (_undoManager == nil) {
        _undoManager = [[[[self windowController] document] undoManager] retain];
    }

    //  If the delegate does not implement this method, the NSWindow creates an NSUndoManager for the window and all its views. -- seems like some duplication vs. NSDocument, but oh well..
    if (_undoManager == nil){
        _undoManager = [[NSUndoManager alloc] init];
        [_undoManager setRunLoopModes:[NSArray arrayWithObjects:NSDefaultRunLoopMode, NSModalPanelRunLoopMode, NSEventTrackingRunLoopMode,nil]];
    }

    return _undoManager;
}

-(void)undo:sender {
    [[self undoManager] undo];
}

-(void)redo:sender {
    [[self undoManager] redo];
}

-(BOOL)validateMenuItem:(NSMenuItem *)item {
    if ([item action] == @selector(undo:))
        return [[self undoManager] canUndo];
    if ([item action] == @selector(redo:))
        return [[self undoManager] canRedo];

    return YES;
}

-(void)_attachDrawer:(NSDrawer *)drawer {
    if (_drawers == nil)
        _drawers = [[NSMutableArray alloc] init];

    [_drawers addObject:drawer];
}

-(void)_detachDrawer:(NSDrawer *)drawer {
    [_drawers removeObject:drawer];
}

-(NSView *)_backgroundView {
    return _backgroundView;
}

-(void)dirtyRect:(NSRect)rect
{
}

void CGNativeBorderFrameWidthsForStyle(unsigned styleMask,CGFloat *top,CGFloat *left,
                                       CGFloat *bottom,CGFloat *right);

/* A rectangle of the window (bottom left origin) in the server's window
   coordinates (top left origin), clipped to the window. */
static BOOL slsWindowRect(NSRect r, NSSize size, SLSIntRect *out) {
    r = NSIntersectionRect(r, NSMakeRect(0, 0, size.width, size.height));
    if(NSIsEmptyRect(r))
        return NO;
    out->x = (int32_t)floor(r.origin.x);
    out->y = (int32_t)floor(size.height - NSMaxY(r));
    out->width = (int32_t)ceil(r.size.width);
    out->height = (int32_t)ceil(r.size.height);
    return YES;
}

/* The views that take their own clicks, as excluded rectangles: a view that
   cannot move the window is excluded whole; one that can lets through what
   its subviews do not take. */
static void slsCollectExcluded(NSView *view, NSSize size, SLSIntRect *rects, uint32_t *count) {
    for(NSView *sub in [view subviews]) {
        if([sub isHidden] || *count >= kSLSMaxRegionRects)
            continue;
        if(![sub mouseDownCanMoveWindow]) {
            if(slsWindowRect([sub convertRect:[sub bounds] toView:nil], size, &rects[*count]))
                (*count)++;
        } else
            slsCollectExcluded(sub, size, rects, count);
    }
}

/* The drag regions the server moves the window from by itself: the title
   bar without its buttons, and the background of a window movable by it.
   Sent when they change. */
-(void)_syncSkyLightDragRegion {
    SLSIntRect drag[kSLSMaxRegionRects], exclude[kSLSMaxRegionRects];
    uint32_t dragCount = 0, excludeCount = 0;
    NSSize size = _frame.size;
    CGFloat top, left, bottom, right;

    CGNativeBorderFrameWidthsForStyle(_styleMask, &top, &left, &bottom, &right);
    /* A full-size content view's own bar may be taller than the title
       bar (_setTitlebarHeight:): the window moves by all of it. */
    CGFloat bar = top;
    if((_styleMask & NSWindowStyleMaskFullSizeContentView) && _titlebarHeight > bar)
        bar = _titlebarHeight;
    if([self isMovable]) {
        if([self isMovableByWindowBackground]) {
            if(slsWindowRect(NSMakeRect(0, 0, size.width, size.height), size, &drag[dragCount]))
                dragCount++;
            slsCollectExcluded(_contentView, size, exclude, &excludeCount);
        } else if(top > 0 &&
            slsWindowRect(NSMakeRect(0, size.height - bar, size.width, bar), size, &drag[dragCount])) {
            dragCount++;
            /* A full-size content view's controls in the title bar take
               their own clicks; the rest of the bar moves the window. */
            if(_styleMask & NSWindowStyleMaskFullSizeContentView)
                slsCollectExcluded(_contentView, size, exclude, &excludeCount);
        }
        if(dragCount > 0 && [_backgroundView isKindOfClass:[NSThemeFrame class]] &&
           excludeCount < kSLSMaxRegionRects &&
           slsWindowRect([(NSThemeFrame *)_backgroundView _windowControlsRect], size, &exclude[excludeCount]))
            excludeCount++;
    }

    NSMutableData *region = [NSMutableData dataWithBytes:drag length:dragCount * sizeof(SLSIntRect)];
    [region appendBytes:&dragCount length:sizeof(dragCount)];
    [region appendBytes:exclude length:excludeCount * sizeof(SLSIntRect)];
    NSNumber *key = [NSNumber numberWithInt:_number];
    @synchronized([NSWindow class]) {
        if([[slsDragRegions objectForKey:key] isEqualToData:region])
            return;
        if(slsDragRegions == nil)
            slsDragRegions = [[NSMutableDictionary alloc] init];
        [slsDragRegions setObject:region forKey:key];
    }
    SLSWindowSetDragRegion(slsWindowFor(_number), drag, dragCount, exclude, excludeCount);
}

-(void)requestMove:(NSEvent *)event {
    [self postNotificationName:NSWindowWillMoveNotification];
    /* The new window server moves the window with the pointer by itself
     * and tells us where it went (NSWindowMovedEventType). */
    SLSWindowStartMove(slsWindowFor(_number));
}

-(void)performWindowDragWithEvent:(NSEvent *)event {
    [self requestMove:event];
}

/* The new window server moved the window: its frame's top left is at x, y
 * (global, from the top left). */
-(void)_skyLightMovedToX:(NSInteger)x y:(NSInteger)y {
    CGFloat height = CGDisplayBounds(CGMainDisplayID()).size.height;
    _frame.origin = NSMakePoint(x, height - y - _frame.size.height);
    /* The server has this frame: it is not sent back. */
    slsFrameIsKnown(_number, slsFrame(_frame));
    [self postNotificationName:NSWindowDidMoveNotification];
}

-(void)requestResize:(NSEvent *)event {
}

-(void)addEntriesToDeviceDictionary:(NSDictionary *)entries {
    [_deviceDictionary addEntriesFromDictionary:entries];
}

-(BOOL)setProperty:(NSString *)property toValue:(NSString *)value {
    // FIXME: implement
    return YES;
}


-(NSPoint)transformPoint:(NSPoint)pos {
    return pos;
}

-(NSRect)transformFrame:(NSRect)frame {
    return frame;
}


-(uint32_t)_skyLightWindowID {
    return slsWindowFor(_number);
}

-(BOOL)_updateWSState {
    SLSWindowID window = slsWindowFor(_number);
    NSNumber *key = [NSNumber numberWithInt:_number];

    /* Only a frame the server does not have: a window the server moved
       while its app was busy keeps its new place, even if events
       queued before the move (a click that orders it front) are
       handled first, with the old frame. */
    if(!slsFrameIsKnown(_number, slsFrame(_frame)))
        SLSSetWindowFrame(window, slsFrame(_frame));
    /* This AppKit's levels are CGWindowLevelKeys; the server takes levels. */
    SLSSetWindowLevel(window, CGWindowLevelForKey((CGWindowLevelKey)_level));
    /* Ordering in is orderWindow:relativeTo:'s; this only takes out. */
    if((_isVisible == NO || _isMiniaturized == YES) && [slsShown containsObject:key]) {
        SLSOrderWindow(window, kSLSOrderOut, 0);
        [slsShown removeObject:key];
    }
    return YES;
}


#pragma mark View controllers and sheets

+(instancetype)windowWithContentViewController:(NSViewController *)contentViewController {
   NSView   *view=[contentViewController view];
   NSSize    size=[contentViewController preferredContentSize];
   NSWindow *window;

   if(size.width<=0 || size.height<=0)
    size=[view frame].size;
   window=[[[self alloc] initWithContentRect:NSMakeRect(0,0,size.width,size.height)
      styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
      backing:NSBackingStoreBuffered defer:NO] autorelease];
   [window setContentViewController:contentViewController];
   if([contentViewController title]!=nil)
    [window setTitle:[contentViewController title]];
   [window setReleasedWhenClosed:NO];
   return window;
}

-(NSAppearance *)appearance {
   return objc_getAssociatedObject(self,&windowAppearanceKey);
}

-(void)setAppearance:(NSAppearance *)appearance {
   objc_setAssociatedObject(self,&windowAppearanceKey,appearance,OBJC_ASSOCIATION_RETAIN);
   [[self contentView] setNeedsDisplay:YES];
}

-(NSAppearance *)effectiveAppearance {
   NSAppearance *appearance=[self appearance];

   return (appearance!=nil)?appearance:[NSApp effectiveAppearance];
}

-(NSViewController *)contentViewController {
   return _contentViewController;
}

/* The window shows its content view controller's title, and follows it
   (TextMate's preferences name the pane shown). */
-(void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
   if(context==&contentTitleContext){
    NSString *title=[_contentViewController title];

    if(title!=nil)
     [self setTitle:title];
    return;
   }
   [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

/* The controller's view becomes the content view; the window keeps its
   frame unless it has no content yet. */
-(void)setContentViewController:(NSViewController *)controller {
   if(controller==_contentViewController)
    return;
   [controller retain];
   [_contentViewController removeObserver:self forKeyPath:@"title" context:&contentTitleContext];
   [_contentViewController release];
   _contentViewController=controller;
   [controller addObserver:self forKeyPath:@"title" options:0 context:&contentTitleContext];
   if([controller title]!=nil)
    [self setTitle:[controller title]];
   if(controller!=nil){
    NSView *view=[controller view];
    NSRect  content=[self contentRectForFrameRect:[self frame]];

    [self setContentView:view];
    if(NSIsEmptyRect(content))
     [self setContentSize:[view frame].size];
    [view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
   }
}


-(void)beginSheet:(NSWindow *)sheetWindow completionHandler:(void (^)(NSModalResponse returnCode))handler {
   NSSheetCompletion *completion=[[NSSheetCompletion alloc] init];

   completion->_handler=[handler copy];
   sheetWindow->_sheetParent=self;
   [NSApp beginSheet:sheetWindow modalForWindow:self modalDelegate:completion didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}

-(void)beginCriticalSheet:(NSWindow *)sheetWindow completionHandler:(void (^)(NSModalResponse returnCode))handler {
   [self beginSheet:sheetWindow completionHandler:handler];
}

-(void)endSheet:(NSWindow *)sheetWindow {
   [self endSheet:sheetWindow returnCode:NSModalResponseStop];
}

-(void)endSheet:(NSWindow *)sheetWindow returnCode:(NSModalResponse)returnCode {
   [NSApp endSheet:sheetWindow returnCode:returnCode];
}

-(NSArray *)sheets {
   NSWindow *sheet=[self attachedSheet];

   return (sheet!=nil)?[NSArray arrayWithObject:sheet]:[NSArray array];
}

-(NSWindow *)sheetParent {
   return _sheetParent;
}

-(void)_setSheetParent:(NSWindow *)window {
   _sheetParent=window;
}

-(BOOL)isSheet {
   return _sheetParent!=nil || ([self styleMask]&NSWindowStyleMaskDocModalWindow)!=0;
}


-(NSRect)convertRectToScreen:(NSRect)rect {
   NSRect frame=[self frame];
   return NSOffsetRect(rect,frame.origin.x,frame.origin.y);
}

-(NSRect)convertRectFromScreen:(NSRect)rect {
   NSRect frame=[self frame];
   return NSOffsetRect(rect,-frame.origin.x,-frame.origin.y);
}

-(NSPoint)convertPointToScreen:(NSPoint)point {
   return [self convertBaseToScreen:point];
}

-(NSPoint)convertPointFromScreen:(NSPoint)point {
   return [self convertScreenToBase:point];
}

static char fullScreenFrameKey;

/* Full screen: the window takes its screen's visible frame, and gets its
   frame back when toggled again (there are no full screen spaces). */
-(void)toggleFullScreen:(id)sender {
   NSValue *saved=objc_getAssociatedObject(self,&fullScreenFrameKey);

   if(saved!=nil){
    objc_setAssociatedObject(self,&fullScreenFrameKey,nil,OBJC_ASSOCIATION_RETAIN);
    [self setFrame:[saved rectValue] display:YES];
   }
   else {
    objc_setAssociatedObject(self,&fullScreenFrameKey,[NSValue valueWithRect:[self frame]],OBJC_ASSOCIATION_RETAIN);
    [self setFrame:[[self screen] ?: [NSScreen mainScreen] visibleFrame] display:YES];
   }
}

static BOOL sAllowsAutomaticWindowTabbing=YES;

+(BOOL)allowsAutomaticWindowTabbing {
   return sAllowsAutomaticWindowTabbing;
}

+(void)setAllowsAutomaticWindowTabbing:(BOOL)value {
   sAllowsAutomaticWindowTabbing=value;
}

static char animationBehaviorKey;
-(NSWindowAnimationBehavior)animationBehavior {
   id value=objc_getAssociatedObject(self,&animationBehaviorKey);
   return (value!=nil)?(NSWindowAnimationBehavior)[value integerValue]:0;
}
-(void)setAnimationBehavior:(NSWindowAnimationBehavior)value {
   objc_setAssociatedObject(self,&animationBehaviorKey,[NSNumber numberWithInteger:(NSInteger)value],OBJC_ASSOCIATION_RETAIN);
}

/* There is no state restoration yet: nothing to save. */
-(void)invalidateRestorableState {
}

@end

void CGNativeBorderFrameWidthsForStyle(unsigned styleMask,CGFloat *top,CGFloat *left,
                                       CGFloat *bottom,CGFloat *right);

/* The content rectangle is the frame without its borders, each side's
   (it used to subtract one border a side, so the content reached 3
   points over the frame's top and 2 past its right). A full-size content
   view covers the title bar: no top border then. */
CGRect CGInsetRectForNativeWindowBorder(CGRect frame,unsigned styleMask) {
    CGFloat top, left, bottom, right;
    CGNativeBorderFrameWidthsForStyle(styleMask, &top, &left, &bottom, &right);
    if(styleMask & NSWindowStyleMaskFullSizeContentView)
        top=0;
    frame.origin.x += left;
    frame.origin.y += bottom;
    frame.size.width -= left + right;
    frame.size.height -= top + bottom;
    return frame;
}

CGRect CGOutsetRectForNativeWindowBorder(CGRect frame,unsigned styleMask) {
    CGFloat top, left, bottom, right;
    CGNativeBorderFrameWidthsForStyle(styleMask, &top, &left, &bottom, &right);
    if(styleMask & NSWindowStyleMaskFullSizeContentView)
        top=0;
    frame.origin.x -= left;
    frame.origin.y -= bottom;
    frame.size.width += left + right;
    frame.size.height += top + bottom;
    return frame;
}

void CGNativeBorderFrameWidthsForStyle(unsigned styleMask,CGFloat *top,CGFloat *left,
                                       CGFloat *bottom,CGFloat *right)
{
    /* A sheet hangs under its window's title bar: no title bar of its own. */
    if(styleMask & NSDocModalWindowMask) {
        *top=0;
        *left=1;
        *bottom=1;
        *right=1;
        return;
    }
    switch(styleMask & 0x0FFF) {
        case NSBorderlessWindowMask:
            *top=0;
            *left=0;
            *bottom=0;
            *right=0;
            break;
        // FIXME: tool window style?
        default:
            /* As the reference platform: the title bar, no other border. */
            *top=22;
            *left=0;
            *bottom=0;
            *right=0;
    }
}

@implementation NSWindow (MinkarWindowExtras)
-(instancetype)animator { return self; }
-(NSDictionary *)animations { return [NSDictionary dictionary]; }
-(void)setAnimations:(NSDictionary *)animations { }
-animationForKey:(NSString *)key { return nil; }
+defaultAnimationForKey:(NSString *)key { return nil; }
-(CGFloat)backingScaleFactor {
   NSScreen *screen=[self screen]?[self screen]:[NSScreen mainScreen];
   CGFloat scale=[screen backingScaleFactor];
   return scale>0?scale:1;
}
@end
