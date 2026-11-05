/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSComparisonPredicate.h>
#import <Foundation/NSExpression.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSOrderedSet.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRegularExpression.h>
#import <Foundation/NSCoder.h>
#import "NSExpression_operator.h"
#import <stdlib.h>

@implementation NSComparisonPredicate

-initWithLeftExpression:(NSExpression *)left rightExpression:(NSExpression *)right modifier:(NSComparisonPredicateModifier)modifier type:(NSPredicateOperatorType)type options:(NSComparisonPredicateOptions)options {
   _left=[left retain];
   _right=[right retain];
   _modifier=modifier;
   _type=type;
   _options=options;
   _customSelector=NULL;
   return self;
}

-initWithLeftExpression:(NSExpression *)left rightExpression:(NSExpression *)right customSelector:(SEL)selector {
   _left=[left retain];
   _right=[right retain];
   _modifier=NSDirectPredicateModifier;
   _type=NSCustomSelectorPredicateOperatorType;
   _options=0;
   _customSelector=selector;
   return self;
}

-(void)dealloc {
   [_left release];
   [_right release];
   [super dealloc];
}

// Read back from its format, as NSPredicate does.
-initWithCoder:(NSCoder *)coder {
   return [super initWithCoder:coder];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

+(NSPredicate *)predicateWithLeftExpression:(NSExpression *)left rightExpression:(NSExpression *)right modifier:(NSComparisonPredicateModifier)modifier type:(NSPredicateOperatorType)type options:(NSComparisonPredicateOptions)options {
   return [[[self alloc] initWithLeftExpression:left rightExpression:right modifier:modifier type:type options:options] autorelease];
}

+(NSPredicate *)predicateWithLeftExpression:(NSExpression *)left rightExpression:(NSExpression *)right customSelector:(SEL)selector {
   return [[[self alloc] initWithLeftExpression:left rightExpression:right customSelector:selector] autorelease];
}

-(NSString *)predicateFormat {
   NSString *modifier=@"";
   NSString *operator=nil;
   NSMutableString *options=[NSMutableString string];

   if(_type==NSCustomSelectorPredicateOperatorType)
    return [NSString stringWithFormat:@"%@ %s %@",_left,sel_getName(_customSelector),_right];

   switch(_modifier){
    case NSDirectPredicateModifier: break;
    case NSAllPredicateModifier: modifier=@"ALL "; break;
    case NSAnyPredicateModifier: modifier=@"ANY "; break;
   }

   if(_options&(NSCaseInsensitivePredicateOption|NSDiacriticInsensitivePredicateOption|NSNormalizedPredicateOption)){
    [options appendString:@"["];
    if(_options&NSCaseInsensitivePredicateOption)
     [options appendString:@"c"];
    if(_options&NSDiacriticInsensitivePredicateOption)
     [options appendString:@"d"];
    if(_options&NSNormalizedPredicateOption)
     [options appendString:@"n"];
    [options appendString:@"]"];
   }

   switch(_type){
    case NSLessThanPredicateOperatorType: operator=@"<"; break;
    case NSLessThanOrEqualToPredicateOperatorType: operator=@"<="; break;
    case NSGreaterThanPredicateOperatorType: operator=@">"; break;
    case NSGreaterThanOrEqualToPredicateOperatorType: operator=@">="; break;
    case NSEqualToPredicateOperatorType: operator=@"=="; break;
    case NSNotEqualToPredicateOperatorType: operator=@"!="; break;
    case NSMatchesPredicateOperatorType: operator=@"MATCHES"; break;
    case NSLikePredicateOperatorType: operator=@"LIKE"; break;
    case NSBeginsWithPredicateOperatorType: operator=@"BEGINSWITH"; break;
    case NSEndsWithPredicateOperatorType: operator=@"ENDSWITH"; break;
    case NSInPredicateOperatorType: operator=@"IN"; break;
    case NSContainsPredicateOperatorType: operator=@"CONTAINS"; break;
    case NSBetweenPredicateOperatorType: operator=@"BETWEEN"; break;
    default: operator=@"?"; break;
   }

   return [NSString stringWithFormat:@"%@%@ %@%@ %@",modifier,_left,operator,options,_right];
}

-(NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables {
   NSExpression *left=[_left _expressionWithSubstitutionVariables:variables];
   NSExpression *right=[_right _expressionWithSubstitutionVariables:variables];

   if(_type!=NSCustomSelectorPredicateOperatorType)
    return [NSComparisonPredicate predicateWithLeftExpression:left rightExpression:right modifier:_modifier type:_type options:_options];
   else
    return [NSComparisonPredicate predicateWithLeftExpression:left rightExpression:right customSelector:_customSelector];
}

-(NSExpression *)leftExpression {
   return _left;
}

-(NSExpression *)rightExpression {
   return _right;
}

-(NSPredicateOperatorType)predicateOperatorType {
   return _type;
}

-(NSComparisonPredicateModifier)comparisonPredicateModifier {
   return _modifier;
}

-(NSComparisonPredicateOptions)options {
   return _options;
}

-(SEL)customSelector {
   return _customSelector;
}

static BOOL isNil(id value) {
   return value==nil || value==(id)[NSNull null];
}

static BOOL isCollection(id value) {
   return [value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSSet class]] ||
          [value isKindOfClass:[NSOrderedSet class]] || [value isKindOfClass:[NSDictionary class]];
}

static id<NSFastEnumeration> elementsOf(id collection) {
   if([collection isKindOfClass:[NSDictionary class]])
    return [collection allValues];
   return collection;
}

// The string as the options compare it: case folded, diacritics removed.
-(NSString *)_foldedString:(NSString *)string {
   if(_options&NSCaseInsensitivePredicateOption)
    string=[string lowercaseString];
   if(_options&NSDiacriticInsensitivePredicateOption)
    string=[string stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:nil];
   return string;
}

-(NSStringCompareOptions)_compareOptions {
   NSStringCompareOptions result=0;

   if(_options&NSCaseInsensitivePredicateOption)
    result|=NSCaseInsensitiveSearch;
   if(_options&NSDiacriticInsensitivePredicateOption)
    result|=NSDiacriticInsensitiveSearch;
   if(!(_options&(NSCaseInsensitivePredicateOption|NSDiacriticInsensitivePredicateOption)))
    result|=NSLiteralSearch;
   return result;
}

-(BOOL)_value:left isEqualTo:right {
   if([left isKindOfClass:[NSString class]] && [right isKindOfClass:[NSString class]])
    return [left compare:right options:[self _compareOptions]]==NSOrderedSame;
   return [left isEqual:right];
}

-(NSComparisonResult)_compare:left with:right {
   if([left isKindOfClass:[NSString class]] && [right isKindOfClass:[NSString class]])
    return [left compare:right options:[self _compareOptions]];
   return [left compare:right];
}

// LIKE: * matches any run of characters, ? any one character.
static BOOL likeMatch(const unichar *s,NSUInteger slen,const unichar *p,NSUInteger plen) {
   NSUInteger si=0,pi=0,star=NSNotFound,mark=0;

   while(si<slen){
    if(pi<plen && p[pi]=='\\' && pi+1<plen){
     if(s[si]==p[pi+1]){ si++; pi+=2; continue; }
    }
    else if(pi<plen && (p[pi]=='?' || p[pi]==s[si])){ si++; pi++; continue; }
    else if(pi<plen && p[pi]=='*'){ star=pi++; mark=si; continue; }

    if(star!=NSNotFound){ pi=star+1; si=++mark; continue; }
    return NO;
   }
   while(pi<plen && p[pi]=='*')
    pi++;
   return pi==plen;
}

-(BOOL)_string:(NSString *)string isLike:(NSString *)pattern {
   string=[self _foldedString:string];
   pattern=[self _foldedString:pattern];

   NSUInteger slen=[string length],plen=[pattern length];
   unichar   *s=malloc(sizeof(unichar)*(slen+1)),*p=malloc(sizeof(unichar)*(plen+1));
   BOOL       result;

   [string getCharacters:s];
   [pattern getCharacters:p];
   result=likeMatch(s,slen,p,plen);
   free(s);
   free(p);
   return result;
}

-(BOOL)_string:(NSString *)string matches:(NSString *)pattern {
   NSRegularExpressionOptions options=0;
   NSError *error=nil;

   if(_options&NSCaseInsensitivePredicateOption)
    options|=NSRegularExpressionCaseInsensitive;
   if(_options&NSDiacriticInsensitivePredicateOption){
    string=[string stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:nil];
    pattern=[pattern stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:nil];
   }

   NSRegularExpression *regex=[NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"\\A(?:%@)\\z",pattern] options:options error:&error];

   if(regex==nil)
    [NSException raise:NSInvalidArgumentException format:@"Can't do regex matching, reason: %@",[error localizedDescription]];

   return [regex numberOfMatchesInString:string options:0 range:NSMakeRange(0,[string length])]>0;
}

-(BOOL)_collection:collection containsValue:value {
   for(id element in elementsOf(collection))
    if([self _value:element isEqualTo:value])
     return YES;
   return NO;
}

-(BOOL)_evaluateValue:leftResult rightValue:rightResult {
   BOOL leftIsNil=isNil(leftResult);
   BOOL rightIsNil=isNil(rightResult);

   switch(_type){
    case NSEqualToPredicateOperatorType:
     if(leftIsNil || rightIsNil)
      return leftIsNil && rightIsNil;
     return [self _value:leftResult isEqualTo:rightResult];

    case NSNotEqualToPredicateOperatorType:
     if(leftIsNil || rightIsNil)
      return !(leftIsNil && rightIsNil);
     return ![self _value:leftResult isEqualTo:rightResult];

    case NSCustomSelectorPredicateOperatorType:{
      if(leftIsNil)
       return NO;
      BOOL (*function)(id,SEL,id)=(BOOL (*)(id,SEL,id))[leftResult methodForSelector:_customSelector];

      return function(leftResult,_customSelector,rightIsNil?nil:rightResult);
     }

    default:
     break;
   }

   if(leftIsNil || rightIsNil)
    return NO;

   switch(_type){

    case NSLessThanPredicateOperatorType:
     return [self _compare:leftResult with:rightResult]==NSOrderedAscending;

    case NSLessThanOrEqualToPredicateOperatorType:
     return [self _compare:leftResult with:rightResult]!=NSOrderedDescending;

    case NSGreaterThanPredicateOperatorType:
     return [self _compare:leftResult with:rightResult]==NSOrderedDescending;

    case NSGreaterThanOrEqualToPredicateOperatorType:
     return [self _compare:leftResult with:rightResult]!=NSOrderedAscending;

    case NSMatchesPredicateOperatorType:
     return [self _string:[leftResult description] matches:[rightResult description]];

    case NSLikePredicateOperatorType:
     return [self _string:[leftResult description] isLike:[rightResult description]];

    case NSBeginsWithPredicateOperatorType:
     if([rightResult length]>[leftResult length])
      return NO;
     return [leftResult compare:rightResult options:[self _compareOptions] range:NSMakeRange(0,[rightResult length])]==NSOrderedSame;

    case NSEndsWithPredicateOperatorType:
     if([rightResult length]>[leftResult length])
      return NO;
     return [leftResult compare:rightResult options:[self _compareOptions] range:NSMakeRange([leftResult length]-[rightResult length],[rightResult length])]==NSOrderedSame;

    case NSInPredicateOperatorType:
     if(isCollection(rightResult))
      return [self _collection:rightResult containsValue:leftResult];
     if([rightResult isKindOfClass:[NSString class]] && [leftResult isKindOfClass:[NSString class]])
      return [rightResult rangeOfString:leftResult options:[self _compareOptions]].location!=NSNotFound;
     [NSException raise:NSInvalidArgumentException format:@"Can't look for value (%@) in string (%@); value is not a string",leftResult,rightResult];
     return NO;

    case NSContainsPredicateOperatorType:
     if(isCollection(leftResult))
      return [self _collection:leftResult containsValue:rightResult];
     if([leftResult isKindOfClass:[NSString class]] && [rightResult isKindOfClass:[NSString class]])
      return [leftResult rangeOfString:rightResult options:[self _compareOptions]].location!=NSNotFound;
     [NSException raise:NSInvalidArgumentException format:@"Can't use in/contains operator with collection %@ (not a collection)",leftResult];
     return NO;

    case NSBetweenPredicateOperatorType:{
      if(![rightResult isKindOfClass:[NSArray class]] || [rightResult count]!=2)
       [NSException raise:NSInvalidArgumentException format:@"BETWEEN needs a collection of two values, not %@",rightResult];

      id lower=[rightResult objectAtIndex:0],upper=[rightResult objectAtIndex:1];

      return [self _compare:leftResult with:lower]!=NSOrderedAscending &&
             [self _compare:leftResult with:upper]!=NSOrderedDescending;
     }

    default:
     return NO;
   }
}

-(BOOL)evaluateWithObject:object substitutionVariables:(NSDictionary *)bindings {
   NSMutableDictionary *context=(bindings!=nil)?[NSMutableDictionary dictionaryWithDictionary:bindings]:nil;
   id leftValue=[_left expressionValueWithObject:object context:context];
   id rightValue=[_right expressionValueWithObject:object context:context];

   if(_modifier==NSDirectPredicateModifier)
    return [self _evaluateValue:leftValue rightValue:rightValue];

   if(isNil(leftValue))
    return NO;
   if(!isCollection(leftValue))
    [NSException raise:NSInvalidArgumentException format:@"The left hand side for an ALL or ANY operator must be either an NSArray or an NSSet"];

   for(id value in elementsOf(leftValue)){
    BOOL eval=[self _evaluateValue:value rightValue:rightValue];

    if(_modifier==NSAllPredicateModifier && !eval)
     return NO;
    if(_modifier==NSAnyPredicateModifier && eval)
     return YES;
   }

   return (_modifier==NSAllPredicateModifier);
}

@end
