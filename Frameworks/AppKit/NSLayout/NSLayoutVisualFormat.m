/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"

/* The visual format language:
 *   (H:|V:)? (|connection)? view (connection view)* (connection|)?
 *   view       [name(predicates)?]
 *   connection -predicates- | -number- | - | (nothing: flush)
 *   predicates (predicate,predicate...) or a bare constant or metric
 *   predicate  (==|<=|>=)? (number|metric|view) (@(number|metric))?
 * A lone "-" is the standard spacing: 8 between views, 20 to the
 * superview. Options align every view to the first by an attribute. */

@interface NSLayoutVisualFormat : NSObject {
   NSString     *_format;
   NSUInteger    _position;
   NSDictionary *_metrics;
   NSDictionary *_views;
   BOOL          _vertical;
   NSMutableArray *_result;
}
@end

@implementation NSLayoutVisualFormat

-(void)fail:(NSString *)reason {
   [NSException raise:NSInvalidArgumentException format:@"Unable to parse constraint format: \n%@ \n%@\n%@^",reason,_format,
    [@"" stringByPaddingToLength:_position withString:@" " startingAtIndex:0]];
}

-(unichar)peek {
   return (_position<[_format length])?[_format characterAtIndex:_position]:0;
}

-(BOOL)consume:(unichar)c {
   if([self peek]==c){
    _position++;
    return YES;
   }
   return NO;
}

-(void)expect:(unichar)c {
   if(![self consume:c])
    [self fail:[NSString stringWithFormat:@"Expected '%C'",c]];
}

-(NSString *)identifier {
   NSUInteger start=_position;

   while(_position<[_format length]){
    unichar c=[_format characterAtIndex:_position];

    if(c=='_' || (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9' && _position>start))
     _position++;
    else
     break;
   }
   return (_position>start)?[_format substringWithRange:NSMakeRange(start,_position-start)]:nil;
}

-(BOOL)number:(CGFloat *)value {
   NSUInteger start=_position;

   if([self peek]=='-' || [self peek]=='+')
    _position++;
   while((([self peek]>='0' && [self peek]<='9') || [self peek]=='.'))
    _position++;
   if(_position==start || (_position==start+1 && ([_format characterAtIndex:start]=='-' || [_format characterAtIndex:start]=='+'))){
    _position=start;
    return NO;
   }
   *value=[[_format substringWithRange:NSMakeRange(start,_position-start)] doubleValue];
   return YES;
}

// A constant or a metric's value.
-(BOOL)constant:(CGFloat *)value {
   NSUInteger start=_position;
   NSString  *name;

   if([self number:value])
    return YES;
   if((name=[self identifier])!=nil){
    id metric=[_metrics objectForKey:name];

    if(metric!=nil){
     *value=[metric doubleValue];
     return YES;
    }
   }
   _position=start;
   return NO;
}

typedef struct {
   NSLayoutRelation relation;
   CGFloat constant;
   NSView *view;
   NSLayoutPriority priority;
} predicate;

-(NSArray *)predicates {
   NSMutableArray *result=[NSMutableArray array];

   do {
    predicate p={ NSLayoutRelationEqual, 0, nil, NSLayoutPriorityRequired };
    NSString *name;

    if([self consume:'=']){ [self expect:'=']; p.relation=NSLayoutRelationEqual; }
    else if([self consume:'<']){ [self expect:'=']; p.relation=NSLayoutRelationLessThanOrEqual; }
    else if([self consume:'>']){ [self expect:'=']; p.relation=NSLayoutRelationGreaterThanOrEqual; }

    if(![self constant:&p.constant]){
     if((name=[self identifier])==nil || (p.view=[_views objectForKey:name])==nil)
      [self fail:@"Expected a number, a metric or a view"];
    }
    if([self consume:'@']){
     CGFloat priority;

     if(![self constant:&priority])
      [self fail:@"Expected a priority"];
     p.priority=priority;
    }
    [result addObject:[NSValue valueWithBytes:&p objCType:@encode(predicate)]];
   } while([self consume:',']);
   return result;
}

static predicate predicateAt(NSArray *predicates,NSUInteger index) {
   predicate p;

   [[predicates objectAtIndex:index] getValue:&p];
   return p;
}

// What lies between two views (or a view and the superview's edge).
-(NSArray *)connection:(BOOL *)present {
   *present=NO;
   if(![self consume:'-'])
    return [NSArray arrayWithObject:[self flush]];
   *present=YES;
   if([self consume:'(']){
    NSArray *result=[self predicates];

    [self expect:')'];
    [self expect:'-'];
    return result;
   }

   CGFloat value;
   NSUInteger start=_position;

   if([self constant:&value] && [self consume:'-']){
    predicate p={ NSLayoutRelationEqual, value, nil, NSLayoutPriorityRequired };

    return [NSArray arrayWithObject:[NSValue valueWithBytes:&p objCType:@encode(predicate)]];
   }
   _position=start;
   return nil; // the standard spacing
}

-(NSValue *)flush {
   predicate p={ NSLayoutRelationEqual, 0, nil, NSLayoutPriorityRequired };

   return [NSValue valueWithBytes:&p objCType:@encode(predicate)];
}

-(void)add:(NSLayoutConstraint *)constraint priority:(NSLayoutPriority)priority {
   [constraint setPriority:priority];
   [_result addObject:constraint];
}

-(NSLayoutAttribute)leading { return _vertical?NSLayoutAttributeTop:NSLayoutAttributeLeading; }
-(NSLayoutAttribute)trailing { return _vertical?NSLayoutAttributeBottom:NSLayoutAttributeTrailing; }
-(NSLayoutAttribute)size { return _vertical?NSLayoutAttributeHeight:NSLayoutAttributeWidth; }

// later.leading == earlier.trailing + spacing, for each predicate of it.
-(void)space:(id)earlier :(NSLayoutAttribute)earlierAttribute to:(id)later :(NSLayoutAttribute)laterAttribute predicates:(NSArray *)predicates standard:(CGFloat)standard {
   if(predicates==nil){
    predicate p={ NSLayoutRelationEqual, standard, nil, NSLayoutPriorityRequired };

    predicates=[NSArray arrayWithObject:[NSValue valueWithBytes:&p objCType:@encode(predicate)]];
   }
   for(NSUInteger i=0;i<[predicates count];i++){
    predicate p=predicateAt(predicates,i);

    if(p.view!=nil)
     [self fail:@"A view can't be a spacing"];
    [self add:[NSLayoutConstraint constraintWithItem:later attribute:laterAttribute relatedBy:p.relation
     toItem:earlier attribute:earlierAttribute multiplier:1 constant:p.constant] priority:p.priority];
   }
}

-(NSArray *)parseWithOptions:(NSLayoutFormatOptions)options {
   NSMutableArray *views=[NSMutableArray array];
   id      previous=nil;
   NSArray *pending=nil;
   BOOL     pendingPresent=NO,leadingEdge=NO;

   _result=[NSMutableArray array];
   if([_format hasPrefix:@"V:"]){ _vertical=YES; _position=2; }
   else if([_format hasPrefix:@"H:"]){ _position=2; }

   if([self consume:'|']){
    leadingEdge=YES;
    pending=[self connection:&pendingPresent];
   }

   while([self consume:'[']){
    NSString *name=[self identifier];
    NSView   *view=[_views objectForKey:name];

    if(view==nil)
     [self fail:[NSString stringWithFormat:@"%@ is not a key in the views dictionary.",name]];
    if([self consume:'(']){
     NSArray *predicates=[self predicates];

     [self expect:')'];
     for(NSUInteger i=0;i<[predicates count];i++){
      predicate p=predicateAt(predicates,i);

      if(p.view!=nil)
       [self add:[NSLayoutConstraint constraintWithItem:view attribute:[self size] relatedBy:p.relation toItem:p.view attribute:[self size] multiplier:1 constant:0] priority:p.priority];
      else
       [self add:[NSLayoutConstraint constraintWithItem:view attribute:[self size] relatedBy:p.relation toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:0 constant:p.constant] priority:p.priority];
     }
    }
    [self expect:']'];

    if(previous!=nil)
     [self space:previous :[self trailing] to:view :[self leading] predicates:pendingPresent?pending:(pending?pending:[NSArray arrayWithObject:[self flush]]) standard:NSLayoutStandardSpacing];
    else if(leadingEdge){
     NSView *superview=[view superview];

     if(superview==nil)
      [self fail:@"Unable to interpret '|' character, because the related view doesn't have a superview"];
     [self space:superview :[self leading] to:view :[self leading] predicates:pendingPresent?pending:(pending?pending:[NSArray arrayWithObject:[self flush]]) standard:NSLayoutStandardSuperviewSpacing];
    }
    [views addObject:view];
    previous=view;
    pending=[self connection:&pendingPresent];
   }

   if([self consume:'|']){
    NSView *superview=[previous superview];

    if(previous==nil || superview==nil)
     [self fail:@"Unable to interpret '|' character, because the related view doesn't have a superview"];
    [self space:previous :[self trailing] to:superview :[self trailing] predicates:pendingPresent?pending:(pending?pending:[NSArray arrayWithObject:[self flush]]) standard:NSLayoutStandardSuperviewSpacing];
   }
   else if(pendingPresent)
    [self fail:@"Expected a view or '|'"];

   if(_position!=[_format length])
    [self fail:@"Expected ']' or '|' or a connection"];

   // Every view aligned to the first, by each attribute the options name.
   for(NSLayoutAttribute attribute=NSLayoutAttributeLeft;attribute<=NSLayoutAttributeFirstBaseline;attribute++)
    if((options&NSLayoutFormatAlignmentMask)&(1<<attribute))
     for(NSUInteger i=1;i<[views count];i++)
      [_result addObject:[NSLayoutConstraint constraintWithItem:[views objectAtIndex:i] attribute:attribute relatedBy:NSLayoutRelationEqual
       toItem:[views objectAtIndex:0] attribute:attribute multiplier:1 constant:0]];
   return _result;
}

-initWithFormat:(NSString *)format metrics:(NSDictionary *)metrics views:(NSDictionary *)views {
   _format=[[format stringByReplacingOccurrencesOfString:@" " withString:@""] retain];
   _metrics=[metrics retain];
   _views=[views retain];
   return self;
}

-(void)dealloc {
   [_format release];
   [_metrics release];
   [_views release];
   [super dealloc];
}

@end

@implementation NSLayoutConstraint (NSVisualFormat)

+(NSArray *)constraintsWithVisualFormat:(NSString *)format options:(NSLayoutFormatOptions)options metrics:(NSDictionary *)metrics views:(NSDictionary *)views {
   NSLayoutVisualFormat *parser=[[[NSLayoutVisualFormat alloc] initWithFormat:format metrics:metrics views:views] autorelease];

   return [parser parseWithOptions:options];
}

@end
