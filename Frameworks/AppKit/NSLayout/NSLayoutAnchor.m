/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"
#import <objc/runtime.h>

static char guideAnchorsKey;

// An item keeps its anchors: the same object each time it is asked.
id NSLayoutAnchorFor(id item,NSLayoutAttribute attribute,Class kind) {
   NSMutableDictionary *anchors;
   NSNumber            *key=[NSNumber numberWithInteger:attribute];
   id                   result;

   if([item isKindOfClass:[NSView class]])
    anchors=NSViewLayoutStateFor(item)->anchors;
   else {
    anchors=objc_getAssociatedObject(item,&guideAnchorsKey);
    if(anchors==nil){
     anchors=[NSMutableDictionary dictionary];
     objc_setAssociatedObject(item,&guideAnchorsKey,anchors,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
   }
   if((result=[anchors objectForKey:key])==nil){
    result=[[kind alloc] _initWithItem:item attribute:attribute];
    [anchors setObject:result forKey:key];
    [result release];
   }
   return result;
}

@implementation NSLayoutAnchor

-(instancetype)_initWithItem:item attribute:(NSLayoutAttribute)attribute {
   _item=item;
   _attribute=attribute;
   return self;
}

-(NSLayoutAttribute)_attribute {
   return _attribute;
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-initWithCoder:(NSCoder *)coder {
   [NSException raise:NSInvalidArgumentException format:@"Layout anchors are not decoded"];
   return nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [NSException raise:NSInvalidArgumentException format:@"Layout anchors are not encoded"];
}

-item {
   return _item;
}

-(NSString *)name {
   static NSString *names[]={ @"notAnAttribute",@"left",@"right",@"top",@"bottom",@"leading",@"trailing",@"width",
    @"height",@"centerX",@"centerY",@"lastBaseline",@"firstBaseline" };

   return (_attribute>=0 && _attribute<=12)?names[_attribute]:@"?";
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@:%p \"%@:%p.%@\">",[self class],self,[_item class],_item,[self name]];
}

-(BOOL)hasAmbiguousLayout {
   return NO;
}

-(NSArray *)constraintsAffectingLayout {
   NSMutableArray *result=[NSMutableArray array];

   for(NSView *check=NSLayoutItemView(_item);check!=nil;check=[check superview])
    for(NSLayoutConstraint *constraint in [check constraints])
     if(([constraint firstItem]==_item && [constraint firstAttribute]==_attribute) ||
        ([constraint secondItem]==_item && [constraint secondAttribute]==_attribute))
      [result addObject:constraint];
   return result;
}

-(NSLayoutConstraint *)_constraintTo:(NSLayoutAnchor *)anchor relation:(NSLayoutRelation)relation multiplier:(CGFloat)multiplier constant:(CGFloat)constant {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:relation
    toItem:anchor->_item attribute:anchor->_attribute multiplier:multiplier constant:constant];
}

-(NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor {
   return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:1 constant:0];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor {
   return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:1 constant:0];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor {
   return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:1 constant:0];
}

-(NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:1 constant:c];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:1 constant:c];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:1 constant:c];
}

@end

/* An anchor of the distance between two others: a dimension, through a
 * layout guide between them owned by the first's view. */
static NSLayoutDimension *offsetAnchor(NSLayoutAnchor *from,NSLayoutAnchor *to,BOOL horizontal) {
   NSLayoutGuide *guide=[[[NSLayoutGuide alloc] init] autorelease];
   NSView        *owner=NSLayoutItemView([from item]);

   [owner addLayoutGuide:guide];
   if(horizontal){
    [[[guide leadingAnchor] constraintEqualToAnchor:(NSLayoutXAxisAnchor *)from] setActive:YES];
    [[[guide trailingAnchor] constraintEqualToAnchor:(NSLayoutXAxisAnchor *)to] setActive:YES];
    return [guide widthAnchor];
   }
   [[[guide topAnchor] constraintEqualToAnchor:(NSLayoutYAxisAnchor *)from] setActive:YES];
   [[[guide bottomAnchor] constraintEqualToAnchor:(NSLayoutYAxisAnchor *)to] setActive:YES];
   return [guide heightAnchor];
}

@implementation NSLayoutXAxisAnchor

-(NSLayoutDimension *)anchorWithOffsetToAnchor:(NSLayoutXAxisAnchor *)other {
   return offsetAnchor(self,other,YES);
}

-(NSLayoutConstraint *)constraintEqualToSystemSpacingAfterAnchor:(NSLayoutXAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToSystemSpacingAfterAnchor:(NSLayoutXAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToSystemSpacingAfterAnchor:(NSLayoutXAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationLessThanOrEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

@end

@implementation NSLayoutYAxisAnchor

-(NSLayoutDimension *)anchorWithOffsetToAnchor:(NSLayoutYAxisAnchor *)other {
   return offsetAnchor(self,other,NO);
}

-(NSLayoutConstraint *)constraintEqualToSystemSpacingBelowAnchor:(NSLayoutYAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToSystemSpacingBelowAnchor:(NSLayoutYAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToSystemSpacingBelowAnchor:(NSLayoutYAxisAnchor *)anchor multiplier:(CGFloat)m {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:NSLayoutRelationLessThanOrEqual toItem:[anchor item] attribute:[anchor _attribute] multiplier:1 constant:NSLayoutStandardSpacing*m];
}

@end

@implementation NSLayoutDimension

-(NSLayoutConstraint *)_constant:(CGFloat)c relation:(NSLayoutRelation)relation {
   return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:relation toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:0 constant:c];
}

-(NSLayoutConstraint *)constraintEqualToConstant:(CGFloat)c { return [self _constant:c relation:NSLayoutRelationEqual]; }
-(NSLayoutConstraint *)constraintGreaterThanOrEqualToConstant:(CGFloat)c { return [self _constant:c relation:NSLayoutRelationGreaterThanOrEqual]; }
-(NSLayoutConstraint *)constraintLessThanOrEqualToConstant:(CGFloat)c { return [self _constant:c relation:NSLayoutRelationLessThanOrEqual]; }

-(NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m {
   return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:m constant:0];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m {
   return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:m constant:0];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m {
   return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:m constant:0];
}

-(NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:m constant:c];
}

-(NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:m constant:c];
}

-(NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c {
   return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:m constant:c];
}

@end
