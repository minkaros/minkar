/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HTML to an attributed string, for -[NSAttributedString initWithHTML:]:
 * the part of HTML that help books, credits and simple documents use.
 * Headings, paragraphs, bold and italic, code and preformatted text, lists,
 * line breaks, rules, links (made absolute against the page) and the
 * common entities; the page's title as a document attribute. Scripts,
 * styles and the head are left out. It is no browser: tables are rows of
 * text, and CSS is ignored.
 */

#import <AppKit/NSAttributedString.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSFontManager.h>
#import <AppKit/NSParagraphStyle.h>
#import <Foundation/Foundation.h>
#import "NSHTMLReader.h"

#define BASE_SIZE 13.0

@implementation NSHTMLReader {
    NSMutableAttributedString *_result;
    NSURL       *_baseURL;
    NSString    *_title;
    int          _bold, _italic, _code, _pre, _heading;
    int          _listDepth;
    NSMutableArray *_listCounters;    /* NSNumber per open list: -1 bullets, else next number */
    NSURL       *_link;
    BOOL         _atLineStart;
    BOOL         _pendingSpace;
    BOOL         _inTitle;
    int          _skip;               /* inside head, script or style */
    NSMutableString *_titleText;
    NSString    *_pendingAnchor;      /* an id or <a name> the next text starts */
}

NSString *const NSHTMLAnchorAttributeName=@"_NSHTMLAnchor";

static NSString *decodeEntity(NSString *name)
{
    static NSDictionary *named;

    if (named == nil)
        named = [[NSDictionary alloc] initWithObjectsAndKeys:
            @"&", @"amp", @"<", @"lt", @">", @"gt", @"\"", @"quot", @"'", @"apos",
            @" ", @"nbsp", @"©", @"copy", @"®", @"reg", @"™", @"trade",
            @"—", @"mdash", @"–", @"ndash", @"…", @"hellip", @"•", @"bull",
            @"‘", @"lsquo", @"’", @"rsquo", @"“", @"ldquo", @"”", @"rdquo",
            @"«", @"laquo", @"»", @"raquo", @"→", @"rarr", @"←", @"larr", nil];
    if ([name hasPrefix:@"#"]) {
        unsigned value = 0;
        NSScanner *scanner = [NSScanner scannerWithString:[name substringFromIndex:1]];

        if ([name hasPrefix:@"#x"] || [name hasPrefix:@"#X"]) {
            scanner = [NSScanner scannerWithString:[name substringFromIndex:2]];
            if (![scanner scanHexInt:&value])
                return nil;
        } else {
            int decimal;

            if (![scanner scanInt:&decimal] || decimal < 0)
                return nil;
            value = (unsigned)decimal;
        }
        if (value == 0 || value > 0x10FFFF)
            return nil;
        if (value > 0xFFFF) {
            unichar pair[2] = { (unichar)(0xD800 + ((value - 0x10000) >> 10)),
                (unichar)(0xDC00 + ((value - 0x10000) & 0x3FF)) };
            return [NSString stringWithCharacters:pair length:2];
        }
        unichar c = (unichar)value;
        return [NSString stringWithCharacters:&c length:1];
    }
    return [named objectForKey:[name lowercaseString]];
}

- (instancetype)initWithBaseURL:(NSURL *)baseURL
{
    if ((self = [super init]) != nil) {
        _result = [[NSMutableAttributedString alloc] init];
        _baseURL = [baseURL retain];
        _listCounters = [[NSMutableArray alloc] init];
        _titleText = [[NSMutableString alloc] init];
        _atLineStart = YES;
    }
    return self;
}

- (void)dealloc
{
    [_result release];
    [_baseURL release];
    [_listCounters release];
    [_titleText release];
    [_link release];
    [_pendingAnchor release];
    [super dealloc];
}

- (NSFont *)font
{
    CGFloat size = BASE_SIZE;
    NSFont *font;

    switch (_heading) {
    case 1: size = 22; break;
    case 2: size = 17; break;
    case 3: size = 14; break;
    case 4: case 5: case 6: size = 13; break;
    }
    font = (_code || _pre) ? [NSFont userFixedPitchFontOfSize:size - 1] : [NSFont systemFontOfSize:size];
    if (_bold || _heading)
        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
    if (_italic)
        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
    return font;
}

- (NSDictionary *)attributes
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    CGFloat indent = 20.0 * _listDepth;

    [attributes setObject:[self font] forKey:NSFontAttributeName];
    [attributes setObject:[NSColor textColor] forKey:NSForegroundColorAttributeName];
    [style setParagraphSpacing:_heading ? 6 : 8];
    [style setHeadIndent:indent];
    [style setFirstLineHeadIndent:indent > 0 ? indent - 12 : 0];
    [attributes setObject:style forKey:NSParagraphStyleAttributeName];
    if (_link != nil) {
        [attributes setObject:_link forKey:NSLinkAttributeName];
        [attributes setObject:[NSColor linkColor] forKey:NSForegroundColorAttributeName];
        [attributes setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
    }
    return attributes;
}

- (void)appendRaw:(NSString *)string
{
    if ([string length] == 0)
        return;
    NSUInteger start = [_result length];
    NSAttributedString *piece = [[NSAttributedString alloc] initWithString:string attributes:[self attributes]];
    [_result appendAttributedString:piece];
    [piece release];
    if (_pendingAnchor != nil) {
        [_result addAttribute:NSHTMLAnchorAttributeName value:_pendingAnchor range:NSMakeRange(start, 1)];
        [_pendingAnchor release];
        _pendingAnchor = nil;
    }
}

/* Text: whitespace collapsed to one space (kept as it is inside pre). */
- (void)appendText:(NSString *)text
{
    if (_skip > 0) {
        if (_inTitle)
            [_titleText appendString:text];
        return;
    }
    if (_pre) {
        [self appendRaw:text];
        _atLineStart = [text hasSuffix:@"\n"];
        return;
    }

    NSMutableString *out = [NSMutableString string];
    NSUInteger i, length = [text length];

    for (i = 0; i < length; i++) {
        unichar c = [text characterAtIndex:i];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
            _pendingSpace = YES;
            continue;
        }
        if (_pendingSpace && !_atLineStart && [out length] + [_result length] > 0)
            [out appendString:@" "];
        _pendingSpace = NO;
        _atLineStart = NO;
        [out appendFormat:@"%C", c];
    }
    [self appendRaw:out];
}

/* Ends the line, once: blocks don't stack empty lines. */
- (void)breakLine
{
    if (!_atLineStart)
        [self appendRaw:@"\n"];
    _atLineStart = YES;
    _pendingSpace = NO;
}

- (NSString *)attribute:(NSString *)name inTag:(NSString *)tag
{
    NSScanner *scanner = [NSScanner scannerWithString:tag];
    NSCharacterSet *space = [NSCharacterSet whitespaceAndNewlineCharacterSet];

    [scanner setCaseSensitive:NO];
    [scanner setCharactersToBeSkipped:nil];
    while (![scanner isAtEnd]) {
        NSString *key = nil, *value = nil;

        [scanner scanCharactersFromSet:space intoString:NULL];
        if (![scanner scanUpToCharactersFromSet:[NSCharacterSet characterSetWithCharactersInString:@"= \t\n\r>"]
                intoString:&key]) {
            [scanner setScanLocation:[scanner scanLocation] + 1];
            continue;
        }
        [scanner scanCharactersFromSet:space intoString:NULL];
        if ([scanner scanString:@"=" intoString:NULL]) {
            [scanner scanCharactersFromSet:space intoString:NULL];
            if ([scanner scanString:@"\"" intoString:NULL]) {
                [scanner scanUpToString:@"\"" intoString:&value];
                [scanner scanString:@"\"" intoString:NULL];
            } else if ([scanner scanString:@"'" intoString:NULL]) {
                [scanner scanUpToString:@"'" intoString:&value];
                [scanner scanString:@"'" intoString:NULL];
            } else
                [scanner scanUpToCharactersFromSet:space intoString:&value];
        }
        if ([key caseInsensitiveCompare:name] == NSOrderedSame)
            return value ?: @"";
    }
    return nil;
}

- (void)startTag:(NSString *)name attributes:(NSString *)rest
{
    if ([name isEqualToString:@"head"] || [name isEqualToString:@"script"] || [name isEqualToString:@"style"]) {
        _skip++;
        return;
    }
    if ([name isEqualToString:@"title"]) {
        _inTitle = YES;
        _skip++;
        return;
    }
    if (_skip > 0)
        return;

    /* Where an anchor is: the next character. */
    NSString *anchor = [self attribute:@"id" inTag:rest];
    if (anchor == nil && [name isEqualToString:@"a"])
        anchor = [self attribute:@"name" inTag:rest];
    if ([anchor length]) {
        [_pendingAnchor release];
        _pendingAnchor = [anchor copy];
    }

    if ([name length] == 2 && [name characterAtIndex:0] == 'h' && [name characterAtIndex:1] >= '1' &&
            [name characterAtIndex:1] <= '6') {
        [self breakLine];
        _heading = [name characterAtIndex:1] - '0';
    } else if ([name isEqualToString:@"p"] || [name isEqualToString:@"div"] || [name isEqualToString:@"blockquote"] ||
            [name isEqualToString:@"tr"] || [name isEqualToString:@"dt"] || [name isEqualToString:@"dd"])
        [self breakLine];
    else if ([name isEqualToString:@"br"]) {
        [self appendRaw:@"\n"];
        _atLineStart = YES;
        _pendingSpace = NO;
    } else if ([name isEqualToString:@"hr"]) {
        [self breakLine];
        [self appendRaw:@"———\n"];
        _atLineStart = YES;
    } else if ([name isEqualToString:@"b"] || [name isEqualToString:@"strong"] || [name isEqualToString:@"th"])
        _bold++;
    else if ([name isEqualToString:@"i"] || [name isEqualToString:@"em"] || [name isEqualToString:@"cite"])
        _italic++;
    else if ([name isEqualToString:@"code"] || [name isEqualToString:@"tt"] || [name isEqualToString:@"kbd"] ||
            [name isEqualToString:@"samp"])
        _code++;
    else if ([name isEqualToString:@"pre"]) {
        [self breakLine];
        _pre++;
    } else if ([name isEqualToString:@"ul"] || [name isEqualToString:@"ol"]) {
        [self breakLine];
        _listDepth++;
        [_listCounters addObject:[NSNumber numberWithInt:[name isEqualToString:@"ol"] ? 1 : -1]];
    } else if ([name isEqualToString:@"li"]) {
        int number = [[_listCounters lastObject] intValue];

        [self breakLine];
        if (number > 0) {
            [self appendRaw:[NSString stringWithFormat:@"%d.\t", number]];
            [_listCounters replaceObjectAtIndex:[_listCounters count] - 1 withObject:[NSNumber numberWithInt:number + 1]];
        } else
            [self appendRaw:@"•\t"];
        _atLineStart = YES;
    } else if ([name isEqualToString:@"td"]) {
        if (!_atLineStart)
            [self appendRaw:@"\t"];
    } else if ([name isEqualToString:@"a"]) {
        NSString *href = [self attribute:@"href" inTag:rest];

        [_link release];
        _link = (href != nil) ? [[NSURL URLWithString:href relativeToURL:_baseURL] absoluteURL] : nil;
        [_link retain];
    } else if ([name isEqualToString:@"img"]) {
        NSString *alt = [self attribute:@"alt" inTag:rest];

        if ([alt length])
            [self appendText:alt];
    }
}

- (void)endTag:(NSString *)name
{
    if ([name isEqualToString:@"head"] || [name isEqualToString:@"script"] || [name isEqualToString:@"style"]) {
        if (_skip > 0)
            _skip--;
        return;
    }
    if ([name isEqualToString:@"title"]) {
        _inTitle = NO;
        if (_skip > 0)
            _skip--;
        return;
    }
    if (_skip > 0)
        return;

    if ([name length] == 2 && [name characterAtIndex:0] == 'h' && [name characterAtIndex:1] >= '1' &&
            [name characterAtIndex:1] <= '6') {
        [self breakLine];
        _heading = 0;
    } else if ([name isEqualToString:@"p"] || [name isEqualToString:@"div"] || [name isEqualToString:@"blockquote"] ||
            [name isEqualToString:@"tr"] || [name isEqualToString:@"li"] || [name isEqualToString:@"dd"])
        [self breakLine];
    else if ([name isEqualToString:@"b"] || [name isEqualToString:@"strong"] || [name isEqualToString:@"th"])
        _bold = MAX(0, _bold - 1);
    else if ([name isEqualToString:@"i"] || [name isEqualToString:@"em"] || [name isEqualToString:@"cite"])
        _italic = MAX(0, _italic - 1);
    else if ([name isEqualToString:@"code"] || [name isEqualToString:@"tt"] || [name isEqualToString:@"kbd"] ||
            [name isEqualToString:@"samp"])
        _code = MAX(0, _code - 1);
    else if ([name isEqualToString:@"pre"]) {
        _pre = MAX(0, _pre - 1);
        [self breakLine];
    } else if ([name isEqualToString:@"ul"] || [name isEqualToString:@"ol"]) {
        [self breakLine];
        _listDepth = MAX(0, _listDepth - 1);
        if ([_listCounters count])
            [_listCounters removeLastObject];
    } else if ([name isEqualToString:@"a"]) {
        [_link release];
        _link = nil;
    }
}

/* Text with its entities decoded. */
- (NSString *)decode:(NSString *)text
{
    NSRange amp = [text rangeOfString:@"&"];

    if (amp.location == NSNotFound)
        return text;

    NSMutableString *out = [NSMutableString string];
    NSUInteger i = 0, length = [text length];

    while (i < length) {
        NSRange next = [text rangeOfString:@"&" options:0 range:NSMakeRange(i, length - i)];

        if (next.location == NSNotFound) {
            [out appendString:[text substringFromIndex:i]];
            break;
        }
        [out appendString:[text substringWithRange:NSMakeRange(i, next.location - i)]];
        NSRange semi = [text rangeOfString:@";" options:0
            range:NSMakeRange(next.location, MIN((NSUInteger)12, length - next.location))];
        NSString *value = nil;

        if (semi.location != NSNotFound)
            value = decodeEntity([text substringWithRange:NSMakeRange(next.location + 1,
                semi.location - next.location - 1)]);
        if (value != nil) {
            [out appendString:value];
            i = semi.location + 1;
        } else {
            [out appendString:@"&"];
            i = next.location + 1;
        }
    }
    return out;
}

- (void)read:(NSString *)html
{
    NSUInteger i = 0, length = [html length];

    while (i < length) {
        NSRange open = [html rangeOfString:@"<" options:0 range:NSMakeRange(i, length - i)];

        if (open.location == NSNotFound) {
            [self appendText:[self decode:[html substringFromIndex:i]]];
            break;
        }
        if (open.location > i)
            [self appendText:[self decode:[html substringWithRange:NSMakeRange(i, open.location - i)]]];

        if ([html length] >= open.location + 4 &&
                [[html substringWithRange:NSMakeRange(open.location, 4)] isEqualToString:@"<!--"]) {
            NSRange close = [html rangeOfString:@"-->" options:0 range:NSMakeRange(open.location, length - open.location)];

            i = (close.location == NSNotFound) ? length : NSMaxRange(close);
            continue;
        }

        NSRange close = [html rangeOfString:@">" options:0 range:NSMakeRange(open.location, length - open.location)];
        if (close.location == NSNotFound)
            break;
        NSString *tag = [html substringWithRange:NSMakeRange(open.location + 1, close.location - open.location - 1)];
        i = NSMaxRange(close);

        if ([tag hasPrefix:@"!"] || [tag hasPrefix:@"?"])
            continue;
        BOOL end = [tag hasPrefix:@"/"];
        if (end)
            tag = [tag substringFromIndex:1];
        if ([tag hasSuffix:@"/"])
            tag = [tag substringToIndex:[tag length] - 1];

        NSRange space = [tag rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        NSString *name = [(space.location == NSNotFound ? tag : [tag substringToIndex:space.location]) lowercaseString];
        NSString *rest = (space.location == NSNotFound) ? @"" : [tag substringFromIndex:space.location];

        if (end)
            [self endTag:name];
        else
            [self startTag:name attributes:rest];

        /* Inside pre, the line after the opening tag starts the text. */
        if (!end && [name isEqualToString:@"pre"] && i < length && [html characterAtIndex:i] == '\n')
            i++;
    }

    /* No trailing empty line. */
    while ([_result length] > 0 && [[_result string] characterAtIndex:[_result length] - 1] == '\n')
        [_result deleteCharactersInRange:NSMakeRange([_result length] - 1, 1)];
}

+ (NSAttributedString *)attributedStringWithHTML:(NSData *)data baseURL:(NSURL *)baseURL
    documentAttributes:(NSDictionary **)documentAttributes
{
    NSString *html = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];

    if (html == nil)
        html = [[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding] autorelease];
    if (html == nil)
        return nil;

    NSHTMLReader *reader = [[[NSHTMLReader alloc] initWithBaseURL:baseURL] autorelease];
    [reader read:html];

    if (documentAttributes != NULL) {
        NSMutableDictionary *attributes = [NSMutableDictionary dictionaryWithObject:NSHTMLTextDocumentType
            forKey:NSDocumentTypeDocumentAttribute];
        NSString *title = [[reader->_titleText stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]] copy];

        if ([title length])
            [attributes setObject:title forKey:NSTitleDocumentAttribute];
        [title release];
        *documentAttributes = attributes;
    }
    return [[reader->_result copy] autorelease];
}

@end
