/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSSQLiteStoreType. The database has the reference's layout, measured on
 * it (a store it made opens here, and the other way): a table a root
 * entity, Z<ENTITY>, its subentities' rows in it too, with Z_PK, Z_ENT (the
 * row's entity, numbered by name from 1) and Z_OPT (1, one more each save
 * that changes the row), then a column an attribute or to-one relationship,
 * Z<NAME>, grouped by SQL type (INTEGER attributes, to-one keys,
 * TIMESTAMP, FLOAT, DECIMAL, VARCHAR, BLOB) and by name in each group; an
 * index on each to-one key; a table a many-to-many relationship;
 * Z_PRIMARYKEY (each entity's number, name, superentity and greatest key),
 * Z_METADATA (the metadata, a binary property list) and Z_MODELCACHE.
 * Dates are seconds since 2001, booleans 0 and 1. The journal is WAL.
 *
 * As the other stores here (NSAtomicStore), it is read whole as it loads,
 * into cache nodes the contexts fetch from; a save writes what changed,
 * in one transaction.
 *
 * Whether the model fits the database is told by the database's tables
 * and columns (the reference compares version hashes of its own): a model
 * the store does not fit fails with NSPersistentStoreIncompatibleVersionHashError,
 * unless migration is asked for (NSMigratePersistentStoresAutomaticallyOption
 * with NSInferMappingModelAutomaticallyOption): then the tables and columns
 * it lacks are added.
 */

#import "NSSQLitePersistentStore.h"
#import <Foundation/Foundation.h>
#import <CoreData/NSPersistentStoreCoordinator.h>
#import <CoreData/NSManagedObjectModel.h>
#import <CoreData/NSEntityDescription.h>
#import <CoreData/NSAttributeDescription.h>
#import <CoreData/NSRelationshipDescription.h>
#import <CoreData/NSManagedObject.h>
#import <CoreData/NSManagedObjectID.h>
#import <CoreData/NSAtomicStoreCacheNode.h>
#import <CoreData/CoreDataErrors.h>
#import <CommonCrypto/CommonDigest.h>
#include <sqlite3.h>

#define DB ((sqlite3 *)_db)

/* ---- The layout ---- */

static NSEntityDescription *rootEntity(NSEntityDescription *entity) {
   while([entity superentity]!=nil)
    entity=[entity superentity];
   return entity;
}

static NSString *tableName(NSEntityDescription *root) {
   return [@"Z" stringByAppendingString:[[root name] uppercaseString]];
}

static NSString *columnName(NSPropertyDescription *property) {
   return [@"Z" stringByAppendingString:[[property name] uppercaseString]];
}

/* An entity and the entities under it. */
static void addHierarchy(NSEntityDescription *entity, NSMutableArray *into) {
   [into addObject:entity];
   for(NSEntityDescription *sub in [entity subentities])
    addHierarchy(sub, into);
}

enum { GroupInteger, GroupKey, GroupTimestamp, GroupFloat, GroupDecimal, GroupVarchar, GroupBlob, GroupNone };

static int attributeGroup(NSAttributeDescription *attribute) {
   switch([attribute attributeType]){
    case NSInteger16AttributeType:
    case NSInteger32AttributeType:
    case NSInteger64AttributeType:
    case NSBooleanAttributeType: return GroupInteger;
    case NSDateAttributeType: return GroupTimestamp;
    case NSDoubleAttributeType:
    case NSFloatAttributeType: return GroupFloat;
    case NSDecimalAttributeType: return GroupDecimal;
    case NSStringAttributeType:
    case NSURIAttributeType: return GroupVarchar;
    case NSBinaryDataAttributeType:
    case NSTransformableAttributeType:
    case NSUUIDAttributeType: return GroupBlob;
    default: return GroupNone;
   }
}

static NSString *groupType(int group) {
   static NSString *types[]={ @"INTEGER", @"INTEGER", @"TIMESTAMP", @"FLOAT", @"DECIMAL", @"VARCHAR", @"BLOB" };
   return types[group];
}

/* The columns of a root entity's table, in the reference's order: each a
   property (one of the hierarchy's; a name taken once). */
static NSArray *tableProperties(NSEntityDescription *root) {
   NSMutableArray      *hierarchy=[NSMutableArray array];
   NSMutableDictionary *byColumn=[NSMutableDictionary dictionary];
   NSMutableArray      *groups[GroupNone];

   addHierarchy(root, hierarchy);
   for(int g=0;g<GroupNone;g++)
    groups[g]=[NSMutableArray array];
   for(NSEntityDescription *entity in hierarchy){
    for(NSPropertyDescription *property in [entity properties]){
     int group=GroupNone;

     if([property isTransient] || [byColumn objectForKey:columnName(property)]!=nil)
      continue;
     if([property isKindOfClass:[NSAttributeDescription class]])
      group=attributeGroup((NSAttributeDescription *)property);
     else if([property isKindOfClass:[NSRelationshipDescription class]] && ![(NSRelationshipDescription *)property isToMany])
      group=GroupKey;
     if(group==GroupNone)
      continue;
     [byColumn setObject:property forKey:columnName(property)];
     [groups[group] addObject:property];
    }
   }
   NSMutableArray *result=[NSMutableArray array];
   NSSortDescriptor *byName=[NSSortDescriptor sortDescriptorWithKey:@"name.uppercaseString" ascending:YES];
   for(int g=0;g<GroupNone;g++)
    [result addObjectsFromArray:[groups[g] sortedArrayUsingDescriptors:[NSArray arrayWithObject:byName]]];
   return result;
}

static NSString *propertySQLType(NSPropertyDescription *property) {
   if([property isKindOfClass:[NSRelationshipDescription class]])
    return @"INTEGER";
   return groupType(attributeGroup((NSAttributeDescription *)property));
}

/* A many-to-many relationship is kept from one side: the one of the
   entity numbered first (by name, then the relationship's name). */
static BOOL isManyToMany(NSRelationshipDescription *relationship) {
   return [relationship isToMany] && ([relationship inverseRelationship]==nil || [[relationship inverseRelationship] isToMany]);
}

/* ---- Values ---- */

static NSValueTransformer *transformerFor(NSAttributeDescription *attribute) {
   NSString *name=[attribute valueTransformerName];
   return [NSValueTransformer valueTransformerForName:(name!=nil)?name:NSKeyedUnarchiveFromDataTransformerName];
}

static void bindValue(sqlite3_stmt *statement, int index, NSPropertyDescription *property, id value) {
   if(value==nil || value==[NSNull null]){
    sqlite3_bind_null(statement, index);
    return;
   }
   if([property isKindOfClass:[NSRelationshipDescription class]]){
    sqlite3_bind_int64(statement, index, [value longLongValue]);
    return;
   }
   NSAttributeDescription *attribute=(NSAttributeDescription *)property;
   switch([attribute attributeType]){
    case NSInteger16AttributeType:
    case NSInteger32AttributeType:
    case NSInteger64AttributeType:
     sqlite3_bind_int64(statement, index, [value longLongValue]);
     break;
    case NSBooleanAttributeType:
     sqlite3_bind_int(statement, index, [value boolValue]?1:0);
     break;
    case NSDateAttributeType:
     sqlite3_bind_double(statement, index, [value timeIntervalSinceReferenceDate]);
     break;
    case NSDoubleAttributeType:
    case NSFloatAttributeType:
     sqlite3_bind_double(statement, index, [value doubleValue]);
     break;
    case NSDecimalAttributeType:
     sqlite3_bind_text(statement, index, [[value description] UTF8String], -1, SQLITE_TRANSIENT);
     break;
    case NSStringAttributeType:
     sqlite3_bind_text(statement, index, [[value description] UTF8String], -1, SQLITE_TRANSIENT);
     break;
    case NSURIAttributeType:
     sqlite3_bind_text(statement, index, [[value absoluteString] UTF8String], -1, SQLITE_TRANSIENT);
     break;
    case NSBinaryDataAttributeType:
     sqlite3_bind_blob(statement, index, [value bytes], (int)[value length], SQLITE_TRANSIENT);
     break;
    case NSTransformableAttributeType:{
      NSData *data=[attribute valueTransformerName]!=nil?[transformerFor(attribute) transformedValue:value]
                                                         :[NSKeyedArchiver archivedDataWithRootObject:value];
      if(data!=nil)
       sqlite3_bind_blob(statement, index, [data bytes], (int)[data length], SQLITE_TRANSIENT);
      else
       sqlite3_bind_null(statement, index);
     }
     break;
    case NSUUIDAttributeType:{
      uuid_t bytes;
      [(NSUUID *)value getUUIDBytes:bytes];
      sqlite3_bind_blob(statement, index, bytes, sizeof(bytes), SQLITE_TRANSIENT);
     }
     break;
    default:
     sqlite3_bind_null(statement, index);
     break;
   }
}

static id columnValue(sqlite3_stmt *statement, int index, NSAttributeDescription *attribute) {
   if(sqlite3_column_type(statement, index)==SQLITE_NULL)
    return nil;
   switch([attribute attributeType]){
    case NSInteger16AttributeType: return [NSNumber numberWithShort:(short)sqlite3_column_int64(statement, index)];
    case NSInteger32AttributeType: return [NSNumber numberWithInt:(int)sqlite3_column_int64(statement, index)];
    case NSInteger64AttributeType: return [NSNumber numberWithLongLong:sqlite3_column_int64(statement, index)];
    case NSBooleanAttributeType: return [NSNumber numberWithBool:sqlite3_column_int64(statement, index)!=0];
    case NSDateAttributeType: return [NSDate dateWithTimeIntervalSinceReferenceDate:sqlite3_column_double(statement, index)];
    case NSDoubleAttributeType: return [NSNumber numberWithDouble:sqlite3_column_double(statement, index)];
    case NSFloatAttributeType: return [NSNumber numberWithFloat:(float)sqlite3_column_double(statement, index)];
    case NSDecimalAttributeType:{
      const unsigned char *text=sqlite3_column_text(statement, index);
      return [NSDecimalNumber decimalNumberWithString:[NSString stringWithUTF8String:(const char *)text]];
     }
    case NSStringAttributeType:{
      const unsigned char *text=sqlite3_column_text(statement, index);
      return [NSString stringWithUTF8String:text?(const char *)text:""];
     }
    case NSURIAttributeType:{
      const unsigned char *text=sqlite3_column_text(statement, index);
      return text?[NSURL URLWithString:[NSString stringWithUTF8String:(const char *)text]]:nil;
     }
    case NSBinaryDataAttributeType:
     return [NSData dataWithBytes:sqlite3_column_blob(statement, index) length:sqlite3_column_bytes(statement, index)];
    case NSTransformableAttributeType:{
      NSData *data=[NSData dataWithBytes:sqlite3_column_blob(statement, index) length:sqlite3_column_bytes(statement, index)];
      return [attribute valueTransformerName]!=nil?[transformerFor(attribute) reverseTransformedValue:data]
                                                   :[NSKeyedUnarchiver unarchiveObjectWithData:data];
     }
    case NSUUIDAttributeType:
     if(sqlite3_column_bytes(statement, index)==sizeof(uuid_t))
      return [[[NSUUID alloc] initWithUUIDBytes:sqlite3_column_blob(statement, index)] autorelease];
     return nil;
    default:
     return nil;
   }
}

/* An entity's version hash: SHA-256 of its name and its properties'
   names and kinds, as this store makes them (its own, not the
   reference's: whether a model fits is told by the tables). */
static NSData *versionHash(NSEntityDescription *entity) {
   NSMutableArray *parts=[NSMutableArray arrayWithObject:[entity name]];
   NSArray *properties=[[entity properties] sortedArrayUsingDescriptors:[NSArray arrayWithObject:[NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]]];
   for(NSPropertyDescription *property in properties){
    if([property isKindOfClass:[NSAttributeDescription class]])
     [parts addObject:[NSString stringWithFormat:@"a:%@:%lu", [property name], (unsigned long)[(NSAttributeDescription *)property attributeType]]];
    else if([property isKindOfClass:[NSRelationshipDescription class]])
     [parts addObject:[NSString stringWithFormat:@"r:%@:%@:%d", [property name],
        [[(NSRelationshipDescription *)property destinationEntity] name], [(NSRelationshipDescription *)property isToMany]]];
   }
   NSData *text=[[parts componentsJoinedByString:@"\n"] dataUsingEncoding:NSUTF8StringEncoding];
   unsigned char digest[CC_SHA256_DIGEST_LENGTH];
   CC_SHA256([text bytes], (CC_LONG)[text length], digest);
   return [NSData dataWithBytes:digest length:sizeof(digest)];
}

static NSError *storeError(NSInteger code, NSString *reason, NSDictionary *extra) {
   NSMutableDictionary *info=[NSMutableDictionary dictionaryWithDictionary:extra?extra:[NSDictionary dictionary]];
   if(reason!=nil){
    [info setObject:reason forKey:@"reason"];
    [info setObject:reason forKey:NSLocalizedDescriptionKey];
   }
   return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:info];
}

@implementation NSSQLitePersistentStore

/* ---- The database ---- */

static BOOL execute(sqlite3 *db, NSString *sql, NSString **message) {
   char *error=NULL;
   if(sqlite3_exec(db, [sql UTF8String], NULL, NULL, &error)==SQLITE_OK)
    return YES;
   if(message!=NULL)
    *message=[NSString stringWithFormat:@"%s (%@)", error?error:"error", sql];
   sqlite3_free(error);
   return NO;
}

static BOOL tableExists(sqlite3 *db, NSString *table) {
   sqlite3_stmt *statement=NULL;
   BOOL          exists=NO;
   if(sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &statement, NULL)==SQLITE_OK){
    sqlite3_bind_text(statement, 1, [table UTF8String], -1, SQLITE_TRANSIENT);
    exists=sqlite3_step(statement)==SQLITE_ROW;
   }
   sqlite3_finalize(statement);
   return exists;
}

static NSSet *tableColumns(sqlite3 *db, NSString *table) {
   NSMutableSet *columns=[NSMutableSet set];
   sqlite3_stmt *statement=NULL;
   NSString     *sql=[NSString stringWithFormat:@"PRAGMA table_info(%@)", table];
   if(sqlite3_prepare_v2(db, [sql UTF8String], -1, &statement, NULL)==SQLITE_OK)
    while(sqlite3_step(statement)==SQLITE_ROW)
     [columns addObject:[NSString stringWithUTF8String:(const char *)sqlite3_column_text(statement, 1)]];
   sqlite3_finalize(statement);
   return columns;
}

static NSDictionary *readMetadata(sqlite3 *db) {
   sqlite3_stmt *statement=NULL;
   NSDictionary *result=nil;
   if(sqlite3_prepare_v2(db, "SELECT Z_PLIST FROM Z_METADATA WHERE Z_VERSION=1", -1, &statement, NULL)==SQLITE_OK &&
      sqlite3_step(statement)==SQLITE_ROW){
    NSData *data=[NSData dataWithBytes:sqlite3_column_blob(statement, 0) length:sqlite3_column_bytes(statement, 0)];
    id      list=[NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:NULL error:NULL];
    if([list isKindOfClass:[NSDictionary class]])
     result=list;
   }
   sqlite3_finalize(statement);
   return result;
}

static BOOL writeMetadata(sqlite3 *db, NSDictionary *metadata, NSString *uuid) {
   NSData *data=[NSPropertyListSerialization dataWithPropertyList:metadata format:NSPropertyListBinaryFormat_v1_0 options:0 error:NULL];
   sqlite3_stmt *statement=NULL;
   BOOL ok=NO;
   if(data!=nil && sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Z_METADATA (Z_VERSION, Z_UUID, Z_PLIST) VALUES (1, ?, ?)", -1, &statement, NULL)==SQLITE_OK){
    sqlite3_bind_text(statement, 1, [uuid UTF8String], -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(statement, 2, [data bytes], (int)[data length], SQLITE_TRANSIENT);
    ok=sqlite3_step(statement)==SQLITE_DONE;
   }
   sqlite3_finalize(statement);
   return ok;
}

+(NSDictionary *)metadataForPersistentStoreWithURL:(NSURL *)url error:(NSError **)error {
   sqlite3      *db=NULL;
   NSDictionary *result=nil;
   if(![url isFileURL] || ![[NSFileManager defaultManager] fileExistsAtPath:[url path]]){
    if(error!=NULL)
     *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadNoSuchFileError
       userInfo:[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]];
    return nil;
   }
   if(sqlite3_open_v2([[url path] fileSystemRepresentation], &db, SQLITE_OPEN_READONLY, NULL)==SQLITE_OK)
    result=readMetadata(db);
   sqlite3_close(db);
   if(result==nil && error!=NULL)
    *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadCorruptFileError
      userInfo:[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]];
   return result;
}

+(BOOL)setMetadata:(NSDictionary *)metadata forPersistentStoreWithURL:(NSURL *)url error:(NSError **)error {
   sqlite3 *db=NULL;
   BOOL     ok=NO;
   if(sqlite3_open_v2([[url path] fileSystemRepresentation], &db, SQLITE_OPEN_READWRITE, NULL)==SQLITE_OK){
    NSDictionary *old=readMetadata(db);
    ok=writeMetadata(db, metadata, [old objectForKey:NSStoreUUIDKey]?[old objectForKey:NSStoreUUIDKey]:[metadata objectForKey:NSStoreUUIDKey]);
   }
   sqlite3_close(db);
   if(!ok && error!=NULL)
    *error=storeError(NSPersistentStoreSaveError, @"The store's metadata could not be written", nil);
   return ok;
}

-(NSString *)type {
   return NSSQLiteStoreType;
}

-initWithPersistentStoreCoordinator:(NSPersistentStoreCoordinator *)coordinator configurationName:(NSString *)configurationName URL:(NSURL *)url options:(NSDictionary *)options {
   if([super initWithPersistentStoreCoordinator:coordinator configurationName:configurationName URL:url options:options]==nil)
    return nil;
   _referenceToCacheNode=[[NSMutableDictionary alloc] init];
   _entityNumbers=[[NSMutableDictionary alloc] init];
   _maxKeys=[[NSMutableDictionary alloc] init];
   _inserted=[[NSMutableSet alloc] init];
   _updated=[[NSMutableSet alloc] init];
   _deleted=[[NSMutableSet alloc] init];
   [self setReadOnly:[[options objectForKey:NSReadOnlyPersistentStoreOption] boolValue]];
   return self;
}

-(void)dealloc {
   if(_db!=NULL)
    sqlite3_close_v2(DB);
   [_referenceToCacheNode release];
   [_entityNumbers release];
   [_maxKeys release];
   [_inserted release];
   [_updated release];
   [_deleted release];
   [super dealloc];
}

-(void)willRemoveFromPersistentStoreCoordinator:(NSPersistentStoreCoordinator *)coordinator {
   if(_db!=NULL){
    sqlite3_close_v2(DB);
    _db=NULL;
   }
   [super willRemoveFromPersistentStoreCoordinator:coordinator];
}

-(NSManagedObjectModel *)_model {
   return [[self persistentStoreCoordinator] managedObjectModel];
}

/* The root entities, each once, by name. */
-(NSArray *)_rootEntities {
   NSMutableDictionary *roots=[NSMutableDictionary dictionary];
   for(NSEntityDescription *entity in [[self _model] entities])
    [roots setObject:rootEntity(entity) forKey:[rootEntity(entity) name]];
   return [[roots allValues] sortedArrayUsingDescriptors:[NSArray arrayWithObject:[NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]]];
}

-(NSArray *)_sortedEntities {
   return [[[self _model] entities] sortedArrayUsingDescriptors:[NSArray arrayWithObject:[NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]]];
}

-(NSInteger)_numberOf:(NSEntityDescription *)entity {
   return [[_entityNumbers objectForKey:[entity name]] integerValue];
}

-(NSEntityDescription *)_entityNumbered:(NSInteger)number {
   for(NSString *name in _entityNumbers)
    if([[_entityNumbers objectForKey:name] integerValue]==number)
     return [[[self _model] entitiesByName] objectForKey:name];
   return nil;
}

/* The many-to-many relationships kept, each from its side (see
   isManyToMany), with their tables and columns. */
-(NSArray *)_manyToManyRelationships {
   NSMutableArray *result=[NSMutableArray array];
   for(NSEntityDescription *entity in [self _sortedEntities])
    for(NSRelationshipDescription *relationship in [[entity relationshipsByName] allValues]){
     if([relationship entity]!=entity || [relationship isTransient] || !isManyToMany(relationship))
      continue;
     NSRelationshipDescription *inverse=[relationship inverseRelationship];
     if(inverse!=nil){
      NSInteger mine=[self _numberOf:entity], theirs=[self _numberOf:[inverse entity]];
      if(theirs<mine || (theirs==mine && [[inverse name] compare:[relationship name]]==NSOrderedAscending))
       continue;   /* kept from the other side */
     }
     [result addObject:relationship];
    }
   return result;
}

-(NSString *)_joinTable:(NSRelationshipDescription *)relationship {
   return [NSString stringWithFormat:@"Z_%ld%@", (long)[self _numberOf:[relationship entity]], [[relationship name] uppercaseString]];
}

/* The column of the relationship's own objects, and of its destination's. */
-(NSString *)_sourceColumn:(NSRelationshipDescription *)relationship {
   NSRelationshipDescription *inverse=[relationship inverseRelationship];
   NSString *name=inverse?[inverse name]:[[relationship entity] name];
   return [NSString stringWithFormat:@"Z_%ld%@", (long)[self _numberOf:[relationship entity]], [name uppercaseString]];
}

-(NSString *)_destinationColumn:(NSRelationshipDescription *)relationship {
   return [NSString stringWithFormat:@"Z_%ld%@", (long)[self _numberOf:[relationship destinationEntity]], [[relationship name] uppercaseString]];
}

-(NSString *)_createTableSQL:(NSEntityDescription *)root {
   NSMutableArray *columns=[NSMutableArray arrayWithObjects:@"Z_PK INTEGER PRIMARY KEY", @"Z_ENT INTEGER", @"Z_OPT INTEGER", nil];
   for(NSPropertyDescription *property in tableProperties(root))
    [columns addObject:[NSString stringWithFormat:@"%@ %@", columnName(property), propertySQLType(property)]];
   return [NSString stringWithFormat:@"CREATE TABLE %@ ( %@ )", tableName(root), [columns componentsJoinedByString:@", "]];
}

/* The tables, then their indexes, as the reference makes them. */
-(BOOL)_createTablesFor:(NSArray *)roots message:(NSString **)message {
   for(NSEntityDescription *root in roots)
    if(!execute(DB, [self _createTableSQL:root], message))
     return NO;
   for(NSEntityDescription *root in roots){
    for(NSPropertyDescription *property in tableProperties(root))
     if([property isKindOfClass:[NSRelationshipDescription class]] &&
        !execute(DB, [NSString stringWithFormat:@"CREATE INDEX %@_%@_INDEX ON %@ (%@)", tableName(root), columnName(property),
                      tableName(root), columnName(property)], message))
      return NO;
   }
   return YES;
}

-(BOOL)_createJoinTableOnly:(NSRelationshipDescription *)relationship message:(NSString **)message {
   NSString *table=[self _joinTable:relationship], *source=[self _sourceColumn:relationship], *destination=[self _destinationColumn:relationship];
   return execute(DB, [NSString stringWithFormat:@"CREATE TABLE %@ ( %@ INTEGER, %@ INTEGER, PRIMARY KEY (%@, %@) )",
                       table, source, destination, source, destination], message);
}

-(BOOL)_createJoinIndex:(NSRelationshipDescription *)relationship message:(NSString **)message {
   NSString *table=[self _joinTable:relationship], *source=[self _sourceColumn:relationship], *destination=[self _destinationColumn:relationship];
   return execute(DB, [NSString stringWithFormat:@"CREATE INDEX %@_%@_INDEX ON %@ (%@, %@)", table, destination, table, destination, source], message);
}

-(BOOL)_createJoinTable:(NSRelationshipDescription *)relationship message:(NSString **)message {
   return [self _createJoinTableOnly:relationship message:message] && [self _createJoinIndex:relationship message:message];
}

/* A new store's tables in the reference's order: each root entity's,
   followed by the many-to-many tables kept from it; then the indexes. */
-(BOOL)_createSchema:(NSString **)message {
   NSArray *joins=[self _manyToManyRelationships];
   for(NSEntityDescription *root in [self _rootEntities]){
    if(!execute(DB, [self _createTableSQL:root], message))
     return NO;
    for(NSRelationshipDescription *relationship in joins)
     if(rootEntity([relationship entity])==root && ![self _createJoinTableOnly:relationship message:message])
      return NO;
   }
   for(NSEntityDescription *root in [self _rootEntities])
    for(NSPropertyDescription *property in tableProperties(root))
     if([property isKindOfClass:[NSRelationshipDescription class]] &&
        !execute(DB, [NSString stringWithFormat:@"CREATE INDEX %@_%@_INDEX ON %@ (%@)", tableName(root), columnName(property),
                      tableName(root), columnName(property)], message))
      return NO;
   for(NSRelationshipDescription *relationship in joins)
    if(![self _createJoinIndex:relationship message:message])
     return NO;
   return YES;
}

-(void)_numberEntities:(NSDictionary *)existing {
   NSInteger next=1;
   [_entityNumbers removeAllObjects];
   [_entityNumbers addEntriesFromDictionary:existing];
   for(NSNumber *number in [existing allValues])
    next=MAX(next, [number integerValue]+1);
   for(NSEntityDescription *entity in [self _sortedEntities])
    if([_entityNumbers objectForKey:[entity name]]==nil)
     [_entityNumbers setObject:[NSNumber numberWithInteger:next++] forKey:[entity name]];
}

-(BOOL)_writePrimaryKeys {
   for(NSEntityDescription *entity in [self _sortedEntities]){
    sqlite3_stmt *statement=NULL;
    NSEntityDescription *super_=[entity superentity];
    if(sqlite3_prepare_v2(DB, "INSERT OR REPLACE INTO Z_PRIMARYKEY (Z_ENT, Z_NAME, Z_SUPER, Z_MAX) VALUES (?, ?, ?, ?)", -1, &statement, NULL)!=SQLITE_OK)
     return NO;
    sqlite3_bind_int64(statement, 1, [self _numberOf:entity]);
    sqlite3_bind_text(statement, 2, [[entity name] UTF8String], -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 3, super_?[self _numberOf:super_]:0);
    /* The greatest key is the hierarchy's, kept on its root. */
    sqlite3_bind_int64(statement, 4, super_==nil?[[_maxKeys objectForKey:[entity name]] longLongValue]:0);
    BOOL ok=sqlite3_step(statement)==SQLITE_DONE;
    sqlite3_finalize(statement);
    if(!ok)
     return NO;
   }
   return YES;
}

-(NSDictionary *)_metadataWith:(NSDictionary *)base {
   NSMutableDictionary *metadata=[NSMutableDictionary dictionaryWithDictionary:base?base:[NSDictionary dictionary]];
   NSMutableDictionary *hashes=[NSMutableDictionary dictionary];
   for(NSEntityDescription *entity in [[self _model] entities])
    [hashes setObject:versionHash(entity) forKey:[entity name]];
   [metadata setObject:NSSQLiteStoreType forKey:NSStoreTypeKey];
   [metadata setObject:[self identifier] forKey:NSStoreUUIDKey];
   [metadata setObject:hashes forKey:NSStoreModelVersionHashesKey];
   [metadata setObject:[NSNumber numberWithInt:3] forKey:@"NSStoreModelVersionHashesVersion"];
   /* A digest of the hashes (SHA-512, by entity name), and the vacuum
      level the database is made with, as the reference keeps them. */
   NSMutableData *all=[NSMutableData data];
   for(NSString *name in [[hashes allKeys] sortedArrayUsingSelector:@selector(compare:)])
    [all appendData:[hashes objectForKey:name]];
   unsigned char digest[CC_SHA512_DIGEST_LENGTH];
   CC_SHA512([all bytes], (CC_LONG)[all length], digest);
   [metadata setObject:[[NSData dataWithBytes:digest length:sizeof(digest)] base64EncodedStringWithOptions:0] forKey:@"NSStoreModelVersionHashesDigest"];
   [metadata setObject:@"2" forKey:@"_NSAutoVacuumLevel"];
   if([metadata objectForKey:NSStoreModelVersionIdentifiersKey]==nil)
    [metadata setObject:[NSArray array] forKey:NSStoreModelVersionIdentifiersKey];
   [metadata setObject:[NSNumber numberWithInt:1] forKey:@"NSPersistenceFrameworkVersion"];
   return metadata;
}

/* Opens (or makes) the database and checks the model fits it. */
-(BOOL)_openDatabase:(NSError **)errorp {
   NSURL    *url=[self URL];
   NSString *message=nil;
   int       flags=[self isReadOnly]?SQLITE_OPEN_READONLY:(SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
   sqlite3  *db=NULL;

   if(![url isFileURL]){
    if(errorp) *errorp=storeError(NSPersistentStoreInvalidTypeError, @"A SQLite store needs a file URL", nil);
    return NO;
   }
   if(sqlite3_open_v2([[url path] fileSystemRepresentation], &db, flags, NULL)!=SQLITE_OK){
    sqlite3_close(db);
    if(errorp)
     *errorp=[NSError errorWithDomain:NSCocoaErrorDomain code:[self isReadOnly]?NSFileReadNoSuchFileError:NSFileWriteUnknownError
       userInfo:[NSDictionary dictionaryWithObjectsAndKeys:url,NSURLErrorKey,[url path],NSFilePathErrorKey,nil]];
    return NO;
   }
   _db=db;
   sqlite3_busy_timeout(DB, 5000);
   if(![self isReadOnly])
    execute(DB, @"PRAGMA journal_mode=WAL", NULL);
   NSDictionary *pragmas=[[self options] objectForKey:NSSQLitePragmasOption];
   for(NSString *pragma in pragmas)
    execute(DB, [NSString stringWithFormat:@"PRAGMA %@=%@", pragma, [pragmas objectForKey:pragma]], NULL);

   BOOL migrate=[[[self options] objectForKey:NSMigratePersistentStoresAutomaticallyOption] boolValue] &&
                [[[self options] objectForKey:NSInferMappingModelAutomaticallyOption] boolValue];

   if(!tableExists(DB, @"Z_METADATA")){
    /* A new store. */
    if([self isReadOnly]){
     if(errorp) *errorp=storeError(NSFileReadCorruptFileError, @"The file is not a store", nil);
     return NO;
    }
    [self _numberEntities:[NSDictionary dictionary]];
    execute(DB, @"PRAGMA auto_vacuum=2", NULL);   /* incremental, before any table */
    for(NSEntityDescription *root in [self _rootEntities])
     [_maxKeys setObject:[NSNumber numberWithLongLong:0] forKey:[root name]];
    if(!execute(DB, @"BEGIN", &message) ||
       ![self _createSchema:&message] ||
       !execute(DB, @"CREATE TABLE Z_PRIMARYKEY (Z_ENT INTEGER PRIMARY KEY, Z_NAME VARCHAR, Z_SUPER INTEGER, Z_MAX INTEGER)", &message) ||
       !execute(DB, @"CREATE TABLE Z_METADATA (Z_VERSION INTEGER PRIMARY KEY, Z_UUID VARCHAR(255), Z_PLIST BLOB)", &message) ||
       !execute(DB, @"CREATE TABLE Z_MODELCACHE (Z_CONTENT BLOB)", &message)){
     execute(DB, @"ROLLBACK", NULL);
     if(errorp) *errorp=storeError(NSPersistentStoreSaveError, message, nil);
     return NO;
    }
    NSDictionary *metadata=[self _metadataWith:nil];
    if(![self _writePrimaryKeys] || !writeMetadata(DB, metadata, [self identifier]) || !execute(DB, @"COMMIT", &message)){
     execute(DB, @"ROLLBACK", NULL);
     if(errorp) *errorp=storeError(NSPersistentStoreSaveError, message?message:@"The store could not be made", nil);
     return NO;
    }
    [self setMetadata:metadata];
    return YES;
   }

   /* An existing store: its identity, entity numbers and keys. */
   NSDictionary *stored=readMetadata(DB);
   if([stored objectForKey:NSStoreUUIDKey]!=nil)
    [super setIdentifier:[stored objectForKey:NSStoreUUIDKey]];
   NSMutableDictionary *numbers=[NSMutableDictionary dictionary], *maxima=[NSMutableDictionary dictionary];
   sqlite3_stmt *statement=NULL;
   if(sqlite3_prepare_v2(DB, "SELECT Z_ENT, Z_NAME, Z_MAX FROM Z_PRIMARYKEY", -1, &statement, NULL)==SQLITE_OK)
    while(sqlite3_step(statement)==SQLITE_ROW){
     NSString *name=[NSString stringWithUTF8String:(const char *)sqlite3_column_text(statement, 1)];
     [numbers setObject:[NSNumber numberWithLongLong:sqlite3_column_int64(statement, 0)] forKey:name];
     [maxima setObject:[NSNumber numberWithLongLong:sqlite3_column_int64(statement, 2)] forKey:name];
    }
   sqlite3_finalize(statement);

   /* Does the model fit? Every entity's table and column, every
      many-to-many table, and no entity of the store's the model lacks. */
   NSDictionary   *modelEntities=[[self _model] entitiesByName];
   NSMutableArray *missingRoots=[NSMutableArray array], *missingColumns=[NSMutableArray array];
   BOOL            extraEntities=NO;
   for(NSString *name in numbers)
    if([modelEntities objectForKey:name]==nil)
     extraEntities=YES;
   [self _numberEntities:numbers];
   for(NSEntityDescription *root in [self _rootEntities]){
    if(!tableExists(DB, tableName(root))){
     [missingRoots addObject:root];
     continue;
    }
    NSSet        *columns=tableColumns(DB, tableName(root));
    NSMutableSet *expected=[NSMutableSet setWithObjects:@"Z_PK", @"Z_ENT", @"Z_OPT", nil];
    for(NSPropertyDescription *property in tableProperties(root)){
     [expected addObject:columnName(property)];
     if(![columns containsObject:columnName(property)])
      [missingColumns addObject:[NSArray arrayWithObjects:root,property,nil]];
    }
    /* A column the model has no property for: another model's store. */
    for(NSString *column in columns)
     if(![expected containsObject:column] && ![column hasPrefix:@"Z_"] && ![column hasPrefix:@"Z1"])
      extraEntities=YES;
   }
   NSMutableArray *missingJoins=[NSMutableArray array];
   for(NSRelationshipDescription *relationship in [self _manyToManyRelationships])
    if(!tableExists(DB, [self _joinTable:relationship]))
     [missingJoins addObject:relationship];

   if(extraEntities || [missingRoots count] || [missingColumns count] || [missingJoins count]){
    if(!migrate || [self isReadOnly]){
     if(errorp)
      *errorp=storeError(NSPersistentStoreIncompatibleVersionHashError,
        @"The model used to open the store is incompatible with the one used to create the store",
        stored?[NSDictionary dictionaryWithObject:stored forKey:@"metadata"]:nil);
     return NO;
    }
    /* Lightweight migration: what the store lacks is added. */
    BOOL ok=execute(DB, @"BEGIN", &message) && [self _createTablesFor:missingRoots message:&message];
    for(NSArray *pair in missingColumns){
     if(!ok)
      break;
     ok=execute(DB, [NSString stringWithFormat:@"ALTER TABLE %@ ADD COLUMN %@ %@", tableName([pair objectAtIndex:0]),
                     columnName([pair objectAtIndex:1]), propertySQLType([pair objectAtIndex:1])], &message);
    }
    for(NSRelationshipDescription *relationship in missingJoins)
     if(ok)
      ok=[self _createJoinTable:relationship message:&message];
    for(NSEntityDescription *root in missingRoots)
     [maxima setObject:[NSNumber numberWithLongLong:0] forKey:[root name]];
    [_maxKeys setDictionary:maxima];
    if(ok)
     ok=[self _writePrimaryKeys] && writeMetadata(DB, [self _metadataWith:stored], [self identifier]) && execute(DB, @"COMMIT", &message);
    if(!ok){
     execute(DB, @"ROLLBACK", NULL);
     if(errorp) *errorp=storeError(134110 /* NSMigrationError */, message?message:@"The store could not be migrated", nil);
     return NO;
    }
   }
   [_maxKeys setDictionary:maxima];
   for(NSEntityDescription *root in [self _rootEntities])
    if([_maxKeys objectForKey:[root name]]==nil)
     [_maxKeys setObject:[NSNumber numberWithLongLong:0] forKey:[root name]];
   [self setMetadata:[self _metadataWith:stored]];
   return YES;
}

/* ---- Loading ---- */

static NSString *referenceKey(NSEntityDescription *root, long long pk) {
   return [NSString stringWithFormat:@"%@/%lld", [root name], pk];
}

-(NSAtomicStoreCacheNode *)_nodeForEntity:(NSEntityDescription *)entity key:(long long)pk {
   NSString               *key=referenceKey(rootEntity(entity), pk);
   NSAtomicStoreCacheNode *node=[_referenceToCacheNode objectForKey:key];

   if(node==nil){
    NSManagedObjectID *objectID=[self objectIDForEntity:entity referenceObject:[NSString stringWithFormat:@"p%lld", pk]];
    node=[[[NSAtomicStoreCacheNode alloc] initWithObjectID:objectID] autorelease];
    [_referenceToCacheNode setObject:node forKey:key];
   }
   return node;
}

static long long keyOfNode(NSAtomicStoreCacheNode *node) {
   return [[[[node objectID] referenceObject] substringFromIndex:1] longLongValue];
}

-(BOOL)load:(NSError **)errorp {
   if(![self _openDatabase:errorp])
    return NO;

   NSMutableArray *pendingKeys=[NSMutableArray array];   /* node, relationship, key */
   NSMutableSet   *nodes=[NSMutableSet set];

   for(NSEntityDescription *root in [self _rootEntities]){
    NSArray        *properties=tableProperties(root);
    NSMutableArray *columns=[NSMutableArray arrayWithObjects:@"Z_PK", @"Z_ENT", nil];
    for(NSPropertyDescription *property in properties)
     [columns addObject:columnName(property)];
    NSString     *sql=[NSString stringWithFormat:@"SELECT %@ FROM %@", [columns componentsJoinedByString:@", "], tableName(root)];
    sqlite3_stmt *statement=NULL;
    if(sqlite3_prepare_v2(DB, [sql UTF8String], -1, &statement, NULL)!=SQLITE_OK){
     if(errorp) *errorp=storeError(NSPersistentStoreIncompatibleVersionHashError, [NSString stringWithUTF8String:sqlite3_errmsg(DB)], nil);
     return NO;
    }
    while(sqlite3_step(statement)==SQLITE_ROW){
     long long            pk=sqlite3_column_int64(statement, 0);
     NSEntityDescription *entity=[self _entityNumbered:sqlite3_column_int64(statement, 1)]?:root;
     NSAtomicStoreCacheNode *node=[self _nodeForEntity:entity key:pk];
     NSDictionary *own=[entity propertiesByName];

     for(NSUInteger i=0;i<[properties count];i++){
      NSPropertyDescription *property=[properties objectAtIndex:i];
      int                    index=(int)i+2;

      if([own objectForKey:[property name]]!=property)
       continue;   /* another entity's of the hierarchy */
      if([property isKindOfClass:[NSAttributeDescription class]])
       [node setValue:columnValue(statement, index, (NSAttributeDescription *)property) forKey:[property name]];
      else if(sqlite3_column_type(statement, index)!=SQLITE_NULL)
       [pendingKeys addObject:[NSArray arrayWithObjects:node, property, [NSNumber numberWithLongLong:sqlite3_column_int64(statement, index)], nil]];
     }
     /* Its to-many relationships, filled below. */
     for(NSRelationshipDescription *relationship in [[entity relationshipsByName] allValues])
      if([relationship isToMany] && ![relationship isTransient])
       [node setValue:[NSMutableSet set] forKey:[relationship name]];
     [nodes addObject:node];
    }
    sqlite3_finalize(statement);
   }

   /* To-one keys, and their inverses. */
   for(NSArray *pending in pendingKeys){
    NSAtomicStoreCacheNode    *node=[pending objectAtIndex:0];
    NSRelationshipDescription *relationship=[pending objectAtIndex:1];
    NSAtomicStoreCacheNode    *target=[_referenceToCacheNode objectForKey:referenceKey(rootEntity([relationship destinationEntity]),
                                                                                       [[pending objectAtIndex:2] longLongValue])];
    if(target==nil)
     continue;
    [node setValue:target forKey:[relationship name]];
    NSRelationshipDescription *inverse=[relationship inverseRelationship];
    if(inverse!=nil && [inverse isToMany])
     [[target valueForKey:[inverse name]] addObject:node];
   }

   /* Many-to-many rows, both ways. */
   for(NSRelationshipDescription *relationship in [self _manyToManyRelationships]){
    NSString     *sql=[NSString stringWithFormat:@"SELECT %@, %@ FROM %@", [self _sourceColumn:relationship],
                       [self _destinationColumn:relationship], [self _joinTable:relationship]];
    sqlite3_stmt *statement=NULL;
    if(sqlite3_prepare_v2(DB, [sql UTF8String], -1, &statement, NULL)!=SQLITE_OK)
     continue;
    NSRelationshipDescription *inverse=[relationship inverseRelationship];
    while(sqlite3_step(statement)==SQLITE_ROW){
     NSAtomicStoreCacheNode *source=[_referenceToCacheNode objectForKey:referenceKey(rootEntity([relationship entity]), sqlite3_column_int64(statement, 0))];
     NSAtomicStoreCacheNode *target=[_referenceToCacheNode objectForKey:referenceKey(rootEntity([relationship destinationEntity]), sqlite3_column_int64(statement, 1))];
     if(source==nil || target==nil)
      continue;
     [[source valueForKey:[relationship name]] addObject:target];
     if(inverse!=nil)
      [[target valueForKey:[inverse name]] addObject:source];
    }
    sqlite3_finalize(statement);
   }

   [self addCacheNodes:nodes];
   return YES;
}

/* ---- Changes ---- */

-newReferenceObjectForManagedObject:(NSManagedObject *)managedObject {
   NSEntityDescription *root=rootEntity([managedObject entity]);
   long long            next=[[_maxKeys objectForKey:[root name]] longLongValue]+1;

   [_maxKeys setObject:[NSNumber numberWithLongLong:next] forKey:[root name]];
   return [NSString stringWithFormat:@"p%lld", next];
}

-(NSAtomicStoreCacheNode *)newCacheNodeForManagedObject:(NSManagedObject *)managedObject {
   NSManagedObjectID      *objectID=[managedObject objectID];
   NSAtomicStoreCacheNode *node=[[NSAtomicStoreCacheNode alloc] initWithObjectID:objectID];

   [_referenceToCacheNode setObject:node forKey:referenceKey(rootEntity([objectID entity]), keyOfNode(node))];
   [_inserted addObject:node];
   [self updateCacheNode:node fromManagedObject:managedObject];
   return node;
}

-(NSAtomicStoreCacheNode *)_nodeForObject:(NSManagedObject *)object {
   NSManagedObjectID *objectID=[object objectID];
   if([objectID isTemporaryID])
    return nil;
   return [_referenceToCacheNode objectForKey:referenceKey(rootEntity([objectID entity]),
                                                           [[[objectID referenceObject] substringFromIndex:1] longLongValue])];
}

-(void)updateCacheNode:(NSAtomicStoreCacheNode *)node fromManagedObject:(NSManagedObject *)managedObject {
   NSEntityDescription *entity=[managedObject entity];

   for(NSAttributeDescription *attribute in [[entity attributesByName] allValues])
    if(![attribute isTransient])
     [node setValue:[managedObject valueForKey:[attribute name]] forKey:[attribute name]];
   for(NSRelationshipDescription *relationship in [[entity relationshipsByName] allValues]){
    if([relationship isTransient])
     continue;
    id value=[managedObject valueForKey:[relationship name]];
    if([relationship isToMany]){
     NSMutableSet *targets=[NSMutableSet set];
     for(NSManagedObject *object in value){
      NSAtomicStoreCacheNode *target=[self _nodeForObject:object];
      if(target!=nil)
       [targets addObject:target];
     }
     [node setValue:targets forKey:[relationship name]];
    }
    else
     [node setValue:value?[self _nodeForObject:value]:nil forKey:[relationship name]];
   }
   if(![_inserted containsObject:node])
    [_updated addObject:node];
}

-(void)willRemoveCacheNodes:(NSSet *)cacheNodes {
   for(NSAtomicStoreCacheNode *node in cacheNodes){
    if([_inserted containsObject:node])
     [_inserted removeObject:node];
    else
     [_deleted addObject:node];
    [_updated removeObject:node];
    [_referenceToCacheNode removeObjectForKey:referenceKey(rootEntity([[node objectID] entity]), keyOfNode(node))];
    [_objectIDToCacheNode removeObjectForKey:[node objectID]];
    [_cacheNodes removeObject:node];
   }
}

-(BOOL)_writeNode:(NSAtomicStoreCacheNode *)node inserting:(BOOL)inserting message:(NSString **)message {
   NSEntityDescription *entity=[[node objectID] entity];
   NSEntityDescription *root=rootEntity(entity);
   NSArray             *properties=tableProperties(root);
   NSDictionary        *own=[entity propertiesByName];
   NSMutableArray      *columns=[NSMutableArray array];

   for(NSPropertyDescription *property in properties)
    [columns addObject:columnName(property)];
   NSString *sql;
   if(inserting){
    NSMutableArray *marks=[NSMutableArray array];
    for(NSUInteger i=0;i<[columns count]+3;i++)
     [marks addObject:@"?"];
    sql=[NSString stringWithFormat:@"INSERT INTO %@ (Z_PK, Z_ENT, Z_OPT%@%@) VALUES (%@)", tableName(root),
         [columns count]?@", ":@"", [columns componentsJoinedByString:@", "], [marks componentsJoinedByString:@", "]];
   }
   else {
    NSMutableArray *sets=[NSMutableArray arrayWithObject:@"Z_OPT = Z_OPT + 1"];
    for(NSString *column in columns)
     [sets addObject:[column stringByAppendingString:@" = ?"]];
    sql=[NSString stringWithFormat:@"UPDATE %@ SET %@ WHERE Z_PK = ?", tableName(root), [sets componentsJoinedByString:@", "]];
   }
   sqlite3_stmt *statement=NULL;
   if(sqlite3_prepare_v2(DB, [sql UTF8String], -1, &statement, NULL)!=SQLITE_OK){
    *message=[NSString stringWithUTF8String:sqlite3_errmsg(DB)];
    return NO;
   }
   int index=1;
   if(inserting){
    sqlite3_bind_int64(statement, index++, keyOfNode(node));
    sqlite3_bind_int64(statement, index++, [self _numberOf:entity]);
    sqlite3_bind_int64(statement, index++, 1);
   }
   for(NSPropertyDescription *property in properties){
    id value=nil;
    if([own objectForKey:[property name]]==property){
     value=[node valueForKey:[property name]];
     if([property isKindOfClass:[NSRelationshipDescription class]])
      value=value?[NSNumber numberWithLongLong:keyOfNode(value)]:nil;
    }
    bindValue(statement, index++, property, value);
   }
   if(!inserting)
    sqlite3_bind_int64(statement, index++, keyOfNode(node));
   BOOL ok=sqlite3_step(statement)==SQLITE_DONE;
   if(!ok)
    *message=[NSString stringWithUTF8String:sqlite3_errmsg(DB)];
   sqlite3_finalize(statement);
   return ok;
}

/* A node's many-to-many rows, written again from its side. */
-(BOOL)_writeJoinsOf:(NSAtomicStoreCacheNode *)node message:(NSString **)message {
   NSEntityDescription *entity=[[node objectID] entity];
   for(NSRelationshipDescription *relationship in [self _manyToManyRelationships]){
    if(![[entity relationshipsByName] objectForKey:[relationship name]] ||
       [[entity relationshipsByName] objectForKey:[relationship name]]!=relationship)
     continue;
    NSString *table=[self _joinTable:relationship], *source=[self _sourceColumn:relationship], *destination=[self _destinationColumn:relationship];
    if(!execute(DB, [NSString stringWithFormat:@"DELETE FROM %@ WHERE %@ = %lld", table, source, keyOfNode(node)], message))
     return NO;
    for(NSAtomicStoreCacheNode *target in [node valueForKey:[relationship name]])
     if(!execute(DB, [NSString stringWithFormat:@"INSERT OR IGNORE INTO %@ (%@, %@) VALUES (%lld, %lld)", table, source, destination,
                      keyOfNode(node), keyOfNode(target)], message))
      return NO;
   }
   return YES;
}

-(BOOL)save:(NSError **)errorp {
   NSString *message=nil;
   BOOL      ok;

   if([self isReadOnly]){
    if(errorp) *errorp=storeError(NSPersistentStoreSaveError, @"The store is read-only", nil);
    return NO;
   }
   ok=execute(DB, @"BEGIN IMMEDIATE", &message);
   for(NSAtomicStoreCacheNode *node in _deleted){
    if(!ok)
     break;
    NSEntityDescription *root=rootEntity([[node objectID] entity]);
    ok=execute(DB, [NSString stringWithFormat:@"DELETE FROM %@ WHERE Z_PK = %lld", tableName(root), keyOfNode(node)], &message);
    for(NSRelationshipDescription *relationship in [self _manyToManyRelationships]){
     if(!ok)
      break;
     if(rootEntity([relationship entity])==root)
      ok=execute(DB, [NSString stringWithFormat:@"DELETE FROM %@ WHERE %@ = %lld", [self _joinTable:relationship],
                      [self _sourceColumn:relationship], keyOfNode(node)], &message);
     if(ok && rootEntity([relationship destinationEntity])==root)
      ok=execute(DB, [NSString stringWithFormat:@"DELETE FROM %@ WHERE %@ = %lld", [self _joinTable:relationship],
                      [self _destinationColumn:relationship], keyOfNode(node)], &message);
    }
   }
   for(NSAtomicStoreCacheNode *node in _inserted)
    if(ok)
     ok=[self _writeNode:node inserting:YES message:&message] && [self _writeJoinsOf:node message:&message];
   for(NSAtomicStoreCacheNode *node in _updated)
    if(ok)
     ok=[self _writeNode:node inserting:NO message:&message] && [self _writeJoinsOf:node message:&message];
   if(ok)
    ok=[self _writePrimaryKeys];
   if(ok){
    NSDictionary *metadata=[self _metadataWith:[self metadata]];
    ok=writeMetadata(DB, metadata, [self identifier]);
    if(ok)
     [self setMetadata:metadata];
   }
   if(ok)
    ok=execute(DB, @"COMMIT", &message);
   if(!ok){
    execute(DB, @"ROLLBACK", NULL);
    if(errorp)
     *errorp=storeError(NSPersistentStoreSaveError, message?message:[NSString stringWithUTF8String:sqlite3_errmsg(DB)], nil);
    return NO;
   }
   [_inserted removeAllObjects];
   [_updated removeAllObjects];
   [_deleted removeAllObjects];
   return YES;
}

@end
