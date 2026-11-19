/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Onyx2D/O2PDFObject_Name.h>
#import <Onyx2D/O2PDFContext.h>
#import <Foundation/NSString.h>
#import <string.h>

@implementation O2PDFObject_Name

static int hexValue(char c) {
   if(c>='0' && c<='9') return c-'0';
   if(c>='a' && c<='f') return c-'a'+10;
   if(c>='A' && c<='F') return c-'A'+10;
   return -1;
}

/* Minkar: a name as read, its #xx escapes decoded (/Some#20Name is
   "Some Name"); a # not followed by two hex digits is kept. */
-initWithBytes:(const char *)bytes length:(unsigned)length {
   unsigned i,j=0;

   _bytes=NSZoneMalloc(NULL,length+1);
   for(i=0;i<length;i++){
    if(bytes[i]=='#' && i+2<length && hexValue(bytes[i+1])>=0 && hexValue(bytes[i+2])>=0){
     _bytes[j++]=(char)(hexValue(bytes[i+1])*16+hexValue(bytes[i+2]));
     i+=2;
    }
    else
     _bytes[j++]=bytes[i];
   }
   _bytes[j]='\0';
   _length=j;
   return self;
}

-(void)dealloc {
   NSZoneFree(NULL,_bytes);
   [super dealloc];
}

+pdfObjectWithBytes:(const char *)bytes length:(unsigned)length {
   return [[(O2PDFObject_Name *)[self alloc] initWithBytes:bytes length:length] autorelease];
}

+pdfObjectWithCString:(const char *)cString {
   return [self pdfObjectWithBytes:cString length:strlen(cString)];
}

-(const char *)name {
   return _bytes;
}

-(O2PDFObjectType)objectType {
   return kO2PDFObjectTypeName;
}

-(BOOL)checkForType:(O2PDFObjectType)type value:(void *)value {
   if(type!=kO2PDFObjectTypeName)
    return NO;
   
   *((char **)value)=_bytes;
   return YES;
}

-(void)encodeWithPDFContext:(O2PDFContext *)encoder {
   /* Minkar: written back with the characters a name cannot hold
      escaped as #xx. */
   NSMutableString *name=[NSMutableString stringWithString:@"/"];
   unsigned         i;

   for(i=0;i<_length;i++){
    unsigned char c=_bytes[i];

    if(c<=' ' || c>=0x7F || c=='#' || strchr("()<>[]{}/%",c)!=NULL)
     [name appendFormat:@"#%02X",c];
    else
     [name appendFormat:@"%c",c];
   }
   [encoder appendFormat:@"%@ ",name];
}

-(NSString *)description {
   return [NSString stringWithFormat:@"/%s",_bytes];
}

@end
