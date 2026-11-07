/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "NSEntityDescription-Private.h"
#import "NSPropertyDescription-Private.h"
#import "NSManagedObjectModel-Private.h"
#import <CoreData/NSManagedObjectContext.h>
#import <CoreData/NSManagedObjectModel.h>
#import <CoreData/NSPersistentStoreCoordinator.h>
#import <CoreData/NSPropertyDescription.h>
#import <CoreData/NSAttributeDescription.h>
#import <CoreData/NSRelationshipDescription.h>
#import <CoreData/NSManagedObject.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSRaise.h>

@implementation NSEntityDescription

-init {
   _properties=[[NSMutableArray alloc] init];
   _subentities=[[NSMutableArray alloc] init];
   _userInfo=[[NSDictionary alloc] init];
   _compoundIndexes=[[NSArray alloc] init];
   _uniquenessConstraints=[[NSArray alloc] init];
   return self;
}

// The values of an array, or of a dictionary (as older archives keep them).
static NSMutableArray *decodedArray(NSCoder *coder,NSString *key) {
   id value=[coder decodeObjectForKey:key];

   if([value isKindOfClass:[NSDictionary class]])
    return [[NSMutableArray alloc] initWithArray:[value allValues]];
   if([value isKindOfClass:[NSArray class]])
    return [value mutableCopy];
   return [[NSMutableArray alloc] init];
}

-initWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];

   _className=[[coder decodeObjectForKey:@"NSClassNameForEntity"] copy];
   _name=[[coder decodeObjectForKey:@"NSEntityName"] copy];
   _model=[coder decodeObjectForKey:@"NSManagedObjectModel"];
   _properties=decodedArray(coder,@"NSProperties");
   _subentities=decodedArray(coder,@"NSSubentities");
   _superentity=[coder decodeObjectForKey:@"NSSuperentity"];
   _userInfo=[[coder decodeObjectForKey:@"NSUserInfo"] copy];
   if(_userInfo==nil)
    _userInfo=[[NSDictionary alloc] init];
   _versionHashModifier=[[coder decodeObjectForKey:@"NSVersionHashModifier"] copy];
   _renamingIdentifier=[[coder decodeObjectForKey:@"NSRenamingIdentifier"] copy];
   _isAbstract=[coder decodeBoolForKey:@"NSIsAbstract"];
   _compoundIndexes=[[coder decodeObjectForKey:@"NSCompoundIndexes"] copy];
   if(_compoundIndexes==nil)
    _compoundIndexes=[[NSArray alloc] init];
   _uniquenessConstraints=[[coder decodeObjectForKey:@"NSUniquenessConstraints"] copy];
   if(_uniquenessConstraints==nil)
    _uniquenessConstraints=[[NSArray alloc] init];

   for(NSPropertyDescription *property in _properties)
    [property _setEntity:self];

   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not encodeWithCoder:%@",[self class],[coder class]];

   [coder encodeObject:_className forKey:@"NSClassNameForEntity"];
   [coder encodeObject:_name forKey:@"NSEntityName"];
   [coder encodeConditionalObject:_model forKey:@"NSManagedObjectModel"];
   [coder encodeObject:_properties forKey:@"NSProperties"];
   [coder encodeObject:_subentities forKey:@"NSSubentities"];
   [coder encodeConditionalObject:_superentity forKey:@"NSSuperentity"];
   if([_userInfo count])
    [coder encodeObject:_userInfo forKey:@"NSUserInfo"];
   [coder encodeObject:_versionHashModifier forKey:@"NSVersionHashModifier"];
   [coder encodeObject:_renamingIdentifier forKey:@"NSRenamingIdentifier"];
   [coder encodeBool:_isAbstract forKey:@"NSIsAbstract"];
   if([_compoundIndexes count])
    [coder encodeObject:_compoundIndexes forKey:@"NSCompoundIndexes"];
   if([_uniquenessConstraints count])
    [coder encodeObject:_uniquenessConstraints forKey:@"NSUniquenessConstraints"];
}

-(void)dealloc {
   for(NSPropertyDescription *property in _properties)
    [property _setEntity:nil];
   for(NSEntityDescription *subentity in _subentities)
    subentity->_superentity=nil;
   [_className release];
   [_name release];
   [_properties release];
   [_subentities release];
   [_userInfo release];
   [_versionHashModifier release];
   [_renamingIdentifier release];
   [_compoundIndexes release];
   [_uniquenessConstraints release];
   [_propertiesByName release];
   [super dealloc];
}

// A copy has copies of the properties, and no model.
-copyWithZone:(NSZone *)zone {
   NSEntityDescription *result=[[[self class] allocWithZone:zone] init];
   NSMutableArray      *properties=[NSMutableArray array];

   [result setName:_name];
   [result setManagedObjectClassName:_className];
   [result setAbstract:_isAbstract];
   [result setUserInfo:_userInfo];
   [result setVersionHashModifier:_versionHashModifier];
   [result setRenamingIdentifier:_renamingIdentifier];
   for(NSPropertyDescription *property in _properties)
    [properties addObject:[[property copy] autorelease]];
   [result setProperties:properties];
   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"(<%@: %p>) name %@, managedObjectClassName %@, superentity name %@, properties %@, subentities %@",[self class],self,_name,[self managedObjectClassName],[_superentity name],[[self propertiesByName] allKeys],[[self subentitiesByName] allKeys]];
}

// Any change to the model may change what an entity inherits.
-(void)_checkMutable {
   if(_model!=nil)
    [_model _checkMutable];
   else
    [self _invalidateCaches];
}

-(void)_invalidateCaches {
   [_propertiesByName release];
   _propertiesByName=nil;
   for(NSEntityDescription *subentity in _subentities)
    [subentity _invalidateCaches];
}

-(void)_setManagedObjectModel:(NSManagedObjectModel *)model {
   _model=model;
}

-(Class)_managedObjectClass {
   Class result=NSClassFromString([self managedObjectClassName]);

   return (result!=Nil)?result:[NSManagedObject class];
}

+(NSEntityDescription *)entityForName:(NSString *)entityName inManagedObjectContext:(NSManagedObjectContext *)context {
   NSPersistentStoreCoordinator *coordinator=[context persistentStoreCoordinator];

   if(coordinator==nil)
    [NSException raise:NSInternalInconsistencyException format:@"+entityForName: nil is not a legal NSPersistentStoreCoordinator for searching for entity name '%@'",entityName];

   return [[[coordinator managedObjectModel] entitiesByName] objectForKey:entityName];
}

+(id)insertNewObjectForEntityForName:(NSString *)entityName inManagedObjectContext:(NSManagedObjectContext *)context {
   NSEntityDescription *entity=[self entityForName:entityName inManagedObjectContext:context];

   if(entity==nil)
    [NSException raise:NSInternalInconsistencyException format:@"+entityForName: could not locate an entity named '%@' in this model.",entityName];

   return [[[[entity _managedObjectClass] alloc] initWithEntity:entity insertIntoManagedObjectContext:context] autorelease];
}

-(NSManagedObjectModel *)managedObjectModel {
   return _model;
}

-(NSString *)name {
   return _name;
}

-(void)setName:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_name release];
   _name=value;
}

-(BOOL)isAbstract {
   return _isAbstract;
}

-(void)setAbstract:(BOOL)value {
   [self _checkMutable];
   _isAbstract=value;
}

-(NSString *)managedObjectClassName {
   return (_className!=nil)?_className:@"NSManagedObject";
}

-(void)setManagedObjectClassName:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_className release];
   _className=value;
}

// The superentity's properties, then the entity's own, which may
// override them by name.
-(NSArray *)properties {
   if(_superentity==nil)
    return [[_properties copy] autorelease];

   NSMutableArray *result=[NSMutableArray array];
   NSSet          *own=[NSSet setWithArray:[_properties valueForKey:@"name"]];

   for(NSPropertyDescription *property in [_superentity properties])
    if(![own containsObject:[property name]])
     [result addObject:property];
   [result addObjectsFromArray:_properties];
   return result;
}

-(void)setProperties:(NSArray *)value {
   [self _checkMutable];

   NSMutableSet *names=[NSMutableSet set];

   for(NSPropertyDescription *property in value){
    if([property name]!=nil && [names containsObject:[property name]])
     [NSException raise:NSInvalidArgumentException format:@"Can't use the property name %@ twice in entity %@",[property name],_name];
    if([property name]!=nil)
     [names addObject:[property name]];
   }

   for(NSPropertyDescription *property in _properties)
    if([property entity]==self)
     [property _setEntity:nil];
   [_properties release];
   _properties=[value mutableCopy];
   for(NSPropertyDescription *property in _properties)
    [property _setEntity:self];
}

-(NSArray *)subentities {
   return [[_subentities copy] autorelease];
}

-(void)setSubentities:(NSArray *)value {
   [self _checkMutable];
   for(NSEntityDescription *subentity in _subentities)
    subentity->_superentity=nil;
   [_subentities release];
   _subentities=[value mutableCopy];
   for(NSEntityDescription *subentity in _subentities){
    subentity->_superentity=self;
    [subentity _invalidateCaches];
   }
}

-(NSDictionary *)subentitiesByName {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   for(NSEntityDescription *subentity in _subentities)
    if([subentity name]!=nil)
     [result setObject:subentity forKey:[subentity name]];
   return result;
}

-(NSEntityDescription *)superentity {
   return _superentity;
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

-(NSDictionary *)propertiesByName {
   if(_propertiesByName==nil){
    NSMutableDictionary *result=[NSMutableDictionary dictionary];

    for(NSPropertyDescription *property in [self properties])
     if([property name]!=nil)
      [result setObject:property forKey:[property name]];
    _propertiesByName=[result copy];
   }
   return _propertiesByName;
}

-(NSDictionary *)_propertiesByNameOfClass:(Class)class {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   for(NSPropertyDescription *property in [self properties])
    if([property isKindOfClass:class] && [property name]!=nil)
     [result setObject:property forKey:[property name]];
   return result;
}

-(NSDictionary *)attributesByName {
   return [self _propertiesByNameOfClass:[NSAttributeDescription class]];
}

-(NSDictionary *)relationshipsByName {
   return [self _propertiesByNameOfClass:[NSRelationshipDescription class]];
}

-(NSArray *)relationshipsWithDestinationEntity:(NSEntityDescription *)entity {
   NSMutableArray *result=[NSMutableArray array];

   for(NSRelationshipDescription *relationship in [[self relationshipsByName] allValues])
    if([relationship destinationEntity]==entity)
     [result addObject:relationship];
   return result;
}

-(BOOL)isKindOfEntity:(NSEntityDescription *)entity {
   return [self _isKindOfEntity:entity];
}

-(BOOL)_isKindOfEntity:(NSEntityDescription *)other {
   NSEntityDescription *check;

   for(check=self;check!=nil;check=check->_superentity)
    if(check==other)
     return YES;
   return NO;
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
   return (_renamingIdentifier!=nil)?_renamingIdentifier:_name;
}

-(void)setRenamingIdentifier:(NSString *)value {
   [self _checkMutable];
   value=[value copy];
   [_renamingIdentifier release];
   _renamingIdentifier=value;
}

-(NSArray *)compoundIndexes {
   return _compoundIndexes;
}

-(void)setCompoundIndexes:(NSArray *)value {
   [self _checkMutable];
   value=(value!=nil)?[value copy]:[[NSArray alloc] init];
   [_compoundIndexes release];
   _compoundIndexes=value;
}

-(NSArray *)uniquenessConstraints {
   return _uniquenessConstraints;
}

-(void)setUniquenessConstraints:(NSArray *)value {
   [self _checkMutable];
   value=(value!=nil)?[value copy]:[[NSArray alloc] init];
   [_uniquenessConstraints release];
   _uniquenessConstraints=value;
}

// What the store keeps for the entity: its name, its superentity's, and
// the hashes of its stored properties, in name order.
-(NSData *)versionHash {
   NSMutableArray *fields=[NSMutableArray array];
   NSArray        *names=[[[self propertiesByName] allKeys] sortedArrayUsingSelector:@selector(compare:)];

   [fields addObject:_name?_name:@""];
   [fields addObject:[_superentity name]?:@""];
   [fields addObject:[NSString stringWithFormat:@"%d",_isAbstract]];
   [fields addObject:_versionHashModifier?_versionHashModifier:@""];
   for(NSString *name in names){
    NSPropertyDescription *property=[[self propertiesByName] objectForKey:name];

    if(![property isTransient])
     [fields addObject:[[property versionHash] description]];
   }
   return _NSCoreDataVersionHash(fields);
}

// Enumerates the properties, as -properties gives them.
-(NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id *)buffer count:(NSUInteger)length {
   NSArray   *properties;
   NSUInteger index,count=0;

   if(state->state==0){
    state->state=1;
    state->extra[0]=(unsigned long)[self properties];
    state->extra[1]=0;
    state->mutationsPtr=&state->extra[2];
   }
   properties=(NSArray *)state->extra[0];
   for(index=state->extra[1];index<[properties count] && count<length;index++)
    buffer[count++]=[properties objectAtIndex:index];
   state->extra[1]=index;
   state->itemsPtr=buffer;
   return count;
}

@end
