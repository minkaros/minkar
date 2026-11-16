/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSPrintInfo.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSThread.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSPrinter.h>
#include <math.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>

NSString * const NSPrintPrinter=@"NSPrintPrinter";	
NSString * const NSPrintPrinterName=@"NSPrintPrinterName"; 
NSString * const NSPrintJobDisposition=@"NSPrintJobDisposition"; 
NSString * const NSPrintDetailedErrorReporting=@"NSPrintDetailedErrorReporting"; 

NSString * const NSPrintSpoolJob = @"NSPrintSpoolJob";
NSString * const NSPrintPreviewJob = @"NSPrintPreviewJob";
NSString * const NSPrintSaveJob = @"NSPrintSaveJob";
NSString * const NSPrintCancelJob = @"NSPrintCancelJob";

NSString * const NSPrintSavePath = @"NSPrintSavePath";

NSString * const NSPrintCopies=@"NSPrintCopies"; 
NSString * const NSPrintAllPages=@"NSPrintAllPages"; 
NSString * const NSPrintFirstPage=@"NSPrintFirstPage"; 
NSString * const NSPrintLastPage=@"NSPrintLastPage"; 

NSString * const NSPrintPaperName=@"NSPrintPaperName"; 
NSString * const NSPrintPaperSize=@"NSPrintPaperSize"; 
NSString * const NSPrintOrientation=@"NSPrintOrientation"; 

NSString * const NSPrintHorizontalPagination=@"NSPrintHorizontalPagination"; 
NSString * const NSPrintVerticalPagination=@"NSPrintVerticalPagination"; 

NSString * const NSPrintTopMargin=@"NSPrintTopMargin"; 
NSString * const NSPrintBottomMargin=@"NSPrintBottomMargin"; 
NSString * const NSPrintLeftMargin=@"NSPrintLeftMargin"; 
NSString * const NSPrintRightMargin=@"NSPrintRightMargin"; 
NSString * const NSPrintHorizontallyCentered=@"NSPrintHorizontallyCentered"; 
NSString * const NSPrintVerticallyCentered=@"NSPrintVerticallyCentered"; 

NSString * const NSPrintHeaderAndFooter=@"NSPrintHeaderAndFooter";
NSString * const NSPrintJobSavingURL=@"NSPrintJobSavingURL";
NSString * const NSPrintJobSavingFileNameExtensionHidden=@"NSPrintJobSavingFileNameExtensionHidden";
NSString * const NSPrintScalingFactor=@"NSPrintScalingFactor";
NSString * const NSPrintMustCollate=@"NSPrintMustCollate";
NSString * const NSPrintReversePageOrder=@"NSPrintReversePageOrder";
NSString * const NSPrintSelectionOnly=@"NSPrintSelectionOnly";
NSString * const NSPrintPagesAcross=@"NSPrintPagesAcross";
NSString * const NSPrintPagesDown=@"NSPrintPagesDown";
NSString * const NSPrintTime=@"NSPrintTime";
NSString * const NSPrintFaxNumber=@"NSPrintFaxNumber";

@implementation NSPrintInfo

/*
 * Minkar: the print info in step with PrintCore. The PM objects are made
 * when first asked for; asking for one puts the dictionary's values in it
 * (paper, orientation and scale; copies, pages, collation, destination,
 * and printSettings' keys), and updateFromPMPageFormat and
 * updateFromPMPrintSettings bring them back. The defaults are PrintCore's:
 * the session's printer and the locale's paper, with the reference
 * platform's margins (72 points left and right, 90 top and bottom).
 */

static NSPrintInfo *sharedPrintInfo;

+(NSPrintInfo *)sharedPrintInfo {
   @synchronized(self){
    if(sharedPrintInfo==nil)
     sharedPrintInfo=[[NSPrintInfo alloc] init];
    return sharedPrintInfo;
   }
}

+(void)setSharedPrintInfo:(NSPrintInfo *)printInfo {
   @synchronized(self){
    [printInfo retain];
    [sharedPrintInfo release];
    sharedPrintInfo=printInfo;
   }
}

+(NSPrinter *)defaultPrinter {
   PMPrintSession session=NULL;
   PMPrinter      printer=NULL;
   NSPrinter     *result=nil;

   if(PMCreateSession(&session)!=kPMNoError)
    return nil;
   if(PMSessionGetCurrentPrinter(session,&printer)==kPMNoError)
    result=[NSPrinter _printerWithPMPrinter:printer];
   PMRelease(session);
   return result;
}

-initWithDictionary:(NSDictionary *)dictionary {
   _attributes=[[NSMutableDictionary alloc] initWithDictionary:dictionary];
   _printSettingsDictionary=[[NSMutableDictionary alloc] init];
   return self;
}

-init {
   NSMutableDictionary *defaults=[NSMutableDictionary dictionaryWithObjectsAndKeys:
    [NSNumber numberWithInt:1],NSPrintCopies,
    [NSNumber numberWithBool:YES],NSPrintAllPages,
    [NSNumber numberWithInt:1],NSPrintFirstPage,
    [NSNumber numberWithFloat:72],NSPrintLeftMargin,
    [NSNumber numberWithFloat:72],NSPrintRightMargin,
    [NSNumber numberWithFloat:90],NSPrintTopMargin,
    [NSNumber numberWithFloat:90],NSPrintBottomMargin,
    [NSNumber numberWithInt:NSPortraitOrientation],NSPrintOrientation,
    [NSNumber numberWithFloat:1.0],NSPrintScalingFactor,
    [NSNumber numberWithBool:YES],NSPrintMustCollate,
    [NSNumber numberWithInt:NSAutoPagination],NSPrintHorizontalPagination,
    [NSNumber numberWithInt:NSClipPagination],NSPrintVerticalPagination,
    [NSNumber numberWithBool:YES],NSPrintHorizontallyCentered,
    NSPrintSpoolJob,NSPrintJobDisposition,
    nil];

   if([self initWithDictionary:defaults]==nil)
    return nil;
   PMPrintSession session=[self PMPrintSession];
   PMPrinter      printer=NULL;
   if(PMSessionGetCurrentPrinter(session,&printer)==kPMNoError)
    [_attributes setObject:[NSPrinter _printerWithPMPrinter:printer] forKey:NSPrintPrinter];
   PMSessionDefaultPageFormat(session,[self PMPageFormat]);
   [self updateFromPMPageFormat];
   return self;
}

-(void)dealloc {
   PMRelease(_session);
   PMRelease(_pageFormat);
   PMRelease(_printSettings);
   [_printSettingsDictionary release];
   [_attributes release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSPrintInfo *copy=[[NSPrintInfo allocWithZone:zone] initWithDictionary:_attributes];

   [copy->_printSettingsDictionary setDictionary:_printSettingsDictionary];
   return copy;
}

/* The dictionary, without the printer (archived by its ID). */
-(void)encodeWithCoder:(NSCoder *)coder {
   NSMutableDictionary *plain=[[_attributes mutableCopy] autorelease];
   NSPrinter *printer=[self printer];

   [plain removeObjectForKey:NSPrintPrinter];
   if([coder allowsKeyedCoding]){
    [coder encodeObject:plain forKey:@"NSAttributes"];
    [coder encodeObject:printer forKey:@"NSPrinter"];
    [coder encodeObject:_printSettingsDictionary forKey:@"NSPrintSettings"];
   } else {
    [coder encodeObject:plain];
    [coder encodeObject:printer];
    [coder encodeObject:_printSettingsDictionary];
   }
}

-initWithCoder:(NSCoder *)coder {
   BOOL keyed=[coder allowsKeyedCoding];
   NSDictionary *plain=keyed?[coder decodeObjectForKey:@"NSAttributes"]:[coder decodeObject];
   NSPrinter *printer=keyed?[coder decodeObjectForKey:@"NSPrinter"]:[coder decodeObject];
   NSDictionary *settings=keyed?[coder decodeObjectForKey:@"NSPrintSettings"]:[coder decodeObject];

   if([self initWithDictionary:plain?plain:[NSDictionary dictionary]]==nil)
    return nil;
   if(printer!=nil)
    [_attributes setObject:printer forKey:NSPrintPrinter];
   if(settings!=nil)
    [_printSettingsDictionary setDictionary:settings];
   return self;
}

/* ---- PrintCore ---- */

-(void *)PMPrintSession {
   if(_session==NULL)
    PMCreateSession((PMPrintSession *)&_session);
   NSPrinter *printer=[self printer];
   if(printer!=nil)
    PMSessionSetCurrentPMPrinter(_session,[printer _PMPrinter]);
   return _session;
}

/* The paper of this name for the printer, else one of this size. */
static PMPaper copyPaper(PMPrinter printer,NSString *name,NSSize size) {
   CFArrayRef papers=NULL;

   if(printer!=NULL && PMPrinterGetPaperList(printer,&papers)==kPMNoError)
    for(CFIndex i=0;i<CFArrayGetCount(papers);i++){
     PMPaper     paper=(PMPaper)CFArrayGetValueAtIndex(papers,i);
     CFStringRef paperID=NULL;
     double      w=0,h=0;

     PMPaperGetID(paper,&paperID);
     PMPaperGetWidth(paper,&w);
     PMPaperGetHeight(paper,&h);
     if((name!=nil && [(NSString *)paperID isEqualToString:name]) ||
        (name==nil && fabs(w-size.width)<0.5 && fabs(h-size.height)<0.5)){
      PMRetain(paper);
      return paper;
     }
    }
   PMPaper custom=NULL;
   PMPaperMargins none={ 0,0,0,0 };
   if(size.width>0 && size.height>0)
    PMPaperCreateCustom(printer,(CFStringRef)(name?name:@"custom"),(CFStringRef)(name?name:@"Custom"),
                        size.width,size.height,&none,&custom);
   return custom;
}

-(void *)PMPageFormat {
   if(_pageFormat==NULL)
    PMCreatePageFormat((PMPageFormat *)&_pageFormat);
   PMPrinter printer=[[self printer] _PMPrinter];
   NSString *name=[_attributes objectForKey:NSPrintPaperName];
   NSValue  *size=[_attributes objectForKey:NSPrintPaperSize];
   PMPaper   paper=NULL;

   if(name!=nil || size!=nil){
    paper=copyPaper(printer,name,[size sizeValue]);
    if(paper==NULL && name!=nil)
     paper=copyPaper(printer,nil,[size sizeValue]);
   }
   if(paper!=NULL){
    PMPageFormat format=NULL;
    if(PMCreatePageFormatWithPMPaper(&format,paper)==kPMNoError){
     PMCopyPageFormat(format,_pageFormat);
     PMRelease(format);
    }
    PMRelease(paper);
   }
   PMSetOrientation(_pageFormat,[self orientation]==NSLandscapeOrientation?kPMLandscape:kPMPortrait,false);
   PMSetScale(_pageFormat,[self scalingFactor]*100);
   return _pageFormat;
}

-(void *)PMPrintSettings {
   if(_printSettings==NULL){
    PMCreatePrintSettings((PMPrintSettings *)&_printSettings);
    PMSessionDefaultPrintSettings([self PMPrintSession],_printSettings);
   }
   PMPrintSettings settings=_printSettings;
   NSNumber *copies=[_attributes objectForKey:NSPrintCopies];

   PMSetCopies(settings,copies!=nil?[copies unsignedIntValue]:1,false);
   if([[_attributes objectForKey:NSPrintAllPages] boolValue] || [_attributes objectForKey:NSPrintFirstPage]==nil){
    PMSetFirstPage(settings,1,false);
    PMSetLastPage(settings,kPMPrintAllPages,false);
   } else {
    NSNumber *last=[_attributes objectForKey:NSPrintLastPage];
    PMSetFirstPage(settings,[[_attributes objectForKey:NSPrintFirstPage] unsignedIntValue],false);
    PMSetLastPage(settings,last!=nil?[last unsignedIntValue]:kPMPrintAllPages,false);
   }
   NSNumber *collate=[_attributes objectForKey:NSPrintMustCollate];
   PMSetCollate(settings,collate==nil || [collate boolValue]);

   /* The disposition as the session's destination. */
   NSString *disposition=[self jobDisposition];
   NSURL    *url=[_attributes objectForKey:NSPrintJobSavingURL];
   NSString *path=[_attributes objectForKey:NSPrintSavePath];
   if(url==nil && path!=nil)
    url=[NSURL fileURLWithPath:path];
   PMDestinationType type=kPMDestinationPrinter;
   if([disposition isEqualToString:NSPrintSaveJob])
    type=kPMDestinationFile;
   else if([disposition isEqualToString:NSPrintPreviewJob])
    type=kPMDestinationPreview;
   PMSessionSetDestination([self PMPrintSession],settings,type,kPMDocumentFormatPDF,
                           type==kPMDestinationFile?(CFURLRef)url:NULL);

   for(NSString *key in _printSettingsDictionary)
    PMPrintSettingsSetValue(settings,(CFStringRef)key,(CFTypeRef)[_printSettingsDictionary objectForKey:key],false);
   return settings;
}

-(void)updateFromPMPageFormat {
   PMPageFormat  format=_pageFormat;
   PMPaper       paper=NULL;
   CFStringRef   paperID=NULL;
   double        w=0,h=0,scale=100;
   PMOrientation orientation=kPMPortrait;
   PMRect        page,paperRect;

   if(format==NULL || PMGetPageFormatPaper(format,&paper)!=kPMNoError)
    return;
   PMPaperGetID(paper,&paperID);
   PMPaperGetWidth(paper,&w);
   PMPaperGetHeight(paper,&h);
   PMGetOrientation(format,&orientation);
   PMGetScale(format,&scale);
   [_attributes setObject:[[(NSString *)paperID copy] autorelease] forKey:NSPrintPaperName];
   [_attributes setObject:[NSValue valueWithSize:NSMakeSize(w,h)] forKey:NSPrintPaperSize];
   [_attributes setObject:[NSNumber numberWithInt:(orientation==kPMLandscape || orientation==kPMReverseLandscape)?
                                                  NSLandscapeOrientation:NSPortraitOrientation]
                   forKey:NSPrintOrientation];
   [_attributes setObject:[NSNumber numberWithFloat:scale/100] forKey:NSPrintScalingFactor];
   /* The imageable area in the paper's portrait space, from its bottom left. */
   PMGetUnadjustedPageRect(format,&page);
   PMGetUnadjustedPaperRect(format,&paperRect);
   NSRect imageable=NSMakeRect(-paperRect.left,paperRect.bottom-page.bottom,page.right-page.left,page.bottom-page.top);
   [_attributes setObject:[NSValue valueWithRect:imageable] forKey:@"_imageableRect"];
}

-(void)updateFromPMPrintSettings {
   PMPrintSettings settings=_printSettings;
   UInt32  copies=1,first=1,last=kPMPrintAllPages;
   Boolean collate=true;

   if(settings==NULL)
    return;
   PMGetCopies(settings,&copies);
   PMGetFirstPage(settings,&first);
   PMGetLastPage(settings,&last);
   PMGetCollate(settings,&collate);
   [_attributes setObject:[NSNumber numberWithUnsignedInt:copies] forKey:NSPrintCopies];
   [_attributes setObject:[NSNumber numberWithBool:first<=1 && last==kPMPrintAllPages] forKey:NSPrintAllPages];
   [_attributes setObject:[NSNumber numberWithUnsignedInt:first] forKey:NSPrintFirstPage];
   if(last!=kPMPrintAllPages)
    [_attributes setObject:[NSNumber numberWithUnsignedInt:last] forKey:NSPrintLastPage];
   else
    [_attributes removeObjectForKey:NSPrintLastPage];
   [_attributes setObject:[NSNumber numberWithBool:collate] forKey:NSPrintMustCollate];

   PMDestinationType type=kPMDestinationPrinter;
   PMSessionGetDestinationType([self PMPrintSession],settings,&type);
   if(type==kPMDestinationFile){
    CFURLRef url=NULL;
    [_attributes setObject:NSPrintSaveJob forKey:NSPrintJobDisposition];
    PMSessionCopyDestinationLocation(_session,settings,&url);
    if(url!=NULL){
     [_attributes setObject:(NSURL *)url forKey:NSPrintJobSavingURL];
     CFRelease(url);
    }
   } else
    [_attributes setObject:type==kPMDestinationPreview?NSPrintPreviewJob:NSPrintSpoolJob forKey:NSPrintJobDisposition];

   CFDictionaryRef all=NULL;
   if(PMPrintSettingsCopyAsDictionary(settings,&all)==kPMNoError && all!=NULL){
    [_printSettingsDictionary setDictionary:(NSDictionary *)all];
    CFRelease(all);
   }
}

-(NSMutableDictionary *)printSettings {
   return _printSettingsDictionary;
}

-(CGFloat)scalingFactor {
   NSNumber *v=[_attributes objectForKey:NSPrintScalingFactor];
   return v!=nil?[v floatValue]:1.0;
}

-(void)setScalingFactor:(CGFloat)value {
   [_attributes setObject:[NSNumber numberWithFloat:value] forKey:NSPrintScalingFactor];
}

-(BOOL)isSelectionOnly {
   return [[_attributes objectForKey:NSPrintSelectionOnly] boolValue];
}

-(void)setSelectionOnly:(BOOL)value {
   [_attributes setObject:[NSNumber numberWithBool:value] forKey:NSPrintSelectionOnly];
}

-(NSMutableDictionary *)dictionary {
   return _attributes;
}

-(NSPrinter *)printer {
   return [_attributes objectForKey:NSPrintPrinter];
}

-(NSString *)jobDisposition {
   return [_attributes objectForKey:NSPrintJobDisposition];
}

-(NSString *)paperName {
   return [_attributes objectForKey:NSPrintPaperName];
}

-(NSSize)paperSize {
   return [[_attributes objectForKey:NSPrintPaperSize] sizeValue];
}

-(NSPrintingOrientation)orientation {
   return [[_attributes objectForKey:NSPrintOrientation] intValue];
}

-(NSPrintingPaginationMode)horizontalPagination {
   return [[_attributes objectForKey:NSPrintHorizontalPagination] intValue];
}

-(NSPrintingPaginationMode)verticalPagination {
   return [[_attributes objectForKey:NSPrintVerticalPagination] intValue];
}

-(CGFloat)topMargin {
   return [[_attributes objectForKey:NSPrintTopMargin] floatValue];
}

-(CGFloat)bottomMargin {
   return [[_attributes objectForKey:NSPrintBottomMargin] floatValue];
}

-(CGFloat)leftMargin {
   return [[_attributes objectForKey:NSPrintLeftMargin] floatValue];
}

-(CGFloat)rightMargin {
   return [[_attributes objectForKey:NSPrintRightMargin] floatValue];
}

-(BOOL)isHorizontallyCentered {
   return [[_attributes objectForKey:NSPrintHorizontallyCentered] boolValue];
}

-(BOOL)isVerticallyCentered {
   return [[_attributes objectForKey:NSPrintVerticallyCentered] boolValue];
}

-(NSString *)localizedPaperName {
   PMPageFormat format=[self PMPageFormat];
   PMPaper      paper=NULL;
   CFStringRef  name=NULL;

   if(PMGetPageFormatPaper(format,&paper)==kPMNoError &&
      PMPaperCreateLocalizedName(paper,[[self printer] _PMPrinter],&name)==kPMNoError && name!=NULL)
    return [(NSString *)name autorelease];
   return [self paperName];
}

-(NSRect)imageablePageBounds {
   NSRect       result;
   NSValue     *imageable=[_attributes objectForKey:@"_imageableRect"];
   
   if(imageable!=nil)
    result=[imageable rectValue];
   else {
    result.origin.x=0;
    result.origin.y=0;
    result.size=[self paperSize];
   }
   
   if([self orientation]==NSLandscapeOrientation){
    NSRect portrait=result;
    
    result.origin.x=portrait.origin.y;
    result.size.width=portrait.size.height;
    result.origin.y=portrait.origin.x;
    result.size.height=portrait.size.width;
   }
   
   return result;
}

-(void)setPrinter:(NSPrinter *)printer {
   if(printer==nil)
    [_attributes removeObjectForKey:NSPrintPrinter];
   else
    [_attributes setObject:printer forKey:NSPrintPrinter];
}

-(void)setJobDisposition:(NSString *)value {
   [_attributes setObject:value forKey:NSPrintJobDisposition];
}

-(void)setPaperName:(NSString *)value {
   [_attributes setObject:value forKey:NSPrintPaperName];
}

-(void)setPaperSize:(NSSize)value {
   [_attributes setObject:[NSValue valueWithSize:value] forKey:NSPrintPaperSize];
}

-(void)setOrientation:(NSPrintingOrientation)value {
   [_attributes setObject:[NSNumber numberWithInt:value] forKey:NSPrintOrientation];
}

-(void)setHorizontalPagination:(NSPrintingPaginationMode)value {
   [_attributes setObject:[NSNumber numberWithInt:value] forKey:NSPrintHorizontalPagination];
}

-(void)setVerticalPagination:(NSPrintingPaginationMode)value {
   [_attributes setObject:[NSNumber numberWithInt:value] forKey:NSPrintVerticalPagination];
}

-(void)setTopMargin:(CGFloat)value {
   [_attributes setObject:[NSNumber numberWithFloat:value] forKey:NSPrintTopMargin];
}

-(void)setBottomMargin:(CGFloat)value {
   [_attributes setObject:[NSNumber numberWithFloat:value] forKey:NSPrintBottomMargin];
}

-(void)setLeftMargin:(CGFloat)value {
   [_attributes setObject:[NSNumber numberWithFloat:value] forKey:NSPrintLeftMargin];
}

-(void)setRightMargin:(CGFloat)value {
   [_attributes setObject:[NSNumber numberWithFloat:value] forKey:NSPrintRightMargin];
}

-(void)setHorizontallyCentered:(BOOL)value {
   [_attributes setObject:[NSNumber numberWithBool:value] forKey:NSPrintHorizontallyCentered];
}

-(void)setVerticallyCentered:(BOOL)value {
   [_attributes setObject:[NSNumber numberWithBool:value] forKey:NSPrintVerticallyCentered];
}

-(void)setUpPrintOperationDefaultValues {
   // do nothing ?
}

@end
