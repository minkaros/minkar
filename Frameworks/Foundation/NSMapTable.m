/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSMapTable.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <objc/runtime.h>

// Minkar: weak keys and values are zeroing weak references (objc_storeWeak
// in the node), and every access goes through keyOf()/valueOf(); a node
// whose weak key or value is gone is dropped where it is met. Tables made
// with NSPointerFunctionsOptions get callbacks from them. The C functions
// keep their behaviour.

OBJC_EXPORT id objc_storeWeak(id *location, id obj);
OBJC_EXPORT id objc_loadWeakRetained(id *location);
OBJC_EXPORT void objc_destroyWeak(id *location);

typedef struct _NSMapNode {
   struct _NSMapNode *next;
   void *key;
   void *value;
} NSMapNode;

const void *NSNotAnIntMapKey=(const void *)0x80000000;
const void *NSNotAPointerMapKey=(const void *)0xffffffff;

static NSUInteger _NSMapPointerHash(NSMapTable *table,const void *object){
   return (NSUInteger)object>>5;
}

static NSUInteger _NSMapObjectHash(NSMapTable *table,const void *object){
   return [(id)object hash];
}

static BOOL _NSMapPointerIsEqual(NSMapTable *table,const void *object1,
  const void *object2){
   return (object1==object2)?YES:NO;
}

static BOOL _NSMapObjectIsEqual(NSMapTable *table,const void *object1,
  const void *object2){
   BOOL result=[(id)object1 isEqual:(id)object2];

   return result;
}

static void _NSMapEmptyRetain(NSMapTable *table,const void *object){
}

static void _NSMapObjectRetain(NSMapTable *table,const void *object){
   [(id)object retain];
}

static void _NSMapEmptyRelease(NSMapTable *table,void *object){
}

static void _NSMapObjectRelease(NSMapTable *table,void *object){
   [(id)object release];
}

static void _NSMapPointerRelease(NSMapTable *table,void *object){
   NSZoneFree(NSZoneFromPointer(object),object);
}

static NSString *_NSMapEmptyDescribe(NSMapTable *table,const void *object){
   return nil;
}

static NSString *_NSMapObjectDescribe(NSMapTable *table,const void *object){
   return [(id)object description];
}

const NSMapTableKeyCallBacks NSIntMapKeyCallBacks={
 NULL, NULL, NULL, NULL, NULL
};

const NSMapTableValueCallBacks NSIntMapValueCallBacks={
 NULL, NULL, NULL
};

const NSMapTableKeyCallBacks NSIntegerMapKeyCallBacks={
 NULL, NULL, NULL, NULL, NULL
};

const NSMapTableValueCallBacks NSIntegerMapValueCallBacks={
 NULL, NULL, NULL
};

const NSMapTableKeyCallBacks NSNonOwnedPointerMapKeyCallBacks={
 NULL, NULL, NULL, NULL, NULL
};
 
const NSMapTableValueCallBacks NSNonOwnedPointerMapValueCallBacks={
 NULL, NULL, NULL
};

const NSMapTableKeyCallBacks NSNonOwnedPointerOrNullMapKeyCallBacks={
 NULL, NULL, NULL, NULL, NULL
};
 
const NSMapTableKeyCallBacks NSNonRetainedObjectMapKeyCallBacks={
 _NSMapObjectHash, _NSMapObjectIsEqual, NULL, NULL, _NSMapObjectDescribe
};
 
const NSMapTableValueCallBacks NSNonRetainedObjectMapValueCallBacks={
 NULL, NULL, _NSMapObjectDescribe
};

const NSMapTableKeyCallBacks NSObjectMapKeyCallBacks={
 _NSMapObjectHash, _NSMapObjectIsEqual, _NSMapObjectRetain, _NSMapObjectRelease, _NSMapObjectDescribe
};
 
const NSMapTableValueCallBacks NSObjectMapValueCallBacks={
 _NSMapObjectRetain, _NSMapObjectRelease, _NSMapObjectDescribe
};
 
const NSMapTableKeyCallBacks NSOwnedPointerMapKeyCallBacks={
 NULL, NULL, NULL, _NSMapPointerRelease, NULL
};
 
const NSMapTableValueCallBacks NSOwnedPointerMapValueCallBacks={
 NULL, _NSMapPointerRelease, NULL
};

/* ---- Node access -------------------------------------------------------- */

@interface NSMapTable (NSMapTableNodes)
-(NSMapTableKeyCallBacks *)_keyCallBacks;
@end

static inline BOOL tableWeakKeys(NSMapTable *t);
static inline BOOL tableWeakValues(NSMapTable *t);

// A weak key or value read back retained and autoreleased (NULL when gone).
static void *keyOf(NSMapTable *t,NSMapNode *n){
   if(!tableWeakKeys(t))
    return n->key;
   return [objc_loadWeakRetained((id *)&n->key) autorelease];
}

static void *valueOf(NSMapTable *t,NSMapNode *n){
   if(!tableWeakValues(t))
    return n->value;
   return [objc_loadWeakRetained((id *)&n->value) autorelease];
}

static BOOL nodeDead(NSMapTable *t,NSMapNode *n){
   return (tableWeakKeys(t) && keyOf(t,n)==NULL) || (tableWeakValues(t) && valueOf(t,n)==NULL);
}

@implementation NSMapTable

static inline BOOL tableWeakKeys(NSMapTable *t){ return t->weakKeys; }
static inline BOOL tableWeakValues(NSMapTable *t){ return t->weakValues; }

static void setNode(NSMapTable *t,NSMapNode *n,const void *key,const void *value){
   if(t->weakKeys)
    objc_storeWeak((id *)&n->key,(id)key);
   else {
    t->keyCallBacks->retain(t,key);
    n->key=(void *)key;
   }
   if(t->weakValues)
    objc_storeWeak((id *)&n->value,(id)value);
   else {
    t->valueCallBacks->retain(t,value);
    n->value=(void *)value;
   }
}

static void releaseNode(NSMapTable *t,NSMapNode *n){
   if(t->weakKeys)
    objc_destroyWeak((id *)&n->key);
   else
    t->keyCallBacks->release(t,n->key);
   if(t->weakValues)
    objc_destroyWeak((id *)&n->value);
   else
    t->valueCallBacks->release(t,n->value);
}

static void freeNode(NSMapTable *t,NSMapNode *n){
   releaseNode(t,n);
   NSZoneFree(NSZoneFromPointer(n),n);
   t->count--;
}

// The node for key, or NULL; drops dead nodes met in the bucket.
static NSMapNode *findNode(NSMapTable *t,const void *key){
   NSMapNode **link=&t->buckets[t->keyCallBacks->hash(t,key)%t->nBuckets];

   while(*link!=NULL){
    NSMapNode *n=*link;
    void *k;

    if(nodeDead(t,n)){
     *link=n->next;
     freeNode(t,n);
     continue;
    }
    k=keyOf(t,n);
    if(k==key || t->keyCallBacks->isEqual(t,k,key))
     return n;
    link=&n->next;
   }
   return NULL;
}

static void purgeDead(NSMapTable *t){
   NSUInteger i;

   if(!t->weakKeys && !t->weakValues)
    return;
   for(i=0;i<t->nBuckets;i++){
    NSMapNode **link=&t->buckets[i];
    while(*link!=NULL){
     NSMapNode *n=*link;
     if(nodeDead(t,n)){
      *link=n->next;
      freeNode(t,n);
      continue;
     }
     link=&n->next;
    }
   }
}

static void initTable(NSMapTable *table,NSMapTableKeyCallBacks keyCallBacks,
   NSMapTableValueCallBacks valueCallBacks,NSUInteger capacity,NSZone *zone){
   table->keyCallBacks=NSZoneMalloc(zone,sizeof(NSMapTableKeyCallBacks));
   table->keyCallBacks->hash=(keyCallBacks.hash!=NULL)?keyCallBacks.hash:_NSMapPointerHash;
   table->keyCallBacks->isEqual=(keyCallBacks.isEqual!=NULL)?keyCallBacks.isEqual:_NSMapPointerIsEqual;
   table->keyCallBacks->retain=(keyCallBacks.retain!=NULL)?keyCallBacks.retain:_NSMapEmptyRetain;
   table->keyCallBacks->release=(keyCallBacks.release!=NULL)?keyCallBacks.release:_NSMapEmptyRelease;
   table->keyCallBacks->describe=(keyCallBacks.describe!=NULL)?keyCallBacks.describe:_NSMapEmptyDescribe;
   table->keyCallBacks->notAKeyMarker=keyCallBacks.notAKeyMarker;

   table->valueCallBacks=NSZoneMalloc(zone,sizeof(NSMapTableValueCallBacks));
   table->valueCallBacks->retain=(valueCallBacks.retain!=NULL)?valueCallBacks.retain:_NSMapEmptyRetain;
   table->valueCallBacks->release=(valueCallBacks.release!=NULL)?valueCallBacks.release:_NSMapEmptyRelease;
   table->valueCallBacks->describe=(valueCallBacks.describe!=NULL)?valueCallBacks.describe:_NSMapEmptyDescribe;

   table->count=0;
   table->nBuckets=(capacity<4)?4:capacity;
   table->buckets=NSZoneCalloc(zone,table->nBuckets,sizeof(NSMapNode *));
}

NSMapTable *NSCreateMapTable(NSMapTableKeyCallBacks keyCallBacks,
   NSMapTableValueCallBacks valueCallBacks,NSUInteger capacity) {
   return NSCreateMapTableWithZone(keyCallBacks,valueCallBacks,capacity,NULL);
}

NSMapTable *NSCreateMapTableWithZone(NSMapTableKeyCallBacks keyCallBacks,
   NSMapTableValueCallBacks valueCallBacks,NSUInteger capacity,NSZone *zone) {
   NSMapTable *table=[NSMapTable allocWithZone:zone];

   initTable(table,keyCallBacks,valueCallBacks,capacity,zone);
   return table;
}

NSMapTable *NSCopyMapTableWithZone(NSMapTable *table,NSZone *zone){
   NSMapTable *newTable=[NSMapTable allocWithZone:zone];
   NSMapEnumerator state;
   void *key,*val;

   initTable(newTable,*(table->keyCallBacks),*(table->valueCallBacks),table->count,zone);
   newTable->keyOptions=table->keyOptions;
   newTable->valueOptions=table->valueOptions;
   newTable->weakKeys=table->weakKeys;
   newTable->weakValues=table->weakValues;
   state=NSEnumerateMapTable(table);
   while(NSNextMapEnumeratorPair(&state,&key,&val))
    NSMapInsert(newTable,key,val);
   return newTable;
}

static void removeAllNodes(NSMapTable *table){
   NSUInteger i;
   NSMapNode *j,*next;

   for(i=0;i<table->nBuckets;i++){
    for(j=table->buckets[i];j!=NULL;j=next){
     next=j->next;
     releaseNode(table,j);
     NSZoneFree(NSZoneFromPointer(j),j);
    }
    table->buckets[i]=NULL;
   }
   table->count=0;
   table->mutations++;
}

void NSFreeMapTable(NSMapTable *table){
   NSZone *zone=NSZoneFromPointer(table);

   removeAllNodes(table);
   NSZoneFree(zone,table->buckets);
   NSZoneFree(zone,table->keyCallBacks);
   NSZoneFree(zone,table->valueCallBacks);
   NSDeallocateObject(table);
}

void NSResetMapTable(NSMapTable *table){
   removeAllNodes(table);
}

BOOL NSCompareMapTables(NSMapTable *table1,NSMapTable *table2){
   NSUInteger i;
   NSMapNode *j;

   purgeDead(table1);
   purgeDead(table2);
   if(table1->count!=table2->count)
    return NO;

   for(i=0;i<table1->nBuckets;i++)
    for(j=table1->buckets[i];j!=NULL;j=j->next){
     void *key=keyOf(table1,j);
     if(NSMapGet(table2,key)!=valueOf(table1,j))
      return NO;
    }

   return YES;
}

NSUInteger NSCountMapTable(NSMapTable *table){
   purgeDead(table);
   return table->count;
}

BOOL NSMapMember(NSMapTable *table,const void *key,void **originalKey,
   void **value){
   NSMapNode *n=findNode(table,key);

   if(n==NULL)
    return NO;
   *originalKey=keyOf(table,n);
   *value=valueOf(table,n);
   return YES;
}

void *NSMapGet(NSMapTable *table,const void *key){
   NSMapNode *n=findNode(table,key);

   return n?valueOf(table,n):NULL;
}

NSMapEnumerator NSEnumerateMapTable(NSMapTable *table){
   NSMapEnumerator state;

   purgeDead(table);
   state.table=table;
   for(state.i=0;state.i<(NSInteger)table->nBuckets;state.i++)
    if(table->buckets[state.i]!=NULL)
     break;
   state.j=(state.i<(NSInteger)table->nBuckets)?table->buckets[state.i]:NULL;

   return state;
}

BOOL NSNextMapEnumeratorPair(NSMapEnumerator *state,void **key,
   void **value){
   NSMapTable *t=state->table;

   for(;;){
    NSMapNode *n=state->j;

    if(n==NULL)
     return NO;
    if((state->j=n->next)==NULL){
     for(state->i++;state->i<(NSInteger)t->nBuckets;state->i++)
      if((state->j=t->buckets[state->i])!=NULL)
       break;
    }
    if(!nodeDead(t,n)){
     *key=keyOf(t,n);
     *value=valueOf(t,n);
     return YES;
    }
   }
}

void NSEndMapTableEnumeration(NSMapEnumerator *state){
   state->j=NULL;
}

NSArray *NSAllMapTableKeys(NSMapTable *table){
   NSMutableArray *array;
   NSMapEnumerator state=NSEnumerateMapTable(table);
   void *key,*value;

   array=[[[NSMutableArray allocWithZone:NULL] initWithCapacity:table->count] autorelease];
   while(NSNextMapEnumeratorPair(&state,&key,&value))
    [array addObject:(id)key];
   return array;
}

NSArray *NSAllMapTableValues(NSMapTable *table){
   NSMutableArray *array;
   NSMapEnumerator state=NSEnumerateMapTable(table);
   void *key,*value;

   array=[[[NSMutableArray allocWithZone:NULL] initWithCapacity:table->count] autorelease];
   while(NSNextMapEnumeratorPair(&state,&key,&value))
    [array addObject:(id)value];
   return array;
}

void NSMapInsert(NSMapTable *table,const void *key,const void *value){
   NSZone    *zone;
   NSUInteger i;
   NSMapNode *j=findNode(table,key);

   table->mutations++;
   if(j!=NULL){
    NSMapNode old=*j;

    setNode(table,j,key,value);
    if(!table->weakKeys)
     table->keyCallBacks->release(table,old.key);
    if(!table->weakValues)
     table->valueCallBacks->release(table,old.value);
    return;
   }
   zone=NSZoneFromPointer(table);

   if(table->count>=table->nBuckets){
    NSUInteger nBuckets=table->nBuckets;
    NSMapNode **buckets=table->buckets,*next;

    table->nBuckets=nBuckets*2;
    table->buckets=NSZoneCalloc(zone,table->nBuckets,sizeof(NSMapNode *));

    for(i=0;i<nBuckets;i++)
     for(j=buckets[i];j!=NULL;j=next){
      next=j->next;
      if(nodeDead(table,j)){
       releaseNode(table,j);
       NSZoneFree(NSZoneFromPointer(j),j);
       table->count--;
       continue;
      }
      NSUInteger newi=table->keyCallBacks->hash(table,keyOf(table,j))%table->nBuckets;
      j->next=table->buckets[newi];
      table->buckets[newi]=j;
     }
    NSZoneFree(zone,buckets);
   }

   i=table->keyCallBacks->hash(table,key)%table->nBuckets;
   j=NSZoneCalloc(zone,1,sizeof(NSMapNode));
   setNode(table,j,key,value);
   j->next=table->buckets[i];
   table->buckets[i]=j;
   table->count++;
}

void *NSMapInsertIfAbsent(NSMapTable *table,const void *key,const void *value){
   void *old=NSMapGet(table,key);

   if(old!=NULL)
    return old;
   NSMapInsert(table,key,value);
   return NULL;
}

void NSMapInsertKnownAbsent(NSMapTable *table,const void *key,
   const void *value){
   NSMapInsert(table,key,value);
}

void NSMapRemove(NSMapTable *table,const void *key){
   NSMapNode **link=&table->buckets[table->keyCallBacks->hash(table,key)%table->nBuckets];

   while(*link!=NULL){
    NSMapNode *n=*link;
    void *k=keyOf(table,n);

    if(nodeDead(table,n) || (k!=NULL && (k==key || table->keyCallBacks->isEqual(table,k,key)))){
     BOOL found=!nodeDead(table,n);
     *link=n->next;
     freeNode(table,n);
     table->mutations++;
     if(found)
      return;
     continue;
    }
    link=&n->next;
   }
}

NSString *NSStringFromMapTable(NSMapTable *table){
   NSMutableString *string=[NSMutableString string];
   NSString *fmt=@"%p",*eq=@" = ",*nl=@";\n";
   NSMapEnumerator state=NSEnumerateMapTable(table);
   void *key,*value;

   while(NSNextMapEnumeratorPair(&state,&key,&value)){
    NSString *desc;

    if((desc=table->keyCallBacks->describe(table,key))!=nil)
     [string appendString:desc];
    else
     [string appendFormat:fmt,key];
    [string appendString:eq];
    if((desc=table->valueCallBacks->describe(table,value))!=nil)
     [string appendString:desc];
    else
     [string appendFormat:fmt,value];
    [string appendString:nl];
   }

   return string;
}

/* ---- The class ---------------------------------------------------------- */

static BOOL optionsHoldObjects(NSPointerFunctionsOptions o){
   NSUInteger p=o&0xff00;
   return p==NSPointerFunctionsObjectPersonality || p==NSPointerFunctionsObjectPointerPersonality;
}

static BOOL optionsWeak(NSPointerFunctionsOptions o){
   NSUInteger m=o&0xff;
   return m==NSPointerFunctionsWeakMemory || m==NSPointerFunctionsZeroingWeakMemory;
}

static void _NSMapCopyInRetain(NSMapTable *table,const void *object){
   // The copy is made by setObject:forKey:; here it is only kept.
}

-(instancetype)initWithKeyOptions:(NSPointerFunctionsOptions)kOptions
                     valueOptions:(NSPointerFunctionsOptions)vOptions capacity:(NSUInteger)capacity {
   NSMapTableKeyCallBacks   k={ NULL, NULL, NULL, NULL, NULL, NULL };
   NSMapTableValueCallBacks v={ NULL, NULL, NULL };

   if((kOptions&0xff00)==NSPointerFunctionsObjectPersonality){
    k.hash=_NSMapObjectHash;
    k.isEqual=_NSMapObjectIsEqual;
   }
   if(optionsHoldObjects(kOptions)){
    k.describe=_NSMapObjectDescribe;
    if((kOptions&0xff)==NSPointerFunctionsStrongMemory){
     k.retain=(kOptions&NSPointerFunctionsCopyIn)?_NSMapCopyInRetain:_NSMapObjectRetain;
     k.release=_NSMapObjectRelease;
    }
   }
   if(optionsHoldObjects(vOptions)){
    v.describe=_NSMapObjectDescribe;
    if((vOptions&0xff)==NSPointerFunctionsStrongMemory){
     v.retain=(vOptions&NSPointerFunctionsCopyIn)?_NSMapCopyInRetain:_NSMapObjectRetain;
     v.release=_NSMapObjectRelease;
    }
   }
   if((kOptions&0xff)==NSPointerFunctionsMallocMemory)
    k.release=_NSMapPointerRelease;
   if((vOptions&0xff)==NSPointerFunctionsMallocMemory)
    v.release=_NSMapPointerRelease;

   initTable(self,k,v,capacity,NSZoneFromPointer(self));
   keyOptions=kOptions;
   valueOptions=vOptions;
   weakKeys=optionsWeak(kOptions);
   weakValues=optionsWeak(vOptions);
   return self;
}

-(instancetype)init {
   return [self initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0];
}

+(NSMapTable *)mapTableWithKeyOptions:(NSPointerFunctionsOptions)kOptions valueOptions:(NSPointerFunctionsOptions)vOptions {
   return [[[self alloc] initWithKeyOptions:kOptions valueOptions:vOptions capacity:0] autorelease];
}

+mapTableWithStrongToStrongObjects {
   return [self mapTableWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory];
}

+mapTableWithStrongToWeakObjects {
   return [self mapTableWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsWeakMemory];
}

+mapTableWithWeakToStrongObjects {
   return [self mapTableWithKeyOptions:NSPointerFunctionsWeakMemory valueOptions:NSPointerFunctionsStrongMemory];
}

+mapTableWithWeakToWeakObjects {
   return [self mapTableWithKeyOptions:NSPointerFunctionsWeakMemory valueOptions:NSPointerFunctionsWeakMemory];
}

+(NSMapTable *)strongToStrongObjectsMapTable {
   return [self mapTableWithStrongToStrongObjects];
}

+(NSMapTable *)strongToWeakObjectsMapTable {
   return [self mapTableWithStrongToWeakObjects];
}

+(NSMapTable *)weakToStrongObjectsMapTable {
   return [self mapTableWithWeakToStrongObjects];
}

+(NSMapTable *)weakToWeakObjectsMapTable {
   return [self mapTableWithWeakToWeakObjects];
}

-(NSMapTableKeyCallBacks *)_keyCallBacks {
   return keyCallBacks;
}

-(void)dealloc {
   NSFreeMapTable(self);
   return;
   [super dealloc];
}

-(id)copyWithZone:(NSZone *)zone {
   return NSCopyMapTableWithZone(self,zone);
}

-(NSPointerFunctions *)keyPointerFunctions {
   return [NSPointerFunctions pointerFunctionsWithOptions:keyOptions];
}

-(NSPointerFunctions *)valuePointerFunctions {
   return [NSPointerFunctions pointerFunctionsWithOptions:valueOptions];
}

-(NSUInteger)count {
   return NSCountMapTable(self);
}

-objectForKey:key {
   if(key==nil)
    return nil;
   return NSMapGet(self,key);
}

-(void)removeObjectForKey:key {
   if(key!=nil)
    NSMapRemove(self,key);
}

-(void)setObject:object forKey:key {
   if(key==nil)
    [NSException raise:NSInvalidArgumentException format:@"-[NSMapTable setObject:forKey:]: nil key"];
   if(object==nil){
    NSMapRemove(self,key);
    return;
   }
   if((keyOptions&NSPointerFunctionsCopyIn) && optionsHoldObjects(keyOptions) && !weakKeys)
    key=[[key copy] autorelease];
   if((valueOptions&NSPointerFunctionsCopyIn) && optionsHoldObjects(valueOptions) && !weakValues)
    object=[[object copy] autorelease];
   if(keyCallBacks->retain==_NSMapCopyInRetain || valueCallBacks->retain==_NSMapCopyInRetain){
    // Copy-in tables own their copies.
    if(keyCallBacks->retain==_NSMapCopyInRetain)
     [key retain];
    if(valueCallBacks->retain==_NSMapCopyInRetain)
     [object retain];
   }
   NSMapInsert(self,key,object);
}

-(void)removeAllObjects {
   NSResetMapTable(self);
}

-(NSEnumerator *)keyEnumerator {
   return [NSAllMapTableKeys(self) objectEnumerator];
}

-(NSEnumerator *)objectEnumerator {
   return [NSAllMapTableValues(self) objectEnumerator];
}

-(NSDictionary *)dictionaryRepresentation {
   return [NSDictionary dictionaryWithObjects:NSAllMapTableValues(self) forKeys:NSAllMapTableKeys(self)];
}

-(NSString *)description {
   return NSStringFromMapTable(self);
}

// Enumerates a snapshot of the keys; a mutation during the loop raises.
-(NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id *)stackbuf count:(NSUInteger)length {
   NSArray       *keys;
   NSUInteger     n;
   NSMutableData *storage;

   if(state->state!=0)
    return 0;
   keys=NSAllMapTableKeys(self);     // autoreleased: keeps the keys alive
   n=[keys count];
   storage=[NSMutableData dataWithLength:(n+1)*sizeof(id)];
   [keys getObjects:(id *)[storage mutableBytes] range:NSMakeRange(0,n)];
   state->itemsPtr=(id *)[storage mutableBytes];
   state->mutationsPtr=&mutations;
   state->state=1;
   return n;
}

@end
