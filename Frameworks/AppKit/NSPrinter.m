/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/*
 * NSPrinter over PrintCore. The description table ("PPD") is the
 * printer's PPD as CUPS gives it, read with libcups's PPD parser: a key is
 * a PPD keyword, its value the default choice (or the attribute's value);
 * lists are an option's choices.
 */

#import <AppKit/NSPrinter.h>
#import <AppKit/NSGraphics.h>
#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#include <cups/ppd.h>

@implementation NSPrinter

static NSArray *allPrinters(void) {
   CFArrayRef      list=NULL;
   NSMutableArray *result=[NSMutableArray array];

   if(PMServerCreatePrinterList(kPMServerLocal,&list)!=kPMNoError || list==NULL)
    return result;
   for(CFIndex i=0;i<CFArrayGetCount(list);i++)
    [result addObject:[NSPrinter _printerWithPMPrinter:(void *)CFArrayGetValueAtIndex(list,i)]];
   CFRelease(list);
   return result;
}

+(NSArray *)printerNames {
   NSMutableArray *names=[NSMutableArray array];

   for(NSPrinter *p in allPrinters())
    [names addObject:[p name]];
   return names;
}

+(NSArray *)printerTypes {
   NSMutableArray *types=[NSMutableArray array];

   for(NSPrinter *p in allPrinters())
    if(![types containsObject:[p type]])
     [types addObject:[p type]];
   return types;
}

/* By its name, or by its queue's (the printer's ID). */
+(NSPrinter *)printerWithName:(NSString *)name {
   if(name==nil)
    return nil;
   for(NSPrinter *p in allPrinters())
    if([[p name] isEqualToString:name] || [(NSString *)PMPrinterGetID((PMPrinter)[p _PMPrinter]) isEqualToString:name])
     return p;
   return nil;
}

+(NSPrinter *)printerWithType:(NSString *)type {
   for(NSPrinter *p in allPrinters())
    if([[p type] isEqualToString:type])
     return p;
   return nil;
}

+(NSPrinter *)_printerWithPMPrinter:(void *)printer {
   if(printer==NULL)
    return nil;
   NSPrinter *p=[[[self alloc] init] autorelease];
   PMRetain(printer);
   p->_printer=printer;
   return p;
}

-(void *)_PMPrinter {
   return _printer;
}

-(void)dealloc {
   PMRelease(_printer);
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-(BOOL)isEqual:(id)other {
   return [other isKindOfClass:[NSPrinter class]] &&
          [(NSString *)PMPrinterGetID(_printer) isEqualToString:(NSString *)PMPrinterGetID(((NSPrinter *)other)->_printer)];
}

-(NSUInteger)hash {
   return [(NSString *)PMPrinterGetID(_printer) hash];
}

/* Archived by its ID, found again by it. */
-(void)encodeWithCoder:(NSCoder *)coder {
   NSString *printerID=(NSString *)PMPrinterGetID(_printer);
   if([coder allowsKeyedCoding])
    [coder encodeObject:printerID forKey:@"NSPrinterID"];
   else
    [coder encodeObject:printerID];
}

-initWithCoder:(NSCoder *)coder {
   NSString *printerID=[coder allowsKeyedCoding]?[coder decodeObjectForKey:@"NSPrinterID"]:[coder decodeObject];
   PMPrinter printer=printerID!=nil?PMPrinterCreateFromPrinterID((CFStringRef)printerID):NULL;

   if(printer==NULL)
    PMCreateGenericPrinter(&printer);
   _printer=printer;
   return self;
}

-(NSString *)name {
   return [[(NSString *)PMPrinterGetName(_printer) copy] autorelease];
}

-(NSString *)type {
   CFStringRef type=NULL;
   PMPrinterGetMakeAndModelName(_printer,&type);
   return type!=NULL?[[(NSString *)type copy] autorelease]:@"";
}

-(NSInteger)languageLevel {
   return PMPrinterIsPostScriptCapable(_printer)?3:0;
}

-(NSDictionary *)deviceDescription {
   PMResolution res={ 72,72 };
   PMPrinterGetOutputResolution(_printer,NULL,&res);
   return [NSDictionary dictionaryWithObjectsAndKeys:
    @"YES",NSDeviceIsPrinter,
    [NSValue valueWithSize:NSMakeSize(res.hRes,res.vRes)],NSDeviceResolution,nil];
}

-(NSSize)pageSizeForPaper:(NSString *)paperName {
   CFArrayRef papers=NULL;

   if(paperName==nil || PMPrinterGetPaperList(_printer,&papers)!=kPMNoError)
    return NSZeroSize;
   for(CFIndex i=0;i<CFArrayGetCount(papers);i++){
    PMPaper     paper=(PMPaper)CFArrayGetValueAtIndex(papers,i);
    CFStringRef paperID=NULL,localized=NULL;
    double      w=0,h=0;
    BOOL        same;

    PMPaperGetID(paper,&paperID);
    PMPaperCreateLocalizedName(paper,_printer,&localized);
    same=[(NSString *)paperID isEqualToString:paperName] || [(NSString *)localized isEqualToString:paperName];
    if(localized!=NULL)
     CFRelease(localized);
    if(same){
     PMPaperGetWidth(paper,&w);
     PMPaperGetHeight(paper,&h);
     return NSMakeSize(w,h);
    }
   }
   return NSZeroSize;
}

/* ---- The description table ---- */

static ppd_file_t *openPPD(void *printer) {
   CFURLRef url=NULL;
   char     path[1024];

   if(PMPrinterCopyDescriptionURL(printer,CFSTR("PPD"),&url)!=kPMNoError || url==NULL)
    return NULL;
   BOOL ok=CFURLGetFileSystemRepresentation(url,true,(UInt8 *)path,sizeof(path));
   CFRelease(url);
   if(!ok)
    return NULL;
   ppd_file_t *ppd=ppdOpenFile(path);
   if(ppd!=NULL)
    ppdMarkDefaults(ppd);
   return ppd;
}

-(NSPrinterTableStatus)statusForTable:(NSString *)tableName {
   if(![tableName isEqualToString:@"PPD"])
    return NSPrinterTableNotFound;
   ppd_file_t *ppd=openPPD(_printer);
   if(ppd==NULL)
    return NSPrinterTableNotFound;
   ppdClose(ppd);
   return NSPrinterTableOK;
}

/* A key's value: the option's chosen default, else the attribute's. */
static NSString *ppdValue(ppd_file_t *ppd,NSString *key) {
   const char *k=[key UTF8String];
   ppd_option_t *option=ppdFindOption(ppd,k);

   if(option!=NULL){
    ppd_choice_t *choice=ppdFindMarkedChoice(ppd,k);
    return [NSString stringWithUTF8String:choice!=NULL?choice->choice:option->defchoice];
   }
   ppd_attr_t *attr=ppdFindAttr(ppd,k,NULL);
   if(attr!=NULL && attr->value!=NULL)
    return [NSString stringWithUTF8String:attr->value];
   return nil;
}

-(NSString *)stringForKey:(NSString *)key inTable:(NSString *)table {
   if(![table isEqualToString:@"PPD"])
    return nil;
   ppd_file_t *ppd=openPPD(_printer);
   if(ppd==NULL)
    return nil;
   NSString *value=ppdValue(ppd,key);
   ppdClose(ppd);
   return value;
}

-(BOOL)isKey:(NSString *)key inTable:(NSString *)table {
   return [self stringForKey:key inTable:table]!=nil;
}

-(NSArray *)stringListForKey:(NSString *)key inTable:(NSString *)table {
   if(![table isEqualToString:@"PPD"])
    return nil;
   ppd_file_t *ppd=openPPD(_printer);
   if(ppd==NULL)
    return nil;
   NSMutableArray *list=[NSMutableArray array];
   ppd_option_t   *option=ppdFindOption(ppd,[key UTF8String]);
   if(option!=NULL)
    for(int i=0;i<option->num_choices;i++)
     [list addObject:[NSString stringWithUTF8String:option->choices[i].choice]];
   else
    for(ppd_attr_t *a=ppdFindAttr(ppd,[key UTF8String],NULL);a!=NULL;a=ppdFindNextAttr(ppd,[key UTF8String],NULL))
     if(a->value!=NULL)
      [list addObject:[NSString stringWithUTF8String:a->value]];
   ppdClose(ppd);
   return list;
}

-(BOOL)booleanForKey:(NSString *)key inTable:(NSString *)table {
   NSString *v=[self stringForKey:key inTable:table];
   return [v caseInsensitiveCompare:@"True"]==NSOrderedSame || [v isEqualToString:@"1"];
}

-(float)floatForKey:(NSString *)key inTable:(NSString *)table {
   return [[self stringForKey:key inTable:table] floatValue];
}

-(int)intForKey:(NSString *)key inTable:(NSString *)table {
   return [[self stringForKey:key inTable:table] intValue];
}

/* "x y w h" or "llx lly urx ury" (PPD's areas): as numbers in order. */
static void numbers(NSString *s,double *out,int count) {
   NSScanner *scanner=[NSScanner scannerWithString:s?s:@""];
   for(int i=0;i<count;i++)
    if(![scanner scanDouble:&out[i]])
     out[i]=0;
}

-(NSRect)rectForKey:(NSString *)key inTable:(NSString *)table {
   double v[4];
   numbers([self stringForKey:key inTable:table],v,4);
   return NSMakeRect(v[0],v[1],v[2]-v[0],v[3]-v[1]);
}

-(NSSize)sizeForKey:(NSString *)key inTable:(NSString *)table {
   double v[2];
   numbers([self stringForKey:key inTable:table],v,2);
   return NSMakeSize(v[0],v[1]);
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %p %@>",[self class],self,[self name]];
}

@end
