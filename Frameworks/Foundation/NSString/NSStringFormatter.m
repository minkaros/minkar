/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xlocale.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSStringFormatter.h>
#import <Foundation/NSAutoreleasePool-private.h>
#import <Foundation/NSRaiseException.h>

#import "NSString_unicodePtr.h"
#import "NSString_cString.h" //appendCString
#import "unibuffer.h"

typedef struct {
   NSZone *zone;
   unsigned max,length;
   unichar *characters;
} NSStringBuffer;

static inline void prepForUse(NSStringBuffer *buffer,NSZone *zone){
   buffer->zone=zone;
   buffer->max=1024;
   buffer->length=0;
   buffer->characters=NSZoneMalloc(buffer->zone,sizeof(unichar)*buffer->max);
}

static inline void makeRoomForNcharacters(NSStringBuffer *buffer,NSUInteger n){
   if(buffer->length+n>buffer->max){
    while(buffer->length+n>buffer->max)
     buffer->max*=2;

    buffer->characters=NSZoneRealloc(buffer->zone,buffer->characters,sizeof(unichar)*buffer->max);
   }
}

static inline void appendCharacter(NSStringBuffer *buffer,unichar unicode){
   makeRoomForNcharacters(buffer,1);

   buffer->characters[buffer->length++]=unicode;
}

static inline void appendCharacters(NSStringBuffer *buffer,unichar *characters,NSUInteger length,unichar fillChar,BOOL leftAdj,int fieldWidth){
   NSUInteger i;

   makeRoomForNcharacters(buffer,(fieldWidth>length)?fieldWidth:length);

   if(!leftAdj && fieldWidth>length){
    for(i=0;i<fieldWidth-length;i++)
     buffer->characters[buffer->length++]=fillChar;
   }

   for(i=0;i<length;i++)
    buffer->characters[buffer->length++]=characters[i];

   if(leftAdj && fieldWidth>length){
    for(i=0;i<fieldWidth-length;i++)
     buffer->characters[buffer->length++]=' ';
   }
}

static inline void appendUnichar(NSStringBuffer *buffer,unichar code,unichar fillChar,BOOL leftAdj,int fieldWidth){
   appendCharacters(buffer,&code,1,fillChar,leftAdj,fieldWidth);
}

static inline void appendUnicodeString(NSStringBuffer *buffer,unichar *characters,unichar fillChar,BOOL leftAdj,int fieldWidth){
   NSInteger length;
   
   for(length=0;characters[length]!=0x0000;length++)
    ;
   appendCharacters(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void reverseCharacters(unichar *characters,unsigned length){
   unsigned i;
   unichar  tmp;

    // reverse chars
   for(i=0;i<length/2;i++){
    tmp=characters[i];
    characters[i]=characters[(length-1)-i];
    characters[(length-1)-i]=tmp;
   }
}

static inline void appendReversed(NSStringBuffer *buffer,
 unichar *characters,unsigned length,
 unichar fillChar,BOOL leftAdj,int fieldWidth){
   reverseCharacters(characters,length);
   appendCharacters(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void appendDecimal(NSStringBuffer *buffer,long long value,unichar fillChar,BOOL leftAdj,BOOL plusSign,BOOL spaceSign,int fieldWidth,NSString *groupingSeparator,NSUInteger groupingSize){
    unsigned length=0,numberOfIntegralDigits=0;
    unichar  characters[100];
    unichar  sign=(value<0)?'-':plusSign?'+':spaceSign?' ':'\0';
    NSUInteger groupingLength=[groupingSeparator length];
    unichar    groupingBuffer[groupingLength];

    [groupingSeparator getCharacters:groupingBuffer];
    
    // Minkar: the magnitude, unsigned; negating LLONG_MIN overflowed and
    // printed garbage.
    unsigned long long magnitude=(value<0)?0ULL-(unsigned long long)value:(unsigned long long)value;

    while(magnitude){
     numberOfIntegralDigits++;
     characters[length++]=(unichar)(magnitude%10)+'0';
     magnitude/=10;
     
     if(groupingSize){
      if((numberOfIntegralDigits%groupingSize)==0){
       NSInteger count=groupingLength; // we append backwards because the string gets reversed
       
       while(--count>=0)
        characters[length++]=groupingBuffer[count];
    }
     }
    }

    if(length==0)
     characters[length++]='0';
    else if(sign)
     characters[length++]=sign;

    appendReversed(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void appendOctal(NSStringBuffer *buffer,unsigned long long value,
  unichar fillChar,BOOL leftAdj,int fieldWidth){
    unsigned length=0;
    unichar  characters[100];

    while(value){
     characters[length++]=(value&0x07)+'0';
     value>>=3;
    }

    if(length==0)
     characters[length++]='0';

    appendReversed(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void appendHex(NSStringBuffer *buffer,unsigned long long value,
  unichar fillChar,BOOL leftAdj,int fieldWidth,const char *hexes){
    unsigned length=0;
    unichar  characters[100];

    while(value){
     characters[length++]=hexes[value&0x0F];
     value>>=4;
    }

    if(length==0)
     characters[length++]='0';

    appendReversed(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void appendUnsigned(NSStringBuffer *buffer,unsigned long long value,
  unichar fillChar,BOOL leftAdj,int fieldWidth,NSString *groupingSeparator,NSUInteger groupingSize){
    unsigned length=0,numberOfIntegralDigits=0;
    unichar  characters[100];
    NSUInteger groupingLength=[groupingSeparator length];
    unichar    groupingBuffer[groupingLength];

    [groupingSeparator getCharacters:groupingBuffer];

    while(value){
     numberOfIntegralDigits++;
     characters[length++]=(value%10)+'0';
     value/=10;

     if(groupingSize){
      if((numberOfIntegralDigits%groupingSize)==0){
       NSInteger count=groupingLength; // we append backwards because the string gets reversed
       
       while(--count>=0)
        characters[length++]=groupingBuffer[count];
    }
     }
    }

    if(length==0)
     characters[length++]='0';

    appendReversed(buffer,characters,length,fillChar,leftAdj,fieldWidth);
}

static inline void appendCString(NSStringBuffer *buffer,const char *cString,
  unichar fillChar,BOOL leftAdj,int fieldWidth){
   NSUInteger length;
   unichar *characters;

   if(cString==NULL)
    cString="(null pointer)";

   characters=NSString_anyCStringToUnicode(NSASCIIStringEncoding,cString,strlen(cString),&length,NULL);
   appendCharacters(buffer,characters,length,fillChar,leftAdj,fieldWidth);

   NSZoneFree(NULL,characters);
}

static inline void appendCStringChar(NSStringBuffer *buffer,char c,
  unichar fillChar,BOOL leftAdj,int fieldWidth){
   char cString[2]={c,'\0'};
   appendCString(buffer,cString,fillChar,leftAdj,fieldWidth);
}

double roundDouble(double value) {
   char *p = (char *)&value;

#ifdef WINDOWS
   value = round(value);
#else
// some OS's don't have round(), Linux?
   value = (value < 0.0) ? ceil(value-0.5) : floor(value+0.5);
#endif

#ifdef __LITTLE_ENDIAN__
   p[0] |= 0x01;
#else
   p[7] |= 0x01;
#endif   

   return value;
}

// Minkar: floating point goes through the C library (snprintf_l in the C
// locale), then takes the locale's decimal separator and grouping. The
// hand-written conversion had no exponent form (%e printed as %f, %g of
// 1e300 printed 300 made-up digits) and no %a.
static locale_t NSFormatterCLocale(void){
   static locale_t locale;

   if(locale==NULL){
    locale_t made=newlocale(LC_ALL_MASK,"C",NULL);
    if(!__sync_bool_compare_and_swap(&locale,NULL,made))
     freelocale(made);
   }
   return locale;
}

static inline void appendFloat(NSStringBuffer *buffer,double value,
  unichar fillChar,BOOL leftAdj,BOOL plusSign,BOOL spaceSign,
  int fieldWidth,int precision,char conversion,BOOL altForm,NSDictionary *locale,NSString *groupingSeparator,NSUInteger groupingSize){
   char       format[32],*f=format,small[128],*text=small;
   int        length;
   NSString  *separatorString;
   unichar    decimalSeparator;
   NSUInteger groupingLength=[groupingSeparator length];

   *f++='%';
   if(leftAdj) *f++='-';
   if(plusSign) *f++='+';
   if(spaceSign) *f++=' ';
   if(altForm) *f++='#';
   if(fillChar=='0' && !leftAdj) *f++='0';
   f+=snprintf(f,format+sizeof(format)-f,"*.*%c",conversion);

   length=snprintf_l(small,sizeof(small),NSFormatterCLocale(),format,fieldWidth,precision,value);
   if(length<0)
    return;
   if((size_t)length>=sizeof(small)){
    text=malloc((size_t)length+1);
    snprintf_l(text,(size_t)length+1,NSFormatterCLocale(),format,fieldWidth,precision,value);
   }

   if(locale)
    separatorString=[locale objectForKey:NSLocaleDecimalSeparator];
   else
    separatorString=[[NSLocale systemLocale] objectForKey:NSLocaleDecimalSeparator];
   decimalSeparator=([separatorString length]>0)?[separatorString characterAtIndex:0]:'.';

   {
    unichar    characters[length*2+groupingLength*(length/(groupingSize?groupingSize:1)+1)+1];
    unichar    groupingBuffer[groupingLength+1];
    NSUInteger out=0,i=0,digitsStart,digitsEnd;

    [groupingSeparator getCharacters:groupingBuffer];
    // Copy up to the integral digits; group them if asked (not in
    // exponent or hexadecimal forms).
    while(i<(NSUInteger)length && !(text[i]>='0' && text[i]<='9'))
     characters[out++]=text[i++];
    digitsStart=i;
    while(i<(NSUInteger)length && text[i]>='0' && text[i]<='9')
     i++;
    digitsEnd=i;
    BOOL group=groupingSize>0 && groupingLength>0 && conversion!='a' && conversion!='A' &&
        strchr(text,'e')==NULL && strchr(text,'E')==NULL;
    for(NSUInteger k=digitsStart;k<digitsEnd;k++){
     characters[out++]=text[k];
     NSUInteger left=digitsEnd-k-1;
     if(group && left>0 && left%groupingSize==0)
      for(NSUInteger g=0;g<groupingLength;g++)
       characters[out++]=groupingBuffer[g];
    }
    for(;i<(NSUInteger)length;i++)
     characters[out++]=(text[i]=='.')?decimalSeparator:(unichar)text[i];
    appendCharacters(buffer,characters,out,' ',leftAdj,0);
   }
   if(text!=small)
    free(text);
}

static inline void appendObject(NSStringBuffer *buffer,id object,
  unichar fillChar,BOOL leftAdj,int fieldWidth){
   if(object==nil)
    appendCString(buffer,"*nil*",fillChar,leftAdj,fieldWidth);
   else {
    NSString *string=[object description];
    unibuffer ubuffer=NewUnibufferWithString(string);

    appendCharacters(buffer,ubuffer.characters,ubuffer.length,fillChar,leftAdj,fieldWidth);

    FreeUnibuffer(ubuffer);
   }
}

static inline unichar *prepForReturn(NSStringBuffer *buffer,NSUInteger *lengthp){

   *lengthp=buffer->max=buffer->length;
   buffer->characters=NSZoneRealloc(buffer->zone,buffer->characters,sizeof(unichar)*buffer->max);

   return buffer->characters;
}

unichar *NSCharactersNewWithFormat(NSString *format,NSDictionary *locale,va_list arguments,NSUInteger *lengthp,NSZone *zone){
   return NSCharactersNewWithFormatAndGrouping(format,locale,arguments,lengthp,zone,nil,0);
}

unichar *NSCharactersNewWithFormatAndGrouping(NSString *format,NSDictionary *locale,va_list arguments,NSUInteger *lengthp,NSZone *zone,NSString *groupingSeparator,NSInteger groupingSize){
   NSUInteger  pos,fmtLength=[format length];
   unichar   fmtBuffer[fmtLength],unicode;
   NSStringBuffer result;

   unichar fillChar=' ',dwModify=' ';
   BOOL    altForm=NO,leftAdj=NO,plusSign=NO,spaceSign=NO;
   int     fieldWidth=0,precision=-1;   // -1: not given (printf then uses its default)

   enum {
    STATE_SCANNING,
    STATE_PERCENT,
    STATE_FIELDWIDTH,
    STATE_PRECISION,
    STATE_MODIFIER,
    STATE_CONVERSION
   } state=STATE_SCANNING;

   [format getCharacters:fmtBuffer];

   prepForUse(&result,zone);

   for(pos=0;pos<fmtLength;pos++){
    unicode=fmtBuffer[pos];

    switch(state){

     case STATE_SCANNING:
      if(unicode!='%')
       appendCharacter(&result,unicode);
      else{
       fillChar=dwModify=' ';
       altForm=leftAdj=plusSign=spaceSign=NO;
       fieldWidth=0;
       precision=-1;
       state=STATE_PERCENT;
      }
      break;

     case STATE_PERCENT:
      switch(unicode){

       case '#': altForm=YES; break;
       case '0': fillChar='0'; break;
       case '-': leftAdj=YES; break;
       case '+': plusSign=YES; break;
       case ' ': spaceSign=YES; break;

       default:
        pos--;
        state=STATE_FIELDWIDTH;
        break;
      }
      break;

     case STATE_FIELDWIDTH:
      switch(unicode){

       case '0': case '1': case '2': case '3': case '4':
       case '5': case '6': case '7': case '8': case '9':
        fieldWidth=fieldWidth*10+(unicode-'0');
        break;

       case '*':
        fieldWidth=va_arg(arguments,int);
        if(fieldWidth<0){
         leftAdj=YES;
         fieldWidth=-fieldWidth;
        }
        break;

       case '$': // FIXME: argument index for localization
        state=STATE_MODIFIER;
        break;
        
       case '.':
        precision=0;
        state=STATE_PRECISION;
        break;

       default:
        pos--;
        state=STATE_MODIFIER;
        break;
      }
      break;

     case STATE_PRECISION:
      switch(unicode){

       case '0': case '1': case '2': case '3': case '4':
       case '5': case '6': case '7': case '8': case '9':
        precision=precision*10+(unicode-'0');
        break;

       case '*': // fix
        precision=va_arg(arguments,int);
        break;

       default:
        pos--;
        state=STATE_MODIFIER;
        break;
      }
      break;

     case STATE_MODIFIER:
      switch(unicode){

       case 'h': case 'l': case 'q':
        // New Cocoa special modes for NSInteger and NSUInteger
        case 'z': case 't':
        dwModify=unicode;
        break;

       default:
        pos--;
        state=STATE_CONVERSION;
        break;
      }
      break;

     case STATE_CONVERSION:
      switch(unicode){

       case 'd': case 'D': case 'i':{
         long long value;

         if(dwModify=='h')
          value=(short)va_arg(arguments,int);
         else if(dwModify=='l')
          value=va_arg(arguments,long);
         else if(dwModify=='q')
          value=va_arg(arguments,long long);
         else if(dwModify=='z')
          value=va_arg(arguments, NSInteger);
         else
          value=va_arg(arguments,int);

         appendDecimal(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,groupingSeparator,groupingSize);
        }
        break;

       case 'o': {
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)va_arg(arguments,int);
         else if(dwModify=='l')
          value=va_arg(arguments,unsigned long);
         else if(dwModify=='q')
          value=va_arg(arguments,unsigned long long);
         else
          value=va_arg(arguments,unsigned int);

         appendOctal(&result,value,fillChar,leftAdj,fieldWidth);
        }
        break;

       case 'x':{
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)va_arg(arguments,int);
         else if(dwModify=='l')
          value=va_arg(arguments,unsigned long);
         else if(dwModify=='q')
          value=va_arg(arguments,unsigned long long);
         else if(dwModify=='t')
          value=va_arg(arguments, NSUInteger);
         else
          value=va_arg(arguments,unsigned int);

         appendHex(&result,value,fillChar,leftAdj,fieldWidth,"0123456789abcdef");
        }
        break;

       case 'X':{
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)va_arg(arguments,int);
         else if(dwModify=='l')
          value=va_arg(arguments,unsigned long);
         else if(dwModify=='q')
          value=va_arg(arguments,unsigned long long);
         else if(dwModify=='t')
          value=va_arg(arguments, NSUInteger);
         else
          value=va_arg(arguments,unsigned int);

         appendHex(&result,value,fillChar,leftAdj,fieldWidth,"0123456789ABCDEF");
        }
        break;

       case 'u': case 'U': {
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)va_arg(arguments,int);
         else if(dwModify=='l')
          value=va_arg(arguments,unsigned long);
         else if(dwModify=='q')
          value=va_arg(arguments,unsigned long long);
         else if(dwModify=='t')
          value=va_arg(arguments, NSUInteger);
         else
          value=va_arg(arguments,unsigned int);

         appendUnsigned(&result,value,fillChar,leftAdj,fieldWidth,groupingSeparator,groupingSize);
        }
        break;

       case 'c':
        appendCStringChar(&result,va_arg(arguments,int),fillChar,leftAdj,fieldWidth);
        break;

       case 'C':
        appendUnichar(&result,va_arg(arguments,int),fillChar,leftAdj,fieldWidth);
        break;

       case 's':
        appendCString(&result,va_arg(arguments,char *),fillChar,leftAdj,fieldWidth);
        break;

       case 'S':
        appendUnicodeString(&result,va_arg(arguments,unichar *),fillChar,leftAdj,fieldWidth);
        break;

       case 'f': case 'F':{
         double value;

         if(dwModify=='l')
          value=va_arg(arguments,double);
         else
          value=va_arg(arguments,double);

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'e': case 'E':{
         double value;

         if(dwModify=='l')
          value=va_arg(arguments,double);
         else
          value=va_arg(arguments,double);

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'a': case 'A':{
         double value=va_arg(arguments,double);

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'g': case 'G':{
         double value;

         if(dwModify=='l')
          value=va_arg(arguments,double);
         else
          value=va_arg(arguments,double);

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'p':
        appendHex(&result,(long)va_arg(arguments,void *),fillChar,leftAdj,fieldWidth,"0123456789ABCDEF");
        break;

       case 'n':
        *va_arg(arguments,int *)=result.length;
        break;

       case '@':
        appendObject(&result,va_arg(arguments,id),fillChar,leftAdj,fieldWidth);
        break;

       case '%':
        appendCharacter(&result,'%');
        break;
      }
      state=STATE_SCANNING;
      break;
    }

   }

   return prepForReturn(&result,lengthp);
}

NSString *NSStringNewWithFormat(NSString *format,NSDictionary *locale,
  va_list arguments,NSZone *zone) {
   NSUInteger  length;
   unichar  *unicode;

   unicode=NSCharactersNewWithFormat(format,locale,arguments,&length,NULL);

   return NSString_unicodePtrNewNoCopy(zone,unicode,length,YES);
}

NSString *NSStringWithFormat(NSString *format,...) {
   va_list arguments;

   va_start(arguments,format);

   NSString *result=NSAutorelease(NSStringNewWithFormat(format,nil,arguments,NULL));
   va_end(arguments);
   return result;
}

NSString *NSStringWithFormatArguments(NSString *format,va_list arguments) {
   return NSAutorelease(NSStringNewWithFormat(format,nil,arguments,NULL));
}

NSString *NSStringWithFormatAndLocale(NSString *format,NSDictionary *locale,...) {
   va_list arguments;

   va_start(arguments,locale);

   NSString *result=NSAutorelease(NSStringNewWithFormat(format,locale,arguments,NULL));
   
   va_end(arguments);
   
   return result;
}

