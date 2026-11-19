#import "O2PDFFont.h"
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFObject_Name.h>
#import <Onyx2D/O2Font.h>
#import <Onyx2D/O2Encoding.h>
#import "O2PDFCharWidths.h"

/* The hex string at *p (<...>), as a number of up to 4 bytes; false if
   none is there. */
static BOOL nextHex(const char **p,const char *end,uint32_t *value,int *byteCount){
   const char *s=*p;
   uint32_t    result=0;
   int         digits=0;

   while(s<end && *s!='<' && *s!='[' && *s!='e')
    s++;
   if(s>=end || *s!='<')
    return NO;
   for(s++;s<end && *s!='>';s++){
    int d=(*s>='0'&&*s<='9')?*s-'0':(*s>='a'&&*s<='f')?*s-'a'+10:(*s>='A'&&*s<='F')?*s-'A'+10:-1;

    if(d>=0){
     if(digits<8)
      result=(result<<4)|d;
     digits++;
    }
   }
   *p=(s<end)?s+1:end;
   *value=result;
   *byteCount=(digits+1)/2;
   return YES;
}

/* A ToUnicode CMap's one-byte codes into unicode (the first UTF-16 unit of
   each); YES if any was. */
static BOOL O2PDFParseToUnicode(NSData *data,unichar *unicode){
   const char *bytes=[data bytes],*end=bytes+[data length],*p=bytes;
   BOOL        mapped=NO;

   while(p<end){
    const char *bfchar=strnstr(p,"beginbfchar",end-p),*bfrange=strnstr(p,"beginbfrange",end-p);
    const char *section=(bfchar && (!bfrange || bfchar<bfrange))?bfchar:bfrange;

    if(section==NULL)
     break;
    if(section==bfchar){
     const char *stop=strnstr(section,"endbfchar",end-section);

     p=section+11;
     stop=stop?stop:end;
     for(;;){
      uint32_t code,value;
      int      codeBytes,valueBytes;

      if(!nextHex(&p,stop,&code,&codeBytes) || !nextHex(&p,stop,&value,&valueBytes))
       break;
      if(codeBytes==1 && code<256){
       unicode[code]=(valueBytes>2)?(unichar)(value>>((valueBytes-2)*8)):(unichar)value;
       mapped=YES;
      }
     }
     p=stop;
    }
    else {
     const char *stop=strnstr(section,"endbfrange",end-section);

     p=section+12;
     stop=stop?stop:end;
     for(;;){
      uint32_t low,high,value;
      int      lowBytes,highBytes,valueBytes;

      if(!nextHex(&p,stop,&low,&lowBytes) || !nextHex(&p,stop,&high,&highBytes))
       break;
      while(p<stop && (*p==' '||*p=='\n'||*p=='\r'||*p=='\t'))
       p++;
      if(p<stop && *p=='['){
       /* One destination string per code. */
       for(uint32_t code=low;code<=high && nextHex(&p,stop,&value,&valueBytes);code++)
        if(lowBytes==1 && code<256){
         unicode[code]=(unichar)value;
         mapped=YES;
        }
       while(p<stop && *p!=']')
        p++;
       if(p<stop)
        p++;
      }
      else if(nextHex(&p,stop,&value,&valueBytes)){
       if(valueBytes>2)
        value>>=(valueBytes-2)*8;
       for(uint32_t code=low;code<=high && code<256;code++)
        if(lowBytes==1){
         unicode[code]=(unichar)(value+(code-low));
         mapped=YES;
        }
      }
      else
       break;
     }
     p=stop;
    }
   }
   return mapped;
}

@implementation O2PDFFont

static inline O2TextEncoding textEncodingWithName(const char *name){
   if(strcmp(name,"MacRomanEncoding")==0)
    return kO2EncodingMacRoman;
   else if(strcmp(name,"MacExpertEncoding")==0)
    return kO2EncodingMacExpert;
   else if(strcmp(name,"WinAnsiEncoding")==0)
    return kO2EncodingWinAnsi;
   else {
    O2PDFError(__FILE__,__LINE__,@"Unknown encoding %s",name);
    return kO2EncodingMacRoman;
   }
}

-initWithPDFDictionary:(O2PDFDictionary *)dictionary {
   const char      *name;
   const char      *subtype;
   O2PDFInteger     firstChar=-1;
   O2PDFInteger     lastChar=-1;
   O2PDFArray      *widthsArray=NULL;
   O2PDFDictionary *fontDescriptor=NULL;
   O2PDFObject     *encoding=NULL;
   O2PDFStream     *toUnicode=NULL;
   O2TextEncoding   textEncoding=0;
   O2Float          missingWidth=0;
   const char      *fontName=NULL;
   O2PDFArray      *differences=NULL;

   _baseFont="Inter";
      
   if([dictionary getNameForKey:"Type" value:&name])
    if(strcmp(name,"Font")!=0){
     O2PDFError(__FILE__,__LINE__,@"Type dictionary in Font resource is wrong %s",name);
     return nil;
    }

   if(![dictionary getNameForKey:"Subtype" value:&subtype]){
    O2PDFError(__FILE__,__LINE__,@"Font dictionary does not contain Subtype");
    return nil;
   }
   
   if(![dictionary getIntegerForKey:"FirstChar" value:&firstChar]){

   }
   if(![dictionary getIntegerForKey:"LastChar" value:&lastChar]){

   }
   
   if([dictionary getDictionaryForKey:"FontDescriptor" value:&fontDescriptor]){
    [fontDescriptor getNumberForKey:"MissingWidth" value:&missingWidth];
     
    [fontDescriptor getNameForKey:"FontName" value:&fontName];
    
    O2PDFStream *fontStream=NULL;
    
    if([fontDescriptor getStreamForKey:"FontFile" value:&fontStream]){
     NSData *data=[fontStream data];

     if(data==nil)
      O2PDFError(__FILE__,__LINE__,@"No data on FontFile stream");
     else {
      O2DataProviderRef provider=O2DataProviderCreateWithCFData((CFDataRef)data);
      _resourceFont=O2FontCreateWithDataProvider(provider);
            
      O2DataProviderRelease(provider);
     }
    }
    else if([fontDescriptor getStreamForKey:"FontFile2" value:&fontStream]){
     NSData *data=[fontStream data];

     if(data==nil)
      O2PDFError(__FILE__,__LINE__,@"No data on FontFile2 stream");
     else {
      O2DataProviderRef provider=O2DataProviderCreateWithCFData((CFDataRef)data);
      _resourceFont=O2FontCreateWithDataProvider(provider);
            
      O2DataProviderRelease(provider);
     }
    }
    else if([fontDescriptor getStreamForKey:"FontFile3" value:&fontStream]){
     O2PDFDictionary *streamDictionary=[fontStream dictionary];
     NSData          *data=[fontStream data];
     const char      *name;
     
     if(![streamDictionary getNameForKey:"Subtype" value:&name]){
      O2PDFError(__FILE__,__LINE__,@"No Subtype on FontFile3 stream");
     }
          
     if(data==nil)
      O2PDFError(__FILE__,__LINE__,@"No data on FontFile3 stream");
     else {
      O2DataProviderRef provider=O2DataProviderCreateWithCFData((CFDataRef)data);
      _resourceFont=O2FontCreateWithDataProvider(provider);
            
      O2DataProviderRelease(provider);
     }
    }
    
   }

   if([dictionary getArrayForKey:"Widths" value:&widthsArray])
    _pdfCharWidths=[[O2PDFCharWidths alloc] initWithArray:widthsArray firstChar:firstChar lastChar:lastChar missingWidth:missingWidth];
   
   
   if([dictionary getObjectForKey:"Encoding" value:&encoding]){
    if([encoding objectType]==kO2PDFObjectTypeName){
     const char *name=[(O2PDFObject_Name *)encoding name];
     
     textEncoding=textEncodingWithName(name);
    }
    else if([encoding objectType]==kO2PDFObjectTypeDictionary){
     O2PDFDictionary *encodingDictionary=(O2PDFDictionary *)encoding;
     const char *baseEncoding=NULL;
     
     if([encodingDictionary getNameForKey:"BaseEncoding" value:&baseEncoding])
     textEncoding=textEncodingWithName(baseEncoding);
     
     [encodingDictionary getArrayForKey:"Differences" value:&differences];     
    }
    else {
     O2PDFError(__FILE__,__LINE__,@"encoding dictionary is wrong type = %d",[encoding objectType]);
    } 
   }
   
   if(![dictionary getStreamForKey:"ToUnicode" value:&toUnicode]){
   }
   
   if(strcmp(subtype,"Type0")==0){
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   else if(strcmp(subtype,"Type1")==0) {
   
    if(![dictionary getNameForKey:"BaseFont" value:&_baseFont]){
     O2PDFError(__FILE__,__LINE__,@"No BaseFont present");
     return nil;
    }
   }
   else if(strcmp(subtype,"MMType1")==0){
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   else if(strcmp(subtype,"Type3")==0){
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   else if(strcmp(subtype,"TrueType")==0){
       
    if(![dictionary getNameForKey:"BaseFont" value:&_baseFont]){
     O2PDFError(__FILE__,__LINE__,@"TrueType font dictionary does not have BaseFont");
     return nil;
    }
   }
   else if(strcmp(subtype,"CIDFontType0")==0){
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   else if(strcmp(subtype,"CIDFontType2")==0){
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   else {
    O2PDFError(__FILE__,__LINE__,@"Font subtype %s unimplemented",subtype);
   }
   
   if(_resourceFont!=nil)
    _graphicsFont=[_resourceFont retain];
   else
    _graphicsFont=O2FontCreateWithFontName([NSString stringWithCString:_baseFont encoding:NSUTF8StringEncoding]);
    
   _encoding=[_graphicsFont createEncodingForTextEncoding:textEncoding];
   
   if(differences!=NULL){
    O2MutableEncoding *newEncoding=[_encoding mutableCopy];
    int i,count=[differences count];
    O2PDFInteger currentIndex=0;
    
    for(i=0;i<count;i++){
     O2PDFObject *check=[differences objectAtIndex:i];
     const char  *name;
     
     if([check checkForType:kO2PDFObjectTypeInteger value:&currentIndex])
      ;
     else if([check checkForType:kO2PDFObjectTypeName value:&name]){
      NSString *string=[[NSString alloc] initWithCString:name];
      
      O2Glyph glyph=O2FontGetGlyphWithGlyphName(_graphicsFont,(CFStringRef)string);
      uint16_t unicode=O2FontUnicodeForGlyphName((CFStringRef)string);
      
      [newEncoding setGlyph:glyph unicode:unicode atIndex:currentIndex];
      
      currentIndex++;
     }
     else {
      O2PDFError(__FILE__,__LINE__,@"Invalid type (%d) in PDF font differences array",[check objectType]);
     }
    }
    [_encoding release];
    _encoding=newEncoding;
   }

   /* Minkar: a ToUnicode CMap's characters (bfchar and bfrange, one-byte
      codes) are the codes' text, over the encoding's. */
   if(toUnicode!=NULL){
    unichar unicode[256];
    BOOL    mapped=NO;

    [_encoding getUnicode:unicode];
    mapped=O2PDFParseToUnicode([toUnicode data],unicode);
    if(mapped){
     O2Glyph            glyphs[256];
     O2MutableEncoding *newEncoding=[_encoding mutableCopy];

     [_encoding getGlyphs:glyphs];
     for(int code=0;code<256;code++)
      [newEncoding setGlyph:glyphs[code] unicode:unicode[code] atIndex:code];
     [_encoding release];
     _encoding=newEncoding;
    }
   }
   
   return self;
}

-(void)dealloc {
   [_info release];
   [_resourceFont release];
   [_graphicsFont release];
   [_encoding release];
   [_pdfCharWidths release];
   [super dealloc];
}

+(O2PDFFont *)createWithPDFDictionary:(O2PDFDictionary *)info {
   return [[self alloc] initWithPDFDictionary:info];
}

-(O2PDFObject *)realObject {
   return self;
}

-(O2PDFObjectType)objectType {
   return O2PDFObjectTypeCached;
}

-(O2Encoding *)encoding {
   return _encoding;
}

-(O2PDFCharWidths *)pdfCharWidths {
   return _pdfCharWidths;
}

-(O2Font *)graphicsFont {
   return _graphicsFont;
}

@end

