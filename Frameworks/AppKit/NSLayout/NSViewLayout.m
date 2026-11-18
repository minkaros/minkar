/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#include <stdlib.h>
#import "NSLayoutInternal.h"
#import <AppKit/NSControl.h>
#import <AppKit/NSCell.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSPopUpButtonCell.h>
#import <objc/runtime.h>

const CGFloat NSViewNoIntrinsicMetric=-1;

@implementation NSViewLayoutState

-init {
   constraints=[[NSMutableArray alloc] init];
   layoutGuides=[[NSMutableArray alloc] init];
   anchors=[[NSMutableDictionary alloc] init];
   translatesAutoresizingMask=YES;
   /* Not set: the view's class gives it (-1). */
   hugging[0]=hugging[1]=-1;
   compressionResistance[0]=compressionResistance[1]=-1;
   return self;
}

-(void)dealloc {
   [constraints release];
   [layoutGuides release];
   [anchors release];
   [super dealloc];
}

@end

static char layoutStateKey;
static char windowNeedsLayoutKey;

NSViewLayoutState *NSViewLayoutStateIfAny(NSView *view) {
   return objc_getAssociatedObject(view,&layoutStateKey);
}

NSViewLayoutState *NSViewLayoutStateFor(NSView *view) {
   NSViewLayoutState *state=objc_getAssociatedObject(view,&layoutStateKey);

   if(state==nil){
    state=[[NSViewLayoutState alloc] init];
    objc_setAssociatedObject(view,&layoutStateKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [state release];
   }
   return state;
}

NSView *NSLayoutItemView(id item) {
   if([item isKindOfClass:[NSView class]])
    return item;
   if([item isKindOfClass:[NSLayoutGuide class]])
    return [(NSLayoutGuide *)item owningView];
   return nil;
}

// The view a layout pass starts at: the window's content view's parent (so
// the content view itself is placed), else the top of the hierarchy.
static NSView *layoutRoot(NSView *view) {
   NSView *root=view;

   while([root superview]!=nil)
    root=[root superview];
   return root;
}

void NSLayoutSetNeedsLayout(id item) {
   NSView *view=NSLayoutItemView(item);

   if(view==nil)
    return;
   NSViewLayoutStateFor(layoutRoot(view))->needsLayout=YES;
   if([view window]!=nil){
    objc_setAssociatedObject([view window],&windowNeedsLayoutKey,[NSNumber numberWithBool:YES],OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [[view window] setViewsNeedDisplay:YES];
   }
}

// Whether a view or one under it is laid out by constraints or has some.
static BOOL usesConstraints(NSView *view) {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(view);

   if(state!=nil && (!state->translatesAutoresizingMask || [state->constraints count]>0))
    return YES;
   for(NSView *subview in [view subviews])
    if(usesConstraints(subview))
     return YES;
   return NO;
}

/* Only when its hierarchy uses constraints (its root has a layout state),
   and the view or one under it does: a view placed by its frame alone (a
   table's row) changes nothing the solver sees. */
void NSLayoutViewDidResize(NSView *view) {
   if(NSViewLayoutStateIfAny(layoutRoot(view))==nil)
    return;
   if([view superview]!=nil && !usesConstraints(view))
    return;
   NSLayoutSetNeedsLayout(view);
}

void NSLayoutViewDidChangeVisibility(NSView *view) {
   NSView *superview=[view superview];

   if(superview!=nil && NSViewLayoutStateIfAny(superview)!=nil)
    [superview setNeedsUpdateConstraints:YES];
}

void NSLayoutViewDidMove(NSView *view) {
   if(NSViewLayoutStateIfAny(view)!=nil)
    NSLayoutSetNeedsLayout(view);
}

@implementation NSView (NSConstraintBasedLayoutInstallingConstraints)

-(NSArray *)constraints {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return (state!=nil)?[[state->constraints copy] autorelease]:[NSArray array];
}

-(void)addConstraint:(NSLayoutConstraint *)constraint {
   NSViewLayoutState *state=NSViewLayoutStateFor(self);

   if([state->constraints indexOfObjectIdenticalTo:constraint]==NSNotFound)
    [state->constraints addObject:constraint];
   [constraint _setActiveFlag:YES];
   NSLayoutSetNeedsLayout(self);
}

-(void)addConstraints:(NSArray *)constraints {
   for(NSLayoutConstraint *constraint in constraints)
    [self addConstraint:constraint];
}

-(void)removeConstraint:(NSLayoutConstraint *)constraint {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   if(state==nil)
    return;
   [constraint retain];
   [state->constraints removeObjectIdenticalTo:constraint];
   [constraint _setActiveFlag:NO];
   [constraint release];
   NSLayoutSetNeedsLayout(self);
}

-(void)removeConstraints:(NSArray *)constraints {
   for(NSLayoutConstraint *constraint in [[constraints copy] autorelease])
    [self removeConstraint:constraint];
}

@end

@implementation NSView (NSConstraintBasedLayoutCoreMethods)

-(void)updateConstraints {
   NSViewLayoutState *state=NSViewLayoutStateFor(self);

   state->needsUpdateConstraints=NO;
   state->constraintsUpdated=YES;
}

// Subviews first, as constraints of a superview may depend on its subviews'.
-(void)updateConstraintsForSubtreeIfNeeded {
   for(NSView *subview in [self subviews])
    [subview updateConstraintsForSubtreeIfNeeded];
   if([self needsUpdateConstraints]){
    [self updateConstraints];
    // Done, even when an override did not call super.
    NSViewLayoutState *state=NSViewLayoutStateFor(self);
    state->needsUpdateConstraints=NO;
    state->constraintsUpdated=YES;
   }
}

-(BOOL)needsUpdateConstraints {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   // Minkar: a view needs its constraints updated until its first update,
   // as on the reference platform (views make theirs in updateConstraints).
   return state==nil || !state->constraintsUpdated || state->needsUpdateConstraints;
}

-(void)setNeedsUpdateConstraints:(BOOL)flag {
   NSViewLayoutStateFor(self)->needsUpdateConstraints=flag;
   if(flag)
    NSLayoutSetNeedsLayout(self);
}

-(void)layout {
}

-(BOOL)needsLayout {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(layoutRoot(self));

   return state!=nil && state->needsLayout;
}

-(void)setNeedsLayout:(BOOL)flag {
   if(flag)
    NSLayoutSetNeedsLayout(self);
   else if(NSViewLayoutStateIfAny(layoutRoot(self))!=nil)
    NSViewLayoutStateFor(layoutRoot(self))->needsLayout=NO;
}

static void sendLayout(NSView *view) {
   [view layout];
   for(NSView *subview in [view subviews])
    sendLayout(subview);
}

// Constraints brought up to date, the subtree solved and placed, then
// -layout for each view, from the top.
-(void)layoutSubtreeIfNeeded {
   NSView *root=layoutRoot(self);
   NSViewLayoutState *state=NSViewLayoutStateIfAny(root);

   int                passes;

   if(state==nil || !state->needsLayout)
    return;
   /* Minkar: the window's mark is cleared first, and the pass run again
      while a view asks for one during it (a frame change it answers by
      changing an intrinsic size, as TextMate's gutter does): cleared at
      the end, a request made during the pass was lost and the view kept
      its old size. A few passes at most, as the reference does, so views
      that keep asking cannot hold the window. */
   static int debug=-1;

   if(debug<0)
    debug=(getenv("NSLayoutDebug")!=NULL);
   for(passes=0;passes<8 && state->needsLayout;passes++){
    NSTimeInterval start=debug?[NSDate timeIntervalSinceReferenceDate]:0;

    state->needsLayout=NO;
    if([root window]!=nil)
     objc_setAssociatedObject([root window],&windowNeedsLayoutKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [root updateConstraintsForSubtreeIfNeeded];
    NSLayoutEngineLayout(root);
    sendLayout(root);
    // NSLayoutDebug=1 in the environment logs each pass and what it cost.
    if(debug)
     NSLog(@"layout pass %d of %@ in %@: %.1f ms%@",passes+1,root,[[root window] title],([NSDate timeIntervalSinceReferenceDate]-start)*1000,state->needsLayout?@", asked again":@"");
   }
}

@end

@implementation NSView (NSConstraintBasedCompatibility)

-(BOOL)translatesAutoresizingMaskIntoConstraints {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return state==nil || state->translatesAutoresizingMask;
}

-(void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)flag {
   NSViewLayoutStateFor(self)->translatesAutoresizingMask=flag;
   NSLayoutSetNeedsLayout(self);
}

+(BOOL)requiresConstraintBasedLayout {
   return NO;
}

@end

@implementation NSView (NSConstraintBasedLayoutLayering)

-(NSRect)alignmentRectForFrame:(NSRect)frame {
   NSEdgeInsets insets=[self alignmentRectInsets];

   return NSMakeRect(frame.origin.x+insets.left,frame.origin.y+([self isFlipped]?insets.top:insets.bottom),
    frame.size.width-insets.left-insets.right,frame.size.height-insets.top-insets.bottom);
}

-(NSRect)frameForAlignmentRect:(NSRect)rect {
   NSEdgeInsets insets=[self alignmentRectInsets];

   return NSMakeRect(rect.origin.x-insets.left,rect.origin.y-([self isFlipped]?insets.top:insets.bottom),
    rect.size.width+insets.left+insets.right,rect.size.height+insets.top+insets.bottom);
}

-(NSEdgeInsets)alignmentRectInsets {
   return NSEdgeInsetsZero;
}

-(CGFloat)firstBaselineOffsetFromTop {
   return 0;
}

-(CGFloat)lastBaselineOffsetFromBottom {
   return 0;
}

-(CGFloat)baselineOffsetFromBottom {
   return [self lastBaselineOffsetFromBottom];
}

-(NSSize)intrinsicContentSize {
   return NSMakeSize(NSViewNoIntrinsicMetric,NSViewNoIntrinsicMetric);
}

-(void)invalidateIntrinsicContentSize {
   NSLayoutSetNeedsLayout(self);
}

/* The standard controls' metrics, as measured on the reference for each
   control size (regular, small, mini): the insets of the alignment
   rectangle in the frame (top, left, bottom, right), what the alignment
   width adds to the title's, and the alignment height. A title in another
   font is a little wider or narrower, the rest the same. */
typedef struct {
   CGFloat insets[3][4];
   CGFloat pad[3];
   CGFloat height[3];
   BOOL    fixedSize;       // the size is the height, both ways (help, disclosure)
} NSControlMetrics;

enum { MetricsNone=-1, MetricsPush, MetricsCheck, MetricsRadio, MetricsPopUp, MetricsPullDown, MetricsSquare,
       MetricsRecessed, MetricsInline, MetricsHelp, MetricsDisclosure, MetricsSlider, MetricsStepper, MetricsSegmented };

static const NSControlMetrics controlMetrics[]={
   [MetricsPush]={{{4,6,7,6},{3,5,6,5},{0,1,1,1}},{28,16,22},{21,18,15},NO},
   [MetricsCheck]={{{2,2,2,2},{3,3,3,2},{5,4,3,2}},{18,17,16},{14,12,10},NO},
   [MetricsRadio]={{{1,1,1,2},{2,2,2,1},{4,4,4,1}},{19,19,17},{16,14,10},NO},
   [MetricsPopUp]={{{1,2,3,3},{1,3,3,3},{0,1,1,2}},{33,30,32},{21,18,15},NO},
   [MetricsPullDown]={{{1,3,3,3},{1,3,3,3},{0,0,1,1}},{33,30,34},{21,18,15},NO},
   [MetricsSquare]={{{2,2,3,2},{2,2,3,2},{2,2,3,2}},{20,20,20},{23,21,18},NO},
   [MetricsRecessed]={{{0,0,1,0},{0,0,1,0},{0,0,1,0}},{14,14,14},{18,16,14},NO},
   [MetricsInline]={{{0,0,1,0},{0,0,1,0},{0,0,1,0}},{12,12,12},{18,16,13},NO},
   [MetricsHelp]={{{0,2,4,2},{0,0,1,0},{0,0,1,0}},{0,0,0},{21,18,16},YES},
   [MetricsDisclosure]={{{0,0,0,0},{0,0,0,0},{0,0,0,0}},{0,0,0},{13,13,13},YES},
   [MetricsSlider]={{{2,2,2,2},{1,0,1,0},{0,0,1,0}},{0,0,0},{15,13,12},NO},
   [MetricsStepper]={{{3,3,3,3},{1,2,2,2},{1,2,2,2}},{0,0,0},{22,19,15},NO},
   [MetricsSegmented]={{{1,2,2,2},{1,2,2,2},{0,1,1,1}},{0,0,0},{21,18,15},NO},
};

static int controlMetricsKind(NSControl *control) {
   static Class popUp,button,slider,stepper,segmented;
   NSCell *cell=[control cell];

   if(button==nil){
    button=NSClassFromString(@"NSButton");
    popUp=NSClassFromString(@"NSPopUpButton");
    slider=NSClassFromString(@"NSSlider");
    stepper=NSClassFromString(@"NSStepper");
    segmented=NSClassFromString(@"NSSegmentedControl");
   }
   if(cell==nil)
    return MetricsNone;
   if([control isKindOfClass:popUp])
    return [(NSPopUpButton *)control pullsDown]?MetricsPullDown:MetricsPopUp;
   if([control isKindOfClass:button]){
    NSButton *b=(NSButton *)control;
    NSString *imageName=[[b image] name];

    if([imageName isEqualToString:@"NSSwitch"])
     return MetricsCheck;
    if([imageName isEqualToString:@"NSRadioButton"])
     return MetricsRadio;
    if(![b isBordered])
     return MetricsNone;
    switch([b bezelStyle]){
     case NSBezelStyleRounded: return MetricsPush;
     case NSBezelStyleRegularSquare: return MetricsSquare;
     case NSBezelStyleRecessed: return MetricsRecessed;
     case NSBezelStyleInline: return MetricsInline;
     case NSBezelStyleHelpButton: return MetricsHelp;
     case NSBezelStyleDisclosure: return MetricsDisclosure;
     default: return MetricsNone;
    }
   }
   if([control isKindOfClass:slider])
    return NSWidth([control frame])>=NSHeight([control frame])?MetricsSlider:MetricsNone;
   if([control isKindOfClass:stepper])
    return MetricsStepper;
   if([control isKindOfClass:segmented])
    return MetricsSegmented;
   return MetricsNone;
}

static int controlSizeIndex(NSControl *control) {
   NSControlSize size=[[control cell] controlSize];

   return (size==NSControlSizeSmall)?1:(size==NSControlSizeMini)?2:0;
}

// The width of a title in the control's font.
static CGFloat titleWidth(NSControl *control,NSString *title) {
   NSFont *font=[control font];
   NSDictionary *attributes=(font!=nil)?[NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName]:nil;

   return (title!=nil)?ceil([title sizeWithAttributes:attributes].width):0;
}

/* A view's own content hugging, as measured on the reference: a control
   hugs its content vertically (750), a view, a button with no bezel to
   size it (a checkbox), an image view and a color well do not (250); a
   stepper hugs both ways. Compression resistance is 750 throughout. */
static NSLayoutPriority defaultHugging(NSView *view,NSLayoutConstraintOrientation orientation) {
   static Class button,popUp,imageView,colorWell,stepper,progress;

   if(button==nil){
    button=NSClassFromString(@"NSButton");
    popUp=NSClassFromString(@"NSPopUpButton");
    imageView=NSClassFromString(@"NSImageView");
    colorWell=NSClassFromString(@"NSColorWell");
    stepper=NSClassFromString(@"NSStepper");
    progress=NSClassFromString(@"NSProgressIndicator");
   }
   if([view isKindOfClass:stepper])
    return NSLayoutPriorityDefaultHigh;
   if([view isKindOfClass:button] && ![view isKindOfClass:popUp]){
    int kind=controlMetricsKind((NSControl *)view);

    if(kind==MetricsHelp || kind==MetricsDisclosure)
     return NSLayoutPriorityDefaultHigh;
   }
   if(orientation==NSLayoutConstraintOrientationHorizontal)
    return NSLayoutPriorityDefaultLow;
   if([view isKindOfClass:popUp] || [view isKindOfClass:progress])
    return NSLayoutPriorityDefaultHigh;
   /* A button's bezel decides: a push button's, a recessed one's and a
      pop-up's have a height of their own; a help or disclosure button
      hugs both ways (above); a square, inline, check or radio one not. */
   if([view isKindOfClass:button]){
    int kind=controlMetricsKind((NSControl *)view);

    return (kind==MetricsPush || kind==MetricsRecessed || kind==MetricsHelp || kind==MetricsDisclosure)?NSLayoutPriorityDefaultHigh:NSLayoutPriorityDefaultLow;
   }
   if([view isKindOfClass:imageView] || [view isKindOfClass:colorWell])
    return NSLayoutPriorityDefaultLow;
   if([view isKindOfClass:[NSControl class]])
    return NSLayoutPriorityDefaultHigh;
   return NSLayoutPriorityDefaultLow;
}

-(NSLayoutPriority)contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   if(state!=nil && state->hugging[orientation&1]>=0)
    return state->hugging[orientation&1];
   return defaultHugging(self,orientation);
}

-(void)setContentHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutStateFor(self)->hugging[orientation&1]=priority;
   NSLayoutSetNeedsLayout(self);
}

-(NSLayoutPriority)contentCompressionResistancePriorityForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return (state!=nil && state->compressionResistance[orientation&1]>=0)?state->compressionResistance[orientation&1]:NSLayoutPriorityDefaultHigh;
}

-(void)setContentCompressionResistancePriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutStateFor(self)->compressionResistance[orientation&1]=priority;
   NSLayoutSetNeedsLayout(self);
}

@end

@implementation NSView (NSConstraintBasedLayoutFittingSize)

-(NSSize)fittingSize {
   [self updateConstraintsForSubtreeIfNeeded];
   return NSLayoutEngineFittingSize(self);
}

@end

@implementation NSView (NSConstraintBasedLayoutDebugging)

-(NSArray *)constraintsAffectingLayoutForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSMutableArray *result=[NSMutableArray array];

   for(NSView *check=self;check!=nil;check=[check superview])
    for(NSLayoutConstraint *constraint in [check constraints])
     if([constraint firstItem]==self || [constraint secondItem]==self)
      [result addObject:constraint];
   return result;
}

-(BOOL)hasAmbiguousLayout {
   return NO;
}

@end

@implementation NSView (NSLayoutGuideSupport)

-(void)addLayoutGuide:(NSLayoutGuide *)guide {
   [[guide owningView] removeLayoutGuide:guide];
   [NSViewLayoutStateFor(self)->layoutGuides addObject:guide];
   [guide setOwningView:self];
   NSLayoutSetNeedsLayout(self);
}

-(void)removeLayoutGuide:(NSLayoutGuide *)guide {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   if(state==nil || [state->layoutGuides indexOfObjectIdenticalTo:guide]==NSNotFound)
    return;
   [guide retain];
   [state->layoutGuides removeObjectIdenticalTo:guide];
   [guide setOwningView:nil];
   [guide release];
   NSLayoutSetNeedsLayout(self);
}

-(NSArray *)layoutGuides {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return (state!=nil)?[[state->layoutGuides copy] autorelease]:[NSArray array];
}

@end

@implementation NSView (NSViewLayoutAnchors)

-(NSLayoutXAxisAnchor *)leadingAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeLeading,[NSLayoutXAxisAnchor class]); }
-(NSLayoutXAxisAnchor *)trailingAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeTrailing,[NSLayoutXAxisAnchor class]); }
-(NSLayoutXAxisAnchor *)leftAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeLeft,[NSLayoutXAxisAnchor class]); }
-(NSLayoutXAxisAnchor *)rightAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeRight,[NSLayoutXAxisAnchor class]); }
-(NSLayoutYAxisAnchor *)topAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeTop,[NSLayoutYAxisAnchor class]); }
-(NSLayoutYAxisAnchor *)bottomAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeBottom,[NSLayoutYAxisAnchor class]); }
-(NSLayoutDimension *)widthAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeWidth,[NSLayoutDimension class]); }
-(NSLayoutDimension *)heightAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeHeight,[NSLayoutDimension class]); }
-(NSLayoutXAxisAnchor *)centerXAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeCenterX,[NSLayoutXAxisAnchor class]); }
-(NSLayoutYAxisAnchor *)centerYAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeCenterY,[NSLayoutYAxisAnchor class]); }
-(NSLayoutYAxisAnchor *)firstBaselineAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeFirstBaseline,[NSLayoutYAxisAnchor class]); }
-(NSLayoutYAxisAnchor *)lastBaselineAnchor { return NSLayoutAnchorFor(self,NSLayoutAttributeLastBaseline,[NSLayoutYAxisAnchor class]); }

@end

@implementation NSWindow (NSConstraintBasedLayoutCoreMethods)

-(void)updateConstraintsIfNeeded {
   NSView *root=[[self contentView] superview];

   [(root!=nil?root:[self contentView]) updateConstraintsForSubtreeIfNeeded];
}

-(void)layoutIfNeeded {
   if(objc_getAssociatedObject(self,&windowNeedsLayoutKey)==nil)
    return;
   [[self contentView] layoutSubtreeIfNeeded];
}

@end

// Controls are as large as their cell needs.
@implementation NSControl (NSConstraintBasedLayoutLayering)

-(NSEdgeInsets)alignmentRectInsets {
   int kind=controlMetricsKind(self);

   if(kind==MetricsNone)
    return NSEdgeInsetsZero;
   const CGFloat *i=controlMetrics[kind].insets[controlSizeIndex(self)];
   return NSEdgeInsetsMake(i[0],i[1],i[2],i[3]);
}

-(NSSize)intrinsicContentSize {
   NSCell *cell=[self cell];
   int     kind=controlMetricsKind(self);

   if(cell==nil)
    return NSMakeSize(NSViewNoIntrinsicMetric,NSViewNoIntrinsicMetric);
   if(kind==MetricsNone)
    return [cell cellSize];

   const NSControlMetrics *m=&controlMetrics[kind];
   int     size=controlSizeIndex(self);
   CGFloat height=m->height[size];

   switch(kind){
    case MetricsHelp:
    case MetricsDisclosure:
     return NSMakeSize(height,height);

    case MetricsSlider:
     return NSMakeSize(NSViewNoIntrinsicMetric,height);

    case MetricsStepper:
     return NSMakeSize(size==0?13:(size==1?11:9),height);

    case MetricsSegmented:{
     NSSize cellSize=[cell cellSize];
     const CGFloat *i=m->insets[size];

     return NSMakeSize(cellSize.width<10000?cellSize.width-i[1]-i[3]:NSViewNoIntrinsicMetric,height);
    }

    case MetricsPopUp:
    case MetricsPullDown:{
     /* As wide as its widest item, its arrows and margins; one showing an
        item of its own (an action menu's gear) as wide as that item. */
     NSPopUpButtonCell *popUpCell=(NSPopUpButtonCell *)cell;
     NSArray *items=[(NSPopUpButton *)self itemArray];
     CGFloat  widest=0;

     if(![popUpCell usesItemFromMenu] && [popUpCell menuItem]!=nil)
      items=[NSArray arrayWithObject:[popUpCell menuItem]];
     for(NSMenuItem *item in items){
      CGFloat width=titleWidth(self,[item title]);

      if([item image]!=nil)
       width+=[[item image] size].width+(width>0?4:0);
      widest=MAX(widest,width);
     }
     return NSMakeSize(widest+m->pad[size],height);
    }

    default:{
     NSButton *button=(NSButton *)self;
     CGFloat   width=titleWidth(self,[button title])+m->pad[size];

     // An image beside or in place of the title takes its own room.
     if([button image]!=nil && kind!=MetricsCheck && kind!=MetricsRadio){
      NSCellImagePosition position=[button imagePosition];
      CGFloat imageWidth=[[button image] size].width;

      if(position==NSImageOnly)
       width=imageWidth+m->pad[size];
      else if(position==NSImageLeft || position==NSImageRight)
       width+=imageWidth+4;
     }
     return NSMakeSize(width,height);
    }
   }
}

@end
