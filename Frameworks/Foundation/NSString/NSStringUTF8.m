/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSStringSymbol.h>
#import <Foundation/NSRaise.h>

// Minkar: the UTF-16 <-> UTF-8 conversions below replace ones that wrote
// each half of a surrogate pair as its own 3-byte sequence (CESU-8, not
// UTF-8), read 4-byte sequences as one 16-bit unit, and accepted any
// bytes.

// The code point at characters[i] and how many units it takes: a
// surrogate pair is one code point; an unpaired surrogate is U+FFFD.
static uint32_t NSUTF16CodePointAt(const unichar *characters,NSUInteger length,NSUInteger i,NSUInteger *units){
    uint32_t c=characters[i];

    *units=1;
    if(c>=0xD800 && c<=0xDBFF && i+1<length && characters[i+1]>=0xDC00 && characters[i+1]<=0xDFFF){
        *units=2;
        return 0x10000+((c-0xD800)<<10)+(characters[i+1]-0xDC00);
    }
    if(c>=0xD800 && c<=0xDFFF)
        return 0xFFFD;
    return c;
}

// Writes the UTF-8 for c into out (4 bytes at most); returns its length.
static int NSUTF8EncodeCodePoint(uint32_t c,uint8_t *out){
    if(c<0x80){
        out[0]=(uint8_t)c;
        return 1;
    }
    if(c<0x800){
        out[0]=(uint8_t)(0xC0|(c>>6));
        out[1]=(uint8_t)(0x80|(c&0x3F));
        return 2;
    }
    if(c<0x10000){
        out[0]=(uint8_t)(0xE0|(c>>12));
        out[1]=(uint8_t)(0x80|((c>>6)&0x3F));
        out[2]=(uint8_t)(0x80|(c&0x3F));
        return 3;
    }
    out[0]=(uint8_t)(0xF0|(c>>18));
    out[1]=(uint8_t)(0x80|((c>>12)&0x3F));
    out[2]=(uint8_t)(0x80|((c>>6)&0x3F));
    out[3]=(uint8_t)(0x80|(c&0x3F));
    return 4;
}

NSUInteger _NSGetUTF8CStringWithMaxLength(const unichar *characters,NSUInteger length,NSUInteger *location,char *cString,NSUInteger maxLength, BOOL zeroTerminate){
    NSUInteger utf8Length=0;
    NSUInteger i=0;

    while(i<length){
        NSUInteger units;
        uint8_t    tmp[4];
        int        tmpLength=NSUTF8EncodeCodePoint(NSUTF16CodePointAt(characters,length,i,&units),tmp);

        if(cString!=NULL){
            if(utf8Length+tmpLength>maxLength){
                cString[0]='\0';
                return NSNotFound;     // buffer too small
            }
            for(int k=0;k<tmpLength;k++)
                cString[utf8Length+k]=(char)tmp[k];
        }
        utf8Length+=tmpLength;
        i+=units;
    }

    if (location != NULL) {
        *location=i;
    }

    if (zeroTerminate && cString) {
        //check if zero termination has space
        if (maxLength <= utf8Length) {
            cString[0]='\0';
            return NSNotFound;
        }

        cString[utf8Length]='\0';
    }

    return utf8Length;
}

NSUInteger NSConvertUTF16toUTF8(const unichar *utf16,NSUInteger utf16Length,uint8_t *utf8){
    NSUInteger utf8Length=0;
    NSUInteger i=0;

    while(i<utf16Length){
        NSUInteger units;
        uint8_t    tmp[4];
        int        tmpLength=NSUTF8EncodeCodePoint(NSUTF16CodePointAt(utf16,utf16Length,i,&units),tmp);

        if(utf8!=NULL)
            for(int k=0;k<tmpLength;k++)
                utf8[utf8Length+k]=tmp[k];
        utf8Length+=tmpLength;
        i+=units;
    }
    return utf8Length;
}

NSUInteger NSGetUTF8CStringWithMaxLength(const unichar *characters,NSUInteger length,NSUInteger *location,char *cString,NSUInteger maxLength){
    return _NSGetUTF8CStringWithMaxLength(characters, length, location, cString, maxLength, YES);
}

char    *NSUnicodeToUTF8(const unichar *characters,NSUInteger length,
  BOOL lossy,NSUInteger *resultLength,NSZone *zone,BOOL zeroTerminate){
  NSUInteger  utf8Length=_NSGetUTF8CStringWithMaxLength(characters,length,NULL, NULL, UINT_MAX, NO);
    NSUInteger maxLength = utf8Length+(zeroTerminate?1:0);
  char     *utf8=NSZoneMalloc(NULL,maxLength*sizeof(unsigned char));

  *resultLength=_NSGetUTF8CStringWithMaxLength(characters,length,NULL, utf8, maxLength, zeroTerminate);
  if(zeroTerminate){
   (*resultLength)++;
  }

  return utf8;
}

// The code point of the well-formed UTF-8 sequence at utf8[i] (Unicode
// 15, table 3-7) and its length, or 0 when it is not well formed.
static int NSUTF8DecodeAt(const unsigned char *utf8,NSUInteger length,NSUInteger i,uint32_t *code){
    unsigned char b0=utf8[i];
    NSUInteger    left=length-i;

    if(b0<0x80){
        *code=b0;
        return 1;
    }
    if(b0>=0xC2 && b0<=0xDF && left>=2 && (utf8[i+1]&0xC0)==0x80){
        *code=((uint32_t)(b0&0x1F)<<6)|(utf8[i+1]&0x3F);
        return 2;
    }
    if(b0>=0xE0 && b0<=0xEF && left>=3){
        unsigned char b1=utf8[i+1], b2=utf8[i+2];
        unsigned char lo=(b0==0xE0)?0xA0:0x80, hi=(b0==0xED)?0x9F:0xBF;

        if(b1>=lo && b1<=hi && (b2&0xC0)==0x80){
            *code=((uint32_t)(b0&0x0F)<<12)|((uint32_t)(b1&0x3F)<<6)|(b2&0x3F);
            return 3;
        }
    }
    if(b0>=0xF0 && b0<=0xF4 && left>=4){
        unsigned char b1=utf8[i+1], b2=utf8[i+2], b3=utf8[i+3];
        unsigned char lo=(b0==0xF0)?0x90:0x80, hi=(b0==0xF4)?0x8F:0xBF;

        if(b1>=lo && b1<=hi && (b2&0xC0)==0x80 && (b3&0xC0)==0x80){
            *code=((uint32_t)(b0&0x07)<<18)|((uint32_t)(b1&0x3F)<<12)|((uint32_t)(b2&0x3F)<<6)|(b3&0x3F);
            return 4;
        }
    }
    return 0;
}

BOOL NSUTF8IsValid(const unsigned char *utf8,NSUInteger length){
    NSUInteger i=0;

    while(i<length){
        uint32_t code;
        int      n=NSUTF8DecodeAt(utf8,length,i,&code);

        if(n==0)
            return NO;
        i+=n;
    }
    return YES;
}

// Code points above U+FFFF become surrogate pairs; each byte that does
// not start a well-formed sequence becomes U+FFFD.
NSUInteger NSConvertUTF8toUTF16(const unsigned char *utf8,NSUInteger utf8Length,unichar *utf16){
    NSUInteger i=0,utf16Length=0;

    while(i<utf8Length){
        uint32_t code;
        int      n=NSUTF8DecodeAt(utf8,utf8Length,i,&code);

        if(n==0){
            code=0xFFFD;
            n=1;
        }
        if(code>=0x10000){
            if(utf16!=NULL){
                utf16[utf16Length]=(unichar)(0xD800+((code-0x10000)>>10));
                utf16[utf16Length+1]=(unichar)(0xDC00+((code-0x10000)&0x3FF));
            }
            utf16Length+=2;
        }
        else {
            if(utf16!=NULL)
                utf16[utf16Length]=(unichar)code;
            utf16Length++;
        }
        i+=n;
    }

    return utf16Length;
}

unichar *NSUTF8ToUnicode(const char *utf8,NSUInteger length,
  NSUInteger *resultLength,NSZone *zone) {
   NSUInteger utf16Length=NSConvertUTF8toUTF16((const unsigned char *)utf8,length,NULL);
   unichar *utf16=NSZoneMalloc(NULL,(utf16Length+1)*sizeof(unichar));

   *resultLength=NSConvertUTF8toUTF16((const unsigned char *)utf8,length,utf16);

   return utf16;
}

BOOL NSUTF8IsASCII(const char *utf8,NSUInteger length) {
  int i;

  for(i=0;i<length;i++)
   if(utf8[i]&0x80)
    return NO;

  return YES;
}

