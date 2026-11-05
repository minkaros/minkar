/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSManagedObjectModel.h>
#import <CoreData/NSEntityDescription.h>
#import <CoreData/NSFetchRequest.h>
#import <CoreData/NSPersistentStoreCoordinator.h>
#import "NSEntityDescription-Private.h"
#import "NSManagedObjectModel-Private.h"

#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSRaise.h>

@implementation NSManagedObjectModel

// The models in each bundle's resources: .mom files, and the current
// version of each .momd.
static NSArray *modelsInBundles(NSArray *bundles) {
   NSMutableArray *result=[NSMutableArray array];

   if(bundles==nil)
    bundles=[NSArray arrayWithObject:[NSBundle mainBundle]];

   for(NSBundle *bundle in bundles){
    NSMutableArray *paths=[NSMutableArray array];

    [paths addObjectsFromArray:[bundle pathsForResourcesOfType:@"mom" inDirectory:nil]];
    [paths addObjectsFromArray:[bundle pathsForResourcesOfType:@"momd" inDirectory:nil]];
    for(NSString *path in paths){
     NSManagedObjectModel *model=[[NSManagedObjectModel alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path]];

     if(model==nil)
      NSLog(@"-[NSManagedObjectModel initWithContentsOfURL:] failed. url=%@",path);
     else {
      [result addObject:model];
      [model release];
     }
    }
   }
   return result;
}

+(NSManagedObjectModel *)modelByMergingModels:(NSArray *)models {
   NSManagedObjectModel *result=[[[NSManagedObjectModel alloc] init] autorelease];
   NSMutableArray       *entities=[NSMutableArray array];

   for(NSManagedObjectModel *merge in models){
    [entities addObjectsFromArray:[merge entities]];
    [result->_fetchRequestTemplates addEntriesFromDictionary:merge->_fetchRequestTemplates];
    [result->_configurations addEntriesFromDictionary:merge->_configurations];
   }

   [result setEntities:entities];
   return result;
}

+(NSManagedObjectModel *)mergedModelFromBundles:(NSArray *)bundles {
   return [self modelByMergingModels:modelsInBundles(bundles)];
}

+(NSManagedObjectModel *)modelByMergingModels:(NSArray *)models forStoreMetadata:(NSDictionary *)metadata {
   NSMutableArray *compatible=[NSMutableArray array];

   for(NSManagedObjectModel *model in models)
    if([model isConfiguration:nil compatibleWithStoreMetadata:metadata])
     [compatible addObject:model];
   if([compatible count]==0)
    return nil;
   return [self modelByMergingModels:compatible];
}

+(NSManagedObjectModel *)mergedModelFromBundles:(NSArray *)bundles forStoreMetadata:(NSDictionary *)metadata {
   return [self modelByMergingModels:modelsInBundles(bundles) forStoreMetadata:metadata];
}

-init {
   _entities=[[NSMutableArray alloc] init];
   _fetchRequestTemplates=[[NSMutableDictionary alloc] init];
   _configurations=[[NSMutableDictionary alloc] init];
   _versionIdentifiers=[[NSSet alloc] init];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];

   id entities=[coder decodeObjectForKey:@"NSEntities"];

   if([entities isKindOfClass:[NSDictionary class]])
    _entities=[[NSMutableArray alloc] initWithArray:[entities allValues]];
   else if([entities isKindOfClass:[NSArray class]])
    _entities=[entities mutableCopy];
   else
    _entities=[[NSMutableArray alloc] init];

   _fetchRequestTemplates=[[coder decodeObjectForKey:@"NSFetchRequestTemplates"] mutableCopy];
   if(_fetchRequestTemplates==nil)
    _fetchRequestTemplates=[[NSMutableDictionary alloc] init];
   _configurations=[[coder decodeObjectForKey:@"NSConfigurations"] mutableCopy];
   if(_configurations==nil)
    _configurations=[[NSMutableDictionary alloc] init];
   _versionIdentifiers=[[coder decodeObjectForKey:@"NSVersionIdentifiers"] copy];
   if(_versionIdentifiers==nil)
    _versionIdentifiers=[[NSSet alloc] init];
   _localizationDictionary=[[coder decodeObjectForKey:@"NSLocalizationDictionary"] copy];

   for(NSEntityDescription *entity in _entities)
    [entity _setManagedObjectModel:self];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not encodeWithCoder:%@",[self class],[coder class]];

   [coder encodeObject:_entities forKey:@"NSEntities"];
   if([_fetchRequestTemplates count])
    [coder encodeObject:_fetchRequestTemplates forKey:@"NSFetchRequestTemplates"];
   if([_configurations count])
    [coder encodeObject:_configurations forKey:@"NSConfigurations"];
   if([_versionIdentifiers count])
    [coder encodeObject:_versionIdentifiers forKey:@"NSVersionIdentifiers"];
   [coder encodeObject:_localizationDictionary forKey:@"NSLocalizationDictionary"];
}

// A file written by a keyed archiver, with the model as its root; or a
// .momd directory, whose VersionInfo.plist names its current model.
-initWithContentsOfURL:(NSURL *)url {
   NSString *path=[url path];
   BOOL      isDirectory=NO;

   [self release];

   if(path==nil || ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory])
    return nil;

   if(isDirectory){
    NSDictionary *info=[NSDictionary dictionaryWithContentsOfFile:[path stringByAppendingPathComponent:@"VersionInfo.plist"]];
    NSString     *current=[info objectForKey:@"NSManagedObjectModel_CurrentVersionName"];
    NSMutableArray *moms=[NSMutableArray array];

    for(NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:NULL])
     if([[name pathExtension] isEqualToString:@"mom"])
      [moms addObject:name];

    if(current!=nil)
     path=[path stringByAppendingPathComponent:[current stringByAppendingPathExtension:@"mom"]];
    else if([moms count]==1)
     path=[path stringByAppendingPathComponent:[moms objectAtIndex:0]];
    else
     return nil;
   }

   NSData *data=[NSData dataWithContentsOfFile:path];
   id      result=nil;

   if(data==nil)
    return nil;

   @try {
    NSKeyedUnarchiver *unarchiver=[[NSKeyedUnarchiver alloc] initForReadingWithData:data];

    result=[[unarchiver decodeObjectForKey:@"root"] retain];
    [unarchiver finishDecoding];
    [unarchiver release];
   }
   @catch(NSException *exception){
    NSLog(@"Can't read the model at %@: %@",path,[exception reason]);
    result=nil;
   }

   if(![result isKindOfClass:[NSManagedObjectModel class]]){
    [result release];
    return nil;
   }
   return result;
}

-(void)dealloc {
   for(NSEntityDescription *entity in _entities)
    if([entity managedObjectModel]==self)
     [entity _setManagedObjectModel:nil];
   [_entities release];
   [_fetchRequestTemplates release];
   [_configurations release];
   [_versionIdentifiers release];
   [_localizationDictionary release];
   [super dealloc];
}

// A copy is mutable, with copies of the entities: their relationships and
// subentities point to the copies.
-copyWithZone:(NSZone *)zone {
   NSData *data=[NSKeyedArchiver archivedDataWithRootObject:self];

   return [[NSKeyedUnarchiver unarchiveObjectWithData:data] retain];
}

-(NSString *)description {
   return [NSString stringWithFormat:@"(<%@: %p>) isEditable %d, entities %@, fetch request templates %@",[self class],self,!_immutable,[self entitiesByName],_fetchRequestTemplates];
}

-(void)_checkMutable {
   if(_immutable)
    [NSException raise:NSInternalInconsistencyException format:@"Can't modify an immutable model."];
   for(NSEntityDescription *entity in _entities)
    [entity _invalidateCaches];
}

-(void)_setImmutable {
   _immutable=YES;
}

-(NSArray *)entities {
   return [[_entities copy] autorelease];
}

-(NSDictionary *)entitiesByName {
   NSMutableDictionary *result=[NSMutableDictionary dictionaryWithCapacity:[_entities count]];

   for(NSEntityDescription *entity in _entities)
    if([entity name]!=nil)
     [result setObject:entity forKey:[entity name]];
   return result;
}

// An entity named in a store: by its name, or by the name in capitals as
// some stores write it.
-(NSEntityDescription *)_entityForStoreName:(NSString *)name {
   for(NSEntityDescription *entity in _entities)
    if([[entity name] isEqualToString:name])
     return entity;
   for(NSEntityDescription *entity in _entities)
    if([[entity name] caseInsensitiveCompare:name]==NSOrderedSame)
     return entity;
   return nil;
}

-(void)setEntities:(NSArray *)entities {
   [self _checkMutable];

   NSMutableSet *names=[NSMutableSet set];

   for(NSEntityDescription *entity in entities){
    if([entity name]!=nil && [names containsObject:[entity name]])
     [NSException raise:NSInvalidArgumentException format:@"Can't use the entity name %@ twice in a model",[entity name]];
    if([entity name]!=nil)
     [names addObject:[entity name]];
   }

   for(NSEntityDescription *entity in _entities)
    if([entity managedObjectModel]==self)
     [entity _setManagedObjectModel:nil];
   [_entities release];
   _entities=[entities mutableCopy];
   for(NSEntityDescription *entity in _entities)
    [entity _setManagedObjectModel:self];
}

-(NSDictionary *)localizationDictionary {
   return _localizationDictionary;
}

-(void)setLocalizationDictionary:(NSDictionary *)dictionary {
   [self _checkMutable];
   dictionary=[dictionary copy];
   [_localizationDictionary release];
   _localizationDictionary=dictionary;
}

-(NSArray *)configurations {
   return [_configurations allKeys];
}

// Without a name: every entity.
-(NSArray *)entitiesForConfiguration:(NSString *)configuration {
   if(configuration==nil)
    return [self entities];
   return [_configurations objectForKey:configuration];
}

-(void)setEntities:(NSArray *)entities forConfiguration:(NSString *)configuration {
   [self _checkMutable];
   [_configurations setObject:[[entities copy] autorelease] forKey:configuration];
}

-(NSDictionary *)fetchRequestTemplatesByName {
   return [[_fetchRequestTemplates copy] autorelease];
}

-(NSFetchRequest *)fetchRequestTemplateForName:(NSString *)name {
   return [_fetchRequestTemplates objectForKey:name];
}

-(void)setFetchRequestTemplate:(NSFetchRequest *)fetchRequest forName:(NSString *)name {
   [self _checkMutable];
   if(fetchRequest==nil)
    [_fetchRequestTemplates removeObjectForKey:name];
   else
    [_fetchRequestTemplates setObject:fetchRequest forKey:name];
}

// A copy of the template, its predicate's variables replaced.
-(NSFetchRequest *)fetchRequestFromTemplateWithName:(NSString *)name substitutionVariables:(NSDictionary *)variables {
   NSFetchRequest *template=[_fetchRequestTemplates objectForKey:name];
   NSFetchRequest *result;

   if(template==nil)
    return nil;

   result=[[template copy] autorelease];
   if([result predicate]!=nil)
    [result setPredicate:[[result predicate] predicateWithSubstitutionVariables:(variables!=nil)?variables:[NSDictionary dictionary]]];
   return result;
}

-(NSSet *)versionIdentifiers {
   return _versionIdentifiers;
}

-(void)setVersionIdentifiers:(NSSet *)value {
   [self _checkMutable];
   value=(value!=nil)?[value copy]:[[NSSet alloc] init];
   [_versionIdentifiers release];
   _versionIdentifiers=value;
}

-(NSDictionary *)entityVersionHashesByName {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   for(NSEntityDescription *entity in _entities)
    if([entity name]!=nil)
     [result setObject:[entity versionHash] forKey:[entity name]];
   return result;
}

// Compatible when the store's hashes are those of the configuration's
// entities; a store that keeps none is taken as compatible.
-(BOOL)isConfiguration:(NSString *)configuration compatibleWithStoreMetadata:(NSDictionary *)metadata {
   NSDictionary *hashes=[metadata objectForKey:NSStoreModelVersionHashesKey];

   if(hashes==nil)
    return YES;

   for(NSEntityDescription *entity in [self entitiesForConfiguration:configuration]){
    NSData *stored=[hashes objectForKey:[entity name]];

    if(stored==nil || ![stored isEqual:[entity versionHash]])
     return NO;
   }
   return YES;
}

-(NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id *)buffer count:(NSUInteger)length {
   NSUInteger index,count=0;

   if(state->state==0){
    state->state=1;
    state->extra[0]=0;
    state->mutationsPtr=&state->extra[1];
   }
   for(index=state->extra[0];index<[_entities count] && count<length;index++)
    buffer[count++]=[_entities objectAtIndex:index];
   state->extra[0]=index;
   state->itemsPtr=buffer;
   return count;
}

@end
