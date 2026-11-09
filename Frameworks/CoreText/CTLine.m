/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Lines laid out one glyph per UTF-16 unit, left to right, in runs of
 * equal attributes. The font is the run's kCTFontAttributeName (a CTFont,
 * or an AppKit font, which stands for one), else Helvetica 12.
 */

#import <CoreText/CTLine.h>
#import <CoreText/CTFont.h>
#import <CoreText/CTStringAttributes.h>
#import "KTFont.h"
#import "CTTypes.h"
#import <Foundation/Foundation.h>

@interface CTRun : NSObject {
@public
   NSDictionary *_attributes;
   KTFont       *_font;
   CFRange       _range;
   CFIndex       _count;
   CGGlyph      *_glyphs;
   CGPoint      *_positions;
   CGSize       *_advances;
   CFIndex      *_indices;
   CGFloat       _width;
}
@end

@interface CTLine : NSObject {
@public
   NSArray *_runs;
   CFRange  _range;
   CGFloat  _width;
   CGFloat  _ascent, _descent, _leading;
   CGFloat  _trailingWhitespace;
}
@end

CT_DEFINE_TYPE(CTLine, "CTLine")
CT_DEFINE_TYPE(CTRun, "CTRun")

/* The KTFont of an attribute value. */
KTFont *CTFontOfAttribute(id value) {
   if([value isKindOfClass:[KTFont class]])
    return value;
   if([value respondsToSelector:@selector(_ctFont)])
    return [value performSelector:@selector(_ctFont)];
   return nil;
}

static KTFont *defaultFont(void) {
   static KTFont *font;
   if(font==nil)
    font=(KTFont *)CTFontCreateWithName(CFSTR("Helvetica"),12,NULL);
   return font;
}

@implementation CTRun

-(void)dealloc {
   [_attributes release];
   [_font release];
   free(_glyphs);
   free(_positions);
   free(_advances);
   free(_indices);
   [super dealloc];
}

@end

@implementation CTLine

-(void)dealloc {
   [_runs release];
   [super dealloc];
}

@end

static CTRun *makeRun(NSString *string, NSRange range, NSDictionary *attributes, CGFloat x) {
   CTRun   *run=[[CTRun alloc] init];
   KTFont  *font=CTFontOfAttribute([attributes objectForKey:(NSString *)kCTFontAttributeName]);
   CGFloat  kern=[[attributes objectForKey:(NSString *)kCTKernAttributeName] doubleValue];
   unichar *characters=malloc(sizeof(unichar)*MAX(range.length,1));

   if(font==nil)
    font=defaultFont();
   run->_attributes=[attributes copy];
   run->_font=[font retain];
   run->_range=CFRangeMake(range.location,range.length);
   run->_count=range.length;
   run->_glyphs=calloc(MAX(range.length,1),sizeof(CGGlyph));
   run->_positions=calloc(MAX(range.length,1),sizeof(CGPoint));
   run->_advances=calloc(MAX(range.length,1),sizeof(CGSize));
   run->_indices=calloc(MAX(range.length,1),sizeof(CFIndex));
   [string getCharacters:characters range:range];
   [font getGlyphs:run->_glyphs forCharacters:characters length:(unsigned)range.length];
   [font getAdvancements:run->_advances forGlyphs:run->_glyphs count:(unsigned)range.length];
   for(NSUInteger i=0;i<range.length;i++){
    run->_advances[i].width+=kern;
    run->_positions[i]=CGPointMake(x+run->_width,0);
    run->_indices[i]=range.location+i;
    run->_width+=run->_advances[i].width;
   }
   free(characters);
   return [run autorelease];
}

CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef attrString) {
   NSAttributedString *text=(NSAttributedString *)attrString;
   NSString           *string=[text string];
   NSUInteger          length=[string length],location=0;
   NSMutableArray     *runs=[NSMutableArray array];
   CTLine             *line=[[CTLine alloc] init];
   CGFloat             x=0;

   while(location<length){
    NSRange       range;
    NSDictionary *attributes=[text attributesAtIndex:location effectiveRange:&range];
    range=NSIntersectionRange(range,NSMakeRange(location,length-location));
    CTRun        *run=makeRun(string,range,attributes,x);
    [runs addObject:run];
    x+=run->_width;
    line->_ascent=MAX(line->_ascent,[run->_font ascender]);
    line->_descent=MAX(line->_descent,[run->_font descender]);
    line->_leading=MAX(line->_leading,[run->_font leading]);
    location=NSMaxRange(range);
   }
   if(length==0){
    KTFont *font=defaultFont();
    line->_ascent=[font ascender];
    line->_descent=[font descender];
    line->_leading=[font leading];
   }
   /* Trailing white space: spaces, tabs and line ends at the end. */
   {
    NSCharacterSet *white=[NSCharacterSet whitespaceAndNewlineCharacterSet];
    for(CTRun *run in [runs reverseObjectEnumerator]){
     CFIndex i;
     for(i=run->_count-1;i>=0;i--){
      if(![white characterIsMember:[string characterAtIndex:run->_indices[i]]])
       break;
      line->_trailingWhitespace+=run->_advances[i].width;
     }
     if(i>=0)
      break;
    }
   }
   line->_runs=[runs copy];
   line->_range=CFRangeMake(0,length);
   line->_width=x;
   return (CTLineRef)line;
}

CFIndex CTLineGetGlyphCount(CTLineRef self) {
   CFIndex count=0;
   for(CTRun *run in ((CTLine *)self)->_runs)
    count+=run->_count;
   return count;
}

CFArrayRef CTLineGetGlyphRuns(CTLineRef self) {
   return (CFArrayRef)((CTLine *)self)->_runs;
}

CFRange CTLineGetStringRange(CTLineRef self) {
   return ((CTLine *)self)->_range;
}

double CTLineGetTypographicBounds(CTLineRef self, CGFloat *ascent, CGFloat *descent, CGFloat *leading) {
   CTLine *line=(CTLine *)self;
   if(ascent!=NULL) *ascent=line->_ascent;
   if(descent!=NULL) *descent=line->_descent;
   if(leading!=NULL) *leading=line->_leading;
   return line->_width;
}

double CTLineGetTrailingWhitespaceWidth(CTLineRef self) {
   return ((CTLine *)self)->_trailingWhitespace;
}

CGRect CTLineGetBoundsWithOptions(CTLineRef self, CTLineBoundsOptions options) {
   CTLine *line=(CTLine *)self;
   CGFloat leading=(options&kCTLineBoundsExcludeTypographicLeading)?0:line->_leading;
   return CGRectMake(0,-line->_descent-leading,line->_width,line->_ascent+line->_descent+leading);
}

CGRect CTLineGetImageBounds(CTLineRef self, CGContextRef context) {
   CTLine *line=(CTLine *)self;
   CGPoint origin=(context!=NULL)?CGContextGetTextPosition(context):CGPointZero;
   CGFloat width=line->_width-line->_trailingWhitespace;
   if(width<=0)
    return CGRectNull;
   return CGRectMake(origin.x,origin.y-line->_descent,width,line->_ascent+line->_descent);
}

double CTLineGetPenOffsetForFlush(CTLineRef self, CGFloat flushFactor, double flushWidth) {
   CTLine *line=(CTLine *)self;
   double  room=flushWidth-(line->_width-line->_trailingWhitespace);
   if(room<=0)
    return 0;
   return room*MIN(MAX(flushFactor,0),1);
}

CGFloat CTLineGetOffsetForStringIndex(CTLineRef self, CFIndex charIndex, CGFloat *secondaryOffset) {
   CTLine *line=(CTLine *)self;
   CGFloat offset=line->_width;

   for(CTRun *run in line->_runs){
    if(charIndex<run->_range.location+run->_range.length && charIndex>=run->_range.location){
     offset=run->_positions[charIndex-run->_range.location].x;
     break;
    }
   }
   if(charIndex<=0)
    offset=0;
   if(secondaryOffset!=NULL)
    *secondaryOffset=offset;
   return offset;
}

/* The caret index nearest the point: before a glyph when the point is in
   its first half, after it otherwise. */
CFIndex CTLineGetStringIndexForPosition(CTLineRef self, CGPoint position) {
   CTLine *line=(CTLine *)self;

   if(position.x<=0)
    return line->_range.location;
   for(CTRun *run in line->_runs)
    for(CFIndex i=0;i<run->_count;i++){
     CGFloat start=run->_positions[i].x,advance=run->_advances[i].width;
     if(position.x<start+advance)
      return run->_indices[i]+((position.x<start+advance/2)?0:1);
    }
   return line->_range.location+line->_range.length;
}

static CGColorRef colorOf(id value) {
   if(value==nil)
    return NULL;
   if([value respondsToSelector:@selector(CGColor)])
    return (CGColorRef)[value performSelector:@selector(CGColor)];
   return (CGColorRef)value;
}

static void drawRun(CTRun *run, CGContextRef context, CFRange range, CGPoint origin) {
   NSDictionary *attributes=run->_attributes;
   CGFontRef     cgFont;
   CFIndex       start=range.location,count=(range.length==0)?run->_count-range.location:range.length;
   CGPoint      *positions;
   CGColorRef    color=colorOf([attributes objectForKey:(NSString *)kCTForegroundColorAttributeName]);
   BOOL          fromContext=[[attributes objectForKey:(NSString *)kCTForegroundColorFromContextAttributeName] boolValue];
   int32_t       underline=[[attributes objectForKey:(NSString *)kCTUnderlineStyleAttributeName] intValue];

   if(count<=0 || start<0 || start+count>run->_count)
    return;
   cgFont=CGFontCreateWithFontName((CFStringRef)[(NSString *)CTFontCopyFullName((CTFontRef)run->_font) autorelease]);
   CGContextSaveGState(context);
   if(color!=NULL && !fromContext)
    CGContextSetFillColorWithColor(context,color);
   if(cgFont!=NULL){
    CGContextSetFont(context,cgFont);
    CGContextSetFontSize(context,[run->_font pointSize]);
   }
   positions=malloc(sizeof(CGPoint)*count);
   for(CFIndex i=0;i<count;i++)
    positions[i]=CGPointMake(origin.x+run->_positions[start+i].x,origin.y+run->_positions[start+i].y);
   CGContextShowGlyphsAtPositions(context,run->_glyphs+start,positions,count);
   if((underline&0xff)!=kCTUnderlineStyleNone){
    CGFloat thickness=[run->_font underlineThickness],y=origin.y+[run->_font underlinePosition];
    CGFloat from=positions[0].x,to=positions[count-1].x+run->_advances[start+count-1].width;
    CGColorRef lineColor=colorOf([attributes objectForKey:(NSString *)kCTUnderlineColorAttributeName]);
    if(lineColor!=NULL)
     CGContextSetFillColorWithColor(context,lineColor);
    if((underline&0xff)==kCTUnderlineStyleThick)
     thickness*=2;
    CGContextFillRect(context,CGRectMake(from,y-thickness/2,to-from,MAX(thickness,1)));
    if((underline&0xff)==kCTUnderlineStyleDouble)
     CGContextFillRect(context,CGRectMake(from,y-thickness*2.5,to-from,MAX(thickness,1)));
   }
   free(positions);
   CGContextRestoreGState(context);
   CGFontRelease(cgFont);
}

void CTLineDraw(CTLineRef self, CGContextRef context) {
   CGPoint origin=CGContextGetTextPosition(context);
   for(CTRun *run in ((CTLine *)self)->_runs)
    drawRun(run,context,CFRangeMake(0,0),origin);
}

CTLineRef CTLineCreateTruncatedLine(CTLineRef self, double width, CTLineTruncationType truncationType, CTLineRef token) {
   CTLine *line=(CTLine *)self;
   if(line->_width<=width)
    return (CTLineRef)[line retain];
   if(width<=0)
    return NULL;
   /* Only from the end: keep the glyphs that fit with the token. */
   CGFloat room=width-((token!=NULL)?((CTLine *)token)->_width:0);
   CTLine         *cut=[[CTLine alloc] init];
   NSMutableArray *runs=[NSMutableArray array];
   CGFloat         x=0;
   for(CTRun *run in line->_runs){
    CFIndex n=0;
    while(n<run->_count && run->_positions[n].x+run->_advances[n].width<=room)
     n++;
    if(n==0)
     break;
    CTRun *part=[[[CTRun alloc] init] autorelease];
    part->_attributes=[run->_attributes retain];
    part->_font=[run->_font retain];
    part->_range=CFRangeMake(run->_range.location,n);
    part->_count=n;
    part->_glyphs=malloc(sizeof(CGGlyph)*n); memcpy(part->_glyphs,run->_glyphs,sizeof(CGGlyph)*n);
    part->_positions=malloc(sizeof(CGPoint)*n); memcpy(part->_positions,run->_positions,sizeof(CGPoint)*n);
    part->_advances=malloc(sizeof(CGSize)*n); memcpy(part->_advances,run->_advances,sizeof(CGSize)*n);
    part->_indices=malloc(sizeof(CFIndex)*n); memcpy(part->_indices,run->_indices,sizeof(CFIndex)*n);
    for(CFIndex i=0;i<n;i++)
     part->_width+=part->_advances[i].width;
    x=part->_positions[n-1].x+part->_advances[n-1].width;
    [runs addObject:part];
    if(n<run->_count)
     break;
   }
   if(token!=NULL)
    for(CTRun *run in ((CTLine *)token)->_runs){
     CTRun *part=[[[CTRun alloc] init] autorelease];
     CFIndex n=run->_count;
     part->_attributes=[run->_attributes retain];
     part->_font=[run->_font retain];
     part->_range=CFRangeMake(line->_range.location+line->_range.length,0);
     part->_count=n;
     part->_glyphs=malloc(sizeof(CGGlyph)*MAX(n,1)); memcpy(part->_glyphs,run->_glyphs,sizeof(CGGlyph)*n);
     part->_positions=malloc(sizeof(CGPoint)*MAX(n,1));
     part->_advances=malloc(sizeof(CGSize)*MAX(n,1)); memcpy(part->_advances,run->_advances,sizeof(CGSize)*n);
     part->_indices=malloc(sizeof(CFIndex)*MAX(n,1));
     for(CFIndex i=0;i<n;i++){
      part->_positions[i]=CGPointMake(x+run->_positions[i].x,0);
      part->_indices[i]=kCFNotFound;
      part->_width+=part->_advances[i].width;
     }
     [runs addObject:part];
    }
   cut->_runs=[runs copy];
   cut->_range=line->_range;
   cut->_ascent=line->_ascent;
   cut->_descent=line->_descent;
   cut->_leading=line->_leading;
   for(CTRun *run in runs)
    cut->_width+=run->_width;
   return (CTLineRef)cut;
}

/* ---- Runs ---- */

CFIndex CTRunGetGlyphCount(CTRunRef self) { return ((CTRun *)self)->_count; }
CFDictionaryRef CTRunGetAttributes(CTRunRef self) { return (CFDictionaryRef)((CTRun *)self)->_attributes; }
CTRunStatus CTRunGetStatus(CTRunRef self) { return kCTRunStatusNoStatus; }
const CGGlyph *CTRunGetGlyphsPtr(CTRunRef self) { return ((CTRun *)self)->_glyphs; }
const CGPoint *CTRunGetPositionsPtr(CTRunRef self) { return ((CTRun *)self)->_positions; }
const CGSize *CTRunGetAdvancesPtr(CTRunRef self) { return ((CTRun *)self)->_advances; }
const CFIndex *CTRunGetStringIndicesPtr(CTRunRef self) { return ((CTRun *)self)->_indices; }
CFRange CTRunGetStringRange(CTRunRef self) { return ((CTRun *)self)->_range; }

#define COPY_RANGE(field,type) \
   CTRun *run=(CTRun *)self; \
   CFIndex n=(range.length==0)?run->_count-range.location:range.length; \
   if(range.location>=0 && n>=0 && range.location+n<=run->_count) \
    memcpy(buffer,run->field+range.location,sizeof(type)*n);

void CTRunGetGlyphs(CTRunRef self, CFRange range, CGGlyph *buffer) { COPY_RANGE(_glyphs,CGGlyph) }
void CTRunGetPositions(CTRunRef self, CFRange range, CGPoint *buffer) { COPY_RANGE(_positions,CGPoint) }
void CTRunGetAdvances(CTRunRef self, CFRange range, CGSize *buffer) { COPY_RANGE(_advances,CGSize) }
void CTRunGetStringIndices(CTRunRef self, CFRange range, CFIndex *buffer) { COPY_RANGE(_indices,CFIndex) }

double CTRunGetTypographicBounds(CTRunRef self, CFRange range, CGFloat *ascent, CGFloat *descent, CGFloat *leading) {
   CTRun  *run=(CTRun *)self;
   CFIndex n=(range.length==0)?run->_count-range.location:range.length;
   double  width=0;
   if(ascent!=NULL) *ascent=[run->_font ascender];
   if(descent!=NULL) *descent=[run->_font descender];
   if(leading!=NULL) *leading=[run->_font leading];
   for(CFIndex i=range.location;i<range.location+n && i<run->_count;i++)
    width+=run->_advances[i].width;
   return width;
}

void CTRunDraw(CTRunRef self, CGContextRef context, CFRange range) {
   drawRun((CTRun *)self,context,range,CGContextGetTextPosition(context));
}
