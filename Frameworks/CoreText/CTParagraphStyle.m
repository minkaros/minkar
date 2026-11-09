/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Paragraph styles keep each setting's value, or its default; text tabs
 * an alignment, a location and options.
 */

#import <CoreText/CTParagraphStyle.h>
#import "CTTypes.h"
#import <Foundation/Foundation.h>

const CFStringRef kCTTabColumnTerminatorsAttributeName=CFSTR("NSTabColumnTerminatorsAttributeName");

@interface CTParagraphStyle : NSObject {
@public
   CTTextAlignment    _alignment;
   CTLineBreakMode    _lineBreakMode;
   CTWritingDirection _direction;
   CGFloat            _values[kCTParagraphStyleSpecifierCount];
   CFArrayRef         _tabStops;
   CFOptionFlags      _lineBoundsOptions;
}
@end

@interface CTTextTab : NSObject {
@public
   CTTextAlignment _alignment;
   double          _location;
   CFDictionaryRef _options;
}
@end

CT_DEFINE_TYPE(CTParagraphStyle, "CTParagraphStyle")
CT_DEFINE_TYPE(CTTextTab, "CTTextTab")

@implementation CTTextTab
-(void)dealloc {
   if(_options!=NULL)
    CFRelease(_options);
   [super dealloc];
}
@end

CTTextTabRef CTTextTabCreate(CTTextAlignment alignment, double location, CFDictionaryRef options) {
   CTTextTab *tab=[[CTTextTab alloc] init];
   tab->_alignment=alignment;
   tab->_location=location;
   tab->_options=(options!=NULL)?CFDictionaryCreateCopy(NULL,options):NULL;
   return (CTTextTabRef)tab;
}

CTTextAlignment CTTextTabGetAlignment(CTTextTabRef tab) { return ((CTTextTab *)tab)->_alignment; }
double CTTextTabGetLocation(CTTextTabRef tab) { return ((CTTextTab *)tab)->_location; }
CFDictionaryRef CTTextTabGetOptions(CTTextTabRef tab) { return ((CTTextTab *)tab)->_options; }

/* Twelve left tabs, 28 points apart, as the default. */
static CFArrayRef defaultTabStops(void) {
   static CFArrayRef stops;
   if(stops==NULL){
    CFMutableArrayRef array=CFArrayCreateMutable(NULL,12,&kCFTypeArrayCallBacks);
    for(int i=1;i<=12;i++){
     CTTextTabRef tab=CTTextTabCreate(kCTTextAlignmentLeft,28.0*i,NULL);
     CFArrayAppendValue(array,tab);
     CFRelease(tab);
    }
    stops=array;
   }
   return stops;
}

@implementation CTParagraphStyle

-init {
   self=[super init];
   if(self!=nil){
    _alignment=kCTTextAlignmentNatural;
    _lineBreakMode=kCTLineBreakByWordWrapping;
    _direction=kCTWritingDirectionNatural;
    _tabStops=CFRetain(defaultTabStops());
   }
   return self;
}

-(void)dealloc {
   if(_tabStops!=NULL)
    CFRelease(_tabStops);
   [super dealloc];
}

@end

static BOOL isFloatSetting(CTParagraphStyleSpecifier spec) {
   switch(spec){
    case kCTParagraphStyleSpecifierAlignment:
    case kCTParagraphStyleSpecifierTabStops:
    case kCTParagraphStyleSpecifierLineBreakMode:
    case kCTParagraphStyleSpecifierBaseWritingDirection:
    case kCTParagraphStyleSpecifierLineBoundsOptions:
     return NO;
    default:
     return spec<kCTParagraphStyleSpecifierCount;
   }
}

CTParagraphStyleRef CTParagraphStyleCreate(const CTParagraphStyleSetting *settings, size_t count) {
   CTParagraphStyle *style=[[CTParagraphStyle alloc] init];

   for(size_t i=0;i<count;i++){
    const CTParagraphStyleSetting *s=&settings[i];
    if(s->value==NULL)
     continue;
    switch(s->spec){
     case kCTParagraphStyleSpecifierAlignment:
      if(s->valueSize==sizeof(CTTextAlignment)) style->_alignment=*(const CTTextAlignment *)s->value;
      break;
     case kCTParagraphStyleSpecifierLineBreakMode:
      if(s->valueSize==sizeof(CTLineBreakMode)) style->_lineBreakMode=*(const CTLineBreakMode *)s->value;
      break;
     case kCTParagraphStyleSpecifierBaseWritingDirection:
      if(s->valueSize==sizeof(CTWritingDirection)) style->_direction=*(const CTWritingDirection *)s->value;
      break;
     case kCTParagraphStyleSpecifierTabStops:
      if(s->valueSize==sizeof(CFArrayRef)){
       CFArrayRef stops=*(const CFArrayRef *)s->value;
       if(style->_tabStops!=NULL)
        CFRelease(style->_tabStops);
       style->_tabStops=(stops!=NULL)?CFArrayCreateCopy(NULL,stops):NULL;
      }
      break;
     case kCTParagraphStyleSpecifierLineBoundsOptions:
      if(s->valueSize==sizeof(CFOptionFlags)) style->_lineBoundsOptions=*(const CFOptionFlags *)s->value;
      break;
     default:
      if(isFloatSetting(s->spec) && s->valueSize==sizeof(CGFloat))
       style->_values[s->spec]=*(const CGFloat *)s->value;
      break;
    }
   }
   return (CTParagraphStyleRef)style;
}

CTParagraphStyleRef CTParagraphStyleCreateCopy(CTParagraphStyleRef paragraphStyle) {
   CTParagraphStyle *from=(CTParagraphStyle *)paragraphStyle;
   CTParagraphStyle *copy=[[CTParagraphStyle alloc] init];

   if(from==nil)
    return (CTParagraphStyleRef)copy;
   copy->_alignment=from->_alignment;
   copy->_lineBreakMode=from->_lineBreakMode;
   copy->_direction=from->_direction;
   memcpy(copy->_values,from->_values,sizeof(copy->_values));
   if(copy->_tabStops!=NULL)
    CFRelease(copy->_tabStops);
   copy->_tabStops=(from->_tabStops!=NULL)?CFRetain(from->_tabStops):NULL;
   copy->_lineBoundsOptions=from->_lineBoundsOptions;
   return (CTParagraphStyleRef)copy;
}

bool CTParagraphStyleGetValueForSpecifier(CTParagraphStyleRef paragraphStyle, CTParagraphStyleSpecifier spec, size_t size, void *buffer) {
   CTParagraphStyle *style=(CTParagraphStyle *)paragraphStyle;
   CTParagraphStyle *defaults=nil;

   if(style==nil)
    style=defaults=[[[CTParagraphStyle alloc] init] autorelease];
   if(buffer==NULL)
    return false;
   switch(spec){
    case kCTParagraphStyleSpecifierAlignment:
     if(size!=sizeof(CTTextAlignment)) return false;
     *(CTTextAlignment *)buffer=style->_alignment;
     return true;
    case kCTParagraphStyleSpecifierLineBreakMode:
     if(size!=sizeof(CTLineBreakMode)) return false;
     *(CTLineBreakMode *)buffer=style->_lineBreakMode;
     return true;
    case kCTParagraphStyleSpecifierBaseWritingDirection:
     if(size!=sizeof(CTWritingDirection)) return false;
     *(CTWritingDirection *)buffer=style->_direction;
     return true;
    case kCTParagraphStyleSpecifierTabStops:
     if(size!=sizeof(CFArrayRef)) return false;
     *(CFArrayRef *)buffer=style->_tabStops;
     return true;
    case kCTParagraphStyleSpecifierLineBoundsOptions:
     if(size!=sizeof(CFOptionFlags)) return false;
     *(CFOptionFlags *)buffer=style->_lineBoundsOptions;
     return true;
    default:
     if(!isFloatSetting(spec) || size!=sizeof(CGFloat)) return false;
     *(CGFloat *)buffer=style->_values[spec];
     return true;
   }
}
