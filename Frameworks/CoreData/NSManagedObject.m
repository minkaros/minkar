/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSManagedObject.h>
#import "NSManagedObjectID-Private.h"
#import "NSManagedObjectContext-Private.h"
#import "NSEntityDescription-Private.h"
#import <CoreData/NSAttributeDescription.h>
#import <CoreData/NSRelationshipDescription.h>
#import <CoreData/NSAtomicStoreCacheNode.h>
#import <Foundation/NSRaise.h>
#import "NSManagedObjectSet.h"
#import "NSManagedObjectSetEnumerator.h"
#import "NSManagedObjectMutableSet.h"
#import <CoreData/NSFetchRequest.h>
#import <CoreData/NSManagedObjectContext.h>
#import <CoreData/NSManagedObjectModel.h>
#import <CoreData/NSPersistentStoreCoordinator.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <ctype.h>
#import <string.h>
#import <pthread.h>

/* Accessors for modeled properties, made when they are first called
 * (+resolveInstanceMethod:): a class's properties are those of the
 * entities it is the class of. The key an accessor works on comes from
 * its selector. */

enum {
   accessorGet,
   accessorSet,
   accessorPrimitiveGet,
   accessorPrimitiveSet,
   accessorAddObject,
   accessorRemoveObject,
   accessorAddSet,
   accessorRemoveSet,
};

static pthread_mutex_t   accessorLock=PTHREAD_MUTEX_INITIALIZER;
static NSMutableDictionary *propertiesByClassName;   // class name -> property name -> NSNumber (to-many)
static NSMutableDictionary *entityByClassName;       // class name -> the entity registered last
static NSMapTable          *keyBySelector;           // SEL -> NSString key

static NSString *keyForSelector(SEL selector) {
   NSString *result;

   pthread_mutex_lock(&accessorLock);
   result=NSMapGet(keyBySelector,selector);
   pthread_mutex_unlock(&accessorLock);
   return result;
}

static void registerEntity(NSEntityDescription *entity) {
   NSString            *className=[entity managedObjectClassName];
   NSMutableDictionary *properties;

   pthread_mutex_lock(&accessorLock);
   if(propertiesByClassName==nil){
    propertiesByClassName=[[NSMutableDictionary alloc] init];
    entityByClassName=[[NSMutableDictionary alloc] init];
    keyBySelector=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSObjectMapValueCallBacks,0);
   }
   if((properties=[propertiesByClassName objectForKey:className])==nil){
    properties=[NSMutableDictionary dictionary];
    [propertiesByClassName setObject:properties forKey:className];
   }
   [entityByClassName setObject:entity forKey:className];
   for(NSPropertyDescription *property in [entity properties]){
    BOOL toMany=[property isKindOfClass:[NSRelationshipDescription class]] && [(NSRelationshipDescription *)property isToMany];

    [properties setObject:[NSNumber numberWithBool:toMany] forKey:[property name]];
   }
   pthread_mutex_unlock(&accessorLock);
}

// The property's to-many flag, or nil when no entity of the class (or a
// superclass) has it.
static NSNumber *modeledProperty(Class class,NSString *key) {
   NSNumber *result=nil;

   pthread_mutex_lock(&accessorLock);
   for(;class!=Nil && result==nil;class=class_getSuperclass(class))
    result=[[propertiesByClassName objectForKey:NSStringFromClass(class)] objectForKey:key];
   pthread_mutex_unlock(&accessorLock);
   return result;
}

static id getObject(NSManagedObject *self,SEL _cmd) {
   return [self valueForKey:keyForSelector(_cmd)];
}

static void setObject(NSManagedObject *self,SEL _cmd,id value) {
   [self setValue:value forKey:keyForSelector(_cmd)];
}

static id getPrimitive(NSManagedObject *self,SEL _cmd) {
   return [self primitiveValueForKey:keyForSelector(_cmd)];
}

static void setPrimitive(NSManagedObject *self,SEL _cmd,id value) {
   [self setPrimitiveValue:value forKey:keyForSelector(_cmd)];
}

static void addObject(NSManagedObject *self,SEL _cmd,id value) {
   [[self mutableSetValueForKey:keyForSelector(_cmd)] addObject:value];
}

static void removeObject(NSManagedObject *self,SEL _cmd,id value) {
   [[self mutableSetValueForKey:keyForSelector(_cmd)] removeObject:value];
}

static void addSet(NSManagedObject *self,SEL _cmd,NSSet *values) {
   [[self mutableSetValueForKey:keyForSelector(_cmd)] unionSet:values];
}

static void removeSet(NSManagedObject *self,SEL _cmd,NSSet *values) {
   [[self mutableSetValueForKey:keyForSelector(_cmd)] minusSet:values];
}

// Scalar properties (@property int16_t age), boxed in NSNumber.
static long long getLongLong(NSManagedObject *self,SEL _cmd) {
   return [[self valueForKey:keyForSelector(_cmd)] longLongValue];
}

static void setInt(NSManagedObject *self,SEL _cmd,int value) {
   [self setValue:[NSNumber numberWithInt:value] forKey:keyForSelector(_cmd)];
}

static void setShort(NSManagedObject *self,SEL _cmd,int value) {
   [self setValue:[NSNumber numberWithShort:(short)value] forKey:keyForSelector(_cmd)];
}

static void setBool(NSManagedObject *self,SEL _cmd,int value) {
   [self setValue:[NSNumber numberWithBool:(BOOL)value] forKey:keyForSelector(_cmd)];
}

static void setLongLong(NSManagedObject *self,SEL _cmd,long long value) {
   [self setValue:[NSNumber numberWithLongLong:value] forKey:keyForSelector(_cmd)];
}

static float getFloat(NSManagedObject *self,SEL _cmd) {
   return [[self valueForKey:keyForSelector(_cmd)] floatValue];
}

static void setFloat(NSManagedObject *self,SEL _cmd,float value) {
   [self setValue:[NSNumber numberWithFloat:value] forKey:keyForSelector(_cmd)];
}

static double getDouble(NSManagedObject *self,SEL _cmd) {
   return [[self valueForKey:keyForSelector(_cmd)] doubleValue];
}

static void setDouble(NSManagedObject *self,SEL _cmd,double value) {
   [self setValue:[NSNumber numberWithDouble:value] forKey:keyForSelector(_cmd)];
}

// The type of a declared property, as its type encoding's first character.
static char declaredType(Class class,NSString *key) {
   for(;class!=Nil;class=class_getSuperclass(class)){
    objc_property_t property=class_getProperty(class,[key UTF8String]);

    if(property!=NULL){
     char *type=property_copyAttributeValue(property,"T");
     char  result=(type!=NULL)?type[0]:'@';

     free(type);
     return result;
    }
   }
   return '@';
}

static NSString *lowercaseFirst(NSString *string) {
   if([string length]==0)
    return string;
   return [[[string substringToIndex:1] lowercaseString] stringByAppendingString:[string substringFromIndex:1]];
}

// The key and kind of accessor a selector names, for a modeled property.
static NSString *parseAccessor(Class class,const char *name,int *kind) {
   size_t      length=strlen(name);
   const char *colon=strchr(name,':');
   struct { const char *prefix,*suffix; int kind; } patterns[]={
    { "setPrimitive", ":", accessorPrimitiveSet },
    { "primitive", "", accessorPrimitiveGet },
    { "add", "Object:", accessorAddObject },
    { "remove", "Object:", accessorRemoveObject },
    { "add", ":", accessorAddSet },
    { "remove", ":", accessorRemoveSet },
    { "set", ":", accessorSet },
   };
   int i;

   if(colon!=NULL && colon!=name+length-1)
    return nil;

   for(i=0;i<sizeof(patterns)/sizeof(patterns[0]);i++){
    size_t plen=strlen(patterns[i].prefix),slen=strlen(patterns[i].suffix);

    if(length>plen+slen && strncmp(name,patterns[i].prefix,plen)==0 &&
       strcmp(name+length-slen,patterns[i].suffix)==0 && isupper((unsigned char)name[plen])){
     NSString *middle=[[[NSString alloc] initWithBytes:name+plen length:length-plen-slen encoding:NSUTF8StringEncoding] autorelease];
     NSString *key=lowercaseFirst(middle);
     NSNumber *toMany=modeledProperty(class,key);

     if(toMany==nil)
      toMany=modeledProperty(class,key=middle);
     if(toMany==nil)
      continue;
     if(patterns[i].kind>=accessorAddObject && ![toMany boolValue])
      continue;
     *kind=patterns[i].kind;
     return key;
    }
   }

   if(colon==NULL){
    NSString *key=[NSString stringWithUTF8String:name];

    if(modeledProperty(class,key)!=nil){
     *kind=accessorGet;
     return key;
    }
   }
   return nil;
}

@implementation NSManagedObject (accessors)

+(void)_registerEntity:(NSEntityDescription *)entity {
   registerEntity(entity);
}


+(BOOL)resolveInstanceMethod:(SEL)selector {
   int       kind;
   NSString *key=parseAccessor(self,sel_getName(selector),&kind);
   IMP       imp=NULL;
   const char *types=NULL;

   if(key==nil)
    return [super resolveInstanceMethod:selector];

   switch(kind){
    case accessorGet:
     switch(declaredType(self,key)){
      case 'f': imp=(IMP)getFloat; types="f@:"; break;
      case 'd': imp=(IMP)getDouble; types="d@:"; break;
      case 'c': case 'C': case 'B': case 's': case 'S': case 'i': case 'I':
      case 'l': case 'L': case 'q': case 'Q':
       imp=(IMP)getLongLong; types="q@:"; break;
      default: imp=(IMP)getObject; types="@@:"; break;
     }
     break;
    case accessorSet:
     switch(declaredType(self,key)){
      case 'f': imp=(IMP)setFloat; types="v@:f"; break;
      case 'd': imp=(IMP)setDouble; types="v@:d"; break;
      case 'c': case 'C': case 'B': imp=(IMP)setBool; types="v@:c"; break;
      case 's': case 'S': imp=(IMP)setShort; types="v@:s"; break;
      case 'i': case 'I': imp=(IMP)setInt; types="v@:i"; break;
      case 'l': case 'L': case 'q': case 'Q': imp=(IMP)setLongLong; types="v@:q"; break;
      default: imp=(IMP)setObject; types="v@:@"; break;
     }
     break;
    case accessorPrimitiveGet: imp=(IMP)getPrimitive; types="@@:"; break;
    case accessorPrimitiveSet: imp=(IMP)setPrimitive; types="v@:@"; break;
    case accessorAddObject: imp=(IMP)addObject; types="v@:@"; break;
    case accessorRemoveObject: imp=(IMP)removeObject; types="v@:@"; break;
    case accessorAddSet: imp=(IMP)addSet; types="v@:@"; break;
    case accessorRemoveSet: imp=(IMP)removeSet; types="v@:@"; break;
   }

   pthread_mutex_lock(&accessorLock);
   NSMapInsert(keyBySelector,selector,key);
   pthread_mutex_unlock(&accessorLock);
   class_addMethod(self,selector,imp,types);
   return YES;
}

@end


@implementation NSManagedObject

// The entity of a coordinator's model whose class this is.
+(NSEntityDescription *)entity {
   NSEntityDescription *result;

   pthread_mutex_lock(&accessorLock);
   result=[[[entityByClassName objectForKey:NSStringFromClass(self)] retain] autorelease];
   pthread_mutex_unlock(&accessorLock);
   return result;
}

+(NSFetchRequest *)fetchRequest {
   return [NSFetchRequest fetchRequestWithEntityName:[[self entity] name]];
}

+(BOOL)contextShouldIgnoreUnmodeledPropertyChanges {
   return YES;
}

-(instancetype)initWithContext:(NSManagedObjectContext *)context {
   NSEntityDescription *entity=nil;

   for(NSEntityDescription *check in [[[context persistentStoreCoordinator] managedObjectModel] entities])
    if([[check managedObjectClassName] isEqualToString:NSStringFromClass([self class])])
     entity=check;
   if(entity==nil)
    [NSException raise:NSInternalInconsistencyException format:@"No entity of the context's model has the class %@",[self class]];
   return [self initWithEntity:entity insertIntoManagedObjectContext:context];
}

-(BOOL)hasChanges {
   return [self isInserted] || [self isUpdated] || [self isDeleted];
}

-(BOOL)hasPersistentChangedValues {
   return [_changedValues count]>0;
}


-init {
   NSLog(@"Error - can't initialize an NSManagedObject with -init");
   return nil;
}

// An object of the entity's class, or of the class it was allocated as
// when that is a subclass of the entity's.
-initWithObjectID:(NSManagedObjectID *)objectID managedObjectContext:(NSManagedObjectContext *)context {
   NSEntityDescription *entity=[objectID entity];
   Class                class=[entity _managedObjectClass];

   if(class!=[self class] && [class isSubclassOfClass:[self class]]){
    [super dealloc];
    self=[class allocWithZone:NULL];
   }

   _objectID=[objectID copy];
   _context=context;
   _committedValues = nil;
   _changedValues = [[NSMutableDictionary alloc] init];
   _isFault=YES;
   return self;
}

-initWithEntity:(NSEntityDescription *)entity insertIntoManagedObjectContext:(NSManagedObjectContext *)context {
   registerEntity(entity);
   NSManagedObjectID *objectID=[[[NSManagedObjectID alloc] initWithEntity:entity] autorelease];
   
   if((self=[self initWithObjectID:objectID managedObjectContext:context])==nil)
    return nil;

   NSDictionary *attributes=[entity attributesByName];
   NSEnumerator *state=[attributes keyEnumerator];
   NSString     *key;
   
   while((key=[state nextObject])!=nil){
    id object=[attributes objectForKey:key];
    id value=[object defaultValue];
    
    if(value!=nil)
     [self setPrimitiveValue:value forKey:key]; 
   }

   _isFault=NO;
   [context insertObject:self];
   [self awakeFromInsert];

   return self;
}

-(void)dealloc {
   [self didTurnIntoFault];
   
   [_objectID release];
   [_changedValues release];
   [super dealloc];
}

-(NSUInteger)hash {
   return [_objectID hash];
}

-(BOOL)isEqual:otherX {
   if(![otherX isKindOfClass:[NSManagedObject class]])
    return NO;
   
   NSManagedObject *other=otherX;
   
   return [_objectID isEqual:other->_objectID];
}

-(NSEntityDescription *)entity {
   return [_objectID entity];
}

-(NSManagedObjectID *)objectID {
   return _objectID;
}

-self {
   return self;
}

-(NSManagedObjectContext *)managedObjectContext {
   return _context;
}

// As the context sees the object, until it is saved.
-(BOOL)isInserted {
   return [[_context insertedObjects] containsObject:self];
}

-(BOOL)isUpdated {
   return [[_context updatedObjects] containsObject:self];
}

-(BOOL)isDeleted {
   return [[_context deletedObjects] containsObject:self];
}

-(BOOL)isFault {
   return _isFault;
}

-(BOOL)hasFaultForRelationshipNamed:(NSString *)key {
   return NO;
}

// For subclasses: called after the object is fetched, or inserted.
-(void)awakeFromFetch {
}

-(void)awakeFromInsert {
}

-(NSDictionary *)changedValues {
   return _changedValues;
}

-(NSDictionary *)_committedValues {

   if([[self objectID] isTemporaryID])
    return nil;
    
   if(_committedValues==nil){
    NSAtomicStoreCacheNode *node=[_context _cacheNodeForObjectID:[self objectID]];
    NSDictionary           *propertyCache=[node propertyCache];
    NSMutableDictionary    *storedValues=[[NSMutableDictionary alloc] init];
    
    NSArray *properties=[[self entity] properties];
    
    for(NSPropertyDescription *property in properties){
     NSString *name=[property name];
     
     if([property isKindOfClass:[NSAttributeDescription class]]){
      id value=[propertyCache objectForKey:name];
      
      if(value!=nil){
       [storedValues setObject:value forKey:name];
      }
     }
     else if([property isKindOfClass:[NSRelationshipDescription class]]){
      NSRelationshipDescription *relationship=(NSRelationshipDescription *)property;
      id                         indirectValue=[propertyCache objectForKey:name];
 
      if(indirectValue!=nil){
       id value;
             
       if(![relationship isToMany])
        value=[indirectValue objectID];
       else {
        value=[NSMutableSet set];
        
        for(NSAtomicStoreCacheNode *relNode in indirectValue){
         [value addObject:[relNode objectID]];
        }
       }
       
       [storedValues setObject:value forKey:name];
      }
     }
    }
    
    [_committedValues release];
    _committedValues=storedValues;
   }
   return _committedValues;
}

-(NSDictionary *)committedValuesForKeys:(NSArray *)keys {   
   if(keys==nil)
    return [self _committedValues];
   else {
    NSMutableDictionary *result=[NSMutableDictionary dictionary];
    
    for(NSString *key in keys){
     id object=[[self _committedValues] objectForKey:key];
     
     if(object!=nil){
      [result setObject:object forKey:key];
     }
    }
    
    return result;
   }
}

- (void) didSave {
}

- (void) willTurnIntoFault {
  // do nothing per doc.s
}

- (void) didTurnIntoFault {
  // do nothing also?
}

- (void) willSave {

}

-valueForKey:(NSString *)key {
   if(!key)
    return [self valueForUndefinedKey:nil];

   NSPropertyDescription *property=[[[self entity] propertiesByName] objectForKey:key];
   NSString *propertyName=[property name];
   
   if([property isKindOfClass:[NSAttributeDescription class]]){
    [self willAccessValueForKey:propertyName];
    
    id result=[self primitiveValueForKey:propertyName];
    
    [self didAccessValueForKey:propertyName];
    
    return result;
   }
   else if([property isKindOfClass:[NSRelationshipDescription class]]){
    NSRelationshipDescription *relationship=(NSRelationshipDescription *)property;
    
    [self willAccessValueForKey:propertyName];

    id result=[self primitiveValueForKey:propertyName];
    
    if(result!=nil){
     if([relationship isToMany])
      result=[[[NSManagedObjectSet alloc] initWithManagedObjectContext:_context set:result] autorelease];
     else
      result=[_context objectWithID:result];
    }
    
    [self didAccessValueForKey:propertyName];
    
    return result;
   }
  
   return [super valueForKey:key];
}


-(void)setValue:value forKey:(NSString *) key {
   NSPropertyDescription *property=[[[self entity] propertiesByName] objectForKey:key];
   NSString              *propertyName=[property name];
   
   if([property isKindOfClass:[NSAttributeDescription class]]){
    [self willChangeValueForKey:propertyName];
    [self setPrimitiveValue:value forKey:propertyName];
    [self didChangeValueForKey:propertyName];
    return;
   }
   else if([property isKindOfClass:[NSRelationshipDescription class]]){
    NSRelationshipDescription *relationship=(NSRelationshipDescription *)property;
    NSRelationshipDescription *inverse=[relationship inverseRelationship];
    NSString                  *inverseName=[inverse name];
    id                         valueByID;
        
    if([relationship isToMany]){
     NSMutableSet *set=[NSMutableSet set];
     
     for(NSManagedObject *object in value)
      [set addObject:[object objectID]];
      
     valueByID=set;
    }
    else {
     valueByID=[value objectID];
    }

    if(inverse!=nil){
     id primitivePrevious=[self primitiveValueForKey:key];
     
     if(primitivePrevious!=nil){
      NSSet *allPrevious=[relationship isToMany]?primitivePrevious:[NSSet setWithObject:primitivePrevious];
      
      for(NSManagedObjectID *previousID in allPrevious){
       NSManagedObject *previous=[_context objectWithID:previousID];

     // FIXME: should be using set mutation notifications for to many
       [previous willChangeValueForKey:inverseName];

       if([inverse isToMany])
        [[previous primitiveValueForKey:inverseName] removeObject:[self objectID]];
       else
        [previous setPrimitiveValue:nil forKey:inverseName];
      
       [previous didChangeValueForKey:inverseName];
      }
     }
    }

    [self willChangeValueForKey:propertyName];
    [self setPrimitiveValue:valueByID forKey:propertyName];
    [self didChangeValueForKey:propertyName];

    if(inverse!=nil){     
     NSSet *allValues=[relationship isToMany]?valueByID:[NSSet setWithObject:valueByID];

     for(NSManagedObjectID *valueID in allValues){
      NSManagedObject *value=[_context objectWithID:valueID];

     // FIXME: should be using set mutation notifications for to many
      [value willChangeValueForKey:inverseName];
     
      if([inverse isToMany]){
       NSMutableSet *set=[value primitiveValueForKey:inverseName];
    
       if(set==nil){
        set=[NSMutableSet set];
        [value setPrimitiveValue:set forKey:inverseName];
       }

       [set addObject:[self objectID]];
      }
      else{
       [value setPrimitiveValue:[self objectID] forKey:inverseName];
      }
     
      [value didChangeValueForKey:inverseName];
     }
    }
    return;
   }
   
   [super setValue:value forKey:key];
}

-(NSMutableSet *) mutableSetValueForKey:(NSString *) key {
   return [[[NSManagedObjectMutableSet alloc] initWithManagedObject:self key:key] autorelease];
}

-primitiveValueForKey:(NSString *) key {
   id result=[_changedValues objectForKey:key];
   
   if(result==nil)
    result=[[self _committedValues] objectForKey:key];
   
   return result;
}

-(void)setPrimitiveValue:value forKey:(NSString *)key {
   if(value==nil)
    [_changedValues removeObjectForKey:key];
   else {
    [_changedValues setObject:value forKey:key];
   }
}

- (BOOL) validateValue:(id *) value forKey:(NSString *) key error:(NSError **) error {
    return YES;
}


- (BOOL) validateForDelete:(NSError **) error {
    return YES;
}


- (BOOL) validateForInsert:(NSError **) error {
    return YES;
}


- (BOOL) validateForUpdate:(NSError **) error {
    return YES;
}


+(BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
  // FIXME: Doc.s for NSManagedObject state this returns YES for unmodeled properties.

   return NO;
}

-(void)didAccessValueForKey:(NSString *) key {
}

-(void *)observationInfo {
   return _observationInfo;
}


-(void)setObservationInfo:(void *) value {
   _observationInfo=value;
}

-(void)willAccessValueForKey:(NSString *)key {
}

-(NSString *)description {
   NSMutableDictionary *values=[NSMutableDictionary dictionaryWithDictionary:[self _committedValues]];
   
   [values addEntriesFromDictionary:_changedValues];
   
   return [NSString stringWithFormat:@"<%@ %x:objectID=%@ entity name=%@, values=%@>",[self class],self,_objectID,[self entity],values];
}

@end

