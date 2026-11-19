/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A PDF document's objects (CGPDFObject, CGPDFArray, CGPDFDictionary,
 * CGPDFString, CGPDFStream) over Onyx2D's, as the reference reads them
 * (measured 19 Nov 2026): references looked through, an integer read as
 * a number too, text strings in UTF-16 or PDFDocEncoding (its undefined bytes spaces), dates in UTC
 * when they name no zone.
 */

#import <CoreGraphics/CGPDFObject.h>
#import <CoreGraphics/CGPDFArray.h>
#import <CoreGraphics/CGPDFDictionary.h>
#import <CoreGraphics/CGPDFString.h>
#import <CoreGraphics/CGPDFStream.h>
#import <Onyx2D/O2PDFObject.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFString.h>
#import <Onyx2D/O2PDFStream.h>
#import <Foundation/NSData.h>
#import <ctype.h>
#import <string.h>
#import <time.h>

#define OBJ(x) ((O2PDFObject *)(x))

/* ---- objects ---- */

CGPDFObjectType CGPDFObjectGetType(CGPDFObjectRef object) {
   O2PDFObjectType type;

   if(object==NULL)
    return 0;
   type=[[OBJ(object) realObject] objectType];
   return (type>=kO2PDFObjectTypeNull && type<=kO2PDFObjectTypeStream)?(CGPDFObjectType)type:0;
}

/* object's value as type, into value: the C types the public API names
   (a CGPDFInteger is wider than Onyx2D's). */
static bool getValue(O2PDFObject *object,CGPDFObjectType type,void *value) {
   object=[object realObject];
   if(object==nil)
    return false;
   switch(type){
    case kCGPDFObjectTypeNull:
     return [object objectType]==kO2PDFObjectTypeNull;
    case kCGPDFObjectTypeBoolean:{
     O2PDFBoolean flag;

     if(![object checkForType:kO2PDFObjectTypeBoolean value:&flag])
      return false;
     if(value!=NULL) *(CGPDFBoolean *)value=flag?1:0;
     return true;
    }
    case kCGPDFObjectTypeInteger:{
     O2PDFInteger integer;

     if([object objectType]!=kO2PDFObjectTypeInteger || ![object checkForType:kO2PDFObjectTypeInteger value:&integer])
      return false;
     if(value!=NULL) *(CGPDFInteger *)value=integer;
     return true;
    }
    case kCGPDFObjectTypeReal:{
     O2PDFReal real;

     if(![object checkForType:kO2PDFObjectTypeReal value:&real])
      return false;
     if(value!=NULL) *(CGPDFReal *)value=real;
     return true;
    }
    case kCGPDFObjectTypeName:{
     const char *name;

     if(![object checkForType:kO2PDFObjectTypeName value:&name])
      return false;
     if(value!=NULL) *(const char **)value=name;
     return true;
    }
    case kCGPDFObjectTypeString:
    case kCGPDFObjectTypeArray:
    case kCGPDFObjectTypeDictionary:
    case kCGPDFObjectTypeStream:
     if([object objectType]!=(O2PDFObjectType)type)
      return false;
     if(value!=NULL) *(void **)value=object;
     return true;
   }
   return false;
}

bool CGPDFObjectGetValue(CGPDFObjectRef object,CGPDFObjectType type,void *value) {
   return (object!=NULL)?getValue(OBJ(object),type,value):false;
}

/* ---- arrays ---- */

size_t CGPDFArrayGetCount(CGPDFArrayRef array) {
   return (array!=NULL)?[(O2PDFArray *)array count]:0;
}

static O2PDFObject *arrayObject(CGPDFArrayRef array,size_t index) {
   if(array==NULL || index>=[(O2PDFArray *)array count])
    return nil;
   return [[(O2PDFArray *)array objectAtIndex:(unsigned)index] realObject];
}

bool CGPDFArrayGetObject(CGPDFArrayRef array,size_t index,CGPDFObjectRef *value) {
   O2PDFObject *object=arrayObject(array,index);

   if(object==nil)
    return false;
   if(value!=NULL) *value=(CGPDFObjectRef)object;
   return true;
}

bool CGPDFArrayGetNull(CGPDFArrayRef array,size_t index) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeNull,NULL);
}

bool CGPDFArrayGetBoolean(CGPDFArrayRef array,size_t index,CGPDFBoolean *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeBoolean,value);
}

bool CGPDFArrayGetInteger(CGPDFArrayRef array,size_t index,CGPDFInteger *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeInteger,value);
}

bool CGPDFArrayGetNumber(CGPDFArrayRef array,size_t index,CGPDFReal *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeReal,value);
}

bool CGPDFArrayGetName(CGPDFArrayRef array,size_t index,const char **value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeName,value);
}

bool CGPDFArrayGetString(CGPDFArrayRef array,size_t index,CGPDFStringRef *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeString,value);
}

bool CGPDFArrayGetArray(CGPDFArrayRef array,size_t index,CGPDFArrayRef *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeArray,value);
}

bool CGPDFArrayGetDictionary(CGPDFArrayRef array,size_t index,CGPDFDictionaryRef *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeDictionary,value);
}

bool CGPDFArrayGetStream(CGPDFArrayRef array,size_t index,CGPDFStreamRef *value) {
   return getValue(arrayObject(array,index),kCGPDFObjectTypeStream,value);
}

/* ---- dictionaries ---- */

size_t CGPDFDictionaryGetCount(CGPDFDictionaryRef dict) {
   return (dict!=NULL)?[(O2PDFDictionary *)dict count]:0;
}

static O2PDFObject *dictionaryObject(CGPDFDictionaryRef dict,const char *key) {
   O2PDFObject *object=nil;

   if(dict==NULL || key==NULL || ![(O2PDFDictionary *)dict getObjectForKey:key value:&object])
    return nil;
   return [object realObject];
}

bool CGPDFDictionaryGetObject(CGPDFDictionaryRef dict,const char *key,CGPDFObjectRef *value) {
   O2PDFObject *object=dictionaryObject(dict,key);

   if(object==nil)
    return false;
   if(value!=NULL) *value=(CGPDFObjectRef)object;
   return true;
}

bool CGPDFDictionaryGetBoolean(CGPDFDictionaryRef dict,const char *key,CGPDFBoolean *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeBoolean,value);
}

bool CGPDFDictionaryGetInteger(CGPDFDictionaryRef dict,const char *key,CGPDFInteger *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeInteger,value);
}

bool CGPDFDictionaryGetNumber(CGPDFDictionaryRef dict,const char *key,CGPDFReal *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeReal,value);
}

bool CGPDFDictionaryGetName(CGPDFDictionaryRef dict,const char *key,const char **value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeName,value);
}

bool CGPDFDictionaryGetString(CGPDFDictionaryRef dict,const char *key,CGPDFStringRef *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeString,value);
}

bool CGPDFDictionaryGetArray(CGPDFDictionaryRef dict,const char *key,CGPDFArrayRef *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeArray,value);
}

bool CGPDFDictionaryGetDictionary(CGPDFDictionaryRef dict,const char *key,CGPDFDictionaryRef *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeDictionary,value);
}

bool CGPDFDictionaryGetStream(CGPDFDictionaryRef dict,const char *key,CGPDFStreamRef *value) {
   return getValue(dictionaryObject(dict,key),kCGPDFObjectTypeStream,value);
}

typedef struct {
   CGPDFDictionaryApplierFunction function;
   void                          *info;
} ApplyContext;

static void applyEntry(const char *key,O2PDFObject *value,void *info) {
   ApplyContext *context=info;

   if(value!=nil)
    context->function(key,(CGPDFObjectRef)value,context->info);
}

void CGPDFDictionaryApplyFunction(CGPDFDictionaryRef dict,CGPDFDictionaryApplierFunction function,void *info) {
   ApplyContext context={function,info};

   if(dict!=NULL && function!=NULL)
    [(O2PDFDictionary *)dict applyFunction:applyEntry info:&context];
}

/* ---- strings ---- */

size_t CGPDFStringGetLength(CGPDFStringRef string) {
   return (string!=NULL)?[(O2PDFString *)string length]:0;
}

const unsigned char *CGPDFStringGetBytePtr(CGPDFStringRef string) {
   return (string!=NULL)?[(O2PDFString *)string bytes]:NULL;
}

/* PDFDocEncoding (PDF 1.7, annex D): ASCII and ISO Latin-1 but for these;
   a byte it leaves undefined is a space, as on the reference (tab, line
   feed and carriage return are kept). */
static UniChar pdfDocCharacter(unsigned char byte) {
   static const UniChar low[8]={0x02D8,0x02C7,0x02C6,0x02D9,0x02DD,0x02DB,0x02DA,0x02DC}; // 0x18-0x1F
   static const UniChar high[32]={
    0x2022,0x2020,0x2021,0x2026,0x2014,0x2013,0x0192,0x2044,0x2039,0x203A,0x2212,0x2030,0x201E,0x201C,0x201D,0x2018,
    0x2019,0x201A,0x2122,0xFB01,0xFB02,0x0141,0x0152,0x0160,0x0178,0x017D,0x0131,0x0142,0x0153,0x0161,0x017E,0x0020}; // 0x80-0x9F

   if(byte<0x18)
    return (byte=='\t' || byte=='\n' || byte=='\r')?byte:' ';
   if(byte<=0x1F)
    return low[byte-0x18];
   if(byte>=0x80 && byte<=0x9F)
    return high[byte-0x80];
   if(byte==0xA0)
    return 0x20AC;
   if(byte==0x7F || byte==0xAD)
    return ' ';
   return byte;
}

CFStringRef CGPDFStringCopyTextString(CGPDFStringRef string) {
   const unsigned char *bytes=CGPDFStringGetBytePtr(string);
   size_t               length=CGPDFStringGetLength(string);

   if(string==NULL)
    return NULL;
   if(length>=2 && bytes[0]==0xFE && bytes[1]==0xFF)
    return CFStringCreateWithBytes(NULL,bytes+2,(length-2)&~(size_t)1,kCFStringEncodingUTF16BE,false);
   {
    UniChar   *characters=malloc(sizeof(UniChar)*(length?length:1));
    CFStringRef result;

    for(size_t i=0;i<length;i++)
     characters[i]=pdfDocCharacter(bytes[i]);
    result=CFStringCreateWithCharacters(NULL,characters,length);
    free(characters);
    return result;
   }
}

/* count digits at *p, as a number; false if fewer are there. */
static bool digits(const char **p,const char *end,int count,int *value) {
   int result=0;

   for(int i=0;i<count;i++){
    if(*p+i>=end || !isdigit((unsigned char)(*p)[i]))
     return false;
    result=result*10+((*p)[i]-'0');
   }
   *p+=count;
   *value=result;
   return true;
}

CFDateRef CGPDFStringCopyDate(CGPDFStringRef string) {
   const char *p=(const char *)CGPDFStringGetBytePtr(string),*end;
   int         year,month=1,day=1,hour=0,minute=0,second=0,offset=0;
   struct tm   parts;

   if(p==NULL)
    return NULL;
   end=p+CGPDFStringGetLength(string);
   if(end-p>=2 && p[0]=='D' && p[1]==':')
    p+=2;
   if(!digits(&p,end,4,&year))
    return NULL;
   /* Each later part is optional; once one is missing, the rest are. */
   if(digits(&p,end,2,&month) && digits(&p,end,2,&day) && digits(&p,end,2,&hour) &&
      digits(&p,end,2,&minute) && digits(&p,end,2,&second)){
    if(p<end && (*p=='+' || *p=='-')){
     int sign=(*p=='-')?-1:1,hours=0,minutes=0;

     p++;
     if(digits(&p,end,2,&hours)){
      if(p<end && *p=='\'')
       p++;
      digits(&p,end,2,&minutes);
     }
     offset=sign*(hours*3600+minutes*60);
    }
   }
   if(month<1 || month>12 || day<1 || day>31 || hour>23 || minute>59 || second>60)
    return NULL;
   memset(&parts,0,sizeof(parts));
   parts.tm_year=year-1900;
   parts.tm_mon=month-1;
   parts.tm_mday=day;
   parts.tm_hour=hour;
   parts.tm_min=minute;
   parts.tm_sec=second;
   return CFDateCreate(NULL,(CFAbsoluteTime)(timegm(&parts)-offset)-kCFAbsoluteTimeIntervalSince1970);
}

/* ---- streams ---- */

CGPDFDictionaryRef CGPDFStreamGetDictionary(CGPDFStreamRef stream) {
   return (stream!=NULL)?(CGPDFDictionaryRef)[(O2PDFStream *)stream dictionary]:NULL;
}

/* The filter a stream's data ends with (its last, if an array). */
static const char *lastFilter(O2PDFStream *stream) {
   O2PDFDictionary *dictionary=[stream dictionary];
   O2PDFArray      *filters;
   const char      *name=NULL;

   if([dictionary getNameForKey:"Filter" value:&name])
    return name;
   if([dictionary getArrayForKey:"Filter" value:&filters] && [filters count]>0 && [filters getNameAtIndex:[filters count]-1 value:&name])
    return name;
   return NULL;
}

CFDataRef CGPDFStreamCopyData(CGPDFStreamRef stream,CGPDFDataFormat *format) {
   const char *filter;
   NSData     *data;

   if(stream==NULL)
    return NULL;
   filter=lastFilter((O2PDFStream *)stream);
   if(filter!=NULL && (strcmp(filter,"DCTDecode")==0 || strcmp(filter,"JPXDecode")==0)){
    /* An image left in its encoding: the stream's bytes, only if it has
       no filter before that one. */
    O2PDFArray *filters;

    if(![[(O2PDFStream *)stream dictionary] getArrayForKey:"Filter" value:&filters] || [filters count]==1){
     if(format!=NULL)
      *format=(strcmp(filter,"DCTDecode")==0)?CGPDFDataFormatJPEGEncoded:CGPDFDataFormatJPEG2000;
     data=[(O2PDFStream *)stream mutableData];
     return (data!=nil)?CFDataCreate(NULL,[data bytes],[data length]):NULL;
    }
   }
   if(format!=NULL)
    *format=CGPDFDataFormatRaw;
   data=[(O2PDFStream *)stream data];
   return (data!=nil)?CFDataCreate(NULL,[data bytes],[data length]):NULL;
}
