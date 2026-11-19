/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A PDF document over CoreGraphics' (CGPDFDocument), read as the reference
 * reads it (measured 19 Nov 2026): page labels from PageLabels, the outline
 * with its destinations and actions, the info dictionary's attributes.
 */

#import "PDFKitPrivate.h"
#import <objc/message.h>
#import <Onyx2D/O2PDFRaw.h>
#import <Onyx2D/O2PDFDocument.h>

NSNotificationName const PDFDocumentDidUnlockNotification = @"PDFDocumentDidUnlock";
NSNotificationName const PDFDocumentDidBeginFindNotification = @"PDFDidBeginDocumentFind";
NSNotificationName const PDFDocumentDidEndFindNotification = @"PDFDidEndDocumentFind";
NSNotificationName const PDFDocumentDidBeginPageFindNotification = @"PDFDidBeginPageFind";
NSNotificationName const PDFDocumentDidEndPageFindNotification = @"PDFDidEndPageFind";
NSNotificationName const PDFDocumentDidFindMatchNotification = @"PDFDidFindMatch";
NSNotificationName const PDFDocumentDidBeginWriteNotification = @"PDFDidBeginDocumentWrite";
NSNotificationName const PDFDocumentDidEndWriteNotification = @"PDFDidEndDocumentWrite";
NSNotificationName const PDFDocumentDidBeginPageWriteNotification = @"PDFDidBeginPageWrite";
NSNotificationName const PDFDocumentDidEndPageWriteNotification = @"PDFDidEndPageWrite";

PDFDocumentAttribute const PDFDocumentTitleAttribute = @"Title";
PDFDocumentAttribute const PDFDocumentAuthorAttribute = @"Author";
PDFDocumentAttribute const PDFDocumentSubjectAttribute = @"Subject";
PDFDocumentAttribute const PDFDocumentCreatorAttribute = @"Creator";
PDFDocumentAttribute const PDFDocumentProducerAttribute = @"Producer";
PDFDocumentAttribute const PDFDocumentCreationDateAttribute = @"CreationDate";
PDFDocumentAttribute const PDFDocumentModificationDateAttribute = @"ModDate";
PDFDocumentAttribute const PDFDocumentKeywordsAttribute = @"Keywords";

PDFDocumentWriteOption const PDFDocumentOwnerPasswordOption = @"PDFDocumentOwnerPasswordOption";
PDFDocumentWriteOption const PDFDocumentUserPasswordOption = @"PDFDocumentUserPasswordOption";

NSString *PDFKitString(CGPDFStringRef string) {
    return (string != NULL) ? (__bridge_transfer NSString *)CGPDFStringCopyTextString(string) : nil;
}

/* An NSDate made from the CFDate (not bridged toll-free in Minkar). */
NSDate *PDFKitDate(CGPDFStringRef string) {
    CFDateRef date = (string != NULL) ? CGPDFStringCopyDate(string) : NULL;
    NSDate *result;

    if (date == NULL)
        return nil;
    result = [NSDate dateWithTimeIntervalSinceReferenceDate:CFDateGetAbsoluteTime(date)];
    CFRelease(date);
    return result;
}

/* ---- page labels ---- */

static NSString *roman(NSInteger value, BOOL upper) {
    static const struct { NSInteger value; const char *digits; } table[] = {
        {1000, "m"}, {900, "cm"}, {500, "d"}, {400, "cd"}, {100, "c"}, {90, "xc"},
        {50, "l"}, {40, "xl"}, {10, "x"}, {9, "ix"}, {5, "v"}, {4, "iv"}, {1, "i"}};
    NSMutableString *result = [NSMutableString string];

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        while (value >= table[i].value) {
            [result appendString:@(table[i].digits)];
            value -= table[i].value;
        }
    return upper ? result.uppercaseString : result;
}

/* a..z, then aa..zz, aaa..: the letter repeated (PDF 1.7, 12.4.2). */
static NSString *letters(NSInteger value, BOOL upper) {
    if (value < 1)
        return @"";
    unichar letter = (unichar)((upper ? 'A' : 'a') + (value - 1) % 26);
    NSInteger count = (value - 1) / 26 + 1;

    return [@"" stringByPaddingToLength:count withString:[NSString stringWithCharacters:&letter length:1] startingAtIndex:0];
}

/* The number tree's entries (key, value dictionary), in order. */
static void collectNumberTree(CGPDFDictionaryRef node, NSMutableArray *entries, int depth) {
    CGPDFArrayRef nums = NULL, kids = NULL;

    if (node == NULL || depth > 32)
        return;
    if (CGPDFDictionaryGetArray(node, "Nums", &nums)) {
        for (size_t i = 0; i + 1 < CGPDFArrayGetCount(nums); i += 2) {
            CGPDFInteger key;
            CGPDFDictionaryRef value;

            if (CGPDFArrayGetInteger(nums, i, &key) && CGPDFArrayGetDictionary(nums, i + 1, &value))
                [entries addObject:@[ @(key), [NSValue valueWithPointer:value] ]];
        }
    }
    if (CGPDFDictionaryGetArray(node, "Kids", &kids))
        for (size_t i = 0; i < CGPDFArrayGetCount(kids); i++) {
            CGPDFDictionaryRef kid;

            if (CGPDFArrayGetDictionary(kids, i, &kid))
                collectNumberTree(kid, entries, depth + 1);
        }
}

@implementation PDFDocument {
    CGPDFDocumentRef _documentRef;
    NSData *_data;
    NSMutableArray *_pages;          // PDFPage, or NSNull for one not made yet
    NSArray *_pageLabelRanges;       // (start index, label dictionary), sorted
    BOOL _outlineRead;
    BOOL _attributesRead;
    BOOL _changed;                   // pages added, removed or moved
    NSMapTable *_pageIndexByDictionary;
    NSDictionary *_documentAttributes;
    PDFOutline *_outlineRoot;
    BOOL _isFinding;
    BOOL _annotationsChanged;
    BOOL _findCancelled;
}

- (instancetype)init {
    if ((self = [super init]))
        _pages = [NSMutableArray array];
    return self;
}

- (instancetype)_initWithData:(NSData *)data URL:(NSURL *)url {
    CGDataProviderRef provider;

    if (data == nil || (self = [self init]) == nil)
        return nil;
    provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    _documentRef = CGPDFDocumentCreateWithProvider(provider);
    CGDataProviderRelease(provider);
    /* A document that is not a PDF, or has no page, is none (as on the
       reference). */
    if (_documentRef == NULL || (!CGPDFDocumentIsEncrypted(_documentRef) && CGPDFDocumentGetNumberOfPages(_documentRef) == 0))
        return nil;
    _data = data;
    _documentURL = [url copy];
    for (size_t i = 0; i < CGPDFDocumentGetNumberOfPages(_documentRef); i++)
        [_pages addObject:[NSNull null]];
    return self;
}

- (instancetype)initWithURL:(NSURL *)url {
    NSData *data = url.isFileURL ? [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:NULL] : nil;

    return [self _initWithData:data URL:url];
}

- (instancetype)initWithData:(NSData *)data {
    return [self _initWithData:data URL:nil];
}

- (void)dealloc {
    if (_documentRef != NULL)
        CGPDFDocumentRelease(_documentRef);
}

- (id)copyWithZone:(NSZone *)zone {
    NSData *data = [self dataRepresentation];

    return (data != nil) ? [[[self class] allocWithZone:zone] initWithData:data] : [[[self class] allocWithZone:zone] init];
}

- (CGPDFDocumentRef)documentRef {
    return _documentRef;
}

/* ---- version, encryption, permissions ---- */

- (NSInteger)majorVersion {
    int major = 0, minor = 0;

    if (_documentRef != NULL)
        CGPDFDocumentGetVersion(_documentRef, &major, &minor);
    return major;
}

- (NSInteger)minorVersion {
    int major = 0, minor = 0;

    if (_documentRef != NULL)
        CGPDFDocumentGetVersion(_documentRef, &major, &minor);
    return minor;
}

- (BOOL)isEncrypted {
    return _documentRef != NULL && CGPDFDocumentIsEncrypted(_documentRef);
}

- (BOOL)isLocked {
    return _documentRef != NULL && !CGPDFDocumentIsUnlocked(_documentRef);
}

- (BOOL)unlockWithPassword:(NSString *)password {
    if (_documentRef == NULL || !self.isLocked)
        return _documentRef != NULL;
    if (!CGPDFDocumentUnlockWithPassword(_documentRef, password.UTF8String ?: ""))
        return NO;
    NSNotification *note = [NSNotification notificationWithName:PDFDocumentDidUnlockNotification object:self];
    [[NSNotificationCenter defaultCenter] postNotification:note];
    if ([_delegate respondsToSelector:@selector(documentDidUnlock:)])
        [_delegate documentDidUnlock:note];
    return YES;
}

/* What an encrypted document allows (its P entry, PDF 1.7 table 22: bit 3
   printing, 5 copying, read through CoreGraphics; the others follow
   whether it is unlocked, as Minkar does not decrypt yet); an open one
   allows everything. */
- (BOOL)_allows:(int)bit {
    if (!self.isEncrypted)
        return YES;
    if (bit == 3)
        return CGPDFDocumentAllowsPrinting(_documentRef);
    if (bit == 5)
        return CGPDFDocumentAllowsCopying(_documentRef);
    return !self.isLocked;
}

- (BOOL)allowsPrinting { return [self _allows:3]; }
- (BOOL)allowsCopying { return [self _allows:5]; }
- (BOOL)allowsDocumentChanges { return [self _allows:4]; }
- (BOOL)allowsCommenting { return [self _allows:6]; }
- (BOOL)allowsFormFieldEntry { return [self _allows:9]; }
- (BOOL)allowsContentAccessibility { return [self _allows:10]; }
- (BOOL)allowsDocumentAssembly { return [self _allows:11]; }

- (PDFDocumentPermissions)permissionsStatus {
    if (self.isLocked)
        return kPDFDocumentPermissionsNone;
    return self.isEncrypted ? kPDFDocumentPermissionsUser : kPDFDocumentPermissionsOwner;
}

/* ---- attributes ---- */

- (NSDictionary *)documentAttributes {
    if (!_attributesRead) {
        NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
        CGPDFDictionaryRef info = (_documentRef != NULL) ? CGPDFDocumentGetInfo(_documentRef) : NULL;
        NSDictionary *strings = @{ PDFDocumentTitleAttribute: @"Title", PDFDocumentAuthorAttribute: @"Author",
                                   PDFDocumentSubjectAttribute: @"Subject", PDFDocumentCreatorAttribute: @"Creator",
                                   PDFDocumentProducerAttribute: @"Producer" };
        CGPDFStringRef string;

        for (NSString *attribute in strings)
            if (CGPDFDictionaryGetString(info, [strings[attribute] UTF8String], &string) && PDFKitString(string) != nil)
                attributes[attribute] = PDFKitString(string);
        if (CGPDFDictionaryGetString(info, "CreationDate", &string) && PDFKitDate(string) != nil)
            attributes[PDFDocumentCreationDateAttribute] = PDFKitDate(string);
        if (CGPDFDictionaryGetString(info, "ModDate", &string) && PDFKitDate(string) != nil)
            attributes[PDFDocumentModificationDateAttribute] = PDFKitDate(string);
        /* Keywords: an array, the string whole. */
        if (CGPDFDictionaryGetString(info, "Keywords", &string) && PDFKitString(string) != nil)
            attributes[PDFDocumentKeywordsAttribute] = @[ PDFKitString(string) ];
        _documentAttributes = attributes;
        _attributesRead = YES;
    }
    return _documentAttributes;
}

- (void)setDocumentAttributes:(NSDictionary *)attributes {
    _documentAttributes = [attributes copy];
    _attributesRead = YES;
    _changed = YES;
}

/* ---- pages ---- */

- (Class)pageClass {
    if ([_delegate respondsToSelector:@selector(classForPage)])
        return [_delegate classForPage];
    return [PDFPage class];
}

- (NSUInteger)pageCount {
    return _pages.count;
}

- (PDFPage *)pageAtIndex:(NSUInteger)index {
    id page;

    if (index >= _pages.count)
        return nil;
    page = _pages[index];
    if (page == [NSNull null]) {
        /* Its CoreGraphics page: the document's page index + 1, those
           still in their place. */
        CGPDFPageRef pageRef = CGPDFDocumentGetPage(_documentRef, index + 1);

        if (pageRef == NULL)
            return nil;
        page = [[self.pageClass alloc] _initWithDocument:self pageRef:pageRef];
        _pages[index] = page;
    }
    return page;
}

- (NSUInteger)indexForPage:(PDFPage *)page {
    NSUInteger index = (page != nil) ? [_pages indexOfObjectIdenticalTo:page] : NSNotFound;

    return index;
}

/* Every page made, so moving them keeps each its CoreGraphics page. */
- (void)_makeAllPages {
    for (NSUInteger i = 0; i < _pages.count; i++)
        [self pageAtIndex:i];
}

- (void)insertPage:(PDFPage *)page atIndex:(NSUInteger)index {
    if (page == nil || index > _pages.count)
        return;
    [self _makeAllPages];
    [page.document removePageAtIndex:[page.document indexForPage:page]];
    [_pages insertObject:page atIndex:index];
    [page _setDocument:self];
    _changed = YES;
    _pageIndexByDictionary = nil;
}

- (void)removePageAtIndex:(NSUInteger)index {
    if (index >= _pages.count)
        return;
    [self _makeAllPages];
    [_pages[index] _setDocument:nil];
    [_pages removeObjectAtIndex:index];
    _changed = YES;
    _pageIndexByDictionary = nil;
}

- (void)exchangePageAtIndex:(NSUInteger)indexA withPageAtIndex:(NSUInteger)indexB {
    if (indexA >= _pages.count || indexB >= _pages.count)
        return;
    [self _makeAllPages];
    [_pages exchangeObjectAtIndex:indexA withObjectAtIndex:indexB];
    _changed = YES;
    _pageIndexByDictionary = nil;
}

- (void)_annotationsChanged {
    _annotationsChanged = YES;
}

/* ---- text ---- */

- (NSString *)string {
    NSMutableArray *strings = [NSMutableArray array];

    for (NSUInteger i = 0; i < self.pageCount; i++)
        [strings addObject:[self pageAtIndex:i].string ?: @""];
    return (strings.count > 0) ? [strings componentsJoinedByString:@"\n"] : nil;
}

/* ---- finding and selecting ---- */

- (NSArray<PDFSelection *> *)_matchesOf:(NSString *)string onPage:(PDFPage *)page options:(NSStringCompareOptions)options {
    NSMutableArray *matches = [NSMutableArray array];
    NSString *text = page.string;
    NSRange search = NSMakeRange(0, text.length), found;

    options &= ~(NSStringCompareOptions)NSBackwardsSearch;
    if (string.length == 0 || text == nil)
        return matches;
    while (search.length > 0 && (found = [text rangeOfString:string options:options range:search]).location != NSNotFound) {
        PDFSelection *selection = [[PDFSelection alloc] initWithDocument:self];

        [selection _addRange:found onPage:page];
        [matches addObject:selection];
        search = NSMakeRange(NSMaxRange(found), text.length - NSMaxRange(found));
    }
    return matches;
}

- (NSArray<PDFSelection *> *)findString:(NSString *)string withOptions:(NSStringCompareOptions)options {
    NSMutableArray *matches = [NSMutableArray array];

    for (NSUInteger i = 0; i < self.pageCount; i++)
        [matches addObjectsFromArray:[self _matchesOf:string onPage:[self pageAtIndex:i] options:options]];
    if (options & NSBackwardsSearch)
        return matches.reverseObjectEnumerator.allObjects;
    return matches;
}

/* A selection's place: its first page's index and first character (its
   last, for searching backwards from it). */
- (BOOL)_placeOf:(PDFSelection *)selection page:(NSUInteger *)pageIndex character:(NSUInteger *)character last:(BOOL)last {
    PDFPage *page = last ? selection.pages.lastObject : selection.pages.firstObject;
    NSUInteger ranges = page ? [selection numberOfTextRangesOnPage:page] : 0;

    if (ranges == 0)
        return NO;
    NSRange range = [selection rangeAtIndex:last ? ranges - 1 : 0 onPage:page];
    *pageIndex = [self indexForPage:page];
    *character = last ? NSMaxRange(range) : range.location;
    return *pageIndex != NSNotFound;
}

- (PDFSelection *)findString:(NSString *)string fromSelection:(PDFSelection *)selection withOptions:(NSStringCompareOptions)options {
    BOOL backwards = (options & NSBackwardsSearch) != 0;
    NSUInteger fromPage = 0, fromCharacter = 0;
    BOOL placed = (selection != nil) && [self _placeOf:selection page:&fromPage character:&fromCharacter last:!backwards];

    for (PDFSelection *match in [self findString:string withOptions:options]) {
        NSUInteger page, character;

        if (![self _placeOf:match page:&page character:&character last:NO])
            continue;
        if (!placed)
            return match;
        if (!backwards && (page > fromPage || (page == fromPage && character >= fromCharacter)))
            return match;
        if (backwards && (page < fromPage || (page == fromPage && character < fromCharacter)))
            return match;
    }
    return nil;
}

- (void)_post:(NSNotificationName)name selector:(SEL)selector info:(NSDictionary *)info {
    NSNotification *note = [NSNotification notificationWithName:name object:self userInfo:info];

    [[NSNotificationCenter defaultCenter] postNotification:note];
    if ([_delegate respondsToSelector:selector])
        ((void (*)(id, SEL, id))objc_msgSend)(_delegate, selector, note);
}

/* Done at once, told as the reference tells it: begin, each page (its
   index), each match (the selection; the delegate's didMatchString: too),
   end. */
- (void)beginFindStrings:(NSArray<NSString *> *)strings withOptions:(NSStringCompareOptions)options {
    _isFinding = YES;
    _findCancelled = NO;
    [self _post:PDFDocumentDidBeginFindNotification selector:@selector(documentDidBeginDocumentFind:) info:nil];
    for (NSUInteger i = 0; i < self.pageCount && !_findCancelled; i++) {
        NSDictionary *pageInfo = @{ @"PDFDocumentPageIndex": @(i) };

        [self _post:PDFDocumentDidBeginPageFindNotification selector:@selector(documentDidBeginPageFind:) info:pageInfo];
        for (NSString *string in strings)
            for (PDFSelection *match in [self _matchesOf:string onPage:[self pageAtIndex:i] options:options]) {
                if (_findCancelled)
                    break;
                if ([_delegate respondsToSelector:@selector(didMatchString:)])
                    [_delegate didMatchString:match];
                [self _post:PDFDocumentDidFindMatchNotification selector:@selector(documentDidFindMatch:) info:@{ @"PDFDocumentFoundSelection": match }];
            }
        [self _post:PDFDocumentDidEndPageFindNotification selector:@selector(documentDidEndPageFind:) info:pageInfo];
    }
    _isFinding = NO;
    [self _post:PDFDocumentDidEndFindNotification selector:@selector(documentDidEndDocumentFind:) info:nil];
}

- (void)beginFindString:(NSString *)string withOptions:(NSStringCompareOptions)options {
    [self beginFindStrings:string ? @[ string ] : @[] withOptions:options];
}

- (void)cancelFindString {
    _findCancelled = YES;
}

- (PDFSelection *)selectionForEntireDocument {
    PDFSelection *selection = [[PDFSelection alloc] initWithDocument:self];

    for (NSUInteger i = 0; i < self.pageCount; i++) {
        PDFPage *page = [self pageAtIndex:i];

        [selection _addRange:NSMakeRange(0, page.numberOfCharacters) onPage:page];
    }
    return (selection.pages.count > 0) ? selection : nil;
}

/* From a character on one page to one on another: the pages between whole. */
- (PDFSelection *)selectionFromPage:(PDFPage *)startPage atCharacterIndex:(NSUInteger)startCharacter toPage:(PDFPage *)endPage atCharacterIndex:(NSUInteger)endCharacter {
    NSUInteger first = [self indexForPage:startPage], last = [self indexForPage:endPage];
    PDFSelection *selection = [[PDFSelection alloc] initWithDocument:self];

    if (first == NSNotFound || last == NSNotFound)
        return nil;
    if (first > last || (first == last && startCharacter > endCharacter)) {
        NSUInteger t = first; first = last; last = t;
        t = startCharacter; startCharacter = endCharacter; endCharacter = t;
    }
    for (NSUInteger i = first; i <= last; i++) {
        PDFPage *page = [self pageAtIndex:i];
        NSUInteger from = (i == first) ? startCharacter : 0;
        NSUInteger to = (i == last) ? MIN(endCharacter + 1, page.numberOfCharacters) : page.numberOfCharacters;

        if (to > from)
            [selection _addRange:NSMakeRange(from, to - from) onPage:page];
    }
    return (selection.pages.count > 0) ? selection : nil;
}

/* From a point on one page to one on another; a point falls between
   characters as on a page (off the text: the page's start or end). A page
   before the last ends without its last line feed, as on the reference. */
- (PDFSelection *)selectionFromPage:(PDFPage *)startPage atPoint:(PDFPoint)startPoint toPage:(PDFPage *)endPage atPoint:(PDFPoint)endPoint {
    NSUInteger first = [self indexForPage:startPage], last = [self indexForPage:endPage];
    PDFSelection *selection;

    if (first == NSNotFound || last == NSNotFound)
        return nil;
    if (first == last)
        return [startPage selectionFromPoint:startPoint toPoint:endPoint];
    if (first > last) {
        PDFPage *page = startPage; startPage = endPage; endPage = page;
        PDFPoint point = startPoint; startPoint = endPoint; endPoint = point;
        NSUInteger t = first; first = last; last = t;
    }
    selection = [[PDFSelection alloc] initWithDocument:self];
    for (NSUInteger i = first; i <= last; i++) {
        PDFPage *page = [self pageAtIndex:i];
        NSString *text = page.string;
        NSUInteger count = text.length, from = 0, to = count;

        if (i == first) {
            from = [page _boundaryAtPoint:startPoint];
            if (from == NSNotFound)
                from = 0;
        }
        if (i == last) {
            to = [page _boundaryAtPoint:endPoint];
            if (to == NSNotFound)
                to = count;
        } else if (count > 0 && [text characterAtIndex:count - 1] == '\n')
            to = count - 1;
        if (to > from)
            [selection _addRange:NSMakeRange(from, to - from) onPage:page];
    }
    return (selection.pages.count > 0) ? selection : nil;
}

- (PDFOutline *)outlineItemForSelection:(PDFSelection *)selection {
    NSUInteger page = 0, character = 0;
    PDFOutline *best = nil;
    NSMutableArray *stack = [NSMutableArray array];

    if (self.outlineRoot == nil || ![self _placeOf:selection page:&page character:&character last:NO])
        return nil;
    for (NSUInteger i = self.outlineRoot.numberOfChildren; i > 0; i--)
        [stack addObject:[self.outlineRoot childAtIndex:i - 1]];
    while (stack.count > 0) {
        PDFOutline *item = stack.lastObject;
        NSUInteger itemPage;

        [stack removeLastObject];
        itemPage = item.destination.page ? [self indexForPage:item.destination.page] : NSNotFound;
        if (itemPage != NSNotFound && itemPage <= page)
            best = item;
        for (NSUInteger i = item.numberOfChildren; i > 0; i--)
            [stack addObject:[item childAtIndex:i - 1]];
    }
    return best;
}

/* ---- page labels ---- */

/* A page's label: its place among the document's own pages as read (a
   page moved keeps its label; one added has none, as on the reference). */
- (NSString *)_labelForPage:(PDFPage *)page {
    CGPDFPageRef pageRef = page.pageRef;

    if (pageRef == NULL || _documentRef == NULL || (void *)CGPDFPageGetDocument(pageRef) != (void *)_documentRef)
        return nil;
    return [self _labelForPageAtIndex:CGPDFPageGetPageNumber(pageRef) - 1];
}

- (NSString *)_labelForPageAtIndex:(NSUInteger)index {
    if (_pageLabelRanges == nil) {
        NSMutableArray *entries = [NSMutableArray array];
        CGPDFDictionaryRef catalog = (_documentRef != NULL) ? CGPDFDocumentGetCatalog(_documentRef) : NULL, labels;

        if (CGPDFDictionaryGetDictionary(catalog, "PageLabels", &labels))
            collectNumberTree(labels, entries, 0);
        [entries sortUsingComparator:^NSComparisonResult(NSArray *a, NSArray *b) { return [a[0] compare:b[0]]; }];
        _pageLabelRanges = entries;
    }
    NSArray *range = nil;

    for (NSArray *entry in _pageLabelRanges)
        if ([entry[0] unsignedIntegerValue] <= index)
            range = entry;
    if (range == nil)
        return [NSString stringWithFormat:@"%lu", (unsigned long)index + 1];

    CGPDFDictionaryRef label = [range[1] pointerValue];
    const char *style = NULL;
    CGPDFInteger start = 1;
    CGPDFStringRef prefix = NULL;
    NSString *result = @"";

    CGPDFDictionaryGetName(label, "S", &style);
    CGPDFDictionaryGetInteger(label, "St", &start);
    if (CGPDFDictionaryGetString(label, "P", &prefix))
        result = PDFKitString(prefix) ?: @"";
    NSInteger value = start + (NSInteger)(index - [range[0] unsignedIntegerValue]);
    if (style == NULL)
        return result;
    switch (style[0]) {
    case 'D': return [result stringByAppendingFormat:@"%ld", (long)value];
    case 'R': return [result stringByAppendingString:roman(value, YES)];
    case 'r': return [result stringByAppendingString:roman(value, NO)];
    case 'A': return [result stringByAppendingString:letters(value, YES)];
    case 'a': return [result stringByAppendingString:letters(value, NO)];
    }
    return result;
}

/* ---- destinations and actions ---- */

- (PDFPage *)_pageForDictionary:(CGPDFDictionaryRef)dictionary {
    if (dictionary == NULL)
        return nil;
    if (_pageIndexByDictionary == nil) {
        _pageIndexByDictionary = [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsOpaqueMemory | NSPointerFunctionsOpaquePersonality
                                                       valueOptions:NSPointerFunctionsStrongMemory];
        for (NSUInteger i = 0; i < _pages.count; i++) {
            CGPDFPageRef pageRef = (_pages[i] == [NSNull null]) ? CGPDFDocumentGetPage(_documentRef, i + 1) : [_pages[i] pageRef];

            if (pageRef != NULL)
                [_pageIndexByDictionary setObject:@(i) forKey:(__bridge id)(void *)CGPDFPageGetDictionary(pageRef)];
        }
    }
    NSNumber *index = [_pageIndexByDictionary objectForKey:(__bridge id)(void *)dictionary];

    return (index != nil) ? [self pageAtIndex:index.unsignedIntegerValue] : nil;
}

/* A named destination: the catalog's Dests dictionary (PDF 1.1), or the
   Names dictionary's Dests name tree. */
static CGPDFObjectRef lookUpNameTree(CGPDFDictionaryRef node, const char *name, size_t length, int depth) {
    CGPDFArrayRef names = NULL, kids = NULL;

    if (node == NULL || depth > 32)
        return NULL;
    if (CGPDFDictionaryGetArray(node, "Names", &names))
        for (size_t i = 0; i + 1 < CGPDFArrayGetCount(names); i += 2) {
            CGPDFStringRef key;
            CGPDFObjectRef value;

            if (CGPDFArrayGetString(names, i, &key) && CGPDFStringGetLength(key) == length &&
                memcmp(CGPDFStringGetBytePtr(key), name, length) == 0 && CGPDFArrayGetObject(names, i + 1, &value))
                return value;
        }
    if (CGPDFDictionaryGetArray(node, "Kids", &kids))
        for (size_t i = 0; i < CGPDFArrayGetCount(kids); i++) {
            CGPDFDictionaryRef kid;
            CGPDFObjectRef found;

            if (CGPDFArrayGetDictionary(kids, i, &kid) && (found = lookUpNameTree(kid, name, length, depth + 1)) != NULL)
                return found;
        }
    return NULL;
}

- (PDFDestination *)_destinationForArray:(CGPDFArrayRef)array {
    CGPDFDictionaryRef pageDictionary;
    CGPDFInteger pageNumber;
    const char *kind = NULL;
    PDFPage *page = nil;
    CGPDFReal left, top, zoom;
    PDFPoint point = CGPointMake(kPDFDestinationUnspecifiedValue, kPDFDestinationUnspecifiedValue);
    CGFloat destinationZoom = kPDFDestinationUnspecifiedValue;

    if (CGPDFArrayGetDictionary(array, 0, &pageDictionary))
        page = [self _pageForDictionary:pageDictionary];
    else if (CGPDFArrayGetInteger(array, 0, &pageNumber))
        page = [self pageAtIndex:(NSUInteger)pageNumber];
    if (page == nil)
        return nil;
    /* Only XYZ names a point and a zoom (0 or null leaves them, as on the
       reference); Fit, FitH, FitV, FitR, FitB... leave both. */
    if (CGPDFArrayGetName(array, 1, &kind) && strcmp(kind, "XYZ") == 0) {
        if (CGPDFArrayGetNumber(array, 2, &left)) point.x = left;
        if (CGPDFArrayGetNumber(array, 3, &top)) point.y = top;
        if (CGPDFArrayGetNumber(array, 4, &zoom) && zoom != 0) destinationZoom = zoom;
    }
    PDFDestination *destination = [[PDFDestination alloc] initWithPage:page atPoint:point];
    destination.zoom = destinationZoom;
    return destination;
}

- (PDFDestination *)_destinationForObject:(CGPDFObjectRef)object {
    CGPDFArrayRef array;
    CGPDFDictionaryRef dictionary, catalog, names, dests;
    const char *name = NULL;
    CGPDFStringRef string;
    CGPDFObjectRef found = NULL;

    if (object == NULL)
        return nil;
    if (CGPDFObjectGetValue(object, kCGPDFObjectTypeArray, &array))
        return [self _destinationForArray:array];
    catalog = CGPDFDocumentGetCatalog(_documentRef);
    /* A name (the catalog's Dests, PDF 1.1) is not looked up, as on the
       reference: a destination with no page. */
    if (CGPDFObjectGetValue(object, kCGPDFObjectTypeName, &name))
        return [[PDFDestination alloc] initWithPage:(PDFPage *_Nonnull)nil
                                            atPoint:CGPointMake(kPDFDestinationUnspecifiedValue, kPDFDestinationUnspecifiedValue)];
    else if (CGPDFObjectGetValue(object, kCGPDFObjectTypeString, &string)) {
        if (CGPDFDictionaryGetDictionary(catalog, "Names", &names) && CGPDFDictionaryGetDictionary(names, "Dests", &dests))
            found = lookUpNameTree(dests, (const char *)CGPDFStringGetBytePtr(string), CGPDFStringGetLength(string), 0);
    }
    if (found == NULL)
        return nil;
    /* The value: a destination array, or a dictionary holding it as D. */
    if (CGPDFObjectGetValue(found, kCGPDFObjectTypeArray, &array))
        return [self _destinationForArray:array];
    if (CGPDFObjectGetValue(found, kCGPDFObjectTypeDictionary, &dictionary) && CGPDFDictionaryGetArray(dictionary, "D", &array))
        return [self _destinationForArray:array];
    return nil;
}

- (PDFAction *)_actionForDictionary:(CGPDFDictionaryRef)dictionary {
    const char *kind = NULL, *name = NULL;
    CGPDFObjectRef object;
    CGPDFStringRef uri;

    if (!CGPDFDictionaryGetName(dictionary, "S", &kind))
        return nil;
    if (strcmp(kind, "GoTo") == 0) {
        PDFDestination *destination = CGPDFDictionaryGetObject(dictionary, "D", &object) ? [self _destinationForObject:object] : nil;

        return (destination != nil) ? [[PDFActionGoTo alloc] initWithDestination:destination] : nil;
    }
    if (strcmp(kind, "URI") == 0 && CGPDFDictionaryGetString(dictionary, "URI", &uri)) {
        NSString *text = [[NSString alloc] initWithBytes:CGPDFStringGetBytePtr(uri) length:CGPDFStringGetLength(uri) encoding:NSISOLatin1StringEncoding];
        NSURL *url = [NSURL URLWithString:text] ?: [NSURL URLWithString:[text stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLQueryAllowedCharacterSet]];

        return (url != nil) ? [[PDFActionURL alloc] initWithURL:url] : nil;
    }
    if (strcmp(kind, "Named") == 0 && CGPDFDictionaryGetName(dictionary, "N", &name)) {
        static const char *names[] = {"", "NextPage", "PrevPage", "FirstPage", "LastPage", "GoBack", "GoForward", "GoToPage", "Find", "Print", "ZoomIn", "ZoomOut"};

        for (NSInteger i = 1; i < (NSInteger)(sizeof(names) / sizeof(names[0])); i++)
            if (strcmp(name, names[i]) == 0)
                return [[PDFActionNamed alloc] initWithName:i];
        return [[PDFActionNamed alloc] initWithName:kPDFActionNamedNone];
    }
    return nil;
}

/* ---- outline ---- */

- (PDFOutline *)_outlineItemForDictionary:(CGPDFDictionaryRef)dictionary visited:(NSMutableSet *)visited depth:(int)depth {
    PDFOutline *item = [[PDFOutline alloc] init];
    CGPDFStringRef title;
    CGPDFInteger count = 0;
    CGPDFObjectRef destination;
    CGPDFDictionaryRef action, child;

    if (CGPDFDictionaryGetString(dictionary, "Title", &title))
        item.label = PDFKitString(title);
    /* Open when Count is positive (PDF 1.7, table 153). */
    item.isOpen = CGPDFDictionaryGetInteger(dictionary, "Count", &count) && count > 0;
    if (CGPDFDictionaryGetObject(dictionary, "Dest", &destination))
        item.destination = [self _destinationForObject:destination];
    else if (CGPDFDictionaryGetDictionary(dictionary, "A", &action))
        item.action = [self _actionForDictionary:action];
    if (depth < 64 && CGPDFDictionaryGetDictionary(dictionary, "First", &child))
        [self _addOutlineChildrenFrom:child to:item visited:visited depth:depth + 1];
    return item;
}

- (void)_addOutlineChildrenFrom:(CGPDFDictionaryRef)child to:(PDFOutline *)parent visited:(NSMutableSet *)visited depth:(int)depth {
    while (child != NULL) {
        NSValue *key = [NSValue valueWithPointer:child];

        if ([visited containsObject:key])   // a cycle: stop
            return;
        [visited addObject:key];
        [parent insertChild:[self _outlineItemForDictionary:child visited:visited depth:depth] atIndex:parent.numberOfChildren];
        if (!CGPDFDictionaryGetDictionary(child, "Next", &child))
            child = NULL;
    }
}

- (PDFOutline *)outlineRoot {
    if (!_outlineRead) {
        CGPDFDictionaryRef outlines, first;

        _outlineRead = YES;
        if (_documentRef != NULL && CGPDFDictionaryGetDictionary(CGPDFDocumentGetCatalog(_documentRef), "Outlines", &outlines)) {
            PDFOutline *root = [[PDFOutline alloc] init];

            CGPDFInteger count = 0;

            root.label = @"";
            root.isOpen = CGPDFDictionaryGetInteger(outlines, "Count", &count) && count > 0;
            if (CGPDFDictionaryGetDictionary(outlines, "First", &first))
                [self _addOutlineChildrenFrom:first to:root visited:[NSMutableSet set] depth:1];
            [root _setDocument:self];
            _outlineRoot = root;
        }
    }
    return _outlineRoot;
}

- (void)setOutlineRoot:(PDFOutline *)outlineRoot {
    _outlineRead = YES;
    [_outlineRoot _setDocument:nil];
    _outlineRoot = outlineRoot;
    [outlineRoot _setDocument:self];
    _changed = YES;
}

/* ---- writing ---- */

/* The document as read, or, once changed or given options, its pages drawn
   into a new PDF with its attributes. Not written yet: the outline, the
   annotations, and encryption (the password options are not applied). */
/* The document as read with an incremental update appended (PDF 1.7,
   7.5.6): each page whose annotations changed written again with its
   Annots, the annotations added or changed written as new objects, those
   as read kept by reference (with their appearances); a cross-reference
   section and a trailer whose Prev is the last one. */
- (NSData *)_incrementalUpdate {
    O2PDFDocument *document = (__bridge O2PDFDocument *)(void *)_documentRef;
    O2PDFDictionary *trailer = O2PDFDocumentGetTrailer(document);
    NSMutableData *out = [_data mutableCopy];
    NSMutableArray *objects = [NSMutableArray array];   // @[number, generation, data]
    CGPDFInteger size = 0;
    long previous = -1;

    /* The last cross-reference section, from the file's startxref. */
    const char *bytes = _data.bytes;
    for (NSInteger i = (NSInteger)_data.length - 9; i >= 0 && i > (NSInteger)_data.length - 2048; i--)
        if (memcmp(bytes + i, "startxref", 9) == 0) {
            previous = strtol(bytes + i + 9, NULL, 10);
            break;
        }
    if (trailer == nil || previous < 0 || !CGPDFDictionaryGetInteger((CGPDFDictionaryRef)(__bridge void *)trailer, "Size", &size))
        return nil;
    __block NSInteger next = size;

    NSString *_Nullable (^references)(PDFPage *) = ^NSString *(PDFPage *page) {
        NSUInteger index = [self indexForPage:page];
        int number, generation;

        if (index == NSNotFound || !O2PDFDocumentGetPageReference(document, (unsigned)index, &number, &generation))
            return nil;
        return [NSString stringWithFormat:@"%d %d R", number, generation];
    };

    for (NSUInteger i = 0; i < self.pageCount; i++) {
        PDFPage *page = (_pages[i] == [NSNull null]) ? nil : _pages[i];
        int pageNumber, pageGeneration;
        NSMutableArray *annots = [NSMutableArray array];

        if (page == nil || ![page _annotationsChangedSinceRead] || page.pageRef == NULL ||
            !O2PDFDocumentGetPageReference(document, (unsigned)i, &pageNumber, &pageGeneration))
            continue;
        NSString *pageReference = [NSString stringWithFormat:@"%d %d R", pageNumber, pageGeneration];
        for (PDFAnnotation *annotation in [page _readAnnotations]) {
            NSArray *source = [annotation _sourceReference];

            if (source != nil && ![annotation _isModified]) {
                [annots addObject:[NSString stringWithFormat:@"%@ %@ R", source[0], source[1]]];
                continue;
            }
            NSInteger number = source ? [source[0] integerValue] : next++;
            NSInteger generation = source ? [source[1] integerValue] : 0;
            [objects addObject:@[ @(number), @(generation), [annotation _pdfDataWithPageReference:pageReference pageReferences:references] ]];
            [annots addObject:[NSString stringWithFormat:@"%ld %ld R", (long)number, (long)generation]];
        }
        NSMutableData *pageData = [O2PDFDictionaryCopyRawEncoding((__bridge O2PDFDictionary *)(void *)CGPDFPageGetDictionary(page.pageRef), @[ @"Annots" ]) mutableCopy];
        [pageData setLength:pageData.length - 2];   // its closing ">>"
        [pageData appendData:[[NSString stringWithFormat:@"/Annots [%@] >>", [annots componentsJoinedByString:@" "]] dataUsingEncoding:NSASCIIStringEncoding]];
        [objects addObject:@[ @(pageNumber), @(pageGeneration), pageData ]];
    }
    if (objects.count == 0)
        return _data;

    if (out.length > 0 && ((const char *)out.bytes)[out.length - 1] != '\n')
        [out appendBytes:"\n" length:1];
    NSMutableDictionary *offsets = [NSMutableDictionary dictionary];
    NSInteger highest = size - 1;
    for (NSArray *object in objects) {
        offsets[object[0]] = @[ @(out.length), object[1] ];
        [out appendData:[[NSString stringWithFormat:@"%@ %@ obj\n", object[0], object[1]] dataUsingEncoding:NSASCIIStringEncoding]];
        [out appendData:object[2]];
        [out appendBytes:"\nendobj\n" length:8];
        highest = MAX(highest, [object[0] integerValue]);
    }
    NSUInteger xref = out.length;
    NSMutableString *table = [NSMutableString stringWithString:@"xref\n"];
    for (NSNumber *number in [offsets.allKeys sortedArrayUsingSelector:@selector(compare:)])
        [table appendFormat:@"%@ 1\n%010lu %05ld n\r\n", number, [offsets[number][0] unsignedLongValue], [offsets[number][1] longValue]];
    NSMutableData *trailerData = [O2PDFDictionaryCopyRawEncoding(trailer, @[ @"Size", @"Prev", @"XRefStm", @"Type", @"W", @"Index", @"Filter", @"DecodeParms", @"Length" ]) mutableCopy];
    [trailerData setLength:trailerData.length - 2];
    [table appendString:@"trailer\n"];
    [out appendData:[table dataUsingEncoding:NSASCIIStringEncoding]];
    [out appendData:trailerData];
    [out appendData:[[NSString stringWithFormat:@"/Size %ld /Prev %ld >>\nstartxref\n%lu\n%%%%EOF\n", (long)(highest + 1), previous, (unsigned long)xref]
                       dataUsingEncoding:NSASCIIStringEncoding]];
    return out;
}

- (NSData *)dataRepresentationWithOptions:(NSDictionary *)options {
    if (!_changed && _data != nil && options.count == 0)
        return _annotationsChanged ? ([self _incrementalUpdate] ?: _data) : _data;

    NSMutableData *data = [NSMutableData data];
    CGDataConsumerRef consumer = CGDataConsumerCreateWithCFData((__bridge CFMutableDataRef)data);
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    NSDictionary *attributes = self.documentAttributes;
    NSDictionary *keys = @{ PDFDocumentTitleAttribute: (__bridge id)kCGPDFContextTitle, PDFDocumentAuthorAttribute: (__bridge id)kCGPDFContextAuthor,
                            PDFDocumentSubjectAttribute: (__bridge id)kCGPDFContextSubject, PDFDocumentCreatorAttribute: (__bridge id)kCGPDFContextCreator };

    for (NSString *attribute in keys)
        if ([attributes[attribute] isKindOfClass:[NSString class]])
            info[keys[attribute]] = attributes[attribute];
    if ([attributes[PDFDocumentKeywordsAttribute] isKindOfClass:[NSArray class]])
        info[(__bridge id)kCGPDFContextKeywords] = attributes[PDFDocumentKeywordsAttribute];

    CGContextRef context = CGPDFContextCreate(consumer, NULL, (__bridge CFDictionaryRef)info);
    CGDataConsumerRelease(consumer);
    if (context == NULL)
        return nil;
    for (NSUInteger i = 0; i < self.pageCount; i++) {
        PDFPage *page = [self pageAtIndex:i];
        CGRect media = [page boundsForBox:kPDFDisplayBoxMediaBox];
        CGRect box = CGRectMake(0, 0, (page.rotation % 180) ? media.size.height : media.size.width,
                                (page.rotation % 180) ? media.size.width : media.size.height);
        NSDictionary *pageInfo = @{ (__bridge id)kCGPDFContextMediaBox: [NSData dataWithBytes:&box length:sizeof(box)] };

        CGPDFContextBeginPage(context, (__bridge CFDictionaryRef)pageInfo);
        [page drawWithBox:kPDFDisplayBoxMediaBox toContext:context];
        CGPDFContextEndPage(context);
    }
    CGPDFContextClose(context);
    CGContextRelease(context);
    return data;
}

- (NSData *)dataRepresentation {
    return [self dataRepresentationWithOptions:@{}];
}

- (BOOL)writeToURL:(NSURL *)url withOptions:(NSDictionary *)options {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    [center postNotificationName:PDFDocumentDidBeginWriteNotification object:self];
    NSData *data = [self dataRepresentationWithOptions:options ?: @{}];
    BOOL written = (data != nil) && [data writeToURL:url atomically:YES];
    [center postNotificationName:PDFDocumentDidEndWriteNotification object:self];
    return written;
}

- (BOOL)writeToURL:(NSURL *)url {
    return [self writeToURL:url withOptions:nil];
}

- (BOOL)writeToFile:(NSString *)path withOptions:(NSDictionary *)options {
    return [self writeToURL:[NSURL fileURLWithPath:path] withOptions:options];
}

- (BOOL)writeToFile:(NSString *)path {
    return [self writeToFile:path withOptions:nil];
}

@end
