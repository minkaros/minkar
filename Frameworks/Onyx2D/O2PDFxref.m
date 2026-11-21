/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Onyx2D/O2PDFxref.h>
#import <Onyx2D/O2PDFxrefEntry.h>
#import <Onyx2D/O2PDFObject_const.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFContext.h>
#import <Onyx2D/O2PDFScanner.h>
#import <Onyx2D/O2PDFStream.h>
#import <Foundation/NSData.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Onyx2D/O2Exceptions.h>

@implementation O2PDFxref

-initWithData:(NSData *)data {
   _data=[data retain];
   _previous=nil;
   _numberToEntries=NSCreateMapTable(NSIntegerMapKeyCallBacks,NSNonRetainedObjectMapValueCallBacks,0);
   _entryToObject=NSCreateMapTable(NSNonRetainedObjectMapKeyCallBacks,NSObjectMapValueCallBacks,0);
   _entriesInOrder=[NSMutableArray new];
   _trailer=nil;   
   return self;
}

-(void)dealloc {
   [_data release];
   [_previous release];
   NSFreeMapTable(_numberToEntries);
   NSFreeMapTable(_entryToObject);
   [_entriesInOrder release];
   [_trailer release];
   [super dealloc];
}

-(BOOL)isByReference {
   return NO;
}

-(NSData *)data {
   return _data;
}

-(O2PDFxref *)previous {
   return _previous;
}

-(NSArray *)allEntries {
   NSMutableArray *result=[NSMutableArray array];
   NSMapEnumerator state=NSEnumerateMapTable(_numberToEntries);
   int             key;
   id              value;
   
   while(NSNextMapEnumeratorPair(&state,(void **)&key,(void **)&value)){
    if([value isKindOfClass:[NSArray class]])
     [result addObjectsFromArray:value];
    else
     [result addObject:value];
   }
   
   return result;
}


/* Minkar: the index-th object of an object stream (PDF 1.5, /Type
   /ObjStm): its decoded data starts with /N pairs of object number and
   offset, the offsets counted from /First. */
static O2PDFObject *objectInObjectStream(O2PDFxref *xref,O2PDFInteger streamNumber,O2PDFInteger index) {
   O2PDFStream     *stream;
   O2PDFInteger     count,first,offset=-1,position=0;
   O2PDFObject     *object=nil;

   if(![[xref objectAtNumber:streamNumber generation:0] checkForType:kO2PDFObjectTypeStream value:&stream])
    return [O2PDFObject_const pdfObjectWithNull];
   if(![[stream dictionary] getIntegerForKey:"N" value:&count] ||
      ![[stream dictionary] getIntegerForKey:"First" value:&first] || index<0 || index>=count)
    return [O2PDFObject_const pdfObjectWithNull];

   NSData     *data=[stream data];
   const char *bytes=[data bytes];
   unsigned    length=[data length];

   for(O2PDFInteger i=0;i<=index;i++){
    O2PDFInteger number;

    if(!O2PDFScanInteger(bytes,length,position,&position,&number) ||
       !O2PDFScanInteger(bytes,length,position,&position,&offset))
     return [O2PDFObject_const pdfObjectWithNull];
   }
   if(offset<0 || first+offset>=length ||
      !O2PDFParseObject(bytes,length,first+offset,&position,&object,xref) || object==nil)
    return [O2PDFObject_const pdfObjectWithNull];

   return object;
}

-(O2PDFxrefEntry *)entryWithNumber:(O2PDFInteger)number generation:(O2PDFInteger)generation {
   void *key=(void *)number;
   id    check=NSMapGet(_numberToEntries,key);
      
   if(check==nil)
    return [_previous entryWithNumber:number generation:generation];
    
   if([check isKindOfClass:[NSArray class]]){
    NSArray *array=check;
    int      i,count=[check count];
    
    for(i=0;i<count;i++){
     O2PDFxrefEntry *entry=[array objectAtIndex:i];
     
     if([entry generation]==generation)
      return entry;
    }
   }
   
   return check;
}

-(O2PDFObject *)objectAtNumber:(O2PDFInteger)number generation:(O2PDFInteger)generation {
   O2PDFxrefEntry *lookup=[self entryWithNumber:number generation:generation];
   
   if(lookup==nil)
    return [O2PDFObject_const pdfObjectWithNull];
   else {
    O2PDFObject *result=NSMapGet(_entryToObject,lookup);

    if(result==nil){
     if([lookup objectStream]!=0)
      result=objectInObjectStream(self,[lookup objectStream],[lookup objectStreamIndex]);
     else if(!O2PDFParseIndirectObject(_data,[lookup position],&result,number,generation,self))
      result=[O2PDFObject_const pdfObjectWithNull];
      
     NSMapInsert(_entryToObject,lookup,result);
    }
    
    return result;
   }
}

-(O2PDFDictionary *)trailer {
   return _trailer;
}

-(void)setPreviousTable:(O2PDFxref *)table {
   [_previous autorelease];
   _previous=[table retain];
}

-(void)addEntry:(O2PDFxrefEntry *)entry {
   void *key=(void *)[entry number];
   id    check=NSMapGet(_numberToEntries,key);
   
   [_entriesInOrder addObject:entry];
   
   if(check==nil)
    NSMapInsert(_numberToEntries,key,entry);
   if([check isKindOfClass:[NSMutableArray class]])
    [check addObject:entry];
   else if([check isKindOfClass:[O2PDFxrefEntry class]])
    NSMapInsert(_numberToEntries,key,[NSMutableArray arrayWithObject:entry]);
}

-(void)addEntry:(O2PDFxrefEntry *)entry object:(O2PDFObject *)object {
   [self addEntry:entry];
   NSMapInsert(_entryToObject,entry,object);
}

-(void)setTrailer:(O2PDFDictionary *)trailer {
   [_trailer autorelease];
   _trailer=[trailer retain];
}

-(void)encodeWithPDFContext:(O2PDFContext *)encoder {
   unsigned startxref=[encoder length];
   int      i,count=[_entriesInOrder count];
   
   [encoder appendCString:"xref\n"];
   [encoder appendFormat:@"%d %d\n",1,count];
   for(i=0;i<count;i++){
    O2PDFxrefEntry *entry=[_entriesInOrder objectAtIndex:i];
    
    [encoder appendFormat:@"%010d %05d n \n",[entry position],[entry generation]];
   }
   [_trailer setIntegerForKey:"Size" value:[[_entriesInOrder lastObject] number]+1];
   [encoder appendCString:"trailer\n"];
   [encoder encodePDFObject:_trailer];
   [encoder appendCString:"startxref\n"];
   [encoder appendFormat:@"%d\n",startxref];
   [encoder appendCString:"%%EOF\n"];
}

@end
