/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CTTypesetter on CTLine: the string kept, lines made from its ranges,
 * widths measured on them. Line breaks are found by the widths of the
 * string's prefixes from the start index, which grow with their length.
 */

#import <CoreText/CTTypesetter.h>
#import <Foundation/Foundation.h>
#import "CTTypes.h"

@interface CTTypesetter : NSObject {
@public
   NSAttributedString *_string;
}
@end

@implementation CTTypesetter
-(void)dealloc {
   [_string release];
   [super dealloc];
}
@end

CT_DEFINE_TYPE(CTTypesetter, "CTTypesetter")

CTTypesetterRef CTTypesetterCreateWithAttributedString(CFAttributedStringRef string) {
   return CTTypesetterCreateWithAttributedStringAndOptions(string, NULL);
}

CTTypesetterRef CTTypesetterCreateWithAttributedStringAndOptions(CFAttributedStringRef string, CFDictionaryRef options) {
   if(string==NULL)
    return NULL;
   CTTypesetter *typesetter=[[CTTypesetter alloc] init];
   typesetter->_string=[(NSAttributedString *)string copy];
   return (CTTypesetterRef)typesetter;
}

static NSAttributedString *stringOf(CTTypesetterRef typesetter) {
   return ((CTTypesetter *)typesetter)->_string;
}

CTLineRef CTTypesetterCreateLine(CTTypesetterRef typesetter, CFRange stringRange) {
   return CTTypesetterCreateLineWithOffset(typesetter, stringRange, 0);
}

CTLineRef CTTypesetterCreateLineWithOffset(CTTypesetterRef typesetter, CFRange stringRange, double offset) {
   if(typesetter==NULL)
    return NULL;
   NSAttributedString *string=stringOf(typesetter);
   NSUInteger length=[string length];
   NSUInteger location=MIN((NSUInteger)MAX(stringRange.location,0),length);
   NSUInteger count=stringRange.length>0?MIN((NSUInteger)stringRange.length,length-location):length-location;
   NSAttributedString *part=[string attributedSubstringFromRange:NSMakeRange(location,count)];
   return CTLineCreateWithAttributedString((CFAttributedStringRef)part);
}

/* The width of the characters [start, start+count). */
static double widthOf(CTTypesetterRef typesetter, CFIndex start, CFIndex count) {
   if(count<=0)
    return 0;
   CTLineRef line=CTTypesetterCreateLine(typesetter, CFRangeMake(start, count));
   double width=line!=NULL?CTLineGetTypographicBounds(line, NULL, NULL, NULL):0;
   if(line!=NULL)
    CFRelease(line);
   return width;
}

/* The cluster boundaries after start (each the length up to it). */
static NSArray *clusterEnds(NSString *string, CFIndex start) {
   NSMutableArray *ends=[NSMutableArray array];
   NSUInteger length=[string length], i=(NSUInteger)start;
   while(i<length){
    NSRange cluster=[string rangeOfComposedCharacterSequenceAtIndex:i];
    i=NSMaxRange(cluster);
    [ends addObject:[NSNumber numberWithInteger:(NSInteger)(i-(NSUInteger)start)]];
   }
   return ends;
}

/* The most clusters from start that fit in width (0 if none does). */
static CFIndex fittingCount(CTTypesetterRef typesetter, CFIndex start, double width, NSArray *ends) {
   NSInteger low=0, high=(NSInteger)[ends count];
   while(low<high){
    NSInteger mid=(low+high+1)/2;
    CFIndex count=[[ends objectAtIndex:(NSUInteger)(mid-1)] integerValue];
    if(widthOf(typesetter, start, count)<=width)
     low=mid;
    else
     high=mid-1;
   }
   return low==0?0:[[ends objectAtIndex:(NSUInteger)(low-1)] integerValue];
}

CFIndex CTTypesetterSuggestClusterBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width) {
   return CTTypesetterSuggestClusterBreakWithOffset(typesetter, startIndex, width, 0);
}

CFIndex CTTypesetterSuggestClusterBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset) {
   if(typesetter==NULL)
    return 0;
   NSString *string=[stringOf(typesetter) string];
   if(startIndex<0 || (NSUInteger)startIndex>=[string length])
    return 0;
   NSArray *ends=clusterEnds(string, startIndex);
   CFIndex count=fittingCount(typesetter, startIndex, width-offset, ends);
   /* At least one cluster. */
   return count>0?count:[[ends objectAtIndex:0] integerValue];
}

CFIndex CTTypesetterSuggestLineBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width) {
   return CTTypesetterSuggestLineBreakWithOffset(typesetter, startIndex, width, 0);
}

CFIndex CTTypesetterSuggestLineBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset) {
   if(typesetter==NULL)
    return 0;
   NSString *string=[stringOf(typesetter) string];
   NSUInteger length=[string length];
   if(startIndex<0 || (NSUInteger)startIndex>=length)
    return 0;
   NSArray *ends=clusterEnds(string, startIndex);
   CFIndex count=fittingCount(typesetter, startIndex, width-offset, ends);
   NSCharacterSet *spaces=[NSCharacterSet whitespaceAndNewlineCharacterSet];
   NSCharacterSet *newlines=[NSCharacterSet newlineCharacterSet];

   /* A newline within what fits ends the line there. */
   for(CFIndex i=0;i<count;i++)
    if([newlines characterIsMember:[string characterAtIndex:(NSUInteger)(startIndex+i)]])
     return i+1;
   /* All of the rest fits. */
   if((NSUInteger)(startIndex+count)>=length)
    return count>0?count:[[ends objectAtIndex:0] integerValue];
   /* Spaces after what fits go with the line. */
   CFIndex end=count;
   while((NSUInteger)(startIndex+end)<length && [spaces characterIsMember:[string characterAtIndex:(NSUInteger)(startIndex+end)]])
    end++;
   if(end>count)
    return end;
   /* Back to the last break: after a space. */
   for(CFIndex i=count;i>0;i--)
    if([spaces characterIsMember:[string characterAtIndex:(NSUInteger)(startIndex+i-1)]])
     return i;
   /* No word break: what fits, at least one cluster. */
   return count>0?count:[[ends objectAtIndex:0] integerValue];
}
