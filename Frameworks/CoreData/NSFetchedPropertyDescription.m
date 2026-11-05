/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSFetchedPropertyDescription.h>
#import <CoreData/NSFetchRequest.h>
#import <Foundation/Foundation.h>
#import "NSPropertyDescription-Private.h"

@implementation NSFetchedPropertyDescription

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])==nil)
    return nil;
   _fetchRequest=[[coder decodeObjectForKey:@"NSFetchRequest"] retain];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [super encodeWithCoder:coder];
   [coder encodeObject:_fetchRequest forKey:@"NSFetchRequest"];
}

-(void)dealloc {
   [_fetchRequest release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSFetchedPropertyDescription *result=[super copyWithZone:zone];

   result->_fetchRequest=[_fetchRequest copy];
   return result;
}

-(NSFetchRequest *)fetchRequest {
   return _fetchRequest;
}

-(void)setFetchRequest:(NSFetchRequest *)value {
   [self _checkMutable];
   value=[value retain];
   [_fetchRequest release];
   _fetchRequest=value;
}

-(void)_appendVersionHashFields:(NSMutableArray *)fields {
   [super _appendVersionHashFields:fields];
   [fields addObject:[[_fetchRequest entity] name]?:@""];
   [fields addObject:[[_fetchRequest predicate] predicateFormat]?:@""];
}

@end
