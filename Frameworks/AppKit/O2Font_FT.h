#import <Onyx2D/O2Font.h>

#import <stddef.h>
#import <ft2build.h>
#import FT_FREETYPE_H
#import FT_RENDER_H
#import <fontconfig.h>

@interface O2Font_FT : O2Font {
    FT_Face _face;
    CGFloat _pixelSize;
}

- (FT_Face)face;
/* Sets the face's size, only when it changes: FreeType runs the font's
 * hinting program again on every change, the same size or not. */
- (void)setPixelSize:(CGFloat)size;

@end

FT_Library O2FontSharedFreeTypeLibrary();
FcConfig *O2FontSharedFontConfig();
