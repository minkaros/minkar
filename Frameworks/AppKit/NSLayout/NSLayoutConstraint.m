/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"

@implementation NSLayoutConstraint

static NSString *attributeName(NSLayoutAttribute attribute) {
   switch(attribute){
    case NSLayoutAttributeLeft: return @"left";
    case NSLayoutAttributeRight: return @"right";
    case NSLayoutAttributeTop: return @"top";
    case NSLayoutAttributeBottom: return @"bottom";
    case NSLayoutAttributeLeading: return @"leading";
    case NSLayoutAttributeTrailing: return @"trailing";
    case NSLayoutAttributeWidth: return @"width";
    case NSLayoutAttributeHeight: return @"height";
    case NSLayoutAttributeCenterX: return @"centerX";
    case NSLayoutAttributeCenterY: return @"centerY";
    case NSLayoutAttributeLastBaseline: return @"lastBaseline";
    case NSLayoutAttributeFirstBaseline: return @"firstBaseline";
    default: return @"notAnAttribute";
   }
}

static BOOL isDimension(NSLayoutAttribute attribute) {
   return attribute==NSLayoutAttributeWidth || attribute==NSLayoutAttributeHeight;
}

static BOOL isHorizontal(NSLayoutAttribute attribute) {
   switch(attribute){
    case NSLayoutAttributeLeft: case NSLayoutAttributeRight: case NSLayoutAttributeLeading:
    case NSLayoutAttributeTrailing: case NSLayoutAttributeWidth: case NSLayoutAttributeCenterX:
     return YES;
    default:
     return NO;
   }
}

+(instancetype)constraintWithItem:view1 attribute:(NSLayoutAttribute)attr1 relatedBy:(NSLayoutRelation)relation toItem:view2 attribute:(NSLayoutAttribute)attr2 multiplier:(CGFloat)multiplier constant:(CGFloat)c {
   if(view1==nil || attr1==NSLayoutAttributeNotAnAttribute)
    [NSException raise:NSInvalidArgumentException format:@"A constraint needs a first item and attribute"];
   if(view2==nil && !isDimension(attr1))
    [NSException raise:NSInvalidArgumentException format:@"A constraint on %@ alone can't set a location (%@) to a constant",view1,attributeName(attr1)];
   if(view2!=nil && attr2==NSLayoutAttributeNotAnAttribute)
    [NSException raise:NSInvalidArgumentException format:@"A constraint with a second item needs its attribute"];
   if(view2!=nil && !isDimension(attr1) && !isDimension(attr2) && isHorizontal(attr1)!=isHorizontal(attr2))
    [NSException raise:NSInvalidArgumentException format:@"A constraint can't relate a horizontal location (%@) to a vertical one (%@)",
     attributeName(isHorizontal(attr1)?attr1:attr2),attributeName(isHorizontal(attr1)?attr2:attr1)];

   NSLayoutConstraint *result=[[[self alloc] init] autorelease];

   result->_firstItem=view1;
   result->_firstAttribute=attr1;
   result->_relation=relation;
   result->_secondItem=view2;
   result->_secondAttribute=(view2!=nil)?attr2:NSLayoutAttributeNotAnAttribute;
   result->_multiplier=(view2!=nil)?multiplier:0;
   result->_constant=c;
   result->_priority=NSLayoutPriorityRequired;
   result->_shouldBeArchived=NO;
   return result;
}

// The nearest view that has both items in its subtree (a guide counts as
// its owning view).
static NSView *commonAncestor(id first,id second) {
   NSView *a=NSLayoutItemView(first),*b=NSLayoutItemView(second);

   if(b==nil)
    return a;
   if(a==nil)
    return nil;
   for(NSView *check=a;check!=nil;check=[check superview])
    if(check==b || [b isDescendantOf:check])
     return check;
   return nil;
}

+(void)activateConstraints:(NSArray *)constraints {
   for(NSLayoutConstraint *constraint in constraints)
    [constraint setActive:YES];
}

+(void)deactivateConstraints:(NSArray *)constraints {
   for(NSLayoutConstraint *constraint in constraints)
    [constraint setActive:NO];
}

-init {
   _priority=NSLayoutPriorityRequired;
   _multiplier=1;
   return self;
}

-(void)dealloc {
   [_identifier release];
   [super dealloc];
}

-(BOOL)isActive {
   return _active;
}

// Active: installed on the items' nearest common ancestor.
-(void)setActive:(BOOL)active {
   if(active==_active)
    return;
   if(active){
    NSView *ancestor=commonAncestor(_firstItem,_secondItem);

    if(ancestor==nil)
     [NSException raise:NSGenericException format:@"Unable to activate constraint with anchors %@ and %@ because they have no common ancestor. Does the constraint or its anchors reference items in different view hierarchies? That's illegal.",[self firstAnchor],[self secondAnchor]];
    [ancestor addConstraint:self];
   }
   else {
    for(NSView *check=NSLayoutItemView(_firstItem);check!=nil;check=[check superview]){
     NSViewLayoutState *state=NSViewLayoutStateIfAny(check);

     if([state->constraints indexOfObjectIdenticalTo:self]!=NSNotFound){
      [check removeConstraint:self];
      break;
     }
    }
    _active=NO;
   }
}

-(void)_setActiveFlag:(BOOL)flag {
   _active=flag;
}

-(NSLayoutPriority)priority {
   return _priority;
}

-(void)setPriority:(NSLayoutPriority)priority {
   if(_active && ((_priority>=NSLayoutPriorityRequired)!=(priority>=NSLayoutPriorityRequired)))
    [NSException raise:NSInternalInconsistencyException format:@"Mutating a priority from required to not on an installed constraint (or vice-versa) is not supported. You passed priority %g and the existing priority was %g.",priority,_priority];
   _priority=priority;
   if(_active)
    NSLayoutSetNeedsLayout(_firstItem);
}

-(CGFloat)constant {
   return _constant;
}

-(void)setConstant:(CGFloat)constant {
   _constant=constant;
   if(_active)
    NSLayoutSetNeedsLayout(_firstItem);
}

-(BOOL)shouldBeArchived { return _shouldBeArchived; }
-(void)setShouldBeArchived:(BOOL)value { _shouldBeArchived=value; }
-firstItem { return _firstItem; }
-secondItem { return _secondItem; }
-(NSLayoutAttribute)firstAttribute { return _firstAttribute; }
-(NSLayoutAttribute)secondAttribute { return _secondAttribute; }
-(NSLayoutRelation)relation { return _relation; }
-(CGFloat)multiplier { return _multiplier; }

-(NSString *)identifier {
   return _identifier;
}

-(void)setIdentifier:(NSString *)identifier {
   identifier=[identifier copy];
   [_identifier release];
   _identifier=identifier;
}

static Class anchorClassFor(NSLayoutAttribute attribute) {
   if(isDimension(attribute))
    return [NSLayoutDimension class];
   return isHorizontal(attribute)?[NSLayoutXAxisAnchor class]:[NSLayoutYAxisAnchor class];
}

-(NSLayoutAnchor *)firstAnchor {
   return NSLayoutAnchorFor(_firstItem,_firstAttribute,anchorClassFor(_firstAttribute));
}

-(NSLayoutAnchor *)secondAnchor {
   if(_secondItem==nil)
    return nil;
   return NSLayoutAnchorFor(_secondItem,_secondAttribute,anchorClassFor(_secondAttribute));
}

static NSString *itemName(id item) {
   return [NSString stringWithFormat:@"%@:%p",[item class],item];
}

-(NSString *)_engineDescription {
   NSString *relation=(_relation==NSLayoutRelationEqual)?@"==":(_relation==NSLayoutRelationLessThanOrEqual)?@"<=":@">=";
   NSMutableString *result=[NSMutableString stringWithFormat:@"%@.%@ %@ ",itemName(_firstItem),attributeName(_firstAttribute),relation];

   if(_secondItem!=nil){
    if(_multiplier!=1)
     [result appendFormat:@"%g*",_multiplier];
    [result appendFormat:@"%@.%@",itemName(_secondItem),attributeName(_secondAttribute)];
    if(_constant!=0)
     [result appendFormat:@" %@ %g",_constant<0?@"-":@"+",fabs(_constant)];
   }
   else
    [result appendFormat:@"%g",_constant];
   if(_priority<NSLayoutPriorityRequired)
    [result appendFormat:@" @%g",_priority];
   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@:%p %@%@%@>",[self class],self,_identifier?[NSString stringWithFormat:@"'%@' ",_identifier]:@"",[self _engineDescription],_active?@"":@" (inactive)"];
}

@end

// The names are the macro's arguments as written: "a, b, self.c".
NSDictionary *_NSDictionaryOfVariableBindings(NSString *keys,id firstValue,...) {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];
   NSArray             *names=[keys componentsSeparatedByString:@","];
   va_list              arguments;
   id                   value=firstValue;
   NSUInteger           i;

   va_start(arguments,firstValue);
   for(i=0;i<[names count] && value!=nil;i++){
    NSString *name=[[names objectAtIndex:i] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSRange   dot=[name rangeOfString:@"." options:NSBackwardsSearch];

    if(dot.location!=NSNotFound)
     name=[name substringFromIndex:NSMaxRange(dot)];
    [result setObject:value forKey:name];
    value=va_arg(arguments,id);
   }
   va_end(arguments);
   return result;
}
