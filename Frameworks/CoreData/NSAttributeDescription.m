/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSAttributeDescription.h>
#import <CoreData/NSEntityDescription.h>
#import <Foundation/Foundation.h>
#import "NSPropertyDescription-Private.h"

@implementation NSAttributeDescription

// The class of an attribute's values, for each type that has one.
static NSString *valueClassNameForType(NSAttributeType type) {
   switch(type){
    case NSInteger16AttributeType:
    case NSInteger32AttributeType:
    case NSInteger64AttributeType:
    case NSDoubleAttributeType:
    case NSFloatAttributeType:
    case NSBooleanAttributeType:
     return @"NSNumber";
    case NSDecimalAttributeType:
     return @"NSDecimalNumber";
    case NSStringAttributeType:
     return @"NSString";
    case NSDateAttributeType:
     return @"NSDate";
    case NSBinaryDataAttributeType:
     return @"NSData";
    case NSUUIDAttributeType:
     return @"NSUUID";
    case NSURIAttributeType:
     return @"NSURL";
    case NSObjectIDAttributeType:
     return @"NSManagedObjectID";
    default:
     return nil;
   }
}

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])==nil)
    return nil;

   _attributeType=[coder decodeIntegerForKey:@"NSAttributeType"];
   _valueClassName=[[coder decodeObjectForKey:@"NSAttributeValueClassName"] copy];
   _defaultValue=[[coder decodeObjectForKey:@"NSDefaultValue"] retain];
   _valueTransformerName=[[coder decodeObjectForKey:@"NSValueTransformerName"] copy];
   _allowsExternalBinaryDataStorage=[coder decodeBoolForKey:@"NSAllowsExternalBinaryDataStorage"];
   _preservesValueInHistoryOnDeletion=[coder decodeBoolForKey:@"NSPreservesValueInHistoryOnDeletion"];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [super encodeWithCoder:coder];
   [coder encodeInteger:_attributeType forKey:@"NSAttributeType"];
   [coder encodeObject:_valueClassName forKey:@"NSAttributeValueClassName"];
   [coder encodeObject:_defaultValue forKey:@"NSDefaultValue"];
   [coder encodeObject:_valueTransformerName forKey:@"NSValueTransformerName"];
   [coder encodeBool:_allowsExternalBinaryDataStorage forKey:@"NSAllowsExternalBinaryDataStorage"];
   [coder encodeBool:_preservesValueInHistoryOnDeletion forKey:@"NSPreservesValueInHistoryOnDeletion"];
}

-(void)dealloc {
   [_valueClassName release];
   [_defaultValue release];
   [_valueTransformerName release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSAttributeDescription *result=[super copyWithZone:zone];

   result->_attributeType=_attributeType;
   result->_valueClassName=[_valueClassName copy];
   result->_defaultValue=[_defaultValue retain];
   result->_valueTransformerName=[_valueTransformerName copy];
   result->_allowsExternalBinaryDataStorage=_allowsExternalBinaryDataStorage;
   result->_preservesValueInHistoryOnDeletion=_preservesValueInHistoryOnDeletion;
   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"%@, attributeType %lu, attributeValueClassName %@, defaultValue %@",[super description],(unsigned long)_attributeType,[self attributeValueClassName],_defaultValue];
}

-(NSAttributeType)attributeType {
   return _attributeType;
}

// Setting the type sets the class of its values.
-(void)setAttributeType:(NSAttributeType)value {
   [self _checkMutable];
   _attributeType=value;
   [_valueClassName release];
   _valueClassName=[valueClassNameForType(value) copy];
}

-(NSString *)attributeValueClassName {
   return _valueClassName;
}

-(void)setAttributeValueClassName:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_valueClassName release];
   _valueClassName=value;
}

-defaultValue {
   return _defaultValue;
}

-(void)setDefaultValue:value {
   [self _checkMutable];
   value=[value retain];
   [_defaultValue release];
   _defaultValue=value;
}

-(NSString *)valueTransformerName {
   return _valueTransformerName;
}

-(void)setValueTransformerName:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_valueTransformerName release];
   _valueTransformerName=value;
}

-(BOOL)allowsExternalBinaryDataStorage {
   return _allowsExternalBinaryDataStorage;
}

-(void)setAllowsExternalBinaryDataStorage:(BOOL)value {
   [self _checkMutable];
   _allowsExternalBinaryDataStorage=value;
}

-(BOOL)preservesValueInHistoryOnDeletion {
   return _preservesValueInHistoryOnDeletion;
}

-(void)setPreservesValueInHistoryOnDeletion:(BOOL)value {
   [self _checkMutable];
   _preservesValueInHistoryOnDeletion=value;
}

-(void)_appendVersionHashFields:(NSMutableArray *)fields {
   [super _appendVersionHashFields:fields];
   [fields addObject:[NSString stringWithFormat:@"%lu",(unsigned long)_attributeType]];
}

@end
