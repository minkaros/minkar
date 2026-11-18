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

/* %p as the reference platform's: 0x and the address in lowercase hex
   (0x0 for NULL), padded with spaces to the field's width. */
static inline void appendPointer(NSStringBuffer *buffer,unsigned long long value,
  BOOL leftAdj,int fieldWidth){
    unsigned length=0;
    unichar  characters[100];

    do {
     characters[length++]="0123456789abcdef"[value&0x0F];
     value>>=4;
    } while(value);
    characters[length++]='x';
    characters[length++]='0';

    appendReversed(buffer,characters,length,' ',leftAdj,fieldWidth);
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

// some OS's don't have round(), Linux?
   value = (value < 0.0) ? ceil(value-0.5) : floor(value+0.5);

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
    appendCString(buffer,"(null)",fillChar,leftAdj,fieldWidth);
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

/* Positional arguments ("%2$@ %1$d", as localized strings reorder them,
   the same one used more than once): their kinds found in a first pass
   over the format, then read from the list once, in order, and taken by
   number while formatting. */
typedef enum { ARG_NONE=0, ARG_INT, ARG_LONG, ARG_LLONG, ARG_DOUBLE, ARG_POINTER } NSFormatArgKind;

typedef union {
   int        i;
   long       l;
   long long  ll;
   double     d;
   void      *p;
} NSFormatArg;

#define NSFORMAT_MAX_POSITIONAL 64

static NSFormatArgKind kindOfConversion(unichar modifier,unichar conversion){
   switch(conversion){
    case 'd': case 'D': case 'i': case 'o': case 'x': case 'X': case 'u': case 'U':
     if(modifier=='l' || modifier=='z' || modifier=='t')
      return ARG_LONG;
     if(modifier=='q')
      return ARG_LLONG;
     return ARG_INT;
    case 'c': case 'C':
     return ARG_INT;
    case 'f': case 'F': case 'e': case 'E': case 'a': case 'A': case 'g': case 'G':
     return ARG_DOUBLE;
    case 's': case 'S': case 'p': case 'n': case '@':
     return ARG_POINTER;
   }
   return ARG_NONE;
}

/* The kinds of the numbered arguments of a format (1-based), and how many
   there are; 0 when it numbers none. */
static int scanPositional(const unichar *fmt,NSUInteger length,NSFormatArgKind *kinds){
   NSUInteger pos=0;
   int        highest=0;

   while(pos<length){
    NSUInteger start;
    int        number=0,index=0;
    unichar    modifier=' ';

    if(fmt[pos++]!='%')
     continue;
    if(pos<length && fmt[pos]=='%'){
     pos++;
     continue;
    }
    start=pos;
    while(pos<length && fmt[pos]>='0' && fmt[pos]<='9')
     number=number*10+(fmt[pos++]-'0');
    if(pos<length && fmt[pos]=='$' && number>0){
     index=number;
     pos++;
    }
    else
     pos=start;
    while(pos<length && (fmt[pos]=='#' || fmt[pos]=='0' || fmt[pos]=='-' || fmt[pos]=='+' || fmt[pos]==' '))
     pos++;
    while(pos<length && ((fmt[pos]>='0' && fmt[pos]<='9') || fmt[pos]=='*' || fmt[pos]=='$'))
     pos++;
    if(pos<length && fmt[pos]=='.'){
     pos++;
     while(pos<length && ((fmt[pos]>='0' && fmt[pos]<='9') || fmt[pos]=='*' || fmt[pos]=='$'))
      pos++;
    }
    while(pos<length && (fmt[pos]=='h' || fmt[pos]=='l' || fmt[pos]=='q' || fmt[pos]=='z' || fmt[pos]=='t' || fmt[pos]=='L' || fmt[pos]=='j'))
     modifier=fmt[pos++];
    if(pos>=length)
     break;
    if(index>0 && index<=NSFORMAT_MAX_POSITIONAL){
     kinds[index]=kindOfConversion(modifier,fmt[pos]);
     if(index>highest)
      highest=index;
    }
    pos++;
   }
   return highest;
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

   NSFormatArgKind kinds[NSFORMAT_MAX_POSITIONAL+1]={ARG_NONE};
   NSFormatArg     values[NSFORMAT_MAX_POSITIONAL+1];
   int             positionalCount=scanPositional(fmtBuffer,fmtLength,kinds),argIndex=0,k;

   if(positionalCount>0){
    va_list copy;

    va_copy(copy,arguments);
    for(k=1;k<=positionalCount;k++)
     switch(kinds[k]){
      case ARG_LONG:    values[k].l=va_arg(copy,long); break;
      case ARG_LLONG:   values[k].ll=va_arg(copy,long long); break;
      case ARG_DOUBLE:  values[k].d=va_arg(copy,double); break;
      case ARG_POINTER: values[k].p=va_arg(copy,void *); break;
      default:          values[k].i=va_arg(copy,int); break;
     }
    va_end(copy);
   }

/* The next argument: the numbered one if the conversion named one. */
#define NEXT_INT()     ((argIndex>0)?values[argIndex].i:va_arg(arguments,int))
#define NEXT_LONG()    ((argIndex>0)?values[argIndex].l:va_arg(arguments,long))
#define NEXT_LLONG()   ((argIndex>0)?values[argIndex].ll:va_arg(arguments,long long))
#define NEXT_DOUBLE()  ((argIndex>0)?values[argIndex].d:va_arg(arguments,double))
#define NEXT_POINTER() ((argIndex>0)?values[argIndex].p:va_arg(arguments,void *))

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
       argIndex=0;
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
        fieldWidth=NEXT_INT();
        if(fieldWidth<0){
         leftAdj=YES;
         fieldWidth=-fieldWidth;
        }
        break;

       case '$': /* the digits were the argument's number: flags follow */
        argIndex=(fieldWidth>0 && fieldWidth<=NSFORMAT_MAX_POSITIONAL)?fieldWidth:0;
        fieldWidth=0;
        state=STATE_PERCENT;
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
        precision=NEXT_INT();
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
          value=(short)NEXT_INT();
         else if(dwModify=='l')
          value=NEXT_LONG();
         else if(dwModify=='q')
          value=NEXT_LLONG();
         else if(dwModify=='z')
          value=((NSInteger)NEXT_LONG());
         else
          value=NEXT_INT();

         appendDecimal(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,groupingSeparator,groupingSize);
        }
        break;

       case 'o': {
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)NEXT_INT();
         else if(dwModify=='l')
          value=((unsigned long)NEXT_LONG());
         else if(dwModify=='q')
          value=((unsigned long long)NEXT_LLONG());
         else
          value=((unsigned int)NEXT_INT());

         appendOctal(&result,value,fillChar,leftAdj,fieldWidth);
        }
        break;

       case 'x':{
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)NEXT_INT();
         else if(dwModify=='l')
          value=((unsigned long)NEXT_LONG());
         else if(dwModify=='q')
          value=((unsigned long long)NEXT_LLONG());
         else if(dwModify=='t')
          value=((NSUInteger)NEXT_LONG());
         else
          value=((unsigned int)NEXT_INT());

         appendHex(&result,value,fillChar,leftAdj,fieldWidth,"0123456789abcdef");
        }
        break;

       case 'X':{
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)NEXT_INT();
         else if(dwModify=='l')
          value=((unsigned long)NEXT_LONG());
         else if(dwModify=='q')
          value=((unsigned long long)NEXT_LLONG());
         else if(dwModify=='t')
          value=((NSUInteger)NEXT_LONG());
         else
          value=((unsigned int)NEXT_INT());

         appendHex(&result,value,fillChar,leftAdj,fieldWidth,"0123456789ABCDEF");
        }
        break;

       case 'u': case 'U': {
         unsigned long long value;

         if(dwModify=='h')
          value=(unsigned short)NEXT_INT();
         else if(dwModify=='l')
          value=((unsigned long)NEXT_LONG());
         else if(dwModify=='q')
          value=((unsigned long long)NEXT_LLONG());
         else if(dwModify=='t')
          value=((NSUInteger)NEXT_LONG());
         else
          value=((unsigned int)NEXT_INT());

         appendUnsigned(&result,value,fillChar,leftAdj,fieldWidth,groupingSeparator,groupingSize);
        }
        break;

       case 'c':
        appendCStringChar(&result,NEXT_INT(),fillChar,leftAdj,fieldWidth);
        break;

       case 'C':
        appendUnichar(&result,NEXT_INT(),fillChar,leftAdj,fieldWidth);
        break;

       case 's':
        appendCString(&result,((char *)NEXT_POINTER()),fillChar,leftAdj,fieldWidth);
        break;

       case 'S':
        appendUnicodeString(&result,((unichar *)NEXT_POINTER()),fillChar,leftAdj,fieldWidth);
        break;

       case 'f': case 'F':{
         double value;

         if(dwModify=='l')
          value=NEXT_DOUBLE();
         else
          value=NEXT_DOUBLE();

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'e': case 'E':{
         double value;

         if(dwModify=='l')
          value=NEXT_DOUBLE();
         else
          value=NEXT_DOUBLE();

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'a': case 'A':{
         double value=NEXT_DOUBLE();

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'g': case 'G':{
         double value;

         if(dwModify=='l')
          value=NEXT_DOUBLE();
         else
          value=NEXT_DOUBLE();

         appendFloat(&result,value,fillChar,leftAdj,plusSign,spaceSign,fieldWidth,precision,(char)unicode,altForm,locale,groupingSeparator,groupingSize);
        }
        break;

       case 'p':
        appendPointer(&result,(unsigned long long)(uintptr_t)NEXT_POINTER(),leftAdj,fieldWidth);
        break;

       case 'n':
        *((int *)NEXT_POINTER())=result.length;
        break;

       case '@':
        appendObject(&result,((id)NEXT_POINTER()),fillChar,leftAdj,fieldWidth);
        break;

       case '%':
        appendCharacter(&result,'%');
        break;
      }
      state=STATE_SCANNING;
      break;
    }

   }

#undef NEXT_INT
#undef NEXT_LONG
#undef NEXT_LLONG
#undef NEXT_DOUBLE
#undef NEXT_POINTER
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

