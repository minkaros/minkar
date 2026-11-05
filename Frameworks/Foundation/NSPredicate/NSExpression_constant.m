/* Copyright (c) 2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "NSExpression_constant.h"
#import <Foundation/NSString.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSArray.h>

@implementation NSExpression_constant

-initWithValue:value {
   [super initWithExpressionType:NSConstantValueExpressionType];
   _value=[value retain]; // not copy despite the object being "constant"
   return self;
}

-(void)dealloc {
   [_value release];
   [super dealloc];
}

-constantValue {
   return _value;
}

-expressionValueWithObject:object context:(NSMutableDictionary *)context {
   return _value;
}

// A value as a predicate format writes it, so that parsing it gives it back.
static NSString *formatOfValue(id value) {
   if(value==nil || value==(id)[NSNull null])
    return @"nil";
   if([value isKindOfClass:[NSString class]]){
    NSMutableString *result=[NSMutableString stringWithString:value];

    [result replaceOccurrencesOfString:@"\\" withString:@"\\\\" options:0 range:NSMakeRange(0,[result length])];
    [result replaceOccurrencesOfString:@"\"" withString:@"\\\"" options:0 range:NSMakeRange(0,[result length])];
    return [NSString stringWithFormat:@"\"%@\"",result];
   }
   if([value isKindOfClass:[NSArray class]]){
    NSMutableArray *elements=[NSMutableArray array];

    for(id element in value)
     [elements addObject:formatOfValue(element)];
    return [NSString stringWithFormat:@"{%@}",[elements componentsJoinedByString:@", "]];
   }
   return [value description];
}

-(NSString *)description {
   return formatOfValue(_value);
}

@end
