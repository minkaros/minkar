/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSWindow.h>

@class CGWindow;

// style flag for private windows (that NSApp doesn't have to know about)
#define NSAppKitPrivateWindow 0x8000000

// Window numbers in a CGWindowListCopyWindowInfo result, front to back;
// only this process's windows unless allApps is set.
APPKIT_EXPORT NSArray *_NSWindowNumbersFromWindowInfo(CFArrayRef info,BOOL allApps);

@interface NSWindow (NSWindow_private)
/* The title bar's close, minimize and zoom controls left out entirely (an
 * alert's), not only shown faint for a style that lacks them. */
- (void)_setHidesWindowControls:(BOOL)hides;
- (BOOL)_hidesWindowControls;
/* The new window server's ID for this window (0 with the legacy one). */
- (uint32_t)_skyLightWindowID;
/* Notes a rect drawn (in the window's base coordinates), sent to the
 * window server at the next flush. */
- (void)_addDamageRect:(NSRect)rect;
/* Entries for the window's device description (an image cache's context). */
- (void)addEntriesToDeviceDictionary:(NSDictionary *)entries;
- (CGContextRef)cgContext;
- (BOOL)_isActive;
- (void)_setVisible:(BOOL)visible;
- (NSArray *)_draggedTypes;
- (void)_addCursorRect:(NSRect)rect cursor:(NSCursor *)cursor view:(NSView *)view;
- (void)_removeCursorRect:(NSRect)rect cursor:(NSCursor *)cursor view:(NSView *)view;
- (void)_discardCursorRectsForView:(NSView *)view;
- (void)_discardTrackingRectsForView:(NSView *)view toolTipsOnly:(BOOL)toolTipsOnly;
- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect view:(NSView *)view flipped:(BOOL)flipped owner:owner userData:(void *)userData assumeInside:(BOOL)assumeInside isToolTip:(BOOL)isToolTip;
- (void)_removeTrackingRect:(NSTrackingRectTag)tag;
- (void)_removeAllToolTips;
- (void)_attachSheetContextOrderFrontAndAnimate:(NSSheetContext *)sheetContext;
- (void)_setSheetParent:(NSWindow *)window;
- (void)_setSheetContext:(NSSheetContext *)sheetContext;
- (NSSheetContext *)_sheetContext;
- (void)_detachSheetContextAnimateAndOrderOut;
- (void)_attachDrawer:(NSDrawer *)drawer;
- (void)_detachDrawer:(NSDrawer *)drawer;
- (NSView *)_backgroundView;
- (BOOL)_updateTrackingAreas;
- (void)_invalidateTrackingAreas;
- (void)_setFrameAutosaveNameNoIO:(NSString *)name;
- (NSWindowAnimationContext *)_animationContext;
- (void)undo:sender;
- (void)redo:sender;
- (BOOL)validateMenuItem:(NSMenuItem *)item;
- (void)dirtyRect:(NSRect)rect;
/* Minkar: the height of the bar a full-size content view draws at its
   top (NSWindowStyleMaskFullSizeContentView), where the window's buttons
   are centered; 0 is the standard title bar's. Until unified toolbars and
   titlebar accessories exist. */
-(CGFloat)_titlebarHeight;
-(void)_setTitlebarHeight:(CGFloat)height;
@end
