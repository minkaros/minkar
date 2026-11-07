/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"

@implementation NSLayoutGuide

-(void)dealloc {
   [_identifier release];
   [super dealloc];
}

-initWithCoder:(NSCoder *)coder {
   _identifier=[[coder decodeObjectForKey:@"NSIdentifier"] copy];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_identifier forKey:@"NSIdentifier"];
}

-(NSRect)frame { return _frame; }
-(void)_setFrame:(NSRect)frame { _frame=frame; }
-(NSView *)owningView { return _owningView; }
-(void)setOwningView:(NSView *)view { _owningView=view; }
-(NSString *)identifier { return _identifier; }

-(void)setIdentifier:(NSString *)identifier {
   identifier=[identifier copy];
   [_identifier release];
   _identifier=identifier;
}

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

-(BOOL)hasAmbiguousLayout {
   return NO;
}

-(NSArray *)constraintsAffectingLayoutForOrientation:(NSLayoutConstraintOrientation)orientation {
   NSMutableArray *result=[NSMutableArray array];

   for(NSView *check=_owningView;check!=nil;check=[check superview])
    for(NSLayoutConstraint *constraint in [check constraints])
     if([constraint firstItem]==self || [constraint secondItem]==self)
      [result addObject:constraint];
   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@:%p '%@' %@>",[self class],self,_identifier,NSStringFromRect(_frame)];
}

@end
