/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSPropertyDescription.h>
#import <CoreData/NSEntityDescription.h>
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#import "NSEntityDescription-Private.h"
#import "NSPropertyDescription-Private.h"

@implementation NSPropertyDescription

-init {
   _optional=YES;
   _userInfo=[[NSDictionary alloc] init];
   _validationPredicates=[[NSArray alloc] init];
   _validationWarnings=[[NSArray alloc] init];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];

   _entity=[coder decodeObjectForKey:@"NSEntity"];
   _propertyName=[[coder decodeObjectForKey:@"NSPropertyName"] copy];
   _optional=[coder containsValueForKey:@"NSIsOptional"]?[coder decodeBoolForKey:@"NSIsOptional"]:YES;
   _transient=[coder decodeBoolForKey:@"NSIsTransient"];
   _indexed=[coder decodeBoolForKey:@"NSIsIndexed"];
   _indexedBySpotlight=[coder decodeBoolForKey:@"NSIsIndexedBySpotlight"];
   _storedInExternalRecord=[coder decodeBoolForKey:@"NSIsStoredInExternalRecord"];
   _userInfo=[[coder decodeObjectForKey:@"NSUserInfo"] copy];
   if(_userInfo==nil)
    _userInfo=[[NSDictionary alloc] init];
   _validationPredicates=[[coder decodeObjectForKey:@"NSValidationPredicates"] copy];
   if(_validationPredicates==nil)
    _validationPredicates=[[NSArray alloc] init];
   _validationWarnings=[[coder decodeObjectForKey:@"NSValidationWarnings"] copy];
   if(_validationWarnings==nil)
    _validationWarnings=[[NSArray alloc] init];
   _versionHashModifier=[[coder decodeObjectForKey:@"NSVersionHashModifier"] copy];
   _renamingIdentifier=[[coder decodeObjectForKey:@"NSRenamingIdentifier"] copy];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not encodeWithCoder:%@",[self class],[coder class]];

   [coder encodeConditionalObject:_entity forKey:@"NSEntity"];
   [coder encodeObject:_propertyName forKey:@"NSPropertyName"];
   [coder encodeBool:_optional forKey:@"NSIsOptional"];
   [coder encodeBool:_transient forKey:@"NSIsTransient"];
   [coder encodeBool:_indexed forKey:@"NSIsIndexed"];
   [coder encodeBool:_indexedBySpotlight forKey:@"NSIsIndexedBySpotlight"];
   [coder encodeBool:_storedInExternalRecord forKey:@"NSIsStoredInExternalRecord"];
   if([_userInfo count])
    [coder encodeObject:_userInfo forKey:@"NSUserInfo"];
   if([_validationPredicates count]){
    [coder encodeObject:_validationPredicates forKey:@"NSValidationPredicates"];
    [coder encodeObject:_validationWarnings forKey:@"NSValidationWarnings"];
   }
   [coder encodeObject:_versionHashModifier forKey:@"NSVersionHashModifier"];
   [coder encodeObject:_renamingIdentifier forKey:@"NSRenamingIdentifier"];
}

-(void)dealloc {
   [_propertyName release];
   [_userInfo release];
   [_validationPredicates release];
   [_validationWarnings release];
   [_versionHashModifier release];
   [_renamingIdentifier release];
   [super dealloc];
}

// A copy belongs to no entity.
-copyWithZone:(NSZone *)zone {
   NSPropertyDescription *result=[[[self class] allocWithZone:zone] init];

   [result _copyPropertiesFrom:self];
   return result;
}

-(void)_copyPropertiesFrom:(NSPropertyDescription *)other {
   [self setName:other->_propertyName];
   _optional=other->_optional;
   _transient=other->_transient;
   _indexed=other->_indexed;
   _indexedBySpotlight=other->_indexedBySpotlight;
   _storedInExternalRecord=other->_storedInExternalRecord;
   [self setUserInfo:other->_userInfo];
   [self setValidationPredicates:other->_validationPredicates withValidationWarnings:other->_validationWarnings];
   [self setVersionHashModifier:other->_versionHashModifier];
   [self setRenamingIdentifier:other->_renamingIdentifier];
}

-(void)_checkMutable {
   [_entity _checkMutable];
}

-(void)_setEntity:(NSEntityDescription *)entity {
   _entity=entity;
}

-(NSEntityDescription *)entity {
   return _entity;
}

-(NSString *)name {
   return _propertyName;
}

-(void)setName:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_propertyName release];
   _propertyName=value;
}

-(BOOL)isOptional {
   return _optional;
}

-(void)setOptional:(BOOL)value {
   [self _checkMutable];
   _optional=value;
}

-(BOOL)isTransient {
   return _transient;
}

-(void)setTransient:(BOOL)value {
   [self _checkMutable];
   _transient=value;
}

-(BOOL)isIndexed {
   return _indexed;
}

-(void)setIndexed:(BOOL)value {
   [self _checkMutable];
   _indexed=value;
}

-(BOOL)isIndexedBySpotlight {
   return _indexedBySpotlight;
}

-(void)setIndexedBySpotlight:(BOOL)value {
   [self _checkMutable];
   _indexedBySpotlight=value;
}

-(BOOL)isStoredInExternalRecord {
   return _storedInExternalRecord;
}

-(void)setStoredInExternalRecord:(BOOL)value {
   [self _checkMutable];
   _storedInExternalRecord=value;
}

-(NSDictionary *)userInfo {
   return _userInfo;
}

-(void)setUserInfo:(NSDictionary *)value {
   [self _checkMutable];
   value=(value!=nil)?[value copy]:[[NSDictionary alloc] init];
   [_userInfo release];
   _userInfo=value;
}

-(NSArray *)validationPredicates {
   return _validationPredicates;
}

-(NSArray *)validationWarnings {
   return _validationWarnings;
}

-(void)setValidationPredicates:(NSArray *)predicates withValidationWarnings:(NSArray *)warnings {
   [self _checkMutable];
   if([predicates count]!=[warnings count])
    [NSException raise:NSInvalidArgumentException format:@"The validation predicates and warnings of %@ differ in number",_propertyName];
   predicates=(predicates!=nil)?[predicates copy]:[[NSArray alloc] init];
   warnings=(warnings!=nil)?[warnings copy]:[[NSArray alloc] init];
   [_validationPredicates release];
   [_validationWarnings release];
   _validationPredicates=predicates;
   _validationWarnings=warnings;
}

-(NSString *)versionHashModifier {
   return _versionHashModifier;
}

-(void)setVersionHashModifier:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_versionHashModifier release];
   _versionHashModifier=value;
}

-(NSString *)renamingIdentifier {
   return (_renamingIdentifier!=nil)?_renamingIdentifier:_propertyName;
}

-(void)setRenamingIdentifier:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_renamingIdentifier release];
   _renamingIdentifier=value;
}

// What the version hash covers: the property's name, its kind, whether it
// is optional, transient (and so on), and its modifier; subclasses add theirs.
-(void)_appendVersionHashFields:(NSMutableArray *)fields {
   [fields addObject:NSStringFromClass([self class])];
   [fields addObject:_propertyName?_propertyName:@""];
   [fields addObject:[NSString stringWithFormat:@"%d%d",_optional,_transient]];
   [fields addObject:_versionHashModifier?_versionHashModifier:@""];
}

-(NSData *)versionHash {
   NSMutableArray *fields=[NSMutableArray array];

   [self _appendVersionHashFields:fields];
   return _NSCoreDataVersionHash(fields);
}

-(NSString *)description {
   return [NSString stringWithFormat:@"(<%@: %p>), name %@, isOptional %d, isTransient %d, entity %@",[self class],self,_propertyName,_optional,_transient,[_entity name]];
}

@end

// A hash of the fields, joined: SHA-256, as 32 bytes.
NSData *_NSCoreDataVersionHash(NSArray *fields) {
   NSData       *bytes=[[fields componentsJoinedByString:@"\x1f"] dataUsingEncoding:NSUTF8StringEncoding];
   unsigned char digest[CC_SHA256_DIGEST_LENGTH];

   CC_SHA256([bytes bytes],(CC_LONG)[bytes length],digest);
   return [NSData dataWithBytes:digest length:sizeof(digest)];
}
