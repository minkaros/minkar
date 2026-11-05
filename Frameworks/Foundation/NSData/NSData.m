/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2009 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSData.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSAutoreleasePool-private.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSPlatform.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSRaiseException.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLError.h>
#import <Foundation/NSCFTypeID.h>
#import <Foundation/NSError.h>
#import <Foundation/NSDictionary.h>

#import "NSString_cString.h"
#import "NSData_concrete.h"
#import <Foundation/NSMutableData.h>
#import <Foundation/NSError.h>
#import <Foundation/FoundationErrors.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <compression.h>

@implementation NSData

+allocWithZone:(NSZone *)zone {
   if(self==objc_lookUpClass("NSData"))
    return NSAllocateObject([NSData_concrete class],0,zone);

   return NSAllocateObject(self,0,zone);
}

-initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length freeWhenDone:(BOOL)freeWhenDone {
   NSInvalidAbstractInvocation();
   return nil;
}

-initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length {
   return [self initWithBytesNoCopy:bytes length:length freeWhenDone:YES];
}

-initWithBytes:(const void *)bytes length:(NSUInteger)length {
   return [self initWithBytesNoCopy:NSBytesReplicate(bytes,length,NSZoneFromPointer(self)) length:length];
}

-initWithData:(NSData *)data {
   return [self initWithBytes:[data bytes] length:[data length]];
}

-initWithContentsOfFile:(NSString *)path {
   return [self initWithContentsOfFile:path options:0 error:NULL];
}

-initWithContentsOfMappedFile:(NSString *)path {
   return [self initWithContentsOfFile:path options:NSMappedRead error:NULL];
}

-initWithContentsOfURL:(NSURL *)url {
   return [self initWithContentsOfURL:url options:0 error:NULL];
}

-initWithContentsOfFile:(NSString *)path options:(NSUInteger)options error:(NSError **)errorp {
   NSUInteger length;
   void *bytes=NULL;

   if (options&NSUncachedRead)
    NSLog(@"-[%@ %s] option NSUncachedRead currently ignored.",[self class],sel_getName(_cmd));

   if (options&NSMappedRead)
    bytes=[[NSPlatform currentPlatform] mapContentsOfFile:path length:&length];
   else
    bytes=NSPlatformContentsOfFile(path,&length);

   if(bytes==NULL){

    if(errorp!=NULL){
     NSDictionary *userInfo=[NSDictionary dictionaryWithObject:[NSString stringWithFormat:@"Could not open file %@", path] forKey:NSLocalizedDescriptionKey];

     *errorp=[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCannotOpenFile userInfo:userInfo];
    }

    [self dealloc];
    return nil;
   }

   return [self initWithBytesNoCopy:bytes length:length];
}

-initWithContentsOfURL:(NSURL *)url options:(NSUInteger)options error:(NSError **)errorp {

   if(![url isFileURL]){

	   if ( [[url scheme] isEqual:@"http"] || [[url scheme] isEqual:@"https"]) {
		   NSError *error=nil;
		   NSURLResponse *response=nil;

		   NSData *data = [NSURLConnection sendSynchronousRequest:[NSURLRequest requestWithURL:url] returningResponse:&response error:&error];
		   if (data == nil) {
			   if (errorp != NULL) {
				   *errorp = error;
			   }
			   [self dealloc];
			   return nil;
		   }
		   return [self initWithData:data];
	   } else {
		   [self dealloc];

           if(errorp!=NULL){
            NSDictionary *userInfo=[NSDictionary dictionaryWithObject:[NSString stringWithFormat:@"Could not open url %@", url] forKey:NSLocalizedDescriptionKey];

            *errorp=[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorBadURL userInfo:userInfo];
	   }
    return nil;
   }
   }

   return [self initWithContentsOfFile:[url path] options:options error:errorp];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-mutableCopyWithZone:(NSZone *)zone {
   return [[NSMutableData allocWithZone:zone] initWithData:self];
}

-(Class)classForCoder {
   return [NSData class];
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    NSData            *data=[keyed decodeObjectForKey:@"NS.data"];

    return [self initWithData:data];
   }
   else {
    [self dealloc];
    return [[coder decodeDataObject] retain];
   }
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if([coder isKindOfClass:[NSKeyedArchiver class]]){
    NSKeyedArchiver *keyed=(NSKeyedArchiver *)coder;

    [keyed encodeObject:self forKey:@"NS.data"];
   }
   else {
    [coder encodeDataObject:self];
   }
}

+data {
   return [[[self allocWithZone:NULL] init] autorelease];
}

+dataWithBytesNoCopy:(void *)bytes length:(NSUInteger)length freeWhenDone:(BOOL)freeWhenDone{
   return [[[self allocWithZone:NULL] initWithBytesNoCopy:bytes length:length freeWhenDone:freeWhenDone] autorelease];
}

+dataWithBytesNoCopy:(void *)bytes length:(NSUInteger)length {
   return [[[self allocWithZone:NULL] initWithBytesNoCopy:bytes length:length] autorelease];
}

+dataWithBytes:(const void *)bytes length:(NSUInteger)length {
   return [[[self allocWithZone:NULL] initWithBytes:bytes length:length] autorelease];
}

+dataWithData:(NSData *)data {
   return [[[self allocWithZone:NULL] initWithBytes:[data bytes] length:[data length]] autorelease];
}

+dataWithContentsOfFile:(NSString *)path {
   return [[[self allocWithZone:NULL] initWithContentsOfFile:path] autorelease];
}

+dataWithContentsOfMappedFile:(NSString *)path {
   return [[[self allocWithZone:NULL] initWithContentsOfMappedFile:path] autorelease];
}

+dataWithContentsOfURL:(NSURL *)url {
   return [[[self allocWithZone:NULL] initWithContentsOfURL:url] autorelease];
}

+dataWithContentsOfFile:(NSString *)path options:(NSUInteger)options error:(NSError **)errorp {
   return [[[self alloc] initWithContentsOfFile:path options:options error:errorp] autorelease];
}

+dataWithContentsOfURL:(NSURL *)url options:(NSUInteger)options error:(NSError **)errorp {
   return [[[self alloc] initWithContentsOfURL:url options:options error:errorp] autorelease];
}

-(const void *)bytes {
   NSInvalidAbstractInvocation();
   return NULL;
}

-(NSUInteger)length {
   NSInvalidAbstractInvocation();
   return 0;
}

-(NSUInteger)hash {
	return [self length];
}

-(BOOL)isEqual:other {
   if(self==other)
    return YES;

   if(![other isKindOfClass:objc_lookUpClass("NSData")])
    return NO;

   return [self isEqualToData:other];
}

-(BOOL)isEqualToData:(NSData *)other {
   NSUInteger length;

   if(self==other)
    return YES;

   length=[self length];
   if(length!=[other length])
    return NO;

   return NSBytesEqual([self bytes],[other bytes],length);
}

-(void)getBytes:(void *)result range:(NSRange)range {
   const char *bytes=[self bytes];
   NSUInteger    i;

   if(NSMaxRange(range)>[self length]){
    NSRaiseException(NSRangeException,self,_cmd,@"range %@ beyond length %d",
     NSStringFromRange(range),[self length]);
   }

   for(i=0;i<range.length;i++)
    ((char *)result)[i]=bytes[range.location+i];
}

-(void)getBytes:(void *)result {
   NSRange range={0,[self length]};
   [self getBytes:result range:range];
}

-(void)getBytes:(void *)result length:(NSUInteger)length {
   NSRange range={0,length};
   [self getBytes:result range:range];
}

// Minkar: the first (or last) occurrence of dataToFind in searchRange;
// anchored, only at the start (or end) of the range.
-(NSRange)rangeOfData:(NSData *)dataToFind options:(NSDataSearchOptions)mask range:(NSRange)searchRange {
   NSUInteger length=[self length];

   if(dataToFind==nil)
    [NSException raise:NSInvalidArgumentException format:@"-[%@ %@]: nil data", [self class], NSStringFromSelector(_cmd)];
   if(searchRange.location>length || searchRange.length>length-searchRange.location)
    [NSException raise:NSRangeException format:@"-[%@ %@]: range {%lu, %lu} beyond length %lu", [self class], NSStringFromSelector(_cmd), (unsigned long)searchRange.location, (unsigned long)searchRange.length, (unsigned long)length];

   NSUInteger findLength=[dataToFind length];
   if(findLength==0 || findLength>searchRange.length)
    return NSMakeRange(NSNotFound,0);

   const uint8_t *bytes=(const uint8_t *)[self bytes]+searchRange.location;
   const uint8_t *find=[dataToFind bytes];
   NSUInteger last=searchRange.length-findLength;
   BOOL backwards=(mask&NSDataSearchBackwards)!=0;
   BOOL anchored=(mask&NSDataSearchAnchored)!=0;
   NSUInteger i;

   for(i=0;i<=last;i++){
    NSUInteger at=backwards?last-i:i;

    if(memcmp(bytes+at,find,findLength)==0)
     return NSMakeRange(searchRange.location+at,findLength);
    if(anchored)
     break;
   }
   return NSMakeRange(NSNotFound,0);
}

-(NSData *)subdataWithRange:(NSRange)range {
   void *buffer;

   if(NSMaxRange(range)>[self length]){
    NSRaiseException(NSRangeException,self,_cmd,@"range %@ beyond length %d",
     NSStringFromRange(range),[self length]);
   }

   buffer=NSZoneCalloc(NSZoneFromPointer(self),range.length,sizeof(char));

   [self getBytes:buffer range:range];

   return NSAutorelease(NSData_concreteNewNoCopy(NULL,buffer,range.length));
}

-(BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically {
   NSUInteger options=0;
   if (atomically) options=NSAtomicWrite;
   return [self writeToFile:path options:options error:(NSError **)0];
}

-(BOOL)writeToURL:(NSURL *)url atomically:(BOOL)atomically {
   NSUInteger options=0;
   if (atomically) options=NSAtomicWrite;
   return [self writeToURL:url options:options error:(NSError **)0];
}

-(BOOL)writeToFile:(NSString *)path options:(NSUInteger)options error:(NSError **)errorp {
   return [[NSPlatform currentPlatform] writeContentsOfFile:path bytes:[self bytes] length:[self length] options:options error:errorp];
}

-(BOOL)writeToURL:(NSURL *)url options:(NSUInteger)options error:(NSError **)errorp {
  if(![url isFileURL]){
   NSLog(@"-[%@ %s]: Only file: URLs are supported so far.",[self class],sel_getName(_cmd));
   return NO;
  }
  return [self writeToFile:[url path] options:options error:errorp];
}

-(NSString *)description {
   const char *hex="0123456789ABCDEF";
   const char *bytes=[self bytes];
   NSUInteger    length=[self length];
   NSUInteger    pos=0,i;
   char       *cString;
   NSString   *string=NSAutorelease(NSString_cStringNewWithCapacity(NULL,
     1+length*2+(length/4)+1,&cString));

   cString[pos++]='<';
   for(i=0;i<length;){
    uint8_t byte=bytes[i];

    cString[pos++]=hex[byte>>4];
    cString[pos++]=hex[byte&0x0F];
    i++;

    if((i%4)==0 && i<length)
     cString[pos++]=' ';
   }
   cString[pos++]='>';

   return string;
}

- (unsigned) _cfTypeID {
   return kNSCFTypeData;
}

@end

/* -- Bytes freed by a block of the caller's -- */

@interface _NSDataWithDeallocator : NSData {
    void *_bytes;
    NSUInteger _length;
    void (^_deallocator)(void *, NSUInteger);
}
- (instancetype)_initWithBytes:(void *)bytes length:(NSUInteger)length deallocator:(void (^)(void *, NSUInteger))deallocator;
@end

@implementation _NSDataWithDeallocator

+ allocWithZone:(NSZone *)zone {
    return NSAllocateObject(self, 0, zone);
}

- (instancetype)_initWithBytes:(void *)bytes length:(NSUInteger)length deallocator:(void (^)(void *, NSUInteger))deallocator {
    _bytes = bytes;
    _length = length;
    _deallocator = [deallocator copy];
    return self;
}

- (void)dealloc {
    if (_deallocator != nil) {
        _deallocator(_bytes, _length);
        [_deallocator release];
    }
    [super dealloc];
}

- (const void *)bytes {
    return _bytes;
}

- (NSUInteger)length {
    return _length;
}

@end

@implementation NSData (NSDataModern)

- (instancetype)initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length deallocator:(void (^)(void *, NSUInteger))deallocator {
    /* A mutable data keeps bytes of its own: the caller's go now. */
    if ([self isKindOfClass:[NSMutableData class]]) {
        self = [self initWithBytes:bytes length:length];
        if (deallocator != nil)
            deallocator(bytes, length);
        return self;
    }
    [self release];
    return [[_NSDataWithDeallocator allocWithZone:NULL] _initWithBytes:bytes length:length deallocator:deallocator];
}

/* The bytes are one range: Minkar's data are contiguous. */
- (void)enumerateByteRangesUsingBlock:(void (^)(const void *bytes, NSRange byteRange, BOOL *stop))block {
    BOOL stop = NO;
    if ([self length] > 0)
        block([self bytes], NSMakeRange(0, [self length]), &stop);
}

static compression_algorithm compressionAlgorithm(NSDataCompressionAlgorithm algorithm) {
    switch (algorithm) {
    case NSDataCompressionAlgorithmLZ4: return COMPRESSION_LZ4;
    case NSDataCompressionAlgorithmLZMA: return COMPRESSION_LZMA;
    case NSDataCompressionAlgorithmZlib: return COMPRESSION_ZLIB;
    default: return COMPRESSION_LZFSE;
    }
}

/* The data through a compression stream, whatever size comes out. */
static NSData *runCompression(NSData *input, compression_stream_operation operation,
    NSDataCompressionAlgorithm algorithm, NSError **error) {
    compression_stream stream;
    NSMutableData *output = [NSMutableData data];
    uint8_t buffer[65536];

    if (compression_stream_init(&stream, operation, compressionAlgorithm(algorithm)) != COMPRESSION_STATUS_OK)
        goto fail;
    stream.src_ptr = [input bytes];
    stream.src_size = [input length];
    for (;;) {
        stream.dst_ptr = buffer;
        stream.dst_size = sizeof(buffer);
        compression_status status = compression_stream_process(&stream, COMPRESSION_STREAM_FINALIZE);
        if (status == COMPRESSION_STATUS_ERROR) {
            compression_stream_destroy(&stream);
            goto fail;
        }
        [output appendBytes:buffer length:sizeof(buffer) - stream.dst_size];
        if (status == COMPRESSION_STATUS_END)
            break;
        if (stream.dst_size == sizeof(buffer) && stream.src_size == 0) {
            /* No progress and nothing left: truncated input. */
            compression_stream_destroy(&stream);
            goto fail;
        }
    }
    compression_stream_destroy(&stream);
    return output;
fail:
    if (error != NULL)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadCorruptFileError userInfo:nil];
    return nil;
}

- (instancetype)compressedDataUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error {
    NSData *result = runCompression(self, COMPRESSION_STREAM_ENCODE, algorithm, error);
    return result != nil ? [[self class] dataWithData:result] : nil;
}

- (instancetype)decompressedDataUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error {
    NSData *result = runCompression(self, COMPRESSION_STREAM_DECODE, algorithm, error);
    return result != nil ? [[self class] dataWithData:result] : nil;
}

@end

@implementation NSMutableData (NSDataModern)

- (BOOL)compressUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error {
    NSData *result = runCompression(self, COMPRESSION_STREAM_ENCODE, algorithm, error);
    if (result == nil)
        return NO;
    [self setData:result];
    return YES;
}

- (BOOL)decompressUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error {
    NSData *result = runCompression(self, COMPRESSION_STREAM_DECODE, algorithm, error);
    if (result == nil)
        return NO;
    [self setData:result];
    return YES;
}

@end

// The deallocators of the reference's Foundation for its own buffers, which
// Swift's Data hands to -initWithBytesNoCopy:length:deallocator: (its
// overlay's NSDataShims.h declares them).
typedef void (^NSDataDeallocator)(void *, NSUInteger);
FOUNDATION_EXPORT const NSDataDeallocator NSDataDeallocatorVM;
FOUNDATION_EXPORT const NSDataDeallocator NSDataDeallocatorUnmap;
FOUNDATION_EXPORT const NSDataDeallocator NSDataDeallocatorFree;
FOUNDATION_EXPORT const NSDataDeallocator NSDataDeallocatorNone;

const NSDataDeallocator NSDataDeallocatorVM = ^(void *bytes, NSUInteger length) {
    vm_deallocate(mach_task_self(), (vm_address_t)bytes, length);
};
const NSDataDeallocator NSDataDeallocatorUnmap = ^(void *bytes, NSUInteger length) {
    munmap(bytes, length);
};
const NSDataDeallocator NSDataDeallocatorFree = ^(void *bytes, NSUInteger length) {
    free(bytes);
};
const NSDataDeallocator NSDataDeallocatorNone = ^(void *bytes, NSUInteger length) {
};
