/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSRelationshipDescription.h>
#import <CoreData/NSEntityDescription.h>
#import <CoreData/NSManagedObjectModel.h>
#import <Foundation/Foundation.h>
#import "NSPropertyDescription-Private.h"

@implementation NSRelationshipDescription

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])==nil)
    return nil;

   _deleteRule=[coder decodeIntegerForKey:@"NSDeleteRule"];
   _destinationEntity=[coder decodeObjectForKey:@"NSDestinationEntity"];
   _inverseRelationship=[coder decodeObjectForKey:@"NSInverseRelationship"];
   _maxCount=[coder decodeIntegerForKey:@"NSMaxCount"];
   _minCount=[coder decodeIntegerForKey:@"NSMinCount"];
   _ordered=[coder decodeBoolForKey:@"NSIsOrdered"];
   _destinationEntityName=[[coder decodeObjectForKey:@"_NSDestinationEntityName"] copy];
   _inverseRelationshipName=[[coder decodeObjectForKey:@"_NSInverseRelationshipName"] copy];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [super encodeWithCoder:coder];
   [coder encodeInteger:_deleteRule forKey:@"NSDeleteRule"];
   [coder encodeConditionalObject:_destinationEntity forKey:@"NSDestinationEntity"];
   [coder encodeConditionalObject:_inverseRelationship forKey:@"NSInverseRelationship"];
   [coder encodeInteger:_maxCount forKey:@"NSMaxCount"];
   [coder encodeInteger:_minCount forKey:@"NSMinCount"];
   [coder encodeBool:_ordered forKey:@"NSIsOrdered"];
   [coder encodeObject:[_destinationEntity name] forKey:@"_NSDestinationEntityName"];
   [coder encodeObject:[_inverseRelationship name] forKey:@"_NSInverseRelationshipName"];
}

-(void)dealloc {
   [_destinationEntityName release];
   [_inverseRelationshipName release];
   [super dealloc];
}

// A copy keeps its destination and inverse, which belong to the model.
-copyWithZone:(NSZone *)zone {
   NSRelationshipDescription *result=[super copyWithZone:zone];

   result->_deleteRule=_deleteRule;
   result->_destinationEntity=_destinationEntity;
   result->_inverseRelationship=_inverseRelationship;
   result->_maxCount=_maxCount;
   result->_minCount=_minCount;
   result->_ordered=_ordered;
   result->_destinationEntityName=[_destinationEntityName copy];
   result->_inverseRelationshipName=[_inverseRelationshipName copy];
   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"%@, destination entity %@, inverse relationship %@, minCount %lu, maxCount %lu, isOrdered %d, deleteRule %lu",[super description],[[self destinationEntity] name],[[self inverseRelationship] name],(unsigned long)_minCount,(unsigned long)_maxCount,_ordered,(unsigned long)_deleteRule];
}

// A to-one relationship has a maximum of one; 0 means no maximum.
-(BOOL)isToMany {
   return _maxCount!=1;
}

-(NSUInteger)maxCount {
   return _maxCount;
}

-(void)setMaxCount:(NSUInteger)value {
   [self _checkMutable];
   _maxCount=value;
}

-(NSUInteger)minCount {
   return _minCount;
}

-(void)setMinCount:(NSUInteger)value {
   [self _checkMutable];
   _minCount=value;
}

-(NSDeleteRule)deleteRule {
   return _deleteRule;
}

-(void)setDeleteRule:(NSDeleteRule)value {
   [self _checkMutable];
   _deleteRule=value;
}

-(BOOL)isOrdered {
   return _ordered;
}

-(void)setOrdered:(BOOL)value {
   [self _checkMutable];
   _ordered=value;
}

-(NSEntityDescription *)destinationEntity {
   if(_destinationEntity==nil && _destinationEntityName!=nil)
    _destinationEntity=[[[_entity managedObjectModel] entitiesByName] objectForKey:_destinationEntityName];
   return _destinationEntity;
}

-(void)setDestinationEntity:(NSEntityDescription *)value {
   [self _checkMutable];
   _destinationEntity=value;
   [_destinationEntityName release];
   _destinationEntityName=[[value name] copy];
}

-(NSRelationshipDescription *)inverseRelationship {
   if(_inverseRelationship==nil && _inverseRelationshipName!=nil)
    _inverseRelationship=[[[self destinationEntity] relationshipsByName] objectForKey:_inverseRelationshipName];
   return _inverseRelationship;
}

-(void)setInverseRelationship:(NSRelationshipDescription *)value {
   [self _checkMutable];
   _inverseRelationship=value;
   [_inverseRelationshipName release];
   _inverseRelationshipName=[[value name] copy];
}

-(void)_appendVersionHashFields:(NSMutableArray *)fields {
   [super _appendVersionHashFields:fields];
   [fields addObject:[[self destinationEntity] name]?:@""];
   [fields addObject:[[self inverseRelationship] name]?:@""];
   [fields addObject:[NSString stringWithFormat:@"%lu %lu %d",(unsigned long)_minCount,(unsigned long)_maxCount,_ordered]];
}

@end
