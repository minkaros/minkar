#import <Onyx2D/O2Font.h>

#import <stddef.h>
#import <ft2build.h>
#import FT_FREETYPE_H
#import FT_RENDER_H
#import FT_MULTIPLE_MASTERS_H
#import <fontconfig.h>

@interface O2Font_FT : O2Font {
    FT_Face _face;
    CGFloat _pixelSize;
    NSString *_path;		/* the file the face is read from, or nil */
    NSData *_data;		/* the bytes the face is read from, or nil */
    NSDictionary *_variations;	/* axis name -> value, for an instance */
}

- (FT_Face)face;
/* Sets the face's size, only when it changes: FreeType runs the font's
 * hinting program again on every change, the same size or not. */
- (void)setPixelSize:(CGFloat)size;

@end

FT_Library O2FontSharedFreeTypeLibrary();
FcConfig *O2FontSharedFontConfig();
