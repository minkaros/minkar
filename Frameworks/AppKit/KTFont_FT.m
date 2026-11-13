/* Copyright (c) 2008 Johannes Fortmann

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/* Modifications Copyright (c) 2026 The Minkar contributors */

#import <AppKit/NSFont.h>
#import "KTFont_FT.h"
#import <AppKit/KTFont.h>
#import <AppKit/NSRaise.h>
#import "O2Font_FT.h"
#import FT_OUTLINE_H
#import <AppKit/NSFontTypeface.h>
//#import <AppKit/NSGraphicsContext.h>

@implementation KTFont(KTFont_FT)
+(id)allocWithZone:(NSZone*)zone
{
   return NSAllocateObject([KTFont_FT class], 0, NULL);
}
@end

@implementation KTFont_FT {
   NSMutableDictionary *_advanceCache;
}


/* Every UI font type: its face and default size. System text is Inter,
   fixed pitch is Agave; the emphasized types are bold. */
-initWithUIFontType:(CTFontUIFontType)uiFontType size:(CGFloat)size language:(NSString *)language {
   static const struct { CGFloat size; const char *name; } types[]={
    [kCTFontUIFontUser]={ 12, "Inter-Regular" },
    [kCTFontUIFontUserFixedPitch]={ 10, "Agave-Regular" },
    [kCTFontUIFontSystem]={ 12, "Inter-Regular" },
    [kCTFontUIFontEmphasizedSystem]={ 12, "Inter-Bold" },
    [kCTFontUIFontSmallSystem]={ 11, "Inter-Regular" },
    [kCTFontUIFontSmallEmphasizedSystem]={ 11, "Inter-Bold" },
    [kCTFontUIFontMiniSystem]={ 9, "Inter-Regular" },
    [kCTFontUIFontMiniEmphasizedSystem]={ 9, "Inter-Bold" },
    [kCTFontUIFontViews]={ 12, "Inter-Regular" },
    [kCTFontUIFontApplication]={ 12, "Inter-Regular" },
    [kCTFontUIFontLabel]={ 10, "Inter-Regular" },
    [kCTFontUIFontMenuTitle]={ 14, "Inter-Regular" },
    [kCTFontUIFontMenuItem]={ 12, "Inter-Regular" },
    [kCTFontUIFontMenuItemMark]={ 14, "Inter-Regular" },
    [kCTFontUIFontMenuItemCmdKey]={ 14, "Inter-Regular" },
    [kCTFontUIFontWindowTitle]={ 13, "Inter-Regular" },
    [kCTFontUIFontPushButton]={ 13, "Inter-Regular" },
    [kCTFontUIFontUtilityWindowTitle]={ 11, "Inter-Regular" },
    [kCTFontUIFontAlertHeader]={ 13, "Inter-Bold" },
    [kCTFontUIFontSystemDetail]={ 12, "Inter-Regular" },
    [kCTFontUIFontEmphasizedSystemDetail]={ 12, "Inter-Bold" },
    [kCTFontUIFontToolbar]={ 11, "Inter-Regular" },
    [kCTFontUIFontSmallToolbar]={ 10, "Inter-Regular" },
    [kCTFontUIFontMessage]={ 13, "Inter-Regular" },
    [kCTFontUIFontPalette]={ 11, "Inter-Regular" },
    [kCTFontUIFontToolTip]={ 11, "Inter-Regular" },
    [kCTFontUIFontControlContent]={ 13, "Inter-Regular" },
   };
   O2Font *font;

   if((uint32_t)uiFontType>=sizeof(types)/sizeof(types[0])){
    [self release];
    return nil;
   }
   if(size<=0)
    size=types[uiFontType].size;
   font=O2FontCreateWithFontName([NSString stringWithUTF8String:types[uiFontType].name]);
   if(font==nil)
    font=O2FontCreateWithFontName(@"Inter-Regular");
   if(font==nil){
    [self release];
    return nil;
   }

   self=[self initWithFont:font size:size];

   [font release];

   return self;
}

-(void)getGlyphs:(CGGlyph *)glyphs forCharacters:(const unichar *)characters length:(unsigned)length {
   O2Font_FT *o2Font=(O2Font_FT *)_font;
   FT_Face    face=[o2Font face];

   int i;
   for(i=0; i<length; i++)
   {
      glyphs[i]=FT_Get_Char_Index(face, characters[i]);
   }
}

/* A glyph's advance at this size, loaded once: loading it runs the font's
   hinting, and layout and drawing ask for it again and again. */
static CGFloat cachedAdvance(KTFont_FT *self,CGGlyph glyph) {
   NSNumber *key=[NSNumber numberWithUnsignedShort:glyph];
   NSNumber *value;

   if(self->_advanceCache==nil)
    self->_advanceCache=[[NSMutableDictionary alloc] init];
   if((value=[self->_advanceCache objectForKey:key])==nil){
    O2Font_FT *o2Font=(O2Font_FT *)self->_font;
    FT_Face    face=[o2Font face];
    CGFloat    advance=0;

    [o2Font setPixelSize:self->_size];
    /* The pen's advance (26.6 fixed point), not the bitmap's left side
       bearing: a click in text was taken for its line's end. */
    if(FT_Load_Glyph(face,glyph,FT_LOAD_DEFAULT)==0)
     advance=face->glyph->advance.x/64.0;
    value=[NSNumber numberWithDouble:advance];
    [self->_advanceCache setObject:value forKey:key];
   }
   return [value doubleValue];
}

-(void)getAdvancements:(CGSize *)advancements forGlyphs:(const CGGlyph *)glyphs count:(unsigned)count {
   unsigned i;

   for(i=0;i<count;i++)
    advancements[i]=CGSizeMake(cachedAdvance(self,glyphs[i]),0);
}

-(NSSize)advancementForGlyph:(NSGlyph)glyph {
   return NSMakeSize(cachedAdvance(self,(CGGlyph)glyph),0);
}

-(CGPoint)positionOfGlyph:(CGGlyph)current precededByGlyph:(CGGlyph)previous isNominal:(BOOL *)isNominalp {
   *isNominalp=YES;

   if(!current)
      return NSZeroPoint;
   return NSMakePoint(cachedAdvance(self,current),0);
}

-(void)dealloc {
   [_advanceCache release];
   [super dealloc];
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %@ %f>",[self class],_font,_size];
}

// FIXME: I feel like all of this below should be inherited from NSFont but these are
// all unrecognized selectors unless implemented here. WHY?

-(NSCharacterSet *)coveredCharacterSet {
   return [_font coveredCharacterSet];
}

-(CGFloat)defaultLineHeightForFont {
   return round(O2FontGetAscent(_font) + O2FontGetDescent(_font) + O2FontGetLeading(_font));
}

#if 0
-(void)setInContext:(NSGraphicsContext *)context {
   CGContextRef cgContext=[context graphicsPort];

   CGContextSetFont(cgContext,_font);
   CGContextSetFontSize(cgContext,_size);

   CGAffineTransform textMatrix;

// FIX, should check the focusView in the context instead of NSView's
   if([[NSGraphicsContext currentContext] isFlipped])
    textMatrix=(CGAffineTransform){1,0,0,-1,0,0};
   else
    textMatrix=CGAffineTransformIdentity;

   CGContextSetTextMatrix(cgContext,textMatrix);
}
#endif
@end

/* Minkar: a glyph's outline at the font's size, in points with y up,
   from its unhinted FreeType outline (quadratic and cubic curves). */
typedef struct {
   CGMutablePathRef path;
   CGFloat          scale;
   const CGAffineTransform *xform;
   BOOL             open;
} OutlineState;

static CGPoint outlinePoint(OutlineState *state,const FT_Vector *v) {
   return CGPointMake(v->x*state->scale,v->y*state->scale);
}

static int outlineMoveTo(const FT_Vector *to,void *user) {
   OutlineState *state=user;
   CGPoint       p=outlinePoint(state,to);

   if(state->open)
    CGPathCloseSubpath(state->path);
   CGPathMoveToPoint(state->path,state->xform,p.x,p.y);
   state->open=YES;
   return 0;
}

static int outlineLineTo(const FT_Vector *to,void *user) {
   OutlineState *state=user;
   CGPoint       p=outlinePoint(state,to);

   CGPathAddLineToPoint(state->path,state->xform,p.x,p.y);
   return 0;
}

static int outlineConicTo(const FT_Vector *control,const FT_Vector *to,void *user) {
   OutlineState *state=user;
   CGPoint       c=outlinePoint(state,control),p=outlinePoint(state,to);

   CGPathAddQuadCurveToPoint(state->path,state->xform,c.x,c.y,p.x,p.y);
   return 0;
}

static int outlineCubicTo(const FT_Vector *c1,const FT_Vector *c2,const FT_Vector *to,void *user) {
   OutlineState *state=user;
   CGPoint       a=outlinePoint(state,c1),b=outlinePoint(state,c2),p=outlinePoint(state,to);

   CGPathAddCurveToPoint(state->path,state->xform,a.x,a.y,b.x,b.y,p.x,p.y);
   return 0;
}

@implementation KTFont_FT (Outlines)

-(CGPathRef)createPathForGlyph:(CGGlyph)glyph transform:(CGAffineTransform *)xform {
   O2Font_FT       *o2Font=(O2Font_FT *)_font;
   FT_Face          face=[o2Font face];
   CGMutablePathRef path=CGPathCreateMutable();
   FT_Outline_Funcs funcs={ outlineMoveTo,outlineLineTo,outlineConicTo,outlineCubicTo,0,0 };
   OutlineState     state={ path,0,xform,NO };

   if(face==NULL || face->units_per_EM==0)
    return path;
   @synchronized(o2Font){
    if(FT_Load_Glyph(face,glyph,FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING|FT_LOAD_NO_BITMAP)==0 &&
       face->glyph->format==FT_GLYPH_FORMAT_OUTLINE){
     state.scale=_size/face->units_per_EM;
     FT_Outline_Decompose(&face->glyph->outline,&funcs,&state);
     if(state.open)
      CGPathCloseSubpath(path);
    }
   }
   return path;
}

@end
