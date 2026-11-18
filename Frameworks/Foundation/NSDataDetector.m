/* Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// ARC.
// NSDataDetector: links (URLs with a scheme, www. addresses, mail addresses
// and bare host names under a known top-level domain) and phone numbers,
// found with ICU's regular expressions, as the reference finds them in the
// cases measured on it. Dates, addresses and transit information are not
// detected yet.

#import <Foundation/NSRegularExpression.h>
#import <Foundation/NSTextCheckingResult.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>

static NSString *const NSDataDetectorLinkPattern =
    @"\\b(?:[A-Za-z][A-Za-z0-9+.-]*://[^\\s<>\"]+"                            /* scheme://... */
    @"|mailto:[^\\s<>\"]+"
    @"|[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)*\\.[A-Za-z]{2,}"   /* mail */
    @"|www\\.[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)+(?:[/?#][^\\s<>\"]*)?"         /* www. */
    @"|(?<![@\\w.])[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)*\\."
    @"(?:com|net|org|edu|gov|mil|int|info|biz|name|pro|aero|coop|museum|mobi|asia|tel|travel|jobs|cat|io|co|app|dev|[A-Za-z]{2})"
    @"(?![\\w-])(?:[/?#][^\\s<>\"]*)?)";

static NSString *const NSDataDetectorPhonePattern =
    @"(?<![\\w+])(?:\\+\\d{1,3}[\\s.-]?)?(?:\\(\\d{2,4}\\)[\\s.-]?|\\d{2,4}[\\s.-])\\d{3,4}[\\s.-]\\d{3,4}(?![\\w])";

@interface NSDataDetector ()
@property (nonatomic) NSTextCheckingTypes checkingTypes;
@property (nonatomic, strong) NSRegularExpression *linkExpression;
@property (nonatomic, strong) NSRegularExpression *phoneExpression;
@end

@implementation NSDataDetector

+ (NSDataDetector *)dataDetectorWithTypes:(NSTextCheckingTypes)checkingTypes error:(NSError **)error {
    return [[self alloc] initWithTypes:checkingTypes error:error];
}

- (instancetype)initWithTypes:(NSTextCheckingTypes)checkingTypes error:(NSError **)error {
    NSTextCheckingTypes known = NSTextCheckingTypeLink | NSTextCheckingTypePhoneNumber | NSTextCheckingTypeDate |
        NSTextCheckingTypeAddress | NSTextCheckingTypeTransitInformation;

    if (checkingTypes == 0 || (checkingTypes & ~known) != 0) {
        if (error != NULL)
            *error = [NSError errorWithDomain:NSCocoaErrorDomain code:2048 userInfo:nil];
        return nil;
    }
    _checkingTypes = checkingTypes;
    if (checkingTypes & NSTextCheckingTypeLink)
        _linkExpression = [NSRegularExpression regularExpressionWithPattern:NSDataDetectorLinkPattern options:0 error:NULL];
    if (checkingTypes & NSTextCheckingTypePhoneNumber)
        _phoneExpression = [NSRegularExpression regularExpressionWithPattern:NSDataDetectorPhonePattern options:0 error:NULL];
    return self;
}

- (instancetype)initWithPattern:(NSString *)pattern options:(NSRegularExpressionOptions)options error:(NSError **)error {
    return [self initWithTypes:NSTextCheckingTypeLink error:error];
}

- (NSString *)pattern {
    return nil;
}

/* A link's text without the punctuation that ends a sentence around it. */
static NSRange trimmedLinkRange(NSString *string, NSRange range) {
    NSCharacterSet *trailing = [NSCharacterSet characterSetWithCharactersInString:@".,;:!?)]}'\""];

    while (range.length > 1 && [trailing characterIsMember:[string characterAtIndex:NSMaxRange(range) - 1]])
        range.length--;
    return range;
}

static NSURL *linkURL(NSString *text) {
    if ([text rangeOfString:@"://"].location != NSNotFound || [text hasPrefix:@"mailto:"])
        return [NSURL URLWithString:text];
    if ([text rangeOfString:@"@"].location != NSNotFound)
        return [NSURL URLWithString:[@"mailto:" stringByAppendingString:text]];
    return [NSURL URLWithString:[@"http://" stringByAppendingString:text]];
}

- (void)enumerateMatchesInString:(NSString *)string options:(NSMatchingOptions)options range:(NSRange)range
                      usingBlock:(void (^)(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop))block {
    NSMutableArray *results = [NSMutableArray array];

    [self.linkExpression enumerateMatchesInString:string options:0 range:range
                                       usingBlock:^(NSTextCheckingResult *match, NSMatchingFlags flags, BOOL *stop) {
        NSRange r = trimmedLinkRange(string, match.range);
        NSURL *url = linkURL([string substringWithRange:r]);

        if (url != nil)
            [results addObject:[NSTextCheckingResult linkCheckingResultWithRange:r URL:url]];
    }];
    [self.phoneExpression enumerateMatchesInString:string options:0 range:range
                                        usingBlock:^(NSTextCheckingResult *match, NSMatchingFlags flags, BOOL *stop) {
        for (NSTextCheckingResult *link in results)
            if (NSIntersectionRange(link.range, match.range).length > 0)
                return;
        [results addObject:[NSTextCheckingResult phoneNumberCheckingResultWithRange:match.range
                                                                        phoneNumber:[string substringWithRange:match.range]]];
    }];
    [results sortUsingComparator:^NSComparisonResult(NSTextCheckingResult *a, NSTextCheckingResult *b) {
        return a.range.location < b.range.location ? NSOrderedAscending
             : a.range.location > b.range.location ? NSOrderedDescending : NSOrderedSame;
    }];

    BOOL stop = NO;
    for (NSTextCheckingResult *result in results) {
        block(result, 0, &stop);
        if (stop)
            break;
    }
}

@end
