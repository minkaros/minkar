/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMTerminalView.h"
#import "TMPreferences.h"

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
    NSFont *_font, *_bold;
    CGFloat _ascent;
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

    [self setFontSize:prefs.fontSize];
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
        NSColor *c = [[prefs paletteColorAtIndex:i] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
        [_session setPaletteIndex:i red:(uint8_t)lround([c redComponent] * 255)
            green:(uint8_t)lround([c greenComponent] * 255) blue:(uint8_t)lround([c blueComponent] * 255)];
    }
    _session.scrollbackLimit = (NSUInteger)prefs.scrollbackLines;
    if (prefs.fontSize != _fontSize)
        [self setFontSize:prefs.fontSize];
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
    NSColor *rgb = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace] ?: [NSColor blackColor];
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
    NSFont *font = [NSFont fontWithName:@"Agave-Regular" size:size] ?: [NSFont userFixedPitchFontOfSize:size];
    NSDictionary *attributes = @{ NSFontAttributeName : font };

    _fontSize = size;
    _font = font;
    _bold = [NSFont fontWithName:@"Agave-Bold" size:size] ?: font;
    _cellSize = NSMakeSize(ceil([@"MMMMMMMMMM" sizeWithAttributes:attributes].width) / 10.0,
        ceil([font ascender] - [font descender] + [font leading]));
    if (_cellSize.height < size)
        _cellSize.height = ceil(size * 1.25);
    _ascent = ceil([font ascender]);
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
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1];
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

        NSMutableString *text = [NSMutableString string];
        for (int c = start; c < end; c++) {
            if (cells[c].chars[0] == 0) {
                if (!(c > 0 && cells[c - 1].width == 2))
                    [text appendString:@" "];
                continue;
            }
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cells[c].chars[i]; i++) {
                uint32_t ch = cells[c].chars[i];
                [text appendString:[[NSString alloc] initWithBytes:&ch length:4 encoding:NSUTF32LittleEndianStringEncoding] ?: @"?"];
            }
        }
        NSString *trimmed = [text stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if ([trimmed length] && !cell->attrs.conceal) {
            NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
            attributes[NSFontAttributeName] = cell->attrs.bold ? _bold : _font;
            attributes[NSForegroundColorAttributeName] = fg;
            if (cell->attrs.underline)
                attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
            if (cell->attrs.strike)
                attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);
            [text drawAtPoint:NSMakePoint(rect.origin.x, y) withAttributes:attributes];
        }
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

    /* The cursor: a block while the window is key, an outline otherwise. */
    if (_session.cursorVisible && !_session.exited) {
        VTermPos p = _session.cursor;
        NSRect r = NSMakeRect(INSET + p.col * _cellSize.width,
            INSET + ((NSInteger)_session.scrollbackCount + p.row) * _cellSize.height, _cellSize.width, _cellSize.height);
        NSColor *accent = _cursorColor ?: [NSColor controlAccentColor];
        if ([[self window] isKeyWindow] && [[self window] firstResponder] == self) {
            [[accent colorWithAlphaComponent:0.55] setFill];
            NSRectFillUsingOperation(r, NSCompositeSourceOver);
        } else {
            [accent setStroke];
            [NSBezierPath strokeRect:NSInsetRect(r, 0.5, 0.5)];
        }
    }
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
