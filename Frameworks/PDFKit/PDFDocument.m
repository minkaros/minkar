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

NSDate *PDFKitDate(CGPDFStringRef string) {
    return (string != NULL) ? (__bridge_transfer NSDate *)CGPDFStringCopyDate(string) : nil;
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
- (NSData *)dataRepresentationWithOptions:(NSDictionary *)options {
    if (!_changed && _data != nil && options.count == 0)
        return _data;

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
