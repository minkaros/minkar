/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"
#import <AppKit/NSControl.h>
#import <AppKit/NSCell.h>
#import <objc/runtime.h>

const CGFloat NSViewNoIntrinsicMetric=-1;

@implementation NSViewLayoutState

-init {
   constraints=[[NSMutableArray alloc] init];
   layoutGuides=[[NSMutableArray alloc] init];
   anchors=[[NSMutableDictionary alloc] init];
   translatesAutoresizingMask=YES;
   hugging[0]=hugging[1]=NSLayoutPriorityDefaultLow;
   compressionResistance[0]=compressionResistance[1]=NSLayoutPriorityDefaultHigh;
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

// Only when its hierarchy uses constraints (its root has a layout state).
void NSLayoutViewDidResize(NSView *view) {
   if(NSViewLayoutStateIfAny(layoutRoot(view))!=nil)
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
   NSViewLayoutStateFor(self)->needsUpdateConstraints=NO;
}

// Subviews first, as constraints of a superview may depend on its subviews'.
-(void)updateConstraintsForSubtreeIfNeeded {
   for(NSView *subview in [self subviews])
    [subview updateConstraintsForSubtreeIfNeeded];
   if(NSViewLayoutStateIfAny(self)->needsUpdateConstraints)
    [self updateConstraints];
}

-(BOOL)needsUpdateConstraints {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return state!=nil && state->needsUpdateConstraints;
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

   if(state==nil || !state->needsLayout)
    return;
   state->needsLayout=NO;
   [root updateConstraintsForSubtreeIfNeeded];
   NSLayoutEngineLayout(root);
   sendLayout(root);
   if([root window]!=nil)
    objc_setAssociatedObject([root window],&windowNeedsLayoutKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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

-(NSLayoutPriority)contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return (state!=nil)?state->hugging[orientation&1]:NSLayoutPriorityDefaultLow;
}

-(void)setContentHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutStateFor(self)->hugging[orientation&1]=priority;
   NSLayoutSetNeedsLayout(self);
}

-(NSLayoutPriority)contentCompressionResistancePriorityForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSViewLayoutState *state=NSViewLayoutStateIfAny(self);

   return (state!=nil)?state->compressionResistance[orientation&1]:NSLayoutPriorityDefaultHigh;
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

-(NSSize)intrinsicContentSize {
   NSCell *cell=[self cell];

   if(cell==nil)
    return NSMakeSize(NSViewNoIntrinsicMetric,NSViewNoIntrinsicMetric);
   return [cell cellSize];
}

@end
