/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import "NSPropertyListReader_xml1.h"
#import "NSOldXMLReader.h"
#import "NSOldXMLDocument.h"
#import "NSOldXMLElement.h"
#import <Foundation/NSException.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSCharacterSet.h>

#import <Foundation/NSCalendarDate.h>
#import <Foundation/NSScanner.h>
#import <Foundation/NSCharacterSet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xlocale.h>

// "YYYY-MM-DDTHH:MM:SSZ", in UTC. Minkar: this read the time as local
// time and a day early.
NSDate* NSDateFromPlistString(NSString* string)
{
	struct tm tm;
	const char *text=[string UTF8String];

	memset(&tm,0,sizeof(tm));
	if(text==NULL || sscanf(text,"%d-%d-%dT%d:%d:%d",&tm.tm_year,&tm.tm_mon,&tm.tm_mday,
	                         &tm.tm_hour,&tm.tm_min,&tm.tm_sec)<3)
		return nil;
	tm.tm_year-=1900;
	tm.tm_mon-=1;
	return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)timegm(&tm)];
}


@implementation NSPropertyListReader_xml1

+(NSDictionary *)dictionaryFromElement:(NSOldXMLElement *)element {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];
   NSArray             *contents=[element contents];
   NSInteger            i,count=[contents count];
   id                   currentKey=nil;

   for(i=0;i<count;i++){
    id check=[contents objectAtIndex:i];

    if([check isKindOfClass:[NSOldXMLElement class]]){
     if([[check name] isEqualToString:@"key"])
      currentKey=[check stringValue];
     else
      [result setObject:[self propertyListFromElement:check] forKey:currentKey];

    }
   }
   return result;
}

+(NSArray *)arrayFromElement:(NSOldXMLElement *)element {
   NSMutableArray *result=[NSMutableArray array];
   NSArray        *contents=[element contents];
   NSInteger       i,count=[contents count];

   for(i=0;i<count;i++){
    id check=[contents objectAtIndex:i];

    if([check isKindOfClass:[NSOldXMLElement class]])
     [result addObject:[self propertyListFromElement:check]];
   }

   return result;
}

+(NSData *)dataFromBase64String:(NSString *)string {
   NSUInteger      i,length=[string length],resultLength=0;
    unichar       *buffer = NSZoneMalloc(NULL, sizeof(unichar)*length);
    if (buffer == NULL) {
        NSLog(@"%@: failed to allocate buffer of size %d", NSStringFromSelector(_cmd), length);
        return nil;
    }
   uint8_t       *result = NSZoneMalloc(NULL, sizeof(uint8_t)*length);
    if (result == NULL) {
        NSLog(@"%@: failed to allocate buffer of size %d", NSStringFromSelector(_cmd), length);
        NSZoneFree(NULL, buffer);
        return nil;        
    }
   uint8_t partial=0;
   enum { load6High, load2Low, load4Low, load6Low } state=load6High;

   [string getCharacters:buffer];

   for(i=0;i<length;i++){
    unichar       code=buffer[i];
    unsigned char bits;

    if(code>='A' && code<='Z')
     bits=code-'A';
    else if(code>='a' && code<='z')
     bits=code-'a'+26;
    else if(code>='0' && code<='9')
     bits=code-'0'+52;
    else if(code=='+')
     bits=62;
    else if(code=='/')
     bits=63;
    else if(code=='='){
     break;
    }
    else
     continue;

    switch(state){

     case load6High:
      partial=bits<<2;
      state=load2Low;
      break;

     case load2Low:
      partial|=bits>>4;
      result[resultLength++]=partial;
      partial=bits<<4;
      state=load4Low;
      break;

     case load4Low:
      partial|=bits>>2;
      result[resultLength++]=partial;
      partial=bits<<6;
      state=load6Low;
      break;

     case load6Low:
      partial|=bits;
      result[resultLength++]=partial;
      state=load6High;
      break;
    }
   }
    NSZoneFree(NULL, buffer);
   return [NSData dataWithBytesNoCopy:result length:resultLength freeWhenDone:YES];
}

+(NSData *)dataFromElement:(NSOldXMLElement *)element {
   NSMutableData *result=[NSMutableData data];
   NSArray       *strings=[element contents];
   NSInteger            i,count=[strings count];

   for(i=0;i<count;i++)
    [result appendData:[self dataFromBase64String:[strings objectAtIndex:i]]];

   return result;
}

// 64-bit integers, and unsigned ones above INT64_MAX. Minkar: integers
// were read as int and reals as float.
static NSNumber *integerFromString(NSString *string) {
   const char *text=[[string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] UTF8String];
   long long  value;

   errno=0;
   value=strtoll(text,NULL,10);
   if(errno==ERANGE && text[0]!='-'){
    errno=0;
    return [NSNumber numberWithUnsignedLongLong:strtoull(text,NULL,10)];
   }
   return [NSNumber numberWithLongLong:value];
}

static NSNumber *realFromString(NSString *string) {
   static locale_t cLocale;
   const char *text=[[string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] UTF8String];

   if(cLocale==NULL)
    cLocale=newlocale(LC_ALL_MASK,"C",NULL);
   return [NSNumber numberWithDouble:strtod_l(text,NULL,cLocale)];
}

+(NSDate *) dateFromElement:(NSOldXMLElement*) element {
   NSString* string=[element stringValue];
	return NSDateFromPlistString(string);
}

+(NSObject *)propertyListFromElement:(NSOldXMLElement *)element {
   NSString *name=[element name];
   id        result=nil;

   if([name isEqualToString:@"dict"])
    result=[self dictionaryFromElement:element];
   else if([name isEqualToString:@"array"])
    result=[self arrayFromElement:element];
   else if([name isEqualToString:@"string"])
    result=[element stringValue];
   else if([name isEqualToString:@"integer"])
    result=integerFromString([element stringValue]);
   else if([name isEqualToString:@"real"])
    result=realFromString([element stringValue]);
   else if([name isEqualToString:@"true"])
    result=[NSNumber numberWithBool:YES];
   else if([name isEqualToString:@"false"])
    result=[NSNumber numberWithBool:NO];
   else if([name isEqualToString:@"data"])
    result=[self dataFromElement:element];
   else if([name isEqualToString:@"date"])
    result=[self dateFromElement:element];

   return result;
}

+(NSObject *)propertyListFromContentsOfElement:(NSOldXMLElement *)element {
   id       result=nil;
   NSArray *contents=[element contents];
   NSInteger      i,count=[contents count];

   for(i=0;i<count;i++){
    id check=[contents objectAtIndex:i];

    if([check isKindOfClass:[NSOldXMLElement class]])
     result=[self propertyListFromElement:check];
   }

   return result;
}

+(NSObject *)propertyListFromDocument:(NSOldXMLDocument *)document {
   NSOldXMLElement *root=[document rootElement];

   return [self propertyListFromContentsOfElement:root];
}

+(NSObject *)propertyListFromData:(NSData *)data {
   id result=nil;

   NS_DURING
    NSOldXMLDocument *document=[NSOldXMLReader documentWithData:data];

    if(document!=nil){
     result=[self propertyListFromDocument:document];
    }
   NS_HANDLER

   NS_ENDHANDLER

   return result;
}

@end
