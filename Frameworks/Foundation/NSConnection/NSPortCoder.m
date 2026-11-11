/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSPortCoder: Distributed Objects' coder (see NSConnectionInternal.h).
 * Values are written as the machine has them (both ends are processes of
 * the same system); an object as a tag and then:
 *   nil         nothing
 *   reference   the id its owner keeps for it (the sender's or, coming
 *               back, the receiver's own)
 *   port        the index of the port among the message's components
 *   value       strings, numbers, data, dates, values, null and the
 *               collections, member by member
 *   archived    its class's name and what its encodeWithCoder: writes
 * What an object is sent as is its replacementObjectForPortCoder:: values
 * and collections themselves, unless asked byref; any other object a
 * reference, unless asked bycopy and it can be archived.
 */

#import <Foundation/NSPortCoder.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSPort.h>
#import <Foundation/NSObjCRuntime.h>
#import "NSConnectionInternal.h"
#include <objc/runtime.h>
#include <string.h>

enum {
   TagNil=0,
   TagReference=1,      // one of the sender's objects
   TagYours=2,          // one of the receiver's own objects, coming back
   TagPort=3,
   TagArchived=4,
   TagString=5,
   TagNumber=6,
   TagData=7,
   TagDate=8,
   TagArray=9,
   TagDictionary=10,
   TagSet=11,
   TagNull=12,
   TagValue=13,
   TagURL=14,
};

enum { Mutable=0x80 };

@implementation NSPortCoder {
   NSConnection   *_connection;
   NSMutableData  *_data;          // being written
   NSMutableArray *_ports;         // the ports written, in order
   NSData         *_input;         // being read
   NSUInteger      _position;
   NSArray        *_components;
   BOOL            _bycopy,_byref;
}

+portCoderWithReceivePort:(NSPort *)receivePort sendPort:(NSPort *)sendPort components:(NSArray *)components {
   return [[self alloc] initWithReceivePort:receivePort sendPort:sendPort components:components];
}

// The connection between the ports, its components read.
-initWithReceivePort:(NSPort *)receivePort sendPort:(NSPort *)sendPort components:(NSArray *)components {
   NSConnection *connection=[NSConnection connectionWithReceivePort:receivePort sendPort:sendPort];

   return [self initForReceivingWithConnection:connection components:components];
}

-initForSendingWithConnection:(NSConnection *)connection {
   _connection=connection;
   _data=[NSMutableData data];
   _ports=[NSMutableArray array];
   return self;
}

-initForReceivingWithConnection:(NSConnection *)connection components:(NSArray *)components {
   _connection=connection;
   _components=components;
   _input=([components count]>0 && [[components objectAtIndex:0] isKindOfClass:[NSData class]])?[components objectAtIndex:0]:[NSData data];
   return self;
}

-(NSArray *)encodedComponents {
   return [[NSArray arrayWithObject:_data] arrayByAddingObjectsFromArray:_ports];
}

-(NSConnection *)connection {
   return _connection;
}

-(BOOL)isBycopy {
   return _bycopy;
}

-(BOOL)isByref {
   return _byref;
}

-(void)setBycopy:(BOOL)bycopy byref:(BOOL)byref {
   _bycopy=bycopy;
   _byref=byref;
}

-(BOOL)allowsKeyedCoding {
   return NO;
}

-(unsigned)systemVersion {
   return 1000;
}

-(NSInteger)versionForClassName:(NSString *)className {
   return 0;
}

// Received messages are dispatched by their connection, as they come.
-(void)dispatch {
}

/* Raw bytes. */

-(void)append:(const void *)bytes length:(NSUInteger)length {
   [_data appendBytes:bytes length:length];
}

-(void)read:(void *)bytes length:(NSUInteger)length {
   if(_position+length>[_input length])
    [NSException raise:NSPortReceiveException format:@"NSPortCoder: the message ends before what it should hold"];
   memcpy(bytes,(const char *)[_input bytes]+_position,length);
   _position+=length;
}

-(BOOL)isAtEnd {
   return _position>=[_input length];
}

-(void)encodeUInt32:(uint32_t)value {
   [self append:&value length:sizeof(value)];
}

-(uint32_t)decodeUInt32 {
   uint32_t value;

   [self read:&value length:sizeof(value)];
   return value;
}

-(void)encodeCString:(const char *)string {
   uint32_t length=(string!=NULL)?(uint32_t)strlen(string):UINT32_MAX;

   [self encodeUInt32:length];
   if(string!=NULL)
    [self append:string length:length];
}

-(NSString *)decodeString {
   uint32_t length=[self decodeUInt32];

   if(length==UINT32_MAX)
    return nil;
   if(_position+length>[_input length])
    [NSException raise:NSPortReceiveException format:@"NSPortCoder: a string runs past the message's end"];
   NSString *result=[[NSString alloc] initWithBytes:(const char *)[_input bytes]+_position length:length encoding:NSUTF8StringEncoding];
   _position+=length;
   return result;
}

-(void)encodeBytes:(const void *)bytes length:(NSUInteger)length {
   [self encodeUInt32:(uint32_t)length];
   [self append:bytes length:length];
}

-(void *)decodeBytesWithReturnedLength:(NSUInteger *)lengthp {
   uint32_t length=[self decodeUInt32];
   NSMutableData *data=[NSMutableData dataWithLength:length];

   [self read:[data mutableBytes] length:length];
   if(lengthp!=NULL)
    *lengthp=length;
   return [data mutableBytes];   // lives as long as the autorelease pool, as NSCoder's does
}

-(void)encodeDataObject:(NSData *)data {
   [self encodeBytes:[data bytes] length:[data length]];
}

-(NSData *)decodeDataObject {
   uint32_t length=[self decodeUInt32];
   NSMutableData *data=[NSMutableData dataWithLength:length];

   [self read:[data mutableBytes] length:length];
   return data;
}

-(void)encodePortObject:(NSPort *)port {
   [_ports addObject:port];
   [self encodeUInt32:(uint32_t)[_ports count]];   // its component's index
}

-(NSPort *)decodePortObject {
   uint32_t index=[self decodeUInt32];

   if(index>=[_components count] || ![[_components objectAtIndex:index] isKindOfClass:[NSPort class]])
    [NSException raise:NSPortReceiveException format:@"NSPortCoder: no port at component %u",index];
   return [_components objectAtIndex:index];
}

/* Values by type. */

-(void)encodeValueOfObjCType:(const char *)type at:(const void *)address {
   while(strchr("rnNoORVA",*type)!=NULL && *type!='\0')
    type++;
   switch(*type){
    case '@':
     [self encodeObject:*(__unsafe_unretained id *)address];
     return;
    case '#': {
     Class cls=*(__unsafe_unretained Class *)address;

     [self encodeCString:(cls!=Nil)?class_getName(cls):NULL];
     return;
    }
    case ':': {
     SEL sel=*(SEL *)address;

     [self encodeCString:(sel!=NULL)?sel_getName(sel):NULL];
     return;
    }
    case '*':
     [self encodeCString:*(const char **)address];
     return;
    default: {
     NSUInteger size;

     NSGetSizeAndAlignment(type,&size,NULL);
     [self append:address length:size];
     return;
    }
   }
}

-(void)decodeValueOfObjCType:(const char *)type at:(void *)address {
   while(strchr("rnNoORVA",*type)!=NULL && *type!='\0')
    type++;
   switch(*type){
    case '@':
     // As NSCoder's: the caller owns what it is given.
     *(void **)address=(__bridge_retained void *)[self decodeObject];
     return;
    case '#': {
     NSString *name=[self decodeString];

     *(void **)address=(__bridge void *)((name!=nil)?NSClassFromString(name):Nil);
     return;
    }
    case ':': {
     NSString *name=[self decodeString];

     *(SEL *)address=(name!=nil)?NSSelectorFromString(name):NULL;
     return;
    }
    case '*': {
     NSString *string=[self decodeString];
     NSData   *bytes=[string dataUsingEncoding:NSUTF8StringEncoding];
     NSMutableData *copy=[NSMutableData dataWithData:bytes];

     [copy appendBytes:"" length:1];
     *(const char **)address=(string!=nil)?(const char *)[copy bytes]:NULL;
     return;
    }
    default: {
     NSUInteger size;

     NSGetSizeAndAlignment(type,&size,NULL);
     [self read:address length:size];
     return;
    }
   }
}

/* Objects. */

-(void)encodeBycopyObject:(id)object {
   BOOL bycopy=_bycopy,byref=_byref;

   _bycopy=YES; _byref=NO;
   [self encodeObject:object];
   _bycopy=bycopy; _byref=byref;
}

-(void)encodeByrefObject:(id)object {
   BOOL bycopy=_bycopy,byref=_byref;

   _bycopy=NO; _byref=YES;
   [self encodeObject:object];
   _bycopy=bycopy; _byref=byref;
}

static uint8_t mutableFlag(id object,Class mutableClass) {
   return [[object classForCoder] isSubclassOfClass:mutableClass]?Mutable:0;
}

// Asked of the class, not the object: a proxy would forward the question.
static BOOL isDistantObject(id object) {
   Class cls;

   for(cls=object_getClass(object);cls!=Nil;cls=class_getSuperclass(cls))
    if(cls==[NSDistantObject class])
     return YES;
   return NO;
}

// Members of collections go as members do: values by copy, the rest as
// the coder is asked for.
-(void)encodeObject:(id)object {
   uint8_t tag;

   if(object==nil){
    tag=TagNil;
    [self append:&tag length:1];
    return;
   }

   id replacement=isDistantObject(object)?object:[object replacementObjectForPortCoder:self];

   if(replacement==nil){
    tag=TagNil;
    [self append:&tag length:1];
    return;
   }

   if(isDistantObject(replacement)){
    NSDistantObject *proxy=(NSDistantObject *)replacement;

    if([proxy _isLocal]){
     tag=TagReference;
     [self append:&tag length:1];
     [self encodeUInt32:[_connection _exportLocalObject:[proxy _localObject]]];
    }
    else if([proxy connectionForProxy]==_connection){
     tag=TagYours;
     [self append:&tag length:1];
     [self encodeUInt32:[proxy _wireID]];
    }
    else {
     // Another connection's: passed on as an object of ours, which
     // forwards to it.
     tag=TagReference;
     [self append:&tag length:1];
     [self encodeUInt32:[_connection _exportLocalObject:proxy]];
    }
    return;
   }

   if([replacement isKindOfClass:[NSPort class]]){
    tag=TagPort;
    [self append:&tag length:1];
    [self encodePortObject:replacement];
    return;
   }

   if([replacement isKindOfClass:[NSString class]]){
    tag=TagString|mutableFlag(replacement,[NSMutableString class]);
    [self append:&tag length:1];
    [self encodeCString:[replacement UTF8String]];
    return;
   }
   if([replacement isKindOfClass:[NSNumber class]]){
    const char *type=[replacement objCType];
    char        kind=(type!=NULL)?type[0]:'q';

    tag=TagNumber;
    [self append:&tag length:1];
    [self append:&kind length:1];
    if(kind=='f' || kind=='d'){
     double value=[replacement doubleValue];

     [self append:&value length:sizeof(value)];
    }
    else if(strchr("CSILQ",kind)!=NULL){
     unsigned long long value=[replacement unsignedLongLongValue];

     [self append:&value length:sizeof(value)];
    }
    else {
     long long value=[replacement longLongValue];

     [self append:&value length:sizeof(value)];
    }
    return;
   }
   if([replacement isKindOfClass:[NSData class]]){
    tag=TagData|mutableFlag(replacement,[NSMutableData class]);
    [self append:&tag length:1];
    [self encodeDataObject:replacement];
    return;
   }
   if([replacement isKindOfClass:[NSDate class]]){
    double value=[replacement timeIntervalSinceReferenceDate];

    tag=TagDate;
    [self append:&tag length:1];
    [self append:&value length:sizeof(value)];
    return;
   }
   if([replacement isKindOfClass:[NSArray class]]){
    tag=TagArray|mutableFlag(replacement,[NSMutableArray class]);
    [self append:&tag length:1];
    [self encodeUInt32:(uint32_t)[replacement count]];
    for(id member in replacement)
     [self encodeObject:member];
    return;
   }
   if([replacement isKindOfClass:[NSDictionary class]]){
    tag=TagDictionary|mutableFlag(replacement,[NSMutableDictionary class]);
    [self append:&tag length:1];
    [self encodeUInt32:(uint32_t)[replacement count]];
    for(id key in replacement){
     [self encodeObject:key];
     [self encodeObject:[replacement objectForKey:key]];
    }
    return;
   }
   if([replacement isKindOfClass:[NSSet class]]){
    tag=TagSet|mutableFlag(replacement,[NSMutableSet class]);
    [self append:&tag length:1];
    [self encodeUInt32:(uint32_t)[replacement count]];
    for(id member in replacement)
     [self encodeObject:member];
    return;
   }
   if(replacement==[NSNull null]){
    tag=TagNull;
    [self append:&tag length:1];
    return;
   }
   if([replacement isKindOfClass:[NSURL class]]){
    tag=TagURL;
    [self append:&tag length:1];
    [self encodeCString:[[replacement absoluteString] UTF8String]];
    return;
   }
   if([replacement isKindOfClass:[NSValue class]]){
    const char *type=[replacement objCType];
    NSUInteger  size;

    NSGetSizeAndAlignment(type,&size,NULL);
    char buffer[size>0?size:1];

    [replacement getValue:buffer];
    tag=TagValue;
    [self append:&tag length:1];
    [self encodeCString:type];
    [self append:buffer length:size];
    return;
   }

   Class cls=[replacement classForPortCoder];

   if(![replacement conformsToProtocol:@protocol(NSCoding)])
    [NSException raise:NSInvalidArgumentException format:@"NSPortCoder: %@ can be sent neither by reference nor by copy",replacement];
   tag=TagArchived;
   [self append:&tag length:1];
   [self encodeCString:class_getName(cls)];
   [replacement encodeWithCoder:self];
}

-(id)decodeObject {
   uint8_t tag;

   [self read:&tag length:1];
   BOOL mutable=(tag&Mutable)!=0;

   switch(tag&~Mutable){
    case TagNil:
     return nil;
    case TagReference:
     return [_connection _proxyForWireID:[self decodeUInt32]];
    case TagYours: {
     uint32_t wireID=[self decodeUInt32];
     id       object=[_connection _localObjectForWireID:wireID];

     if(object==nil)
      [NSException raise:NSPortReceiveException format:@"NSPortCoder: no object %u in this connection",wireID];
     return object;
    }
    case TagPort:
     return [self decodePortObject];
    case TagString: {
     NSString *string=[self decodeString];

     return mutable?[string mutableCopy]:string;
    }
    case TagNumber: {
     char kind;

     [self read:&kind length:1];
     if(kind=='f' || kind=='d'){
      double value;

      [self read:&value length:sizeof(value)];
      return (kind=='f')?[NSNumber numberWithFloat:(float)value]:[NSNumber numberWithDouble:value];
     }
     if(strchr("CSILQ",kind)!=NULL){
      unsigned long long value;

      [self read:&value length:sizeof(value)];
      switch(kind){
       case 'C': return [NSNumber numberWithUnsignedChar:(unsigned char)value];
       case 'S': return [NSNumber numberWithUnsignedShort:(unsigned short)value];
       case 'I': return [NSNumber numberWithUnsignedInt:(unsigned)value];
       case 'L': return [NSNumber numberWithUnsignedLong:(unsigned long)value];
       default: return [NSNumber numberWithUnsignedLongLong:value];
      }
     }
     long long value;

     [self read:&value length:sizeof(value)];
     switch(kind){
      case 'B': return [NSNumber numberWithBool:value!=0];
      case 'c': return [NSNumber numberWithChar:(char)value];
      case 's': return [NSNumber numberWithShort:(short)value];
      case 'i': return [NSNumber numberWithInt:(int)value];
      case 'l': return [NSNumber numberWithLong:(long)value];
      default: return [NSNumber numberWithLongLong:value];
     }
    }
    case TagData: {
     NSData *data=[self decodeDataObject];

     return mutable?data:[NSData dataWithData:data];
    }
    case TagDate: {
     double value;

     [self read:&value length:sizeof(value)];
     return [NSDate dateWithTimeIntervalSinceReferenceDate:value];
    }
    case TagArray: {
     uint32_t        count=[self decodeUInt32],i;
     NSMutableArray *array=[NSMutableArray arrayWithCapacity:count];

     for(i=0;i<count;i++){
      id member=[self decodeObject];

      [array addObject:(member!=nil)?member:[NSNull null]];
     }
     return mutable?array:[NSArray arrayWithArray:array];
    }
    case TagDictionary: {
     uint32_t             count=[self decodeUInt32],i;
     NSMutableDictionary *dictionary=[NSMutableDictionary dictionaryWithCapacity:count];

     for(i=0;i<count;i++){
      id key=[self decodeObject];
      id value=[self decodeObject];

      if(key!=nil && value!=nil)
       [dictionary setObject:value forKey:key];
     }
     return mutable?dictionary:[NSDictionary dictionaryWithDictionary:dictionary];
    }
    case TagSet: {
     uint32_t      count=[self decodeUInt32],i;
     NSMutableSet *set=[NSMutableSet setWithCapacity:count];

     for(i=0;i<count;i++){
      id member=[self decodeObject];

      if(member!=nil)
       [set addObject:member];
     }
     return mutable?set:[NSSet setWithSet:set];
    }
    case TagNull:
     return [NSNull null];
    case TagURL:
     return [NSURL URLWithString:[self decodeString]];
    case TagValue: {
     NSString  *type=[self decodeString];
     NSUInteger size;

     NSGetSizeAndAlignment([type UTF8String],&size,NULL);
     char buffer[size>0?size:1];

     [self read:buffer length:size];
     return [NSValue valueWithBytes:buffer objCType:[type UTF8String]];
    }
    case TagArchived: {
     NSString *name=[self decodeString];
     Class     cls=NSClassFromString(name);

     if(cls==Nil)
      [NSException raise:NSPortReceiveException format:@"NSPortCoder: no class %@ in this process",name];
     id object=[[cls alloc] initWithCoder:self];

     return [object awakeAfterUsingCoder:self];
    }
    default:
     [NSException raise:NSPortReceiveException format:@"NSPortCoder: unknown object tag %u",tag];
     return nil;
   }
}

@end

/* What objects are sent as. */

@implementation NSObject (NSPortCoder)

-(Class)classForPortCoder {
   return [self classForCoder];
}

// By reference, unless a copy was asked for and the object can be archived.
-replacementObjectForPortCoder:(NSPortCoder *)coder {
   if([coder isBycopy] && [self conformsToProtocol:@protocol(NSCoding)])
    return self;
   return [NSDistantObject proxyWithLocal:self connection:[coder connection]];
}

@end

// Values and collections go by copy unless a reference is asked for.
#define BY_COPY(cls) \
@implementation cls (NSPortCoderByCopy) \
-replacementObjectForPortCoder:(NSPortCoder *)coder { \
   if([coder isByref]) \
    return [NSDistantObject proxyWithLocal:self connection:[coder connection]]; \
   return self; \
} \
@end

BY_COPY(NSString)
BY_COPY(NSValue)
BY_COPY(NSData)
BY_COPY(NSDate)
BY_COPY(NSArray)
BY_COPY(NSDictionary)
BY_COPY(NSSet)
BY_COPY(NSNull)
BY_COPY(NSURL)

@implementation NSPort (NSPortCoder)
-replacementObjectForPortCoder:(NSPortCoder *)coder {
   return self;
}
@end
