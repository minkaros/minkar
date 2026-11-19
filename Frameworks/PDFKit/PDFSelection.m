/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * As the reference's (measured 19 Nov 2026): ranges kept per page in
 * document order, merged when they touch.
 */

#import "PDFKitPrivate.h"

@implementation PDFSelection {
    __weak PDFDocument *_document;
    NSMutableArray<PDFPage *> *_pages;              // in document order
    NSMapTable<PDFPage *, NSMutableIndexSet *> *_characters;
}

- (instancetype)initWithDocument:(PDFDocument *)document {
    if ((self = [super init])) {
        _document = document;
        _pages = [NSMutableArray array];
        _characters = [NSMapTable strongToStrongObjectsMapTable];
    }
    return self;
}

- (instancetype)init {
    return [self initWithDocument:(PDFDocument *_Nonnull)nil];
}

- (id)copyWithZone:(NSZone *)zone {
    PDFSelection *copy = [[PDFSelection allocWithZone:zone] initWithDocument:_document];

    for (PDFPage *page in _pages)
        [copy _addRange:NSMakeRange(0, 0) indexes:[_characters objectForKey:page] onPage:page];
    copy.color = _color;
    return copy;
}

- (NSUInteger)_indexOfPage:(PDFPage *)page {
    PDFDocument *document = _document ?: page.document;
    NSUInteger index = document ? [document indexForPage:page] : NSNotFound;

    return index;
}

/* Characters on a page added, the page placed in document order. */
- (void)_addRange:(NSRange)range indexes:(NSIndexSet *)indexes onPage:(PDFPage *)page {
    NSMutableIndexSet *set = [_characters objectForKey:page];

    if (page == nil)
        return;
    if (set == nil) {
        NSUInteger mine = [self _indexOfPage:page], i;

        set = [NSMutableIndexSet indexSet];
        [_characters setObject:set forKey:page];
        for (i = 0; i < _pages.count && [self _indexOfPage:_pages[i]] < mine; i++)
            ;
        [_pages insertObject:page atIndex:i];
    }
    if (range.length > 0)
        [set addIndexesInRange:range];
    if (indexes != nil)
        [set addIndexes:indexes];
    if (set.count == 0) {
        [_characters removeObjectForKey:page];
        [_pages removeObjectIdenticalTo:page];
    }
}

- (void)_addRange:(NSRange)range onPage:(PDFPage *)page {
    NSUInteger count = page.numberOfCharacters;

    if (range.location >= count)
        return;
    range.length = MIN(range.length, count - range.location);
    [self _addRange:range indexes:nil onPage:page];
}

- (NSArray<PDFPage *> *)pages {
    return [_pages copy];
}

- (NSString *)string {
    NSMutableArray *pages = [NSMutableArray array];

    for (PDFPage *page in _pages) {
        NSMutableArray *ranges = [NSMutableArray array];
        NSString *text = page.string;

        for (NSUInteger i = 0; i < [self numberOfTextRangesOnPage:page]; i++)
            [ranges addObject:[text substringWithRange:[self rangeAtIndex:i onPage:page]]];
        [pages addObject:[ranges componentsJoinedByString:@"\r"]];
    }
    return (pages.count > 0) ? [pages componentsJoinedByString:@"\n"] : nil;
}

- (NSAttributedString *)attributedString {
    NSString *string = self.string;

    return (string != nil) ? [[NSAttributedString alloc] initWithString:string] : nil;
}

- (PDFRect)boundsForPage:(PDFPage *)page {
    NSIndexSet *set = [_characters objectForKey:page];
    __block CGRect bounds = CGRectNull;

    [set enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *stop) {
        bounds = CGRectUnion(bounds, [page characterBoundsAtIndex:index]);
    }];
    return CGRectIsNull(bounds) ? CGRectZero : bounds;
}

- (NSUInteger)numberOfTextRangesOnPage:(PDFPage *)page {
    __block NSUInteger count = 0;

    [[_characters objectForKey:page] enumerateRangesUsingBlock:^(NSRange range, BOOL *stop) { count++; }];
    return count;
}

- (NSRange)rangeAtIndex:(NSUInteger)index onPage:(PDFPage *)page {
    __block NSUInteger at = 0;
    __block NSRange result = NSMakeRange(NSNotFound, 0);

    [[_characters objectForKey:page] enumerateRangesUsingBlock:^(NSRange range, BOOL *stop) {
        if (at++ == index) {
            result = range;
            *stop = YES;
        }
    }];
    return result;
}

/* A selection per line: its characters on one line, without the line's
   separator. */
- (NSArray<PDFSelection *> *)selectionsByLine {
    NSMutableArray *lines = [NSMutableArray array];

    for (PDFPage *page in _pages) {
        NSString *text = page.string;
        __block PDFSelection *line = nil;
        __block CGFloat baseline = 0;

        [[_characters objectForKey:page] enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *stop) {
            unichar c = [text characterAtIndex:index];
            CGRect box = [page characterBoundsAtIndex:index];
            BOOL separator = (c == '\n') || (c == ' ' && box.size.width == 0);

            if (line != nil && fabs(CGRectGetMinY(box) - baseline) > box.size.height / 2) {
                [lines addObject:line];
                line = nil;
            }
            if (separator)
                return;
            if (line == nil) {
                line = [[PDFSelection alloc] initWithDocument:self->_document];
                baseline = CGRectGetMinY(box);
            }
            [line _addRange:NSMakeRange(index, 1) onPage:page];
        }];
        if (line != nil)
            [lines addObject:line];
    }
    return lines;
}

- (void)addSelection:(PDFSelection *)selection {
    for (PDFPage *page in selection->_pages)
        [self _addRange:NSMakeRange(0, 0) indexes:[selection->_characters objectForKey:page] onPage:page];
}

- (void)addSelections:(NSArray<PDFSelection *> *)selections {
    for (PDFSelection *selection in selections)
        [self addSelection:selection];
}

/* The last range made longer (shorter if negative) by count characters. */
- (void)extendSelectionAtEnd:(NSInteger)count {
    PDFPage *page = _pages.lastObject;
    NSMutableIndexSet *set = [_characters objectForKey:page];

    if (set.count == 0 || count == 0)
        return;
    NSUInteger last = set.lastIndex;
    if (count > 0)
        [self _addRange:NSMakeRange(last + 1, (NSUInteger)count) onPage:page];
    else
        [set removeIndexesInRange:NSMakeRange(last + 1 - MIN((NSUInteger)-count, last + 1), MIN((NSUInteger)-count, last + 1))];
}

- (void)extendSelectionAtStart:(NSInteger)count {
    PDFPage *page = _pages.firstObject;
    NSMutableIndexSet *set = [_characters objectForKey:page];

    if (set.count == 0 || count == 0)
        return;
    NSUInteger first = set.firstIndex;
    if (count > 0) {
        NSUInteger start = (first >= (NSUInteger)count) ? first - count : 0;
        [self _addRange:NSMakeRange(start, first - start) onPage:page];
    } else
        [set removeIndexesInRange:NSMakeRange(first, (NSUInteger)-count)];
}

/* Its characters' boxes on the page, filled with its color (the system's
   selection color, lighter when not active). */
- (void)drawForPage:(PDFPage *)page withBox:(PDFDisplayBox)box active:(BOOL)active {
    NSIndexSet *set = [_characters objectForKey:page];
    NSColor *color = _color ?: (active ? [NSColor selectedTextBackgroundColor] : [NSColor secondarySelectedControlColor]);

    if (set.count == 0)
        return;
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    if (context == NULL)
        return;
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, kCGBlendModeMultiply);
    CGContextSetFillColorWithColor(context, color.CGColor);
    [set enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *stop) {
        CGRect r = [page characterBoundsAtIndex:index];

        if (r.size.width > 0)
            CGContextFillRect(context, r);
    }];
    CGContextRestoreGState(context);
}

- (void)drawForPage:(PDFPage *)page active:(BOOL)active {
    [self drawForPage:page withBox:kPDFDisplayBoxCropBox active:active];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p \"%@\">", [self class], self, self.string];
}

@end
