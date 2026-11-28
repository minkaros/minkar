/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMTerminalView.h"
#import "TMPreferences.h"
#import <CoreText/CoreText.h>

#define INSET 6.0

typedef struct {
    NSInteger line;     /* scrollback lines first, then the screen's rows */
    int column;
} TMPoint;

static BOOL before(TMPoint a, TMPoint b)
{
    return a.line < b.line || (a.line == b.line && a.column < b.column);
}

@implementation TMTerminalView {
    NSFont *_font, *_bold, *_italic, *_boldItalic;
    NSString *_fontName;
    NSMutableDictionary<NSNumber *, NSFont *> *_fallbacks;   /* by character, at this size */
    CGFloat _ascent;
    NSTimer *_blinkTimer;
    BOOL _cursorOff;       /* the blink's dark half */
    TMCursorShape _cursorShape;
    BOOL _cursorBlink;
    BOOL _hasSelection, _selecting;
    TMPoint _anchor, _head;
    NSColor *_foreground, *_background, *_cursorColor;
    BOOL _flashing;
}

- (instancetype)initWithFrame:(NSRect)frame arguments:(NSArray<NSString *> *)arguments
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    TMPreferences *prefs = [TMPreferences sharedPreferences];

    [self setFontName:prefs.fontName size:prefs.fontSize];
    [TMSession setDefaultShell:prefs.shell];
    _session = [[TMSession alloc] initWithRows:prefs.rows columns:prefs.columns arguments:arguments directory:nil];
    _session.delegate = self;
    [self applyPreferences];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(preferencesDidChange:)
        name:TMPreferencesDidChangeNotification object:nil];
    return self;
}

- (void)dealloc
{
    [_blinkTimer invalidate];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

/* Colors, palette, scrollback and size from the preferences, at once. */
- (void)applyPreferences
{
    TMPreferences *prefs = [TMPreferences sharedPreferences];

    _foreground = prefs.textColor;
    _background = prefs.backgroundColor;
    _cursorColor = prefs.cursorColor;
    [self applyDefaultColors];
    for (int i = 0; i < 16; i++) {
        NSColor *c = [[prefs paletteColorAtIndex:i] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        [_session setPaletteIndex:i red:(uint8_t)lround([c redComponent] * 255)
            green:(uint8_t)lround([c greenComponent] * 255) blue:(uint8_t)lround([c blueComponent] * 255)];
    }
    _session.scrollbackLimit = (NSUInteger)prefs.scrollbackLines;
    _cursorShape = prefs.cursorShape;
    if (_cursorBlink != prefs.cursorBlink) {
        _cursorBlink = prefs.cursorBlink;
        [self updateBlinkTimer];
    }
    if (prefs.fontSize != _fontSize || ![prefs.fontName isEqual:_fontName])
        [self setFontName:prefs.fontName size:prefs.fontSize];
    [[self enclosingScrollView] setBackgroundColor:_background];
    [self setNeedsDisplay:YES];
}

- (void)preferencesDidChange:(NSNotification *)note
{
    [self applyPreferences];
    if (self.titleChanged)
        self.titleChanged(self);
}

static VTermColor rgbOf(NSColor *color)
{
    NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]] ?: [NSColor blackColor];
    VTermColor c;

    vterm_color_rgb(&c, (uint8_t)lround([rgb redComponent] * 255), (uint8_t)lround([rgb greenComponent] * 255),
        (uint8_t)lround([rgb blueComponent] * 255));
    return c;
}

- (void)applyDefaultColors
{
    [_session setDefaultForeground:rgbOf(_foreground) background:rgbOf(_background)];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)setFontSize:(CGFloat)size
{
    [self setFontName:_fontName ?: [TMPreferences sharedPreferences].fontName size:size];
}

/* A face of the font with a trait, or nil when it has none (drawn then
   from the plain one: bold twice, italic slanted). */
static NSFont *faceWithTrait(NSFont *font, NSFontTraitMask trait)
{
    NSFontManager *fm = [NSFontManager sharedFontManager];
    NSFont *face = [fm convertFont:font toHaveTrait:trait];

    if (face == nil || [[face fontName] isEqual:[font fontName]] || !([fm traitsOfFont:face] & trait))
        return nil;
    return face;
}

- (void)setFontName:(NSString *)name size:(CGFloat)size
{
    NSFont *font = [[TMPreferences sharedPreferences] fontOfSize:size];
    if (name != nil && ![name isEqual:[TMPreferences sharedPreferences].fontName])
        font = [NSFont fontWithName:name size:size] ?: font;
    NSDictionary *attributes = @{ NSFontAttributeName : font };
    BOOL placed = _session != nil && [self window] != nil;
    int rows = _session.rows, columns = _session.columns;

    _fontSize = size;
    _fontName = [name copy];
    _font = font;
    _bold = faceWithTrait(font, NSBoldFontMask);
    _italic = faceWithTrait(font, NSItalicFontMask);
    _boldItalic = _bold ? faceWithTrait(_bold, NSItalicFontMask) : nil;
    _fallbacks = [NSMutableDictionary dictionary];
    _cellSize = NSMakeSize(ceil([@"MMMMMMMMMM" sizeWithAttributes:attributes].width) / 10.0,
        ceil([font ascender] - [font descender] + [font leading]));
    if (_cellSize.height < size)
        _cellSize.height = ceil(size * 1.25);
    _ascent = ceil([font ascender]);
    /* In a window: it keeps its rows and columns, and the window takes
       the new size (as on the reference); otherwise they fit the view. */
    if (placed && self.wantsGrid)
        self.wantsGrid(self, rows, columns);
    else
        [self fitToScrollView];
    [self setNeedsDisplay:YES];
}

- (NSSize)contentSizeForRows:(int)rows columns:(int)columns
{
    return NSMakeSize(ceil(columns * _cellSize.width) + 2 * INSET, rows * _cellSize.height + 2 * INSET);
}

/* The rows and columns that fit, then the frame for them and the scrollback. */
- (void)fitToScrollView
{
    NSScrollView *scroll = [self enclosingScrollView];
    NSSize size = scroll ? [scroll contentSize] : [self frame].size;
    int columns = MAX(10, (int)floor((size.width - 2 * INSET) / _cellSize.width));
    int rows = MAX(2, (int)floor((size.height - 2 * INSET) / _cellSize.height));

    [_session resizeToRows:rows columns:columns];
    [self updateFrame];
}

- (void)updateFrame
{
    NSScrollView *scroll = [self enclosingScrollView];
    NSClipView *clip = [scroll contentView];
    BOOL atBottom = clip == nil || NSMaxY([clip documentVisibleRect]) >= NSHeight([self frame]) - _cellSize.height;
    NSInteger lines = (NSInteger)_session.scrollbackCount + _session.rows;
    NSSize size = NSMakeSize(MAX([scroll contentSize].width, ceil(_session.columns * _cellSize.width) + 2 * INSET),
        lines * _cellSize.height + 2 * INSET);

    if (!NSEqualSizes(size, [self frame].size))
        [self setFrameSize:size];
    if (atBottom && clip != nil)
        [self scrollPoint:NSMakePoint(0, size.height - [clip bounds].size.height)];
}

- (void)viewDidMoveToSuperview
{
    [self fitToScrollView];
}

/* ---- Drawing ---- */

- (NSColor *)colorOf:(VTermColor)color
{
    uint8_t r, g, b;

    /* The defaults are the view's own, so a new scheme reaches old text. */
    if (VTERM_COLOR_IS_DEFAULT_FG(&color))
        return _foreground;
    if (VTERM_COLOR_IS_DEFAULT_BG(&color))
        return _background;

    [_session getRed:&r green:&g blue:&b ofColor:color];
    return [NSColor colorWithSRGBRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1];
}

- (BOOL)cell:(VTermScreenCell *)cell line:(NSInteger)line column:(int)column
{
    NSInteger scrollback = (NSInteger)_session.scrollbackCount;

    if (line < scrollback)
        return [_session getCell:cell scrollbackLine:(NSUInteger)line column:column];
    return [_session getCell:cell row:(int)(line - scrollback) column:column];
}

- (BOOL)isSelectedLine:(NSInteger)line column:(int)column
{
    TMPoint start = before(_anchor, _head) ? _anchor : _head, end = before(_anchor, _head) ? _head : _anchor;
    TMPoint p = { line, column };

    return _hasSelection && !before(p, start) && before(p, (TMPoint){ end.line, end.column + 1 });
}

static BOOL sameStyle(const VTermScreenCell *a, const VTermScreenCell *b)
{
    return a->attrs.bold == b->attrs.bold && a->attrs.underline == b->attrs.underline &&
        a->attrs.italic == b->attrs.italic && a->attrs.reverse == b->attrs.reverse &&
        a->attrs.strike == b->attrs.strike && memcmp(&a->fg, &b->fg, sizeof(VTermColor)) == 0 &&
        memcmp(&a->bg, &b->bg, sizeof(VTermColor)) == 0;
}

/* The font that draws a character: the terminal's, or the one the
   system falls back to for it (kept per character). */
- (NSFont *)fontFor:(uint32_t)ch base:(NSFont *)base
{
    if (ch < 0x80)
        return base;
    NSNumber *key = @(ch);
    NSFont *font = _fallbacks[key];
    if (font == nil) {
        unichar utf16[2];
        NSUInteger n = 1;
        if (ch > 0xFFFF) {
            utf16[0] = (unichar)(0xD800 + ((ch - 0x10000) >> 10));
            utf16[1] = (unichar)(0xDC00 + ((ch - 0x10000) & 0x3FF));
            n = 2;
        } else
            utf16[0] = (unichar)ch;
        /* Core Text's cascade picks it, as for any other text. */
        CTFontRef ct = CTFontCreateWithName((__bridge CFStringRef)[_font fontName], [_font pointSize], NULL);
        CFStringRef string = CFStringCreateWithCharacters(NULL, utf16, (CFIndex)n);
        CTFontRef fallback = ct && string ? CTFontCreateForString(ct, string, CFRangeMake(0, (CFIndex)n)) : NULL;
        if (fallback != NULL) {
            NSString *name = CFBridgingRelease(CTFontCopyPostScriptName(fallback));
            font = [NSFont fontWithName:name size:[_font pointSize]];
            CFRelease(fallback);
        }
        if (string != NULL)
            CFRelease(string);
        if (ct != NULL)
            CFRelease(ct);
        font = font ?: _font;
        _fallbacks[key] = font;
    }
    return [[font fontName] isEqual:[_font fontName]] ? base : font;
}

static NSString *stringOfCell(const VTermScreenCell *cell)
{
    NSMutableString *text = [NSMutableString string];
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell->chars[i]; i++) {
        uint32_t ch = cell->chars[i];
        [text appendString:[[NSString alloc] initWithBytes:&ch length:4 encoding:NSUTF32LittleEndianStringEncoding] ?: @"\uFFFD"];
    }
    return text;
}

/* Text at its baseline, the terminal's: x is the cell's left. Italic
   without a face of its own is slanted about the baseline, glyph by
   glyph from their outlines, one per cell. */
- (void)drawString:(NSString *)text font:(NSFont *)font at:(NSPoint)p attributes:(NSMutableDictionary *)attributes
    slant:(BOOL)slant
{
    attributes[NSFontAttributeName] = font;
    if (!slant) {
        [text drawAtPoint:NSMakePoint(p.x, p.y + _ascent - ceil([font ascender])) withAttributes:attributes];
        return;
    }
    CGFloat baseline = p.y + _ascent;
    NSUInteger n = [text length];
    unichar chars[n ? n : 1];
    CGGlyph glyphs[n ? n : 1];
    [text getCharacters:chars range:NSMakeRange(0, n)];
    CTFontRef ct = CTFontCreateWithName((__bridge CFStringRef)[font fontName], [font pointSize], NULL);
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    if (ct == NULL || context == NULL)
        return;
    memset(glyphs, 0, sizeof(glyphs));
    CTFontGetGlyphsForCharacters(ct, chars, glyphs, (CFIndex)n);
    [(NSColor *)attributes[NSForegroundColorAttributeName] setFill];
    int cell = 0;
    for (NSUInteger i = 0; i < n; i++) {
        if (CFStringIsSurrogateLowCharacter(chars[i]))
            continue;
        CGAffineTransform t = CGAffineTransformMake(1, 0, 0.2, -1, p.x + cell * _cellSize.width, baseline);
        CGPathRef path = glyphs[i] ? CTFontCreatePathForGlyph(ct, glyphs[i], &t) : NULL;
        if (path != NULL) {
            CGContextAddPath(context, path);
            CGContextFillPath(context);
            CGPathRelease(path);
        }
        cell++;
    }
    CFRelease(ct);
    NSRect line = NSMakeRect(p.x, 0, cell * _cellSize.width, 1);
    if (attributes[NSUnderlineStyleAttributeName]) {
        line.origin.y = baseline + 1;
        NSRectFill(line);
    }
    if (attributes[NSStrikethroughStyleAttributeName]) {
        line.origin.y = baseline - round([font xHeight] / 2);
        NSRectFill(line);
    }
}

/* ---- Box drawing and blocks: geometry that fills the cell, so lines
   meet across cells whatever the font has (as terminals do) ---- */

/* Segments from the center: left, right, up, down; 1 light, 2 heavy,
   3 double. */
typedef struct { uint8_t l, r, u, d; } TMBox;

static BOOL boxOf(uint32_t ch, TMBox *b)
{
    static const struct { uint32_t ch; TMBox box; } table[] = {
        { 0x2500, { 1, 1, 0, 0 } }, { 0x2501, { 2, 2, 0, 0 } }, { 0x2502, { 0, 0, 1, 1 } }, { 0x2503, { 0, 0, 2, 2 } },
        { 0x250C, { 0, 1, 0, 1 } }, { 0x250F, { 0, 2, 0, 2 } }, { 0x2510, { 1, 0, 0, 1 } }, { 0x2513, { 2, 0, 0, 2 } },
        { 0x2514, { 0, 1, 1, 0 } }, { 0x2517, { 0, 2, 2, 0 } }, { 0x2518, { 1, 0, 1, 0 } }, { 0x251B, { 2, 0, 2, 0 } },
        { 0x251C, { 0, 1, 1, 1 } }, { 0x2523, { 0, 2, 2, 2 } }, { 0x2524, { 1, 0, 1, 1 } }, { 0x252B, { 2, 0, 2, 2 } },
        { 0x252C, { 1, 1, 0, 1 } }, { 0x2533, { 2, 2, 0, 2 } }, { 0x2534, { 1, 1, 1, 0 } }, { 0x253B, { 2, 2, 2, 0 } },
        { 0x253C, { 1, 1, 1, 1 } }, { 0x254B, { 2, 2, 2, 2 } },
        { 0x2550, { 3, 3, 0, 0 } }, { 0x2551, { 0, 0, 3, 3 } }, { 0x2554, { 0, 3, 0, 3 } }, { 0x2557, { 3, 0, 0, 3 } },
        { 0x255A, { 0, 3, 3, 0 } }, { 0x255D, { 3, 0, 3, 0 } }, { 0x2560, { 0, 3, 3, 3 } }, { 0x2563, { 3, 0, 3, 3 } },
        { 0x2566, { 3, 3, 0, 3 } }, { 0x2569, { 3, 3, 3, 0 } }, { 0x256C, { 3, 3, 3, 3 } },
        { 0x2574, { 1, 0, 0, 0 } }, { 0x2575, { 0, 0, 1, 0 } }, { 0x2576, { 0, 1, 0, 0 } }, { 0x2577, { 0, 0, 0, 1 } },
        { 0x2578, { 2, 0, 0, 0 } }, { 0x2579, { 0, 0, 2, 0 } }, { 0x257A, { 0, 2, 0, 0 } }, { 0x257B, { 0, 0, 0, 2 } },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (table[i].ch == ch) {
            *b = table[i].box;
            return YES;
        }
    return NO;
}

- (BOOL)drawBox:(uint32_t)ch in:(NSRect)cell color:(NSColor *)color
{
    CGFloat x = NSMinX(cell), y = NSMinY(cell), w = NSWidth(cell), h = NSHeight(cell);
    CGFloat light = MAX(1, round(w / 8)), heavy = light * 2;
    CGFloat cx = floor(x + w / 2), cy = floor(y + h / 2);
    TMBox box;

    [color setFill];
    if (boxOf(ch, &box)) {
        uint8_t seg[4] = { box.l, box.r, box.u, box.d };
        for (int i = 0; i < 4; i++) {
            if (seg[i] == 0)
                continue;
            /* A double is two light lines, a line's width either side of the center. */
            CGFloat t = seg[i] == 2 ? heavy : light;
            int lines = seg[i] == 3 ? 2 : 1;
            CGFloat reach = seg[i] == 3 ? light + t : t - floor(t / 2);   /* past the center */
            CGFloat back = lines == 2 ? light + t : floor(t / 2);          /* before the center */
            for (int k = 0; k < lines; k++) {
                CGFloat o = lines == 2 ? (k ? light : -2 * light) : -floor(t / 2);
                NSRect r;
                if (i == 0)
                    r = NSMakeRect(x, cy + o, cx + reach - x, t);
                else if (i == 1)
                    r = NSMakeRect(cx - back, cy + o, x + w - (cx - back), t);
                else if (i == 2)
                    r = NSMakeRect(cx + o, y, t, cy + reach - y);
                else
                    r = NSMakeRect(cx + o, cy - back, t, y + h - (cy - back));
                NSRectFill(r);
            }
        }
        return YES;
    }
    /* Rounded corners: a quarter circle to the cell's sides. */
    if (ch >= 0x256D && ch <= 0x2570) {
        /* The centers of the light lines the corner joins. */
        CGFloat m = cx - floor(light / 2) + light / 2, n = cy - floor(light / 2) + light / 2;
        NSBezierPath *path = [NSBezierPath bezierPath];
        CGFloat r = MIN(w, h) / 2;
        NSPoint a, b, c;
        switch (ch) {
        case 0x256D: a = NSMakePoint(x + w, n); b = NSMakePoint(m, y + h); c = NSMakePoint(m, n); break;   /* ╭ */
        case 0x256E: a = NSMakePoint(x, n); b = NSMakePoint(m, y + h); c = NSMakePoint(m, n); break;       /* ╮ */
        case 0x256F: a = NSMakePoint(x, n); b = NSMakePoint(m, y); c = NSMakePoint(m, n); break;           /* ╯ */
        default: a = NSMakePoint(x + w, n); b = NSMakePoint(m, y); c = NSMakePoint(m, n); break;          /* ╰ */
        }
        [path moveToPoint:a];
        [path appendBezierPathWithArcFromPoint:c toPoint:b radius:r];
        [path lineToPoint:b];
        [path setLineWidth:light];
        [color setStroke];
        [path stroke];
        return YES;
    }
    /* Blocks: eighths from below or from the left, halves, shades, quadrants. */
    if (ch == 0x2580) { NSRectFill(NSMakeRect(x, y, w, round(h / 2))); return YES; }
    if (ch >= 0x2581 && ch <= 0x2588) {
        CGFloat part = round(h * (ch - 0x2580) / 8);
        NSRectFill(NSMakeRect(x, y + h - part, w, part));
        return YES;
    }
    if (ch >= 0x2589 && ch <= 0x258F) {
        NSRectFill(NSMakeRect(x, y, round(w * (0x2590 - ch) / 8), h));
        return YES;
    }
    if (ch == 0x2590) { NSRectFill(NSMakeRect(x + round(w / 2), y, w - round(w / 2), h)); return YES; }
    if (ch >= 0x2591 && ch <= 0x2593) {
        [[color colorWithAlphaComponent:0.25 * (ch - 0x2590)] setFill];
        NSRectFillUsingOperation(cell, NSCompositeSourceOver);
        return YES;
    }
    if (ch == 0x2594) { NSRectFill(NSMakeRect(x, y, w, MAX(1, round(h / 8)))); return YES; }
    if (ch == 0x2595) { NSRectFill(NSMakeRect(x + w - MAX(1, round(w / 8)), y, MAX(1, round(w / 8)), h)); return YES; }
    if (ch >= 0x2596 && ch <= 0x259F) {
        /* Quadrants: upper left 1, upper right 2, lower left 4, lower right 8. */
        static const uint8_t quads[] = { 4, 8, 1, 1 | 4 | 8, 1 | 8, 1 | 2 | 4, 1 | 2 | 8, 2, 2 | 4, 2 | 4 | 8 };
        uint8_t q = quads[ch - 0x2596];
        CGFloat hw = round(w / 2), hh = round(h / 2);
        if (q & 1) NSRectFill(NSMakeRect(x, y, hw, hh));
        if (q & 2) NSRectFill(NSMakeRect(x + hw, y, w - hw, hh));
        if (q & 4) NSRectFill(NSMakeRect(x, y + hh, hw, h - hh));
        if (q & 8) NSRectFill(NSMakeRect(x + hw, y + hh, w - hw, h - hh));
        return YES;
    }
    return NO;
}

/* A run's text: plain ASCII in one go (the grid is the font's own
   advance), any other character in its own cell, so a wide one or one
   from a fallback font never pushes the rest off the grid. */
- (void)drawTextOf:(VTermScreenCell *)cells from:(int)start to:(int)end y:(CGFloat)y color:(NSColor *)fg
{
    VTermScreenCell *style = &cells[start];
    BOOL bold = style->attrs.bold, italic = style->attrs.italic;
    NSFont *base = bold ? (italic && _boldItalic ? _boldItalic : (_bold ?: _font)) : (italic && _italic ? _italic : _font);
    BOOL slant = italic && (bold ? _boldItalic == nil : _italic == nil);
    BOOL fakeBold = bold && _bold == nil;
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    attributes[NSForegroundColorAttributeName] = fg;
    if (style->attrs.underline)
        attributes[NSUnderlineStyleAttributeName] = @(style->attrs.underline == VTERM_UNDERLINE_DOUBLE ?
            NSUnderlineStyleDouble : NSUnderlineStyleSingle);
    if (style->attrs.strike)
        attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);

    for (int c = start; c < end;) {
        uint32_t ch = cells[c].chars[0];
        if (ch == (uint32_t)-1) {           /* behind a wide character */
            c++;
            continue;
        }
        BOOL ascii = (ch == 0 || (ch >= 0x20 && ch < 0x7F)) && cells[c].chars[1] == 0 && cells[c].width <= 1;
        if (ascii) {
            int e = c;
            NSMutableString *text = [NSMutableString string];
            while (e < end) {
                uint32_t k = cells[e].chars[0];
                if (!((k == 0 || (k >= 0x20 && k < 0x7F)) && cells[e].chars[1] == 0 && cells[e].width <= 1))
                    break;
                [text appendFormat:@"%c", k ? (char)k : ' '];
                e++;
            }
            if ([[text stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length]) {
                NSPoint p = NSMakePoint(INSET + c * _cellSize.width, y);
                [self drawString:text font:base at:p attributes:attributes slant:slant];
                if (fakeBold)
                    [self drawString:text font:base at:NSMakePoint(p.x + 0.6, p.y) attributes:attributes slant:slant];
            } else if (style->attrs.underline || style->attrs.strike)
                [self drawString:text font:base at:NSMakePoint(INSET + c * _cellSize.width, y)
                    attributes:attributes slant:NO];
            c = e;
            continue;
        }
        int width = MAX(1, cells[c].width);
        NSRect box = NSMakeRect(INSET + c * _cellSize.width, y, width * _cellSize.width, _cellSize.height);
        if (width == 1 && cells[c].chars[1] == 0 && ch >= 0x2500 && ch <= 0x259F &&
            [self drawBox:ch in:box color:fg]) {
            c++;
            continue;
        }
        NSFont *font = [self fontFor:ch base:base];
        NSString *text = stringOfCell(&cells[c]);
        [NSGraphicsContext saveGraphicsState];
        NSRectClip(NSInsetRect(box, -1, 0));
        /* A glyph wider than its cells is narrowed to them. */
        CGFloat advance = [text sizeWithAttributes:@{ NSFontAttributeName : font }].width;
        if (advance > NSWidth(box) + 1 && advance > 0) {
            NSAffineTransform *t = [NSAffineTransform transform];
            [t translateXBy:NSMinX(box) yBy:0];
            [t scaleXBy:NSWidth(box) / advance yBy:1];
            [t translateXBy:-NSMinX(box) yBy:0];
            [t concat];
        } else if (width > 1 || advance < NSWidth(box) - 1)
            box.origin.x += floor((NSWidth(box) - advance) / 2);
        [self drawString:text font:font at:box.origin attributes:attributes slant:slant && font == base];
        if (fakeBold && font == base)
            [self drawString:text font:font at:NSMakePoint(box.origin.x + 0.6, y) attributes:attributes slant:slant];
        [NSGraphicsContext restoreGraphicsState];
        c += width;
    }
}

- (void)drawLine:(NSInteger)line
{
    int columns = _session.columns;
    VTermScreenCell cells[columns];
    CGFloat y = INSET + line * _cellSize.height;
    int count = 0;

    for (int c = 0; c < columns; c++) {
        if (![self cell:&cells[c] line:line column:c]) {
            /* Past a short scrollback line: an empty cell in the defaults. */
            memset(&cells[c], 0, sizeof(VTermScreenCell));
            cells[c].fg = rgbOf(_foreground);
            cells[c].bg = rgbOf(_background);
            cells[c].fg.type |= VTERM_COLOR_DEFAULT_FG;
            cells[c].bg.type |= VTERM_COLOR_DEFAULT_BG;
        }
        count = c + 1;
    }

    /* Runs of cells of one style: their background, then their text. */
    for (int start = 0; start < count;) {
        int end = start + 1;
        while (end < count && sameStyle(&cells[start], &cells[end]) &&
            [self isSelectedLine:line column:end] == [self isSelectedLine:line column:start])
            end++;

        VTermScreenCell *cell = &cells[start];
        BOOL selected = [self isSelectedLine:line column:start];
        NSColor *fg = [self colorOf:cell->fg], *bg = [self colorOf:cell->bg];
        if (cell->attrs.reverse) {
            NSColor *t = fg;
            fg = bg;
            bg = t;
        }
        if (selected)
            bg = [NSColor selectedTextBackgroundColor];
        NSRect rect = NSMakeRect(INSET + start * _cellSize.width, y, (end - start) * _cellSize.width, _cellSize.height);
        if (selected || cell->attrs.reverse || !VTERM_COLOR_IS_DEFAULT_BG(&cell->bg)) {
            [bg setFill];
            NSRectFill(NSIntegralRect(rect));
        }

        if (!cell->attrs.conceal)
            [self drawTextOf:cells from:start to:end y:y color:fg];
        start = end;
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSInteger lines = (NSInteger)_session.scrollbackCount + _session.rows;
    NSInteger first = MAX(0, (NSInteger)floor((NSMinY(dirty) - INSET) / _cellSize.height));
    NSInteger last = MIN(lines - 1, (NSInteger)ceil((NSMaxY(dirty) - INSET) / _cellSize.height));

    [_background setFill];
    NSRectFill(dirty);
    for (NSInteger line = first; line <= last; line++)
        [self drawLine:line];
    /* The visual bell: the visible part, inverted for a moment. */
    if (_flashing) {
        [[_foreground colorWithAlphaComponent:0.35] setFill];
        NSRectFillUsingOperation(dirty, NSCompositeSourceOver);
    }

    /* The cursor, in the shape a program asked for or the preference's:
       solid while the view types (blinking if asked), an outline
       otherwise; a block shows its character in the background color. */
    if (_session.cursorVisible && !_session.exited && NSIntersectsRect(dirty, [self cursorRect])) {
        NSRect r = [self cursorRect];
        NSColor *accent = _cursorColor ?: [NSColor controlAccentColor];
        TMCursorShape shape = [self effectiveCursorShape];
        if ([self isTyping]) {
            if (!_cursorOff) {
                [accent setFill];
                if (shape == TMCursorUnderline)
                    NSRectFill(NSMakeRect(NSMinX(r), NSMaxY(r) - 2, NSWidth(r), 2));
                else if (shape == TMCursorBar)
                    NSRectFill(NSMakeRect(NSMinX(r), NSMinY(r), 2, NSHeight(r)));
                else {
                    NSRectFill(r);
                    VTermPos p = _session.cursor;
                    VTermScreenCell cell;
                    NSInteger line = (NSInteger)_session.scrollbackCount + p.row;
                    if ([self cell:&cell line:line column:p.col] && cell.chars[0] != 0 && cell.chars[0] != (uint32_t)-1) {
                        [NSGraphicsContext saveGraphicsState];
                        NSRectClip(r);
                        [self drawTextOf:&cell from:0 to:1 y:NSMinY(r) - 0 color:_background];
                        [NSGraphicsContext restoreGraphicsState];
                    }
                }
            }
        } else {
            [accent setStroke];
            [NSBezierPath strokeRect:NSInsetRect(r, 0.5, 0.5)];
        }
    }
}

- (NSRect)cursorRect
{
    VTermPos p = _session.cursor;
    return NSMakeRect(INSET + p.col * _cellSize.width,
        INSET + ((NSInteger)_session.scrollbackCount + p.row) * _cellSize.height, _cellSize.width, _cellSize.height);
}

- (TMCursorShape)effectiveCursorShape
{
    switch (_session.cursorShape) {
    case VTERM_PROP_CURSORSHAPE_BLOCK: return TMCursorBlock;
    case VTERM_PROP_CURSORSHAPE_UNDERLINE: return TMCursorUnderline;
    case VTERM_PROP_CURSORSHAPE_BAR_LEFT: return TMCursorBar;
    default: return _cursorShape;
    }
}

/* Typed into now: its window is key and it is the first responder. */
- (BOOL)isTyping
{
    return [[self window] isKeyWindow] && [[self window] firstResponder] == self;
}

/* ---- The blink ---- */

- (void)updateBlinkTimer
{
    [_blinkTimer invalidate];
    _blinkTimer = nil;
    _cursorOff = NO;
    if (_cursorBlink && [self window] != nil) {
        __weak TMTerminalView *weakSelf = self;
        _blinkTimer = [NSTimer timerWithTimeInterval:0.53 repeats:YES block:^(NSTimer *timer) {
            [weakSelf blink:timer];
        }];
        [[NSRunLoop currentRunLoop] addTimer:_blinkTimer forMode:NSRunLoopCommonModes];
    }
    if (_session != nil)
        [self setNeedsDisplayInRect:[self cursorRect]];
}

- (void)blink:(NSTimer *)timer
{
    BOOL off = [self isTyping] && !_cursorOff;
    if (off == _cursorOff)
        return;
    _cursorOff = off;
    [self setNeedsDisplayInRect:[self cursorRect]];
}

/* Typing shows the cursor at once and starts the blink over. */
- (void)restartBlink
{
    if (_blinkTimer == nil)
        return;
    [_blinkTimer setFireDate:[NSDate dateWithTimeIntervalSinceNow:0.53]];
    if (_cursorOff) {
        _cursorOff = NO;
        [self setNeedsDisplayInRect:[self cursorRect]];
    }
}

- (void)viewDidMoveToWindow
{
    [self updateBlinkTimer];
    if ([self window] != nil) {
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self selector:@selector(keyChanged:) name:NSWindowDidBecomeKeyNotification object:[self window]];
        [nc addObserver:self selector:@selector(keyChanged:) name:NSWindowDidResignKeyNotification object:[self window]];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    if ([self window] != nil) {
        [nc removeObserver:self name:NSWindowDidBecomeKeyNotification object:[self window]];
        [nc removeObserver:self name:NSWindowDidResignKeyNotification object:[self window]];
    }
    if (window == nil) {
        [_blinkTimer invalidate];
        _blinkTimer = nil;
    }
}

/* Solid or an outline as the window gains or loses the keyboard. */
- (void)keyChanged:(NSNotification *)note
{
    _cursorOff = NO;
    [self setNeedsDisplayInRect:[self cursorRect]];
}

- (BOOL)becomeFirstResponder
{
    [self keyChanged:nil];
    return YES;
}

- (BOOL)resignFirstResponder
{
    [self keyChanged:nil];
    return YES;
}

/* ---- The session's news ---- */

- (NSRect)rectOfRows:(int)first to:(int)last
{
    NSInteger scrollback = (NSInteger)_session.scrollbackCount;

    return NSMakeRect(0, INSET + (scrollback + first) * _cellSize.height, NSWidth([self bounds]),
        (last - first + 1) * _cellSize.height);
}

- (void)session:(TMSession *)session didDamageRect:(VTermRect)rect
{
    [self setNeedsDisplayInRect:[self rectOfRows:rect.start_row to:rect.end_row - 1]];
}

- (void)sessionDidMoveCursor:(TMSession *)session
{
    [self setNeedsDisplayInRect:[self rectOfRows:0 to:session.rows - 1]];
}

- (void)session:(TMSession *)session didChangeTitle:(NSString *)title
{
    if (self.titleChanged)
        self.titleChanged(self);
}

- (void)sessionDidRing:(TMSession *)session
{
    switch ([TMPreferences sharedPreferences].bell) {
    case TMBellSound:
        NSBeep();
        break;
    case TMBellFlash:
        _flashing = YES;
        [self setNeedsDisplay:YES];
        [self performSelector:@selector(endFlash) withObject:nil afterDelay:0.12];
        break;
    case TMBellNone:
        break;
    }
    _bellCount++;
}

- (void)endFlash
{
    _flashing = NO;
    [self setNeedsDisplay:YES];
}

- (void)sessionScrollbackDidChange:(TMSession *)session
{
    [self updateFrame];
    [self setNeedsDisplay:YES];
}

- (void)session:(TMSession *)session didExitWithStatus:(int)status
{
    [self setNeedsDisplay:YES];
    if (self.exited)
        self.exited(self, status);
}

/* ---- The keyboard ---- */

static VTermModifier modifiersOf(NSEvent *event)
{
    NSUInteger flags = [event modifierFlags];
    VTermModifier m = VTERM_MOD_NONE;

    if (flags & NSShiftKeyMask)
        m |= VTERM_MOD_SHIFT;
    if (flags & NSControlKeyMask)
        m |= VTERM_MOD_CTRL;
    return m;
}

/* ⌘= makes the text bigger too (the menu's ⌘+ needs Shift). */
- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    NSUInteger mods = [event modifierFlags] & (NSCommandKeyMask | NSAlternateKeyMask | NSControlKeyMask);
    if (mods == NSCommandKeyMask && [[event charactersIgnoringModifiers] isEqual:@"="] &&
        [[self window] firstResponder] == self)
        return [NSApp sendAction:@selector(biggerText:) to:nil from:self];
    return [super performKeyEquivalent:event];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *keys = [event charactersIgnoringModifiers];
    NSString *typed = [event characters];
    VTermModifier mods = modifiersOf(event);
    unichar key = [keys length] ? [keys characterAtIndex:0] : 0;
    VTermKey special = VTERM_KEY_NONE;

    if ([event modifierFlags] & NSCommandKeyMask) {
        [super keyDown:event];
        return;
    }
    [self restartBlink];
    switch (key) {
    case NSUpArrowFunctionKey: special = VTERM_KEY_UP; break;
    case NSDownArrowFunctionKey: special = VTERM_KEY_DOWN; break;
    case NSLeftArrowFunctionKey: special = VTERM_KEY_LEFT; break;
    case NSRightArrowFunctionKey: special = VTERM_KEY_RIGHT; break;
    case NSHomeFunctionKey: special = VTERM_KEY_HOME; break;
    case NSEndFunctionKey: special = VTERM_KEY_END; break;
    case NSPageUpFunctionKey: special = VTERM_KEY_PAGEUP; break;
    case NSPageDownFunctionKey: special = VTERM_KEY_PAGEDOWN; break;
    case NSInsertFunctionKey: special = VTERM_KEY_INS; break;
    case NSDeleteFunctionKey: special = VTERM_KEY_DEL; break;
    case '\r': case 3: special = VTERM_KEY_ENTER; break;
    case '\t': case 25: special = VTERM_KEY_TAB; break;
    case 0x7f: case '\b': special = VTERM_KEY_BACKSPACE; break;
    case 0x1b: special = VTERM_KEY_ESCAPE; break;
    default:
        if (key >= NSF1FunctionKey && key <= NSF35FunctionKey)
            special = VTERM_KEY_FUNCTION(key - NSF1FunctionKey + 1);
        break;
    }
    _hasSelection = NO;
    if (special != VTERM_KEY_NONE)
        [_session sendKey:special modifiers:mods];
    else if (mods & VTERM_MOD_CTRL)
        [_session sendCharacter:key modifiers:mods];
    else {
        /* Option's characters as typed (é, ñ, @ on some layouts). */
        NSString *text = [typed length] ? typed : keys;
        for (NSUInteger i = 0; i < [text length];) {
            NSRange r = [text rangeOfComposedCharacterSequenceAtIndex:i];
            NSData *u32 = [[text substringWithRange:r] dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
            const uint32_t *chars = [u32 bytes];
            for (NSUInteger k = 0; k < [u32 length] / 4; k++)
                [_session sendCharacter:chars[k] modifiers:VTERM_MOD_NONE];
            i = NSMaxRange(r);
        }
    }
    [self scrollToBottom];
}

- (void)scrollToBottom
{
    NSClipView *clip = [[self enclosingScrollView] contentView];

    if (clip != nil)
        [self scrollPoint:NSMakePoint(0, NSHeight([self frame]) - [clip bounds].size.height)];
}

/* ---- Selection, copy, paste ---- */

- (TMPoint)pointOf:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger lines = (NSInteger)_session.scrollbackCount + _session.rows;
    TMPoint t;

    t.line = MAX(0, MIN(lines - 1, (NSInteger)floor((p.y - INSET) / _cellSize.height)));
    t.column = MAX(0, MIN(_session.columns - 1, (int)floor((p.x - INSET) / _cellSize.width)));
    return t;
}

- (void)mouseDown:(NSEvent *)event
{
    [[self window] makeFirstResponder:self];
    _anchor = _head = [self pointOf:event];
    _hasSelection = NO;
    if ([event clickCount] == 2) {
        /* A word: the run of non-space cells around the click. */
        VTermScreenCell cell;
        int start = _anchor.column, end = _anchor.column;
        while (start > 0 && [self cell:&cell line:_anchor.line column:start - 1] && cell.chars[0] > ' ')
            start--;
        while (end < _session.columns - 1 && [self cell:&cell line:_anchor.line column:end + 1] && cell.chars[0] > ' ')
            end++;
        _anchor.column = start;
        _head.column = end;
        _hasSelection = YES;
    } else if ([event clickCount] >= 3) {
        _anchor.column = 0;
        _head.column = _session.columns - 1;
        _hasSelection = YES;
    }
    _selecting = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (!_selecting)
        return;
    _head = [self pointOf:event];
    _hasSelection = YES;
    [self autoscroll:event];
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    _selecting = NO;
}

- (NSString *)selectedText
{
    if (!_hasSelection)
        return @"";
    TMPoint start = before(_anchor, _head) ? _anchor : _head, end = before(_anchor, _head) ? _head : _anchor;
    return [_session textFromLine:start.line column:start.column toLine:end.line column:end.column];
}

- (IBAction)copy:(id)sender
{
    NSString *text = [self selectedText];
    NSPasteboard *board = [NSPasteboard generalPasteboard];

    if ([text length] == 0) {
        NSBeep();
        return;
    }
    [board declareTypes:@[ NSStringPboardType ] owner:nil];
    [board setString:text forType:NSStringPboardType];
}

- (IBAction)paste:(id)sender
{
    NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];

    if ([text length])
        [_session paste:text];
    [self scrollToBottom];
}

- (IBAction)selectAll:(id)sender
{
    _anchor = (TMPoint){ 0, 0 };
    _head = (TMPoint){ (NSInteger)_session.scrollbackCount + _session.rows - 1, _session.columns - 1 };
    _hasSelection = YES;
    [self setNeedsDisplay:YES];
}

- (IBAction)clearScrollback:(id)sender
{
    _hasSelection = NO;
    [_session clearScrollback];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(copy:))
        return _hasSelection;
    return YES;
}

@end
