/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Window tabs (Atlas.md, W0; API 22 and 23). Windows with the same tabbing
 * identifier can be the tabs of one window: a tab group, of which one
 * window, the selected one, is on the screen; the others are ordered out
 * and take its frame when they are chosen. Ordering a window of a group in
 * (makeKeyAndOrderFront:, orderFront:) chooses it; closing one chooses its
 * neighbour. Each window has a group of its own until it joins another
 * (addTabbedWindow:ordered:, mergeAllWindows:); moveTabToNewWindow: takes
 * one out again. A group holds its windows without retaining them (a
 * window retains its group), and loses each as it closes.
 */

#import <AppKit/NSWindowTabGroup.h>
#import <AppKit/NSWindowTab.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSTrackingArea.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSParagraphStyle.h>
#import <AppKit/NSBezierPath.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

static char kTabGroupKey, kTabKey, kTabbingModeKey, kTabbingIdentifierKey, kTabBarKey;

BOOL _NSSystemAppearanceIsDark(void);
static BOOL canBeTab(NSWindow *window);

/* The tab bar's height, under the title bar (and the toolbar). */
extern const CGFloat NSWindowTabBarHeight;
const CGFloat NSWindowTabBarHeight = 25;
static BOOL allowsAutomaticWindowTabbing = YES;
/* While the group itself orders its windows in and out. */
static BOOL switchingTabs;
/* The window whose + is being answered: the next window ordered in is its
   new tab. */
static NSWindow *newTabParent;

@interface NSWindow (NSWindowTabbingPrivate)
-(void)_tabGroupDidChange;
-(NSWindowTabGroup *)_existingTabGroup;
@end

@interface NSWindowTab ()
-(void)_setWindow:(NSWindow *)window;
@end

@implementation NSWindowTab {
   NSWindow           *_window;        /* not retained: the window has its tab */
   NSString           *_title;
   NSAttributedString *_attributedTitle;
   NSString           *_toolTip;
   NSView             *_accessoryView;
}

-(void)_setWindow:(NSWindow *)window {
   _window=window;
}

-(void)dealloc {
   [_title release];
   [_attributedTitle release];
   [_toolTip release];
   [_accessoryView release];
   [super dealloc];
}

/* The window's title unless one was given. */
-(NSString *)title {
   return _title ?: [_window title];
}

-(void)setTitle:(NSString *)title {
   title=[title copy];
   [_title release];
   _title=title;
}

-(NSAttributedString *)attributedTitle {
   return _attributedTitle;
}

-(void)setAttributedTitle:(NSAttributedString *)title {
   title=[title copy];
   [_attributedTitle release];
   _attributedTitle=title;
}

-(NSString *)toolTip {
   return _toolTip;
}

-(void)setToolTip:(NSString *)toolTip {
   toolTip=[toolTip copy];
   [_toolTip release];
   _toolTip=toolTip;
}

-(NSView *)accessoryView {
   return _accessoryView;
}

-(void)setAccessoryView:(NSView *)view {
   [view retain];
   [_accessoryView release];
   _accessoryView=view;
}

@end

@implementation NSWindowTabGroup {
   NSString       *_identifier;
   NSMutableArray *_windows;          /* not retaining */
   NSWindow       *_selected;
   BOOL            _overviewVisible;
   BOOL            _tabBarShown;      /* asked for (toggleTabBar:) */
}

-(instancetype)initWithIdentifier:(NSString *)identifier {
   if((self=[super init])!=nil){
    _identifier=[identifier copy];
    _windows=(NSMutableArray *)CFArrayCreateMutable(NULL,0,NULL);
   }
   return self;
}

-(void)dealloc {
   [_identifier release];
   [_windows release];
   [super dealloc];
}

-(NSString *)identifier {
   return _identifier;
}

-(NSArray *)windows {
   return [[_windows copy] autorelease];
}

-(BOOL)isOverviewVisible {
   return _overviewVisible;
}

-(void)setOverviewVisible:(BOOL)visible {
   _overviewVisible=visible;
}

/* Shown with more than one tab, or when asked for. */
-(BOOL)isTabBarVisible {
   return [_windows count]>1 || _tabBarShown;
}

-(void)_setTabBarShown:(BOOL)shown {
   _tabBarShown=shown;
}

-(BOOL)_tabBarShown {
   return _tabBarShown;
}

-(NSWindow *)selectedWindow {
   return _selected;
}

/* The window chosen: it takes the selected one's frame and its place on
   the screen, the selected one goes out. */
-(void)setSelectedWindow:(NSWindow *)window {
   NSWindow *old=_selected;

   if(window==old || (window!=nil && ![_windows containsObject:window]))
    return;
   _selected=window;
   if(window==nil || old==nil || ![old isVisible])
    return;
   switchingTabs=YES;
   [window setFrame:[old frame] display:NO];
   [window orderWindow:NSWindowAbove relativeTo:[old windowNumber]];
   [old orderWindow:NSWindowOut relativeTo:0];
   switchingTabs=NO;
}

-(void)_groupChanged {
   for(NSWindow *window in _windows)
    [window _tabGroupDidChange];
}

-(void)insertWindow:(NSWindow *)window atIndex:(NSInteger)index {
   NSWindowTabGroup *previous=objc_getAssociatedObject(window,&kTabGroupKey);

   if(previous==self)
    return;
   [previous removeWindow:window];
   index=MAX(0,MIN(index,(NSInteger)[_windows count]));
   [_windows insertObject:window atIndex:(NSUInteger)index];
   objc_setAssociatedObject(window,&kTabGroupKey,self,OBJC_ASSOCIATION_RETAIN);
   if(_selected==nil)
    _selected=window;
   /* The tab bar first (the windows grow by it), then a tab joining takes
      the selected one's frame, and goes behind it if it is shown. */
   [self _groupChanged];
   if(window!=_selected){
    switchingTabs=YES;
    [window setFrame:[_selected frame] display:NO];
    if([window isVisible])
     [window orderWindow:NSWindowOut relativeTo:0];
    switchingTabs=NO;
   }
}

/* A tab to another place in the bar. */
-(void)_moveWindow:(NSWindow *)window toIndex:(NSInteger)index {
   NSUInteger from=[_windows indexOfObjectIdenticalTo:window];

   if(from==NSNotFound)
    return;
   [_windows removeObjectAtIndex:from];
   [_windows insertObject:window atIndex:(NSUInteger)MAX(0,MIN(index,(NSInteger)[_windows count]))];
   [self _groupChanged];
}

-(void)addWindow:(NSWindow *)window {
   [self insertWindow:window atIndex:[_windows count]];
}

/* A window leaves: a group of its own again; when it was the selected
   one, its neighbour is chosen, in its place. */
-(void)removeWindow:(NSWindow *)window {
   NSUInteger index=[_windows indexOfObjectIdenticalTo:window];

   if(index==NSNotFound)
    return;
   [[self retain] autorelease];
   [_windows removeObjectAtIndex:index];
   if(_selected==window){
    _selected=nil;
    if([_windows count]>0){
     NSWindow *next=[_windows objectAtIndex:MIN(index,[_windows count]-1)];
     _selected=next;
     if([window isVisible]){
      switchingTabs=YES;
      [next setFrame:[window frame] display:NO];
      [next orderWindow:NSWindowAbove relativeTo:[window windowNumber]];
      switchingTabs=NO;
      [next makeKeyWindow];
      if([next canBecomeMainWindow])
       [next makeMainWindow];
     }
    }
   }
   objc_setAssociatedObject(window,&kTabGroupKey,nil,OBJC_ASSOCIATION_RETAIN);
   [window _tabGroupDidChange];
   [self _groupChanged];
}

@end

/* A window's tab bar: its group's tabs, of one width, the selected one
   in the title bar's grey and the others darker; a tab's close button at
   its left, under the pointer; + at the right when something in the
   responder chain makes a window for a new tab (newWindowForTab:). A
   click chooses a tab. */
@interface NSWindowTabBarView : NSView {
   NSInteger       _hover;
   NSTrackingArea *_tracking;
}
@end

@implementation NSWindowTabBarView

enum { kTabPlusWidth = 28, kTabCloseSize = 16 };

-(instancetype)initWithFrame:(NSRect)frame {
   if((self=[super initWithFrame:frame])!=nil)
    _hover=-1;
   return self;
}

-(void)dealloc {
   [_tracking release];
   [super dealloc];
}

-(BOOL)isOpaque {
   return YES;
}

-(BOOL)mouseDownCanMoveWindow {
   return NO;
}

-(NSArray *)_windows {
   NSWindowTabGroup *group=[[self window] _existingTabGroup];

   return group!=nil ? [group windows] : ([self window]!=nil ? [NSArray arrayWithObject:[self window]] : [NSArray array]);
}

-(BOOL)_hasPlus {
   return [NSApp targetForAction:@selector(newWindowForTab:) to:nil from:self]!=nil;
}

/* Tab i's rect; the tabs share the bar's width but +'s. */
-(NSRect)_tabRect:(NSUInteger)i count:(NSUInteger)count {
   NSRect  b=[self bounds];
   CGFloat width=NSWidth(b)-([self _hasPlus] ? kTabPlusWidth : 0);
   CGFloat x0=floor(width*i/count),x1=floor(width*(i+1)/count);

   return NSMakeRect(x0,0,x1-x0,NSHeight(b));
}

-(NSRect)_closeRectInTab:(NSRect)tab {
   return NSMakeRect(NSMinX(tab)+6,floor(NSMidY(tab)-kTabCloseSize/2),kTabCloseSize,kTabCloseSize);
}

-(NSRect)_plusRect {
   NSRect b=[self bounds];

   return NSMakeRect(NSMaxX(b)-kTabPlusWidth,0,kTabPlusWidth,NSHeight(b));
}

-(void)updateTrackingAreas {
   if(_tracking!=nil){
    [self removeTrackingArea:_tracking];
    [_tracking release];
   }
   _tracking=[[NSTrackingArea alloc] initWithRect:[self bounds]
      options:NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveAlways|NSTrackingInVisibleRect
      owner:self userInfo:nil];
   [self addTrackingArea:_tracking];
   [super updateTrackingAreas];
}

-(NSInteger)_tabAtPoint:(NSPoint)point {
   NSArray   *windows=[self _windows];
   NSUInteger count=[windows count];

   for(NSUInteger i=0;i<count;i++)
    if(NSPointInRect(point,[self _tabRect:i count:count]))
     return (NSInteger)i;
   return -1;
}

-(void)mouseMoved:(NSEvent *)event {
   NSInteger hover=[self _tabAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

   if(hover!=_hover){
    _hover=hover;
    [self setNeedsDisplay:YES];
   }
}

-(void)mouseEntered:(NSEvent *)event {
   [self mouseMoved:event];
}

-(void)mouseExited:(NSEvent *)event {
   _hover=-1;
   [self setNeedsDisplay:YES];
}

-(void)mouseDown:(NSEvent *)event {
   NSPoint   point=[self convertPoint:[event locationInWindow] fromView:nil];
   NSArray  *windows=[self _windows];
   NSInteger i=[self _tabAtPoint:point];

   if([self _hasPlus] && NSPointInRect(point,[self _plusRect])){
    newTabParent=[self window];
    [NSApp sendAction:@selector(newWindowForTab:) to:nil from:[self window]];
    newTabParent=nil;
    return;
   }
   if(i<0)
    return;
   NSWindow *window=[windows objectAtIndex:(NSUInteger)i];
   if([windows count]>1 && NSPointInRect(point,[self _closeRectInTab:[self _tabRect:(NSUInteger)i count:[windows count]]])){
    [window performClose:self];
    return;
   }
   [window makeKeyAndOrderFront:nil];
   [self _dragTab:window from:event];
}

/* The shown window of this app (not the dragged tab's group) whose title
   bar or tab bar is at a point on the screen. */
-(NSWindow *)_windowToJoinAt:(NSPoint)screenPoint excluding:(NSWindowTabGroup *)group {
   for(NSWindow *window in [NSApp windows]){
    NSRect  frame=[window frame];
    CGFloat bar=22+(objc_getAssociatedObject(window,&kTabBarKey)!=nil ? NSWindowTabBarHeight : 0);

    if(![window isVisible] || [window _existingTabGroup]==group || !canBeTab(window) ||
       ![[window tabbingIdentifier] isEqualToString:[[self window] tabbingIdentifier]])
     continue;
    if(NSPointInRect(screenPoint,NSMakeRect(NSMinX(frame),NSMaxY(frame)-bar,NSWidth(frame),bar)))
     return window;
   }
   return nil;
}

/* A tab dragged (once the pointer has moved a few points): let go in the
   bar, it takes the place there; over another window's title bar or tab
   bar, it joins that window's tabs; elsewhere, it becomes a window of its
   own, its title bar where it was let go. */
-(void)_dragTab:(NSWindow *)window from:(NSEvent *)down {
   NSWindow *barWindow=[self window];
   NSPoint   start=[down locationInWindow],last=start;
   BOOL      dragged=NO;

   while(YES){
    NSEvent *event=[NSApp nextEventMatchingMask:NSLeftMouseDraggedMask|NSLeftMouseUpMask
       untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];

    last=[event locationInWindow];
    if(fabs(last.x-start.x)>4 || fabs(last.y-start.y)>4)
     dragged=YES;
    if([event type]==NSLeftMouseUp)
     break;
   }
   if(!dragged)
    return;

   NSWindowTabGroup *group=[window _existingTabGroup];
   NSPoint           inBar=[self convertPoint:last fromView:nil];
   NSPoint           onScreen=[barWindow convertPointToScreen:last];
   NSWindow         *target;

   if(NSPointInRect(inBar,NSInsetRect([self bounds],0,-8))){
    NSInteger to=[self _tabAtPoint:NSMakePoint(inBar.x,NSMidY([self bounds]))];

    if(to>=0 && group!=nil && [[group windows] objectAtIndex:(NSUInteger)to]!=window){
     [group _moveWindow:window toIndex:to];
     [window makeKeyAndOrderFront:nil];
    }
    return;
   }
   if((target=[self _windowToJoinAt:onScreen excluding:group])!=nil){
    [[window _existingTabGroup] removeWindow:window];
    [target addTabbedWindow:window ordered:NSWindowAbove];
    [window makeKeyAndOrderFront:nil];
    return;
   }
   if([[group windows] count]<2)
    return;
   [group removeWindow:window];
   NSRect frame=[window frame];
   frame.origin.x=onScreen.x-NSWidth(frame)/2;
   frame.origin.y=onScreen.y-NSHeight(frame)+11;
   [window setFrame:frame display:NO];
   [window makeKeyAndOrderFront:nil];
}

static void fillGray(CGFloat gray,NSRect rect) {
   [[NSColor colorWithGenericGamma22White:gray alpha:1] setFill];
   NSRectFill(rect);
}

-(void)drawRect:(NSRect)rect {
   BOOL       dark=_NSSystemAppearanceIsDark();
   NSArray   *windows=[self _windows];
   NSUInteger count=[windows count];
   NSWindow  *selected=[[[self window] _existingTabGroup] selectedWindow] ?: [self window];
   CGFloat    frameGray=dark ? 0.176 : 0.8, barGray=dark ? 0.12 : 0.70, lineGray=dark ? 0.08 : 0.60;
   NSMutableParagraphStyle *style=[[[NSMutableParagraphStyle alloc] init] autorelease];

   [style setAlignment:NSCenterTextAlignment];
   [style setLineBreakMode:NSLineBreakByTruncatingTail];
   fillGray(barGray,[self bounds]);
   for(NSUInteger i=0;i<count;i++){
    NSWindow *window=[windows objectAtIndex:i];
    NSRect    tab=[self _tabRect:i count:count];
    BOOL      chosen=(window==selected);

    if(chosen)
     fillGray(frameGray,tab);
    else if((NSInteger)i==_hover)
     fillGray(dark ? 0.15 : 0.74,tab);
    if(i>0)
     fillGray(lineGray,NSMakeRect(NSMinX(tab),0,1,NSHeight(tab)));
    NSDictionary *attributes=@{
       NSFontAttributeName:[NSFont systemFontOfSize:12],
       NSForegroundColorAttributeName:[NSColor colorWithGenericGamma22White:(dark ? (chosen ? 0.92 : 0.60) : (chosen ? 0.10 : 0.35)) alpha:1],
       NSParagraphStyleAttributeName:style };
    NSString *title=[[window tab] title] ?: @"";
    NSSize    size=[title sizeWithAttributes:attributes];
    NSRect    titleRect=NSInsetRect(tab,kTabCloseSize+10,0);

    titleRect.origin.y=floor(NSMidY(tab)-size.height/2);
    titleRect.size.height=size.height;
    [title drawInRect:titleRect withAttributes:attributes];
    if(count>1 && (NSInteger)i==_hover){
     NSRect     close=NSInsetRect([self _closeRectInTab:tab],4,4);
     NSBezierPath *path=[NSBezierPath bezierPath];

     [path moveToPoint:close.origin];
     [path lineToPoint:NSMakePoint(NSMaxX(close),NSMaxY(close))];
     [path moveToPoint:NSMakePoint(NSMinX(close),NSMaxY(close))];
     [path lineToPoint:NSMakePoint(NSMaxX(close),NSMinY(close))];
     [path setLineWidth:1.5];
     [[NSColor colorWithGenericGamma22White:(dark ? 0.75 : 0.30) alpha:1] setStroke];
     [path stroke];
    }
   }
   if([self _hasPlus]){
    NSRect        plus=[self _plusRect];
    NSBezierPath *path=[NSBezierPath bezierPath];

    fillGray(lineGray,NSMakeRect(NSMinX(plus),0,1,NSHeight(plus)));
    [path moveToPoint:NSMakePoint(NSMidX(plus)-5,NSMidY(plus))];
    [path lineToPoint:NSMakePoint(NSMidX(plus)+5,NSMidY(plus))];
    [path moveToPoint:NSMakePoint(NSMidX(plus),NSMidY(plus)-5)];
    [path lineToPoint:NSMakePoint(NSMidX(plus),NSMidY(plus)+5)];
    [path setLineWidth:1.5];
    [[NSColor colorWithGenericGamma22White:(dark ? 0.75 : 0.30) alpha:1] setStroke];
    [path stroke];
   }
   /* Its bottom edge, against the content. */
   fillGray(lineGray,NSMakeRect(0,0,NSWidth([self bounds]),1));
}

@end

@implementation NSWindow (NSWindowTabbingPrivate)

-(NSWindowTabGroup *)_existingTabGroup {
   return objc_getAssociatedObject(self,&kTabGroupKey);
}

/* Its group changed: the tab bar shown or taken away (the window grows,
   downwards, by its height, or shrinks: the content keeps its size), and
   redrawn. */
-(void)_tabGroupDidChange {
   NSWindowTabGroup  *group=[self _existingTabGroup];
   NSWindowTabBarView *bar=objc_getAssociatedObject(self,&kTabBarKey);
   BOOL show=([self styleMask]&NSWindowStyleMaskTitled) && ![self isSheet] && [group isTabBarVisible];
   NSView *frameView=[[self contentView] superview];

   if(show && bar==nil){
    NSRect frame=[self frame];

    bar=[[[NSWindowTabBarView alloc] initWithFrame:NSMakeRect(0,0,NSWidth(frame),NSWindowTabBarHeight)] autorelease];
    objc_setAssociatedObject(self,&kTabBarKey,bar,OBJC_ASSOCIATION_RETAIN);
    [frameView addSubview:bar];
    frame.origin.y-=NSWindowTabBarHeight;
    frame.size.height+=NSWindowTabBarHeight;
    [self setFrame:frame display:[self isVisible]];
   }
   else if(!show && bar!=nil){
    NSRect frame=[self frame];

    [bar removeFromSuperview];
    objc_setAssociatedObject(self,&kTabBarKey,nil,OBJC_ASSOCIATION_RETAIN);
    bar=nil;
    frame.origin.y+=NSWindowTabBarHeight;
    frame.size.height-=NSWindowTabBarHeight;
    [self setFrame:frame display:[self isVisible]];
   }
   [frameView resizeSubviewsWithOldSize:[frameView frame].size];
   [bar setNeedsDisplay:YES];
}

@end

@implementation NSWindow (NSWindowTabbing)

+(BOOL)allowsAutomaticWindowTabbing {
   return allowsAutomaticWindowTabbing;
}

+(void)setAllowsAutomaticWindowTabbing:(BOOL)allows {
   allowsAutomaticWindowTabbing=allows;
}

-(NSWindowTabbingMode)tabbingMode {
   return (NSWindowTabbingMode)[objc_getAssociatedObject(self,&kTabbingModeKey) integerValue];
}

-(void)setTabbingMode:(NSWindowTabbingMode)mode {
   objc_setAssociatedObject(self,&kTabbingModeKey,[NSNumber numberWithInteger:mode],OBJC_ASSOCIATION_RETAIN);
}

/* By default, the app's and the window's class: its document windows
   join each other, not its panels. */
-(NSString *)tabbingIdentifier {
   NSString *identifier=objc_getAssociatedObject(self,&kTabbingIdentifierKey);

   if(identifier!=nil)
    return identifier;
   return [NSString stringWithFormat:@"%@-%@",[[NSBundle mainBundle] bundleIdentifier] ?: @"",
      NSStringFromClass([self class])];
}

-(void)setTabbingIdentifier:(NSString *)identifier {
   objc_setAssociatedObject(self,&kTabbingIdentifierKey,identifier,OBJC_ASSOCIATION_COPY);
}

/* Its group: one of its own until it joins another. */
-(NSWindowTabGroup *)tabGroup {
   NSWindowTabGroup *group=objc_getAssociatedObject(self,&kTabGroupKey);

   if(group==nil){
    group=[[[NSWindowTabGroup alloc] initWithIdentifier:[self tabbingIdentifier]] autorelease];
    [group addWindow:self];
   }
   return group;
}

/* Its group's windows when it has others, else nil (as the reference). */
-(NSArray *)tabbedWindows {
   NSArray *windows=[[self _existingTabGroup] windows];

   return [windows count]>1 ? windows : nil;
}

-(NSWindowTab *)tab {
   NSWindowTab *tab=objc_getAssociatedObject(self,&kTabKey);

   if(tab==nil){
    tab=[[[NSWindowTab alloc] init] autorelease];
    [tab _setWindow:self];
    objc_setAssociatedObject(self,&kTabKey,tab,OBJC_ASSOCIATION_RETAIN);
   }
   return tab;
}

/* window joins this window's group, after it (NSWindowAbove) or before. */
-(void)addTabbedWindow:(NSWindow *)window ordered:(NSWindowOrderingMode)ordered {
   NSWindowTabGroup *group=[self tabGroup];
   NSUInteger        index=[[group windows] indexOfObjectIdenticalTo:self];

   if(window==nil || window==self)
    return;
   [group insertWindow:window atIndex:(NSInteger)(ordered==NSWindowBelow ? index : index+1)];
}

-(void)_selectTabBy:(NSInteger)step {
   NSArray   *windows=[self tabbedWindows];
   NSUInteger index=[windows indexOfObjectIdenticalTo:self];

   if([windows count]<2 || index==NSNotFound)
    return;
   NSWindow *next=[windows objectAtIndex:((NSInteger)index+step+(NSInteger)[windows count])%[windows count]];
   [next makeKeyAndOrderFront:nil];
}

-(void)selectNextTab:(id)sender {
   [self _selectTabBy:1];
}

-(void)selectPreviousTab:(id)sender {
   [self _selectTabBy:-1];
}

/* Out of its group, a window of its own beside it. */
-(void)moveTabToNewWindow:(id)sender {
   NSWindowTabGroup *group=[self _existingTabGroup];
   NSRect            frame;

   if([[group windows] count]<2)
    return;
   [group removeWindow:self];
   /* Without the tab bar now. */
   frame=[self frame];
   frame.origin.x+=24;
   frame.origin.y-=24;
   [self setFrame:frame display:NO];
   [self makeKeyAndOrderFront:nil];
}

/* The app's windows that can be tabs of this one (the same tabbing
   identifier, not panels, not disallowed) join its group, at its end. */
-(void)mergeAllWindows:(id)sender {
   NSString *identifier=[self tabbingIdentifier];

   for(NSWindow *window in [NSApp windows]){
    if(window==self || [window isKindOfClass:[NSPanel class]] || [window tabbingMode]==NSWindowTabbingModeDisallowed)
     continue;
    if(![window isVisible] && [window _existingTabGroup]==nil)
     continue;
    if(![[window tabbingIdentifier] isEqualToString:identifier] || [window isSheet])
     continue;
    [[self tabGroup] addWindow:window];
   }
   [self makeKeyAndOrderFront:nil];
}

-(void)toggleTabBar:(id)sender {
   NSWindowTabGroup *group=[self tabGroup];

   [group _setTabBarShown:![group _tabBarShown]];
   [group _groupChanged];
}

-(void)toggleTabOverview:(id)sender {
   NSWindowTabGroup *group=[self tabGroup];

   [group setOverviewVisible:![group isOverviewVisible]];
}

@end

BOOL _NSWindowTabWillOrderIn(NSWindow *window);
void _NSWindowTabWillClose(NSWindow *window);
void _NSWindowTabTitleDidChange(NSWindow *window);

/* From -[NSWindow setTitle:]: its tab's title, in every tab bar of its
   group. */
void _NSWindowTabTitleDidChange(NSWindow *window) {
   for(NSWindow *tabbed in [window tabbedWindows])
    [objc_getAssociatedObject(tabbed,&kTabBarKey) setNeedsDisplay:YES];
}

/* A window that becomes a tab by itself as it appears: a document window
   (titled, not a panel or a sheet, not disallowed) when the app allows it
   and either the window prefers tabs or the user does (the Dock pane's
   Prefer tabs: always). */
static BOOL canBeTab(NSWindow *window) {
   return [window tabbingMode]!=NSWindowTabbingModeDisallowed && ([window styleMask]&NSWindowStyleMaskTitled) &&
      ![window isKindOfClass:[NSPanel class]] && ![window isSheet] && [window level]==NSNormalWindowLevel;
}

static BOOL tabsAutomatically(NSWindow *window) {
   if(![NSWindow allowsAutomaticWindowTabbing] || !canBeTab(window))
    return NO;
   return [window tabbingMode]==NSWindowTabbingModePreferred ||
      [NSWindow userTabbingPreference]==NSWindowUserTabbingPreferenceAlways;
}

/* The shown window a new one joins as a tab: the one whose + made it, or
   the main (else key) window with its tabbing identifier. */
static NSWindow *tabParentFor(NSWindow *window) {
   NSWindow *parent=canBeTab(window) ? newTabParent : nil;

   if(parent==nil && tabsAutomatically(window)){
    parent=[NSApp mainWindow] ?: [NSApp keyWindow];
    if(parent!=nil && (![parent isVisible] || [parent isKindOfClass:[NSPanel class]] || [parent tabbingMode]==NSWindowTabbingModeDisallowed ||
       ![[parent tabbingIdentifier] isEqualToString:[window tabbingIdentifier]]))
     parent=nil;
   }
   if(parent==window || [[window _existingTabGroup] windows].count>1)
    return nil;
   return parent;
}

/* From -[NSWindow orderWindow:relativeTo:]: a window of a group ordered in
   while another is selected is chosen instead; a new window that becomes
   a tab joins its parent's group and is chosen (YES: done). */
BOOL _NSWindowTabWillOrderIn(NSWindow *window) {
   NSWindowTabGroup *group;
   NSWindow         *parent;

   if(switchingTabs)
    return NO;
   if((parent=tabParentFor(window))!=nil){
    [parent addTabbedWindow:window ordered:NSWindowAbove];
    [[parent tabGroup] setSelectedWindow:window];
    return YES;
   }
   if((group=[window _existingTabGroup])==nil)
    return NO;
   if([[group windows] count]<2 || [group selectedWindow]==window || ![[group selectedWindow] isVisible])
    return NO;
   [group setSelectedWindow:window];
   return YES;
}

/* From -[NSWindow close]: the window leaves its group. */
void _NSWindowTabWillClose(NSWindow *window) {
   [[window _existingTabGroup] removeWindow:window];
}
