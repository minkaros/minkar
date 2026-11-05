/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>
#import <Foundation/NSSortDescriptor.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSCoder.h> 
#import <Foundation/NSKeyedUnarchiver.h>

@implementation NSSortDescriptor

+sortDescriptorWithKey:(NSString *)key ascending:(BOOL)ascending {
   return [[[NSSortDescriptor allocWithZone:NULL] initWithKey:key ascending:ascending selector:NULL] autorelease];
}

+sortDescriptorWithKey:(NSString *)key ascending:(BOOL)ascending selector:(SEL)selector {
   return [[[NSSortDescriptor allocWithZone:NULL] initWithKey:key ascending:ascending selector:selector] autorelease];
}

-initWithKey:(NSString *)key ascending:(BOOL)ascending {
   return [self initWithKey:key ascending:ascending selector:NULL];
}

-initWithKey:(NSString *)key ascending:(BOOL)ascending selector:(SEL)selector {
   _key=[key copy];
   _ascending=ascending;
   
   if(selector==NULL) // Yes it does this
    _selector=@selector(compare:);
   else
   _selector=selector;
   return self;
}

-(void)dealloc {
   [_key release];
   [_comparator release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-(NSString *)key {
   return _key;
}

-(BOOL)ascending {
   return _ascending;
}

-(SEL)selector {
   return _selector;
}

+(instancetype)sortDescriptorWithKey:(NSString *)key ascending:(BOOL)ascending comparator:(NSComparator)cmptr {
   return [[[self alloc] initWithKey:key ascending:ascending comparator:cmptr] autorelease];
}

/* Ordered by a block of the caller's instead of a selector. */
-(instancetype)initWithKey:(NSString *)key ascending:(BOOL)ascending comparator:(NSComparator)cmptr {
   self=[self initWithKey:key ascending:ascending selector:@selector(compare:)];
   _comparator=[cmptr copy];
   return self;
}

-(NSComparator)comparator {
   return _comparator;
}

/* Archived descriptors may be evaluated: nothing to allow here. */
-(void)allowEvaluation {
}

-(NSComparisonResult)compareObject:first toObject:second {
   id checkFirst=_key!=nil ? [first valueForKeyPath:_key] : first;
   id checkSecond=_key!=nil ? [second valueForKeyPath:_key] : second;
   NSComparisonResult result;

   if(_comparator!=nil)
    return _ascending ? _comparator(checkFirst,checkSecond) : _comparator(checkSecond,checkFirst);
   if(_ascending)
    result=(NSComparisonResult)[checkFirst performSelector:_selector withObject:checkSecond];
   else
    result=(NSComparisonResult)[checkSecond performSelector:_selector withObject:checkFirst];
   
   return result;
}

-reversedSortDescriptor {
   if(_comparator!=nil)
    return [[[object_getClass(self) alloc] initWithKey:_key ascending:!_ascending comparator:_comparator] autorelease];
   return [[[object_getClass(self) alloc] initWithKey:_key ascending:!_ascending selector:_selector] autorelease];
}

- (void)encodeWithCoder:(NSCoder *)coder { 
   if ([coder allowsKeyedCoding]) 
   { 
      [coder encodeObject:_key forKey:@"Key"]; 
      [coder encodeBool:_ascending forKey:@"Ascending"]; 
      [coder encodeObject:NSStringFromSelector(_selector) forKey: @"Selector"]; 
   } 
   else 
   { 
      [coder encodeObject:_key]; 
      [coder encodeValueOfObjCType:@encode(BOOL) at:&_ascending]; 
      [coder encodeObject:NSStringFromSelector(_selector)]; 
   } 
} 

- (id)initWithCoder:(NSCoder *)coder { 
   if ([coder allowsKeyedCoding]) 
   { 
      NSKeyedUnarchiver *keyed = (NSKeyedUnarchiver *)coder; 
      _key = [[keyed decodeObjectForKey:@"Key"] copy]; 
      _ascending = [keyed decodeBoolForKey:@"Ascending"]; 
      _selector = NSSelectorFromString([keyed decodeObjectForKey:@"Selector"]); 
   } 

   else 
   { 
      _key = [[coder decodeObject] copy]; 
      [coder decodeValueOfObjCType:@encode(BOOL) at:&_ascending]; 
      _selector = NSSelectorFromString([coder decodeObject]); 
   } 

   return self; 
}

@end
