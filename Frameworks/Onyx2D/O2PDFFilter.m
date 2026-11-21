/* Copyright (c) 2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/*  zlib decode is based on the public domain zlib decode v0.2 by Sean Barrett 2006-11-18  http://www.nothings.org/stb_image.c  V 0.57 */

#import <Onyx2D/O2PDFFilter.h>
#import <Onyx2D/O2PDFObject.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Onyx2D/O2zlib.h>
#import <Onyx2D/O2LZW.h>
#include <stdlib.h>
#import <string.h>
#import <Onyx2D/O2ImageSource_JPEG.h>

NSData *O2PDFFilterWithName(const char *name,NSData *data,O2PDFDictionary *parameters) {
   return [O2PDFFilter decodeWithName:name data:data parameters:parameters];
}

@implementation O2PDFFilter


+(NSData *)FlateDecode_data:(NSData *)data parameters:(O2PDFDictionary *)parameters {
   int len;
   unsigned char *result=stbi_zlib_decode_malloc([data bytes],[data length],&len);
   
   if(result==NULL)
    return nil;

   return [NSData dataWithBytesNoCopy:result length:len];
}

+(NSData *)LZWDecode_data:(NSData *)data parameters:(O2PDFDictionary *)parameters {
   return LZWDecodeWithExpectedResultLength(data,[data length]*10);
}

+(NSData *)decodeWithName:(const char *)name data:(NSData *)data parameters:(O2PDFDictionary *)parameters {
   if((strcmp(name,"FlateDecode")==0) || (strcmp(name,"FL")==0))
     data=[self FlateDecode_data:data parameters:parameters];
   else if((strcmp(name,"DCTDecode")==0) || (strcmp(name,"DCT")==0)){
    return O2DCTDecode(data);
   }
   else if((strcmp(name,"LZWDecode")==0) || (strcmp(name,"LZW")==0)){
    data=LZWDecodeWithExpectedResultLength(data,[data length]*10);
   }
   else {
    O2PDFError(__FILE__,__LINE__,@"Unknown O2PDFFilter name = %s, parameters=%@",name,parameters);
    return nil;
   }
    
   O2PDFInteger predictor=0;

    /* Minkar: the predictors as the PDF reference defines them: 2 is TIFF's
       (each component plus the same component of the pixel to its left), 10
       and up PNG's (a filter byte before each row: None, Sub, Up, Average or
       Paeth, against the pixel to the left and the row above). */
    if(data!=nil && [parameters getIntegerForKey:"Predictor" value:&predictor] && predictor>1){
     const unsigned char *bytes=[data bytes];
     NSUInteger           length=[data length];
     O2PDFInteger         colors,bitsPerComponent,columns;

     if(![parameters getIntegerForKey:"Colors" value:&colors] || colors<1)
      colors=1;
     if(![parameters getIntegerForKey:"BitsPerComponent" value:&bitsPerComponent] || bitsPerComponent<1)
      bitsPerComponent=8;
     if(![parameters getIntegerForKey:"Columns" value:&columns] || columns<1)
      columns=1;

     NSUInteger bytesPerPixel=(colors*bitsPerComponent+7)/8;
     NSUInteger bytesPerRow=(colors*bitsPerComponent*columns+7)/8;

     if(predictor==2){
      NSMutableData *mutable=[NSMutableData dataWithBytes:bytes length:length];
      unsigned char *out=[mutable mutableBytes];

      if(bitsPerComponent==8)
       for(NSUInteger row=0;row+bytesPerRow<=length;row+=bytesPerRow)
        for(NSUInteger i=bytesPerPixel;i<bytesPerRow;i++)
         out[row+i]+=out[row+i-bytesPerPixel];
      data=mutable;
     }
     else if(predictor>=10){
      NSMutableData *mutable=[NSMutableData dataWithCapacity:length];
      unsigned char *previous=calloc(bytesPerRow,1),*current=calloc(bytesPerRow,1);

      for(NSUInteger at=0;at+1+bytesPerRow<=length;at+=1+bytesPerRow){
       int filter=bytes[at];

       for(NSUInteger i=0;i<bytesPerRow;i++){
        unsigned raw=bytes[at+1+i];
        unsigned left=(i>=bytesPerPixel)?current[i-bytesPerPixel]:0;
        unsigned up=previous[i];
        unsigned upLeft=(i>=bytesPerPixel)?previous[i-bytesPerPixel]:0;

        switch(filter){
         case 1: raw+=left; break;
         case 2: raw+=up; break;
         case 3: raw+=(left+up)/2; break;
         case 4: {
          int p=(int)left+(int)up-(int)upLeft;
          int pa=abs(p-(int)left),pb=abs(p-(int)up),pc=abs(p-(int)upLeft);

          raw+=(pa<=pb && pa<=pc)?left:(pb<=pc)?up:upLeft;
          break;
         }
         default: break;
        }
        current[i]=(unsigned char)raw;
       }
       [mutable appendBytes:current length:bytesPerRow];
       unsigned char *swap=previous;

       previous=current;
       current=swap;
      }
      free(previous);
      free(current);
      data=mutable;
     }
    }

    return data;
   }

@end
