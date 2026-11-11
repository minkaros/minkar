/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <Foundation/NSFormatter.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSCoder.h>

@implementation NSFormatter

/* Minkar: only stringForObjectValue: and getObjectValue:forString:... are
   for subclasses to define, as on the reference platform; the rest have
   these defaults (a formatter with only those two raised as soon as a
   field began editing). */
-(BOOL)isPartialStringValid:(NSString *)partial
   newEditingString:(NSString **)editing errorDescription:(NSString **)error {
   return YES;
}

-(BOOL)getObjectValue:(id *)object
   forString:(NSString *)string errorDescription:(NSString **)error {
   NSInvalidAbstractInvocation();
   return NO;
}

-(NSString *)stringForObjectValue:(id)object {
   NSInvalidAbstractInvocation();
   return nil;
}

-(NSString *)editingStringForObjectValue:(id)object {
   return [self stringForObjectValue:object];
}

-(NSAttributedString *)attributedStringForObjectValue:(id)object
   withDefaultAttributes:(NSDictionary *)attributes {
   return nil;
}

-(BOOL)isPartialStringValid:(NSString **)partialStringp proposedSelectedRange:(NSRange *)proposedRangep originalString:(NSString *)originalString originalSelectedRange:(NSRange)originalRange errorDescription:(NSString **)errorStringp {
   NSString *edited=nil;
   BOOL      valid=[self isPartialStringValid:*partialStringp newEditingString:&edited errorDescription:errorStringp];

   if(!valid && edited!=nil){
    *partialStringp=edited;
    if(proposedRangep!=NULL)
     *proposedRangep=NSMakeRange([edited length],0);
   }
   return valid;
}

-initWithCoder:(NSCoder*)coder {

	return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {


}

@end
