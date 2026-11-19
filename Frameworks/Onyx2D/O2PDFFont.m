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

/* A ToUnicode CMap's codes (one or two bytes) and their text, as a
   dictionary code -> NSString (a code may be several characters: a
   ligature's). */
static NSString *hexText(const char **p,const char *end,int *byteCount){
   const char *s=*p;
   unichar     units[64];
   int         count=0,digits=0;
   unsigned    unit=0;

   while(s<end && *s!='<')
    s++;
   if(s>=end)
    return nil;
   for(s++;s<end && *s!='>';s++){
    int d=(*s>='0'&&*s<='9')?*s-'0':(*s>='a'&&*s<='f')?*s-'a'+10:(*s>='A'&&*s<='F')?*s-'A'+10:-1;

    if(d<0)
     continue;
    unit=(unit<<4)|d;
    if(++digits%4==0 && count<64){
     units[count++]=(unichar)unit;
     unit=0;
    }
   }
   if(digits%4!=0 && count<64)
    units[count++]=(unichar)unit;   // fewer than 4 digits: one unit
   *p=(s<end)?s+1:end;
   if(byteCount!=NULL)
    *byteCount=(digits+1)/2;
   return [NSString stringWithCharacters:units length:count];
}

static NSMutableDictionary *O2PDFParseToUnicodeTable(NSData *data){
   const char          *bytes=[data bytes],*end=bytes+[data length],*p=bytes;
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   while(p<end){
    const char *bfchar=strnstr(p,"beginbfchar",end-p),*bfrange=strnstr(p,"beginbfrange",end-p);
    const char *section=(bfchar && (!bfrange || bfchar<bfrange))?bfchar:bfrange;
    const char *stop;

    if(section==NULL)
     break;
    stop=strnstr(section,(section==bfchar)?"endbfchar":"endbfrange",end-section);
    stop=stop?stop:end;
    p=section+((section==bfchar)?11:12);
    for(;;){
     uint32_t code,high;
     int      codeBytes,highBytes,valueBytes;
     NSString *value;

     if(!nextHex(&p,stop,&code,&codeBytes))
      break;
     if(section==bfchar){
      if((value=hexText(&p,stop,&valueBytes))==nil)
       break;
      [result setObject:value forKey:[NSNumber numberWithUnsignedInt:code]];
      continue;
     }
     if(!nextHex(&p,stop,&high,&highBytes))
      break;
     while(p<stop && (*p==' '||*p=='\n'||*p=='\r'||*p=='\t'))
      p++;
     if(p<stop && *p=='['){
      for(uint32_t c=code;c<=high && (value=hexText(&p,stop,&valueBytes))!=nil;c++)
       [result setObject:value forKey:[NSNumber numberWithUnsignedInt:c]];
      while(p<stop && *p!=']')
       p++;
      if(p<stop)
       p++;
     }
     else {
      if((value=hexText(&p,stop,&valueBytes))==nil || [value length]==0)
       break;
      /* The last character counts up through the range. */
      for(uint32_t c=code;c<=high && c-code<65536;c++){
       NSMutableString *text=[NSMutableString stringWithString:value];
       unichar          last=[value characterAtIndex:[value length]-1]+(unichar)(c-code);

       [text replaceCharactersInRange:NSMakeRange([text length]-1,1) withString:[NSString stringWithCharacters:&last length:1]];
       [result setObject:text forKey:[NSNumber numberWithUnsignedInt:c]];
      }
     }
    }
    p=stop;
   }
   return result;
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

/* Minkar: a composite font (PDF 1.7, 9.7): its descendant CIDFont's
   embedded font (FontFile2, FontFile3) or one named by BaseFont, W and DW,
   CIDToGIDMap; Identity-H and Identity-V codes (other CMaps are read as
   Identity). */
-initCompositeWithDictionary:(O2PDFDictionary *)dictionary toUnicode:(O2PDFStream *)toUnicode {
   O2PDFArray      *descendants,*w;
   O2PDFDictionary *cidFont=nil,*descriptor;
   O2PDFStream     *fontFile=nil,*map;
   const char      *encodingName=NULL,*mapName=NULL,*baseFont=NULL;
   O2PDFReal        dw=1000;

   _composite=YES;
   _cidWidths=[[NSMutableDictionary alloc] init];
   if([dictionary getNameForKey:"Encoding" value:&encodingName])
    _vertical=(strcmp(encodingName,"Identity-V")==0);
   if([dictionary getArrayForKey:"DescendantFonts" value:&descendants])
    [descendants getDictionaryAtIndex:0 value:&cidFont];
   if(![cidFont getNameForKey:"BaseFont" value:&baseFont] && ![dictionary getNameForKey:"BaseFont" value:&baseFont])
    baseFont="Inter";
   _baseFont=baseFont;
   [cidFont getNumberForKey:"DW" value:&dw];
   _defaultWidth=dw/1000.0;
   if([cidFont getArrayForKey:"W" value:&w]){
    unsigned i=0,count=[w count];

    while(i<count){
     O2PDFInteger first,last;
     O2PDFArray  *list;
     O2PDFReal    width;

     if(![w getIntegerAtIndex:i value:&first])
      break;
     if([w getArrayAtIndex:i+1 value:&list]){
      for(unsigned k=0;k<[list count];k++)
       if([list getNumberAtIndex:k value:&width])
        [_cidWidths setObject:[NSNumber numberWithDouble:width/1000.0] forKey:[NSNumber numberWithUnsignedInt:first+k]];
      i+=2;
     }
     else if([w getIntegerAtIndex:i+1 value:&last] && [w getNumberAtIndex:i+2 value:&width]){
      for(O2PDFInteger c=first;c<=last && c-first<65536;c++)
       [_cidWidths setObject:[NSNumber numberWithDouble:width/1000.0] forKey:[NSNumber numberWithUnsignedInt:c]];
      i+=3;
     }
     else
      break;
    }
   }
   if([cidFont getStreamForKey:"CIDToGIDMap" value:&map])
    _cidToGID=[[map data] retain];
   else if([cidFont getNameForKey:"CIDToGIDMap" value:&mapName])
    _cidToGID=nil;      /* Identity */
   if([cidFont getDictionaryForKey:"FontDescriptor" value:&descriptor] &&
      ([descriptor getStreamForKey:"FontFile2" value:&fontFile] || [descriptor getStreamForKey:"FontFile3" value:&fontFile] ||
       [descriptor getStreamForKey:"FontFile" value:&fontFile])){
    NSData *data=[fontFile data];

    if(data!=nil){
     O2DataProviderRef provider=O2DataProviderCreateWithCFData((CFDataRef)data);

     _resourceFont=O2FontCreateWithDataProvider(provider);
     O2DataProviderRelease(provider);
    }
   }
   _embedded=(_resourceFont!=nil);
   _graphicsFont=_embedded?[_resourceFont retain]:O2FontCreateWithFontName([NSString stringWithCString:_baseFont encoding:NSUTF8StringEncoding]);
   if(toUnicode!=nil)
    _toUnicode=[O2PDFParseToUnicodeTable([toUnicode data]) retain];
   return self;
}

-(BOOL)isComposite {
   return _composite;
}

-(unsigned)decodeBytes:(const uint8_t *)bytes length:(unsigned)length codes:(unsigned *)codes glyphs:(O2Glyph *)glyphs widths:(O2Float *)widths {
   unsigned count=0;

   for(unsigned i=0;i+1<length;i+=2,count++){
    unsigned  cid=(bytes[i]<<8)|bytes[i+1];
    NSNumber *width=[_cidWidths objectForKey:[NSNumber numberWithUnsignedInt:cid]];
    O2Glyph   glyph=cid;

    if(_cidToGID!=nil)
     glyph=(2*cid+1<[_cidToGID length])?((((const uint8_t *)[_cidToGID bytes])[2*cid]<<8)|((const uint8_t *)[_cidToGID bytes])[2*cid+1]):0;
    if(!_embedded){
     /* A font named, not embedded: the stand-in's glyph for the code's
        text. */
     NSString *text=[self unicodeForCode:cid];
     unichar   c=([text length]>0)?[text characterAtIndex:0]:0;

     glyph=(c!=0)?[_graphicsFont glyphForCharacter:c]:0;
    }
    codes[count]=cid;
    glyphs[count]=glyph;
    widths[count]=(width!=nil)?[width doubleValue]:_defaultWidth;
   }
   return count;
}

-(NSString *)unicodeForCode:(unsigned)code {
   NSString *text=[_toUnicode objectForKey:[NSNumber numberWithUnsignedInt:code]];

   if(text!=nil || _composite)
    return text;
   if(code<256 && _encoding!=nil){
    uint16_t unicode;
    uint8_t  byte=code;

    O2EncodingGetUnicodeForBytes(_encoding,&unicode,&byte,1);
    return unicode?[NSString stringWithCharacters:&unicode length:1]:nil;
   }
   return nil;
}

static inline O2TextEncoding textEncodingWithName(const char *name){
   if(strcmp(name,"MacRomanEncoding")==0)
    return kO2EncodingMacRoman;
   else if(strcmp(name,"MacExpertEncoding")==0)
    return kO2EncodingMacExpert;
   else if(strcmp(name,"WinAnsiEncoding")==0)
    return kO2EncodingWinAnsi;
   else {
    /* A composite font's CMap (Identity-H...) is read by its own code. */
    if(strncmp(name,"Identity",8)!=0)
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
    /* Minkar: a composite font. */
    return [self initCompositeWithDictionary:dictionary toUnicode:toUnicode];
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
    _toUnicode=[O2PDFParseToUnicodeTable([toUnicode data]) retain];
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
   [_cidWidths release];
   [_cidToGID release];
   [_toUnicode release];
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

