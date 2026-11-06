/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <AppKit/NSStackView.h>
#import "NSLayoutInternal.h"

const CGFloat NSStackViewSpacingUseDefault=FLT_MAX;

/* The arranged views in a row (horizontal) or column (vertical), held by
 * constraints the stack view makes in -updateConstraints: each one after
 * the other with the spacing between them, the first and last at the edge
 * insets, and across, the alignment. Hidden views are left out when it
 * detaches them. Its own size follows its views (with its hugging
 * priority) unless something else sets it. */

@implementation NSStackView

+(instancetype)stackViewWithViews:(NSArray *)views {
   NSStackView *result=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   for(NSView *view in views)
    [result addArrangedSubview:view];
   return result;
}

-(void)_stackInit {
   _arrangedSubviews=[[NSMutableArray alloc] init];
   _gravities=[[NSMutableArray alloc] init];
   _customSpacing=[[NSMutableDictionary alloc] init];
   _visibilityPriorities=[[NSMutableDictionary alloc] init];
   _stackConstraints=[[NSMutableArray alloc] init];
   _orientation=NSUserInterfaceLayoutOrientationHorizontal;
   _alignment=NSLayoutAttributeCenterY;
   _distribution=NSStackViewDistributionGravityAreas;
   _spacing=8;
   _edgeInsets=NSEdgeInsetsZero;
   _detachesHiddenViews=YES;
   _hugging[0]=_hugging[1]=NSLayoutPriorityDefaultLow;
   _clippingResistance[0]=_clippingResistance[1]=NSLayoutPriorityRequired;
   [self setNeedsUpdateConstraints:YES];
}

-initWithFrame:(NSRect)frame {
   if((self=[super initWithFrame:frame])!=nil)
    [self _stackInit];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])!=nil){
    [self _stackInit];
    if([coder allowsKeyedCoding]){
     if([coder containsValueForKey:@"NSStackViewOrientation"])
      _orientation=[coder decodeIntegerForKey:@"NSStackViewOrientation"];
     if([coder containsValueForKey:@"NSStackViewSpacing"])
      _spacing=[coder decodeDoubleForKey:@"NSStackViewSpacing"];
     for(NSView *view in [coder decodeObjectForKey:@"NSStackViewArrangedSubviews"])
      [self addArrangedSubview:view];
    }
   }
   return self;
}

-(void)dealloc {
   [_arrangedSubviews release];
   [_gravities release];
   [_customSpacing release];
   [_visibilityPriorities release];
   [_stackConstraints release];
   [super dealloc];
}

-(BOOL)isFlipped {
   return YES;
}

-(void)_changed {
   [self setNeedsUpdateConstraints:YES];
}

-delegate { return _delegate; }
-(void)setDelegate:delegate { _delegate=delegate; }

-(NSUserInterfaceLayoutOrientation)orientation { return _orientation; }

-(void)setOrientation:(NSUserInterfaceLayoutOrientation)orientation {
   _orientation=orientation;
   // The default alignment is the other axis's center.
   if(_alignment==NSLayoutAttributeCenterY || _alignment==NSLayoutAttributeCenterX)
    _alignment=(orientation==NSUserInterfaceLayoutOrientationHorizontal)?NSLayoutAttributeCenterY:NSLayoutAttributeCenterX;
   [self _changed];
}

-(NSLayoutAttribute)alignment { return _alignment; }
-(void)setAlignment:(NSLayoutAttribute)alignment { _alignment=alignment; [self _changed]; }
-(NSEdgeInsets)edgeInsets { return _edgeInsets; }
-(void)setEdgeInsets:(NSEdgeInsets)insets { _edgeInsets=insets; [self _changed]; }
-(NSStackViewDistribution)distribution { return _distribution; }
-(void)setDistribution:(NSStackViewDistribution)distribution { _distribution=distribution; [self _changed]; }
-(CGFloat)spacing { return _spacing; }
-(void)setSpacing:(CGFloat)spacing { _spacing=spacing; [self _changed]; }
-(BOOL)detachesHiddenViews { return _detachesHiddenViews; }
-(void)setDetachesHiddenViews:(BOOL)flag { _detachesHiddenViews=flag; [self _changed]; }

-(void)setCustomSpacing:(CGFloat)spacing afterView:(NSView *)view {
   NSValue *key=[NSValue valueWithNonretainedObject:view];

   if(spacing==NSStackViewSpacingUseDefault)
    [_customSpacing removeObjectForKey:key];
   else
    [_customSpacing setObject:[NSNumber numberWithDouble:spacing] forKey:key];
   [self _changed];
}

-(CGFloat)customSpacingAfterView:(NSView *)view {
   NSNumber *value=[_customSpacing objectForKey:[NSValue valueWithNonretainedObject:view]];

   return (value!=nil)?[value doubleValue]:NSStackViewSpacingUseDefault;
}

-(NSArray *)arrangedSubviews {
   return [[_arrangedSubviews copy] autorelease];
}

-(void)addArrangedSubview:(NSView *)view {
   [self insertArrangedSubview:view atIndex:[_arrangedSubviews count]];
}

-(void)insertArrangedSubview:(NSView *)view atIndex:(NSInteger)index {
   [view retain];
   [_arrangedSubviews removeObjectIdenticalTo:view];
   if(index>(NSInteger)[_arrangedSubviews count])
    index=[_arrangedSubviews count];
   [_arrangedSubviews insertObject:view atIndex:index];
   [_gravities insertObject:[NSNumber numberWithInteger:NSStackViewGravityCenter] atIndex:index];
   [view setTranslatesAutoresizingMaskIntoConstraints:NO];
   if([view superview]!=self)
    [self addSubview:view];
   [view release];
   [self _changed];
}

// The view stays a subview; it is just no longer arranged.
-(void)removeArrangedSubview:(NSView *)view {
   NSUInteger index=[_arrangedSubviews indexOfObjectIdenticalTo:view];

   if(index==NSNotFound)
    return;
   [_arrangedSubviews removeObjectAtIndex:index];
   [_gravities removeObjectAtIndex:index];
   [self _changed];
}

-(void)willRemoveSubview:(NSView *)subview {
   [self removeArrangedSubview:subview];
   [super willRemoveSubview:subview];
}

-(NSArray *)detachedViews {
   NSMutableArray *result=[NSMutableArray array];

   for(NSView *view in _arrangedSubviews)
    if(_detachesHiddenViews && [view isHidden])
     [result addObject:view];
   return result;
}

-(void)setVisibilityPriority:(NSStackViewVisibilityPriority)priority forView:(NSView *)view {
   [_visibilityPriorities setObject:[NSNumber numberWithFloat:priority] forKey:[NSValue valueWithNonretainedObject:view]];
   [self _changed];
}

-(NSStackViewVisibilityPriority)visibilityPriorityForView:(NSView *)view {
   NSNumber *value=[_visibilityPriorities objectForKey:[NSValue valueWithNonretainedObject:view]];

   return (value!=nil)?[value floatValue]:NSStackViewVisibilityPriorityMustHold;
}

-(NSLayoutPriority)clippingResistancePriorityForOrientation:(NSLayoutConstraintOrientation)o { return _clippingResistance[o&1]; }
-(void)setClippingResistancePriority:(NSLayoutPriority)p forOrientation:(NSLayoutConstraintOrientation)o { _clippingResistance[o&1]=p; [self _changed]; }
-(NSLayoutPriority)huggingPriorityForOrientation:(NSLayoutConstraintOrientation)o { return _hugging[o&1]; }
-(void)setHuggingPriority:(NSLayoutPriority)p forOrientation:(NSLayoutConstraintOrientation)o { _hugging[o&1]=p; [self _changed]; }

-(void)addView:(NSView *)view inGravity:(NSStackViewGravity)gravity {
   [self insertView:view atIndex:[[self viewsInGravity:gravity] count] inGravity:gravity];
}

// Gravity areas, in order: leading (top), center, trailing (bottom).
-(void)insertView:(NSView *)view atIndex:(NSUInteger)index inGravity:(NSStackViewGravity)gravity {
   NSUInteger position=0,seen=0,i;

   for(i=0;i<[_arrangedSubviews count];i++){
    NSStackViewGravity g=[[_gravities objectAtIndex:i] integerValue];

    if(g<gravity || (g==gravity && seen<index)){
     position=i+1;
     if(g==gravity)
      seen++;
    }
   }
   [self insertArrangedSubview:view atIndex:position];
   [_gravities replaceObjectAtIndex:position withObject:[NSNumber numberWithInteger:gravity]];
}

-(void)removeView:(NSView *)view {
   [self removeArrangedSubview:view];
   [view removeFromSuperview];
}

-(NSArray *)viewsInGravity:(NSStackViewGravity)gravity {
   NSMutableArray *result=[NSMutableArray array];

   for(NSUInteger i=0;i<[_arrangedSubviews count];i++)
    if([[_gravities objectAtIndex:i] integerValue]==gravity)
     [result addObject:[_arrangedSubviews objectAtIndex:i]];
   return result;
}

-(void)setViews:(NSArray *)views inGravity:(NSStackViewGravity)gravity {
   for(NSView *view in [self viewsInGravity:gravity])
    [self removeView:view];
   for(NSView *view in views)
    [self addView:view inGravity:gravity];
}

-(NSArray *)views {
   return [self arrangedSubviews];
}

-(void)_add:(NSLayoutConstraint *)constraint priority:(NSLayoutPriority)priority {
   [constraint setPriority:priority];
   [_stackConstraints addObject:constraint];
}

-(void)updateConstraints {
   BOOL horizontal=(_orientation==NSUserInterfaceLayoutOrientationHorizontal);
   NSLayoutAttribute leading=horizontal?NSLayoutAttributeLeading:NSLayoutAttributeTop;
   NSLayoutAttribute trailing=horizontal?NSLayoutAttributeTrailing:NSLayoutAttributeBottom;
   NSLayoutAttribute size=horizontal?NSLayoutAttributeWidth:NSLayoutAttributeHeight;
   NSLayoutAttribute crossStart=horizontal?NSLayoutAttributeTop:NSLayoutAttributeLeading;
   NSLayoutAttribute crossEnd=horizontal?NSLayoutAttributeBottom:NSLayoutAttributeTrailing;
   CGFloat insetLeading=horizontal?_edgeInsets.left:_edgeInsets.top;
   CGFloat insetTrailing=horizontal?_edgeInsets.right:_edgeInsets.bottom;
   CGFloat insetCrossStart=horizontal?_edgeInsets.top:_edgeInsets.left;
   CGFloat insetCrossEnd=horizontal?_edgeInsets.bottom:_edgeInsets.right;
   NSLayoutConstraintOrientation along=horizontal?NSLayoutConstraintOrientationHorizontal:NSLayoutConstraintOrientationVertical;
   NSLayoutConstraintOrientation across=horizontal?NSLayoutConstraintOrientationVertical:NSLayoutConstraintOrientationHorizontal;
   NSMutableArray *views=[NSMutableArray array];
   NSView *previous=nil;

   [self removeConstraints:_stackConstraints];
   [_stackConstraints removeAllObjects];

   for(NSView *view in _arrangedSubviews)
    if(!(_detachesHiddenViews && [view isHidden]))
     [views addObject:view];

   for(NSView *view in views){
    if(previous==nil)
     [self _add:[NSLayoutConstraint constraintWithItem:view attribute:leading relatedBy:NSLayoutRelationEqual toItem:self attribute:leading multiplier:1 constant:insetLeading] priority:NSLayoutPriorityRequired];
    else {
     CGFloat spacing=[self customSpacingAfterView:previous];
     NSLayoutRelation relation=(_distribution==NSStackViewDistributionEqualSpacing || _distribution==NSStackViewDistributionGravityAreas)?NSLayoutRelationGreaterThanOrEqual:NSLayoutRelationEqual;

     if(spacing==NSStackViewSpacingUseDefault)
      spacing=_spacing;
     [self _add:[NSLayoutConstraint constraintWithItem:view attribute:leading relatedBy:relation toItem:previous attribute:trailing multiplier:1 constant:spacing] priority:NSLayoutPriorityRequired];
     if(relation!=NSLayoutRelationEqual)
      [self _add:[NSLayoutConstraint constraintWithItem:view attribute:leading relatedBy:NSLayoutRelationEqual toItem:previous attribute:trailing multiplier:1 constant:spacing] priority:_hugging[along]];
     if(_distribution==NSStackViewDistributionFillEqually)
      [self _add:[NSLayoutConstraint constraintWithItem:view attribute:size relatedBy:NSLayoutRelationEqual toItem:[views objectAtIndex:0] attribute:size multiplier:1 constant:0] priority:NSLayoutPriorityRequired];
    }

    // Across: inside the insets, and placed by the alignment.
    [self _add:[NSLayoutConstraint constraintWithItem:view attribute:crossStart relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:self attribute:crossStart multiplier:1 constant:insetCrossStart] priority:_clippingResistance[across]];
    [self _add:[NSLayoutConstraint constraintWithItem:self attribute:crossEnd relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:view attribute:crossEnd multiplier:1 constant:insetCrossEnd] priority:_clippingResistance[across]];
    switch(_alignment){
     case NSLayoutAttributeNotAnAttribute:
      break;
     case NSLayoutAttributeWidth:
     case NSLayoutAttributeHeight:
      [self _add:[NSLayoutConstraint constraintWithItem:view attribute:_alignment relatedBy:NSLayoutRelationEqual toItem:self attribute:_alignment multiplier:1 constant:-(insetCrossStart+insetCrossEnd)] priority:NSLayoutPriorityRequired];
      break;
     default:{
       CGFloat offset=0;

       if(_alignment==NSLayoutAttributeTop || _alignment==NSLayoutAttributeLeading || _alignment==NSLayoutAttributeLeft)
        offset=insetCrossStart;
       else if(_alignment==NSLayoutAttributeBottom || _alignment==NSLayoutAttributeTrailing || _alignment==NSLayoutAttributeRight)
        offset=-insetCrossEnd;
       [self _add:[NSLayoutConstraint constraintWithItem:view attribute:_alignment relatedBy:NSLayoutRelationEqual toItem:self attribute:_alignment multiplier:1 constant:offset] priority:NSLayoutPriorityRequired];
      }
      break;
    }
    // Hugging across: as close to the views as they allow, on both sides.
    [self _add:[NSLayoutConstraint constraintWithItem:view attribute:crossStart relatedBy:NSLayoutRelationEqual toItem:self attribute:crossStart multiplier:1 constant:insetCrossStart] priority:_hugging[across]];
    [self _add:[NSLayoutConstraint constraintWithItem:self attribute:crossEnd relatedBy:NSLayoutRelationEqual toItem:view attribute:crossEnd multiplier:1 constant:insetCrossEnd] priority:_hugging[across]];
    previous=view;
   }

   if(previous!=nil){
    NSLayoutRelation relation=(_distribution==NSStackViewDistributionGravityAreas || _distribution==NSStackViewDistributionEqualSpacing)?NSLayoutRelationGreaterThanOrEqual:NSLayoutRelationEqual;

    [self _add:[NSLayoutConstraint constraintWithItem:self attribute:trailing relatedBy:relation toItem:previous attribute:trailing multiplier:1 constant:insetTrailing] priority:NSLayoutPriorityRequired];
    [self _add:[NSLayoutConstraint constraintWithItem:self attribute:trailing relatedBy:NSLayoutRelationEqual toItem:previous attribute:trailing multiplier:1 constant:insetTrailing] priority:_hugging[along]];
   }

   [self addConstraints:_stackConstraints];
   [super updateConstraints];
}

@end
