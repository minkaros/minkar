// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// An Objective-C owner of one AEDesc.

#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSError.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSBundle.h>
#include <unistd.h>

/* Seconds from 1904 (long date times) to 2001 (NSDate's reference). */
static const int64_t kLongDateTimeTo2001 = 3061152000LL;

@implementation NSAppleEventDescriptor

+(BOOL)supportsSecureCoding {
   return YES;
}

+(NSAppleEventDescriptor *)nullDescriptor {
   AEDesc desc;
   AEInitializeDesc(&desc);
   return [[[self alloc] initWithAEDescNoCopy:&desc] autorelease];
}

+(NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)type bytes:(const void *)bytes length:(NSUInteger)length {
   return [[[self alloc] initWithDescriptorType:type bytes:bytes length:length] autorelease];
}

+(NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)type data:(NSData *)data {
   return [[[self alloc] initWithDescriptorType:type data:data] autorelease];
}

+(NSAppleEventDescriptor *)descriptorWithBoolean:(Boolean)boolean {
   Boolean value=boolean?1:0;
   return [self descriptorWithDescriptorType:typeBoolean bytes:&value length:1];
}

+(NSAppleEventDescriptor *)descriptorWithEnumCode:(OSType)enumerator {
   return [self descriptorWithDescriptorType:typeEnumerated bytes:&enumerator length:sizeof(enumerator)];
}

+(NSAppleEventDescriptor *)descriptorWithInt32:(SInt32)value {
   return [self descriptorWithDescriptorType:typeSInt32 bytes:&value length:sizeof(value)];
}

+(NSAppleEventDescriptor *)descriptorWithDouble:(double)value {
   return [self descriptorWithDescriptorType:typeIEEE64BitFloatingPoint bytes:&value length:sizeof(value)];
}

+(NSAppleEventDescriptor *)descriptorWithTypeCode:(OSType)typeCode {
   return [self descriptorWithDescriptorType:typeType bytes:&typeCode length:sizeof(typeCode)];
}

+(NSAppleEventDescriptor *)descriptorWithString:(NSString *)string {
   NSData *data=[string dataUsingEncoding:NSUTF16StringEncoding];
   /* Native order without a byte order mark. */
   if([data length]>=2){
    const uint16_t *units=[data bytes];
    if(units[0]==0xFEFF)
     data=[data subdataWithRange:NSMakeRange(2,[data length]-2)];
   }
   return [self descriptorWithDescriptorType:typeUnicodeText data:data];
}

+(NSAppleEventDescriptor *)descriptorWithDate:(NSDate *)date {
   int64_t seconds=(int64_t)[date timeIntervalSinceReferenceDate]+kLongDateTimeTo2001;
   return [self descriptorWithDescriptorType:typeLongDateTime bytes:&seconds length:sizeof(seconds)];
}

+(NSAppleEventDescriptor *)descriptorWithFileURL:(NSURL *)url {
   NSData *data=[[url absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
   return [self descriptorWithDescriptorType:typeFileURL data:data];
}

+(NSAppleEventDescriptor *)appleEventWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
   targetDescriptor:(NSAppleEventDescriptor *)target returnID:(AEReturnID)returnID transactionID:(AETransactionID)transactionID {
   return [[[self alloc] initWithEventClass:eventClass eventID:eventID targetDescriptor:target returnID:returnID transactionID:transactionID] autorelease];
}

+(NSAppleEventDescriptor *)listDescriptor {
   return [[[self alloc] initListDescriptor] autorelease];
}

+(NSAppleEventDescriptor *)recordDescriptor {
   return [[[self alloc] initRecordDescriptor] autorelease];
}

+(NSAppleEventDescriptor *)currentProcessDescriptor {
   /* {0, kCurrentProcess} */
   UInt32 psn[2]={ 0, 2 };
   return [self descriptorWithDescriptorType:typeProcessSerialNumber bytes:psn length:sizeof(psn)];
}

+(NSAppleEventDescriptor *)descriptorWithProcessIdentifier:(pid_t)pid {
   return [self descriptorWithDescriptorType:typeKernelProcessID bytes:&pid length:sizeof(pid)];
}

+(NSAppleEventDescriptor *)descriptorWithBundleIdentifier:(NSString *)identifier {
   return [self descriptorWithDescriptorType:typeApplicationBundleID data:[identifier dataUsingEncoding:NSUTF8StringEncoding]];
}

+(NSAppleEventDescriptor *)descriptorWithApplicationURL:(NSURL *)url {
   return [self descriptorWithDescriptorType:typeApplicationURL data:[[url absoluteString] dataUsingEncoding:NSUTF8StringEncoding]];
}

-init {
   AEDesc desc;
   AEInitializeDesc(&desc);
   return [self initWithAEDescNoCopy:&desc];
}

-initWithAEDescNoCopy:(const AEDesc *)desc {
   self=[super init];
   if(self!=nil)
    _desc=*desc;
   return self;
}

-initWithDescriptorType:(DescType)type bytes:(const void *)bytes length:(NSUInteger)length {
   AEDesc desc;
   if(AECreateDesc(type,bytes,(Size)length,&desc)!=noErr){
    [self release];
    return nil;
   }
   return [self initWithAEDescNoCopy:&desc];
}

-initWithDescriptorType:(DescType)type data:(NSData *)data {
   return [self initWithDescriptorType:type bytes:[data bytes] length:[data length]];
}

-initWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID targetDescriptor:(NSAppleEventDescriptor *)target
   returnID:(AEReturnID)returnID transactionID:(AETransactionID)transactionID {
   AEDesc desc;
   AECreateAppleEvent(eventClass,eventID,[target aeDesc],returnID,transactionID,&desc);
   return [self initWithAEDescNoCopy:&desc];
}

-initListDescriptor {
   AEDesc desc;
   AECreateList(NULL,0,false,&desc);
   return [self initWithAEDescNoCopy:&desc];
}

-initRecordDescriptor {
   AEDesc desc;
   AECreateList(NULL,0,true,&desc);
   return [self initWithAEDescNoCopy:&desc];
}

-(void)dealloc {
   AEDisposeDesc(&_desc);
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   AEDesc copy;
   AEDuplicateDesc(&_desc,&copy);
   return [[NSAppleEventDescriptor allocWithZone:zone] initWithAEDescNoCopy:&copy];
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeInt32:(int32_t)_desc.descriptorType forKey:@"NSAppleEventDescriptorType"];
   [coder encodeObject:[self data] forKey:@"NSAppleEventDescriptorData"];
}

-initWithCoder:(NSCoder *)coder {
   DescType type=(DescType)[coder decodeInt32ForKey:@"NSAppleEventDescriptorType"];
   NSData  *data=[coder decodeObjectOfClass:[NSData class] forKey:@"NSAppleEventDescriptorData"];
   return [self initWithDescriptorType:type data:data];
}

-(const AEDesc *)aeDesc {
   return &_desc;
}

-(DescType)descriptorType {
   return _desc.descriptorType;
}

-(NSData *)data {
   Size           size=AEGetDescDataSize(&_desc);
   NSMutableData *data=[NSMutableData dataWithLength:(NSUInteger)size];
   AEGetDescData(&_desc,[data mutableBytes],size);
   return data;
}

-(BOOL)_getValue:(void *)value size:(Size)size asType:(DescType)type {
   AEDesc coerced;
   BOOL   ok;
   if(AECoerceDesc(&_desc,type,&coerced)!=noErr)
    return NO;
   ok=(AEGetDescDataSize(&coerced)>=size && AEGetDescData(&coerced,value,size)==noErr);
   AEDisposeDesc(&coerced);
   return ok;
}

-(Boolean)booleanValue {
   Boolean value=0;
   return [self _getValue:&value size:1 asType:typeBoolean]?value:0;
}

-(OSType)enumCodeValue {
   OSType value=0;
   return [self _getValue:&value size:4 asType:typeEnumerated]?value:0;
}

-(SInt32)int32Value {
   SInt32 value=0;
   return [self _getValue:&value size:4 asType:typeSInt32]?value:0;
}

-(double)doubleValue {
   double value=0;
   return [self _getValue:&value size:8 asType:typeIEEE64BitFloatingPoint]?value:0;
}

-(OSType)typeCodeValue {
   OSType value=0;
   return [self _getValue:&value size:4 asType:typeType]?value:0;
}

-(NSString *)stringValue {
   AEDesc coerced;
   if(AECoerceDesc(&_desc,typeUTF8Text,&coerced)!=noErr)
    return nil;
   Size           size=AEGetDescDataSize(&coerced);
   NSMutableData *data=[NSMutableData dataWithLength:(NSUInteger)size];
   AEGetDescData(&coerced,[data mutableBytes],size);
   AEDisposeDesc(&coerced);
   return [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
}

-(NSDate *)dateValue {
   int64_t seconds;
   if(_desc.descriptorType!=typeLongDateTime || ![self _getValue:&seconds size:8 asType:typeLongDateTime])
    return nil;
   return [NSDate dateWithTimeIntervalSinceReferenceDate:(NSTimeInterval)(seconds-kLongDateTimeTo2001)];
}

-(NSURL *)fileURLValue {
   if(_desc.descriptorType==typeFileURL){
    NSString *string=[[[NSString alloc] initWithData:[self data] encoding:NSUTF8StringEncoding] autorelease];
    NSURL    *url=(string!=nil)?[NSURL URLWithString:string]:nil;
    return [url isFileURL]?url:nil;
   }
   NSString *path=[self stringValue];
   return ([path hasPrefix:@"/"])?[NSURL fileURLWithPath:path]:nil;
}

-(FourCharCode)_attributeCode:(AEKeyword)keyword type:(DescType)type {
   FourCharCode value=0;
   AEGetAttributePtr(&_desc,keyword,type,NULL,&value,sizeof(value),NULL);
   return value;
}

-(AEEventClass)eventClass {
   return [self _attributeCode:keyEventClassAttr type:typeType];
}

-(AEEventID)eventID {
   return [self _attributeCode:keyEventIDAttr type:typeType];
}

-(AEReturnID)returnID {
   AEReturnID value=0;
   AEGetAttributePtr(&_desc,keyReturnIDAttr,typeSInt16,NULL,&value,sizeof(value),NULL);
   return value;
}

-(AETransactionID)transactionID {
   AETransactionID value=0;
   AEGetAttributePtr(&_desc,keyTransactionIDAttr,typeSInt32,NULL,&value,sizeof(value),NULL);
   return value;
}

-(BOOL)isRecordDescriptor {
   return AECheckIsRecord(&_desc);
}

-(NSInteger)numberOfItems {
   long count=0;
   AECountItems(&_desc,&count);
   return count;
}

-(void)setParamDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
   AEPutParamDesc(&_desc,keyword,[descriptor aeDesc]);
}

-(NSAppleEventDescriptor *)_wrap:(AEDesc *)desc status:(OSErr)err {
   if(err!=noErr)
    return nil;
   return [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:desc] autorelease];
}

-(NSAppleEventDescriptor *)paramDescriptorForKeyword:(AEKeyword)keyword {
   AEDesc desc;
   return [self _wrap:&desc status:AEGetParamDesc(&_desc,keyword,typeWildCard,&desc)];
}

-(void)removeParamDescriptorWithKeyword:(AEKeyword)keyword {
   AEDeleteParam(&_desc,keyword);
}

-(void)setAttributeDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
   AEPutAttributeDesc(&_desc,keyword,[descriptor aeDesc]);
}

-(NSAppleEventDescriptor *)attributeDescriptorForKeyword:(AEKeyword)keyword {
   AEDesc desc;
   return [self _wrap:&desc status:AEGetAttributeDesc(&_desc,keyword,typeWildCard,&desc)];
}

-(NSAppleEventDescriptor *)sendEventWithOptions:(NSAppleEventSendOptions)options timeout:(NSTimeInterval)timeout error:(NSError **)error {
   AppleEvent reply;
   long       ticks=(timeout<0)?kAEDefaultTimeout:(long)(timeout*60);
   OSStatus   status=AESendMessage(&_desc,&reply,(AESendMode)options,ticks);
   SInt32     replyError=0;

   if(status==noErr && reply.descriptorType==typeAppleEvent &&
      AEGetParamPtr(&reply,keyErrorNumber,typeSInt32,NULL,&replyError,sizeof(replyError),NULL)==noErr && replyError!=0)
    status=replyError;
   if(status!=noErr){
    AEDisposeDesc(&reply);
    if(error!=NULL)
     *error=[NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
    return nil;
   }
   if(reply.descriptorType==typeNull)
    return [NSAppleEventDescriptor nullDescriptor];
   return [[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&reply] autorelease];
}

-(void)insertDescriptor:(NSAppleEventDescriptor *)descriptor atIndex:(NSInteger)index {
   long count=0;
   AECountItems(&_desc,&count);
   if(index<=0 || index>count)
    AEPutDesc(&_desc,0,[descriptor aeDesc]);
   else {
    /* Insert before the item at index: rebuild the list. */
    AEDesc list;
    AECreateList(NULL,0,false,&list);
    for(long i=1;i<=count;i++){
     AEDesc item;
     if(i==index)
      AEPutDesc(&list,0,[descriptor aeDesc]);
     if(AEGetNthDesc(&_desc,i,typeWildCard,NULL,&item)==noErr){
      AEPutDesc(&list,0,&item);
      AEDisposeDesc(&item);
     }
    }
    AEDisposeDesc(&_desc);
    _desc=list;
   }
}

-(NSAppleEventDescriptor *)descriptorAtIndex:(NSInteger)index {
   AEDesc desc;
   return [self _wrap:&desc status:AEGetNthDesc(&_desc,index,typeWildCard,NULL,&desc)];
}

-(void)removeDescriptorAtIndex:(NSInteger)index {
   AEDeleteItem(&_desc,index);
}

-(void)setDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
   AEPutKeyDesc(&_desc,keyword,[descriptor aeDesc]);
}

-(NSAppleEventDescriptor *)descriptorForKeyword:(AEKeyword)keyword {
   AEDesc desc;
   return [self _wrap:&desc status:AEGetKeyDesc(&_desc,keyword,typeWildCard,&desc)];
}

-(void)removeDescriptorWithKeyword:(AEKeyword)keyword {
   AEDeleteKeyDesc(&_desc,keyword);
}

-(AEKeyword)keywordForDescriptorAtIndex:(NSInteger)index {
   AEKeyword keyword=0;
   AEDesc    desc;
   if(AEGetNthDesc(&_desc,index,typeWildCard,&keyword,&desc)==noErr)
    AEDisposeDesc(&desc);
   return keyword;
}

-(NSAppleEventDescriptor *)coerceToDescriptorType:(DescType)type {
   AEDesc desc;
   return [self _wrap:&desc status:AECoerceDesc(&_desc,type,&desc)];
}

-(BOOL)isEqual:(id)other {
   if(other==self)
    return YES;
   if(![other isKindOfClass:[NSAppleEventDescriptor class]])
    return NO;
   return [other descriptorType]==_desc.descriptorType && [[other data] isEqual:[self data]];
}

-(NSUInteger)hash {
   return _desc.descriptorType;
}

-(NSString *)description {
   FourCharCode t=_desc.descriptorType;
   return [NSString stringWithFormat:@"<NSAppleEventDescriptor: '%c%c%c%c'>",(char)(t>>24),(char)(t>>16),(char)(t>>8),(char)t];
}

@end
