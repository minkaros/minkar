/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Linguistic tagging (API 19) without statistical models: tokens from the
 * string's word, sentence and paragraph boundaries (ICU, through
 * enumerateSubstringsInRange:options:usingBlock:); the token type and the
 * script tagged; the language guessed from the scripts and, for the Latin
 * script, a few common words. ARC.
 */

#import <Foundation/NSLinguisticTagger.h>
#import <Foundation/NSOrthography.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSMutableDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSSet.h>

#define SCHEME(name, value) NSLinguisticTagScheme const name = @value;
SCHEME(NSLinguisticTagSchemeTokenType, "TokenType")
SCHEME(NSLinguisticTagSchemeLexicalClass, "LexicalClass")
SCHEME(NSLinguisticTagSchemeNameType, "NameType")
SCHEME(NSLinguisticTagSchemeNameTypeOrLexicalClass, "NameTypeOrLexicalClass")
SCHEME(NSLinguisticTagSchemeLemma, "Lemma")
SCHEME(NSLinguisticTagSchemeLanguage, "Language")
SCHEME(NSLinguisticTagSchemeScript, "Script")
#undef SCHEME
#define TAG(name, value) NSLinguisticTag const name = @value;
TAG(NSLinguisticTagWord, "Word")
TAG(NSLinguisticTagPunctuation, "Punctuation")
TAG(NSLinguisticTagWhitespace, "Whitespace")
TAG(NSLinguisticTagOther, "Other")
TAG(NSLinguisticTagNoun, "Noun")
TAG(NSLinguisticTagVerb, "Verb")
TAG(NSLinguisticTagAdjective, "Adjective")
TAG(NSLinguisticTagAdverb, "Adverb")
TAG(NSLinguisticTagPronoun, "Pronoun")
TAG(NSLinguisticTagDeterminer, "Determiner")
TAG(NSLinguisticTagParticle, "Particle")
TAG(NSLinguisticTagPreposition, "Preposition")
TAG(NSLinguisticTagNumber, "Number")
TAG(NSLinguisticTagConjunction, "Conjunction")
TAG(NSLinguisticTagInterjection, "Interjection")
TAG(NSLinguisticTagClassifier, "Classifier")
TAG(NSLinguisticTagIdiom, "Idiom")
TAG(NSLinguisticTagOtherWord, "OtherWord")
TAG(NSLinguisticTagSentenceTerminator, "SentenceTerminator")
TAG(NSLinguisticTagOpenQuote, "OpenQuote")
TAG(NSLinguisticTagCloseQuote, "CloseQuote")
TAG(NSLinguisticTagOpenParenthesis, "OpenParenthesis")
TAG(NSLinguisticTagCloseParenthesis, "CloseParenthesis")
TAG(NSLinguisticTagWordJoiner, "WordJoiner")
TAG(NSLinguisticTagDash, "Dash")
TAG(NSLinguisticTagOtherPunctuation, "OtherPunctuation")
TAG(NSLinguisticTagParagraphBreak, "ParagraphBreak")
TAG(NSLinguisticTagOtherWhitespace, "OtherWhitespace")
TAG(NSLinguisticTagPersonalName, "PersonalName")
TAG(NSLinguisticTagPlaceName, "PlaceName")
TAG(NSLinguisticTagOrganizationName, "OrganizationName")
#undef TAG

/* -- Scripts and languages -- */

static NSString *scriptOf(unichar c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xc0 && c <= 0x24f)) return @"Latn";
    if (c >= 0x370 && c <= 0x3ff) return @"Grek";
    if (c >= 0x400 && c <= 0x52f) return @"Cyrl";
    if (c >= 0x590 && c <= 0x5ff) return @"Hebr";
    if (c >= 0x600 && c <= 0x6ff) return @"Arab";
    if (c >= 0x900 && c <= 0x97f) return @"Deva";
    if (c >= 0xe00 && c <= 0xe7f) return @"Thai";
    if (c >= 0x3040 && c <= 0x30ff) return @"Jpan";
    if (c >= 0xac00 && c <= 0xd7af) return @"Kore";
    if (c >= 0x4e00 && c <= 0x9fff) return @"Hani";
    return nil;
}

static NSString *languageOfScript(NSString *script) {
    static NSDictionary *languages;
    if (languages == nil)
        languages = @{ @"Grek": @"el", @"Cyrl": @"ru", @"Hebr": @"he", @"Arab": @"ar", @"Deva": @"hi",
            @"Thai": @"th", @"Jpan": @"ja", @"Kore": @"ko", @"Hani": @"zh-Hans" };
    return languages[script];
}

/* Latin-script text: the language whose common words it has most of. */
static NSString *latinLanguage(NSString *string) {
    static NSDictionary<NSString *, NSSet *> *words;
    if (words == nil)
        words = @{
            @"en": [NSSet setWithObjects:@"the", @"and", @"is", @"of", @"to", @"in", @"it", @"that", @"this", @"with", @"for", @"you", nil],
            @"es": [NSSet setWithObjects:@"el", @"la", @"de", @"que", @"y", @"en", @"los", @"las", @"es", @"un", @"una", @"por", @"con", nil],
            @"fr": [NSSet setWithObjects:@"le", @"la", @"les", @"et", @"est", @"un", @"une", @"des", @"du", @"dans", @"pour", @"que", nil],
            @"de": [NSSet setWithObjects:@"der", @"die", @"das", @"und", @"ist", @"nicht", @"ein", @"eine", @"mit", @"zu", @"den", @"ich", nil],
            @"it": [NSSet setWithObjects:@"il", @"la", @"di", @"che", @"e", @"un", @"una", @"per", @"non", @"sono", @"della", @"gli", nil],
            @"pt": [NSSet setWithObjects:@"o", @"a", @"de", @"que", @"e", @"um", @"uma", @"não", @"para", @"com", @"os", @"do", nil],
        };
    NSMutableDictionary *score = [NSMutableDictionary dictionary];
    [string enumerateSubstringsInRange:NSMakeRange(0, [string length]) options:NSStringEnumerationByWords
        usingBlock:^(NSString *word, NSRange r, NSRange e, BOOL *stop) {
            NSString *lower = [word lowercaseString];
            for (NSString *language in words)
                if ([words[language] containsObject:lower])
                    score[language] = @([score[language] integerValue] + 1);
        }];
    NSString *best = @"en";
    NSInteger bestScore = 0;
    for (NSString *language in @[@"en", @"es", @"fr", @"de", @"it", @"pt"])
        if ([score[language] integerValue] > bestScore) {
            best = language;
            bestScore = [score[language] integerValue];
        }
    return best;
}

static NSString *dominantScript(NSString *string) {
    NSMutableDictionary *count = [NSMutableDictionary dictionary];
    NSUInteger length = MIN([string length], 4096);
    for (NSUInteger i = 0; i < length; i++) {
        NSString *script = scriptOf([string characterAtIndex:i]);
        if (script != nil)
            count[script] = @([count[script] integerValue] + 1);
    }
    /* Kana with Han: Japanese. */
    if (count[@"Jpan"] != nil && count[@"Hani"] != nil)
        count[@"Jpan"] = @([count[@"Jpan"] integerValue] + [count[@"Hani"] integerValue]);
    NSString *best = nil;
    NSInteger most = 0;
    for (NSString *script in count)
        if ([count[script] integerValue] > most) {
            best = script;
            most = [count[script] integerValue];
        }
    return best;
}

static NSString *dominantLanguage(NSString *string) {
    NSString *script = dominantScript(string);
    if (script == nil)
        return nil;
    if ([script isEqualToString:@"Latn"])
        return latinLanguage(string);
    return languageOfScript(script);
}

/* -- Tokens -- */

static NSLinguisticTag punctuationTag(unichar c) {
    switch (c) {
    case '.': case '!': case '?': return NSLinguisticTagSentenceTerminator;
    case '(': case '[': case '{': return NSLinguisticTagOpenParenthesis;
    case ')': case ']': case '}': return NSLinguisticTagCloseParenthesis;
    case '"': case 0x201c: case 0x2018: case 0xab: return NSLinguisticTagOpenQuote;
    case 0x201d: case 0x2019: case 0xbb: return NSLinguisticTagCloseQuote;
    case '-': case 0x2013: case 0x2014: return NSLinguisticTagDash;
    default: return NSLinguisticTagOtherPunctuation;
    }
}

/* The words of a range, and what is between them, as tokens: whitespace
 * runs, single punctuation marks, other characters. */
static void enumerateWordTokens(NSString *string, NSRange range, void (^block)(NSRange token, NSLinguisticTag type, BOOL *stop)) {
    NSCharacterSet *spaces = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    NSCharacterSet *punctuation = [NSCharacterSet punctuationCharacterSet];
    __block NSUInteger at = range.location;
    __block BOOL stop = NO;
    void (^gap)(NSUInteger) = ^(NSUInteger end) {
        while (at < end && !stop) {
            unichar c = [string characterAtIndex:at];
            NSUInteger start = at;
            if ([spaces characterIsMember:c]) {
                while (at < end && [spaces characterIsMember:[string characterAtIndex:at]])
                    at++;
                block(NSMakeRange(start, at - start), NSLinguisticTagWhitespace, &stop);
            } else {
                at++;
                block(NSMakeRange(start, 1), [punctuation characterIsMember:c] ? NSLinguisticTagPunctuation : NSLinguisticTagOther, &stop);
            }
        }
    };
    [string enumerateSubstringsInRange:range options:NSStringEnumerationByWords | NSStringEnumerationSubstringNotRequired
        usingBlock:^(NSString *word, NSRange wordRange, NSRange enclosing, BOOL *outStop) {
            gap(wordRange.location);
            if (!stop) {
                block(wordRange, NSLinguisticTagWord, &stop);
                at = NSMaxRange(wordRange);
            }
            *outStop = stop;
        }];
    if (!stop)
        gap(NSMaxRange(range));
}

static NSStringEnumerationOptions enumerationOf(NSLinguisticTaggerUnit unit) {
    switch (unit) {
    case NSLinguisticTaggerUnitSentence: return NSStringEnumerationBySentences;
    case NSLinguisticTaggerUnitParagraph: return NSStringEnumerationByParagraphs;
    default: return NSStringEnumerationByWords;
    }
}

static BOOL omitted(NSLinguisticTag type, NSLinguisticTaggerOptions options) {
    if (type == NSLinguisticTagWord) return (options & NSLinguisticTaggerOmitWords) != 0;
    if (type == NSLinguisticTagPunctuation) return (options & NSLinguisticTaggerOmitPunctuation) != 0;
    if (type == NSLinguisticTagWhitespace) return (options & NSLinguisticTaggerOmitWhitespace) != 0;
    return (options & NSLinguisticTaggerOmitOther) != 0;
}

/* A token's tag in a scheme; nil where Minkar has no model. */
static NSLinguisticTag tagOf(NSString *string, NSRange token, NSLinguisticTag type, NSLinguisticTagScheme scheme) {
    if ([scheme isEqualToString:NSLinguisticTagSchemeTokenType])
        return type;
    if ([scheme isEqualToString:NSLinguisticTagSchemeScript])
        return type == NSLinguisticTagWord ? dominantScript([string substringWithRange:token]) : nil;
    if ([scheme isEqualToString:NSLinguisticTagSchemeLanguage])
        return type == NSLinguisticTagWord ? dominantLanguage([string substringWithRange:token]) : nil;
    if ([scheme isEqualToString:NSLinguisticTagSchemeLexicalClass] || [scheme isEqualToString:NSLinguisticTagSchemeNameTypeOrLexicalClass]) {
        if (type == NSLinguisticTagWhitespace) return NSLinguisticTagWhitespace;
        if (type == NSLinguisticTagPunctuation) return punctuationTag([string characterAtIndex:token.location]);
        if (type == NSLinguisticTagWord && [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:[string characterAtIndex:token.location]])
            return NSLinguisticTagNumber;
        return nil;
    }
    return nil;
}

static void enumerateTags(NSString *string, NSRange range, NSLinguisticTaggerUnit unit, NSLinguisticTagScheme scheme,
    NSLinguisticTaggerOptions options, void (^block)(NSLinguisticTag tag, NSRange token, BOOL *stop)) {
    if (string == nil || range.length == 0)
        return;
    if (unit == NSLinguisticTaggerUnitWord) {
        enumerateWordTokens(string, range, ^(NSRange token, NSLinguisticTag type, BOOL *stop) {
            if (!omitted(type, options))
                block(tagOf(string, token, type, scheme), token, stop);
        });
        return;
    }
    if (unit == NSLinguisticTaggerUnitDocument) {
        BOOL stop = NO;
        NSString *text = [string substringWithRange:range];
        NSLinguisticTag tag = [scheme isEqualToString:NSLinguisticTagSchemeLanguage] ? dominantLanguage(text) :
            [scheme isEqualToString:NSLinguisticTagSchemeScript] ? dominantScript(text) : nil;
        block(tag, range, &stop);
        return;
    }
    [string enumerateSubstringsInRange:range options:enumerationOf(unit) | NSStringEnumerationSubstringNotRequired
        usingBlock:^(NSString *s, NSRange r, NSRange e, BOOL *stop) {
            NSString *text = [string substringWithRange:r];
            NSLinguisticTag tag = [scheme isEqualToString:NSLinguisticTagSchemeLanguage] ? dominantLanguage(text) :
                [scheme isEqualToString:NSLinguisticTagSchemeScript] ? dominantScript(text) : nil;
            block(tag, r, stop);
        }];
}

@implementation NSLinguisticTagger

+ (NSArray *)availableTagSchemesForUnit:(NSLinguisticTaggerUnit)unit language:(NSString *)language {
    if (unit == NSLinguisticTaggerUnitWord)
        return @[NSLinguisticTagSchemeTokenType, NSLinguisticTagSchemeLanguage, NSLinguisticTagSchemeScript];
    return @[NSLinguisticTagSchemeLanguage, NSLinguisticTagSchemeScript];
}

+ (NSArray *)availableTagSchemesForLanguage:(NSString *)language {
    return [self availableTagSchemesForUnit:NSLinguisticTaggerUnitWord language:language];
}

- (instancetype)init {
    return [self initWithTagSchemes:@[NSLinguisticTagSchemeTokenType] options:0];
}

- (instancetype)initWithTagSchemes:(NSArray *)tagSchemes options:(NSUInteger)opts {
    if ((self = [super init]) == nil)
        return nil;
    _schemes = [tagSchemes copy];
    return self;
}

- (NSArray *)tagSchemes {
    return _schemes;
}

- (NSString *)string {
    return _string;
}

- (void)setString:(NSString *)string {
    _string = [string copy];
}

- (void)setOrthography:(NSOrthography *)orthography range:(NSRange)range {
}

- (NSOrthography *)orthographyAtIndex:(NSUInteger)charIndex effectiveRange:(NSRangePointer)effectiveRange {
    if (effectiveRange != NULL)
        *effectiveRange = NSMakeRange(0, [_string length]);
    return nil;
}

- (void)stringEditedInRange:(NSRange)newRange changeInLength:(NSInteger)delta {
}

- (NSRange)tokenRangeAtIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit {
    __block NSRange found = NSMakeRange(NSNotFound, 0);
    enumerateTags(_string, NSMakeRange(0, [_string length]), unit, NSLinguisticTagSchemeTokenType, 0,
        ^(NSLinguisticTag tag, NSRange token, BOOL *stop) {
            if (NSLocationInRange(charIndex, token)) {
                found = token;
                *stop = YES;
            }
        });
    return found;
}

- (NSRange)sentenceRangeForRange:(NSRange)range {
    NSRange first = [self tokenRangeAtIndex:range.location unit:NSLinguisticTaggerUnitSentence];
    NSRange last = [self tokenRangeAtIndex:NSMaxRange(range) > 0 ? NSMaxRange(range) - 1 : 0 unit:NSLinguisticTaggerUnitSentence];
    if (first.location == NSNotFound)
        return range;
    if (last.location == NSNotFound)
        return first;
    return NSUnionRange(first, last);
}

- (void)enumerateTagsInRange:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options usingBlock:(void (^)(NSLinguisticTag, NSRange, BOOL *))block {
    enumerateTags(_string, range, unit, scheme, options, block);
}

- (NSLinguisticTag)tagAtIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme tokenRange:(NSRangePointer)tokenRange {
    __block NSLinguisticTag found = nil;
    enumerateTags(_string, NSMakeRange(0, [_string length]), unit, scheme, 0, ^(NSLinguisticTag tag, NSRange token, BOOL *stop) {
        if (NSLocationInRange(charIndex, token)) {
            found = tag;
            if (tokenRange != NULL)
                *tokenRange = token;
            *stop = YES;
        }
    });
    return found;
}

- (NSArray *)tagsInRange:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options tokenRanges:(NSArray **)tokenRanges {
    NSMutableArray *tags = [NSMutableArray array], *ranges = [NSMutableArray array];
    enumerateTags(_string, range, unit, scheme, options, ^(NSLinguisticTag tag, NSRange token, BOOL *stop) {
        [tags addObject:tag ?: @""];
        [ranges addObject:[NSValue valueWithRange:token]];
    });
    if (tokenRanges != NULL)
        *tokenRanges = ranges;
    return tags;
}

- (void)enumerateTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)tagScheme options:(NSLinguisticTaggerOptions)opts usingBlock:(void (^)(NSLinguisticTag, NSRange, NSRange, BOOL *))block {
    enumerateTags(_string, range, NSLinguisticTaggerUnitWord, tagScheme, opts, ^(NSLinguisticTag tag, NSRange token, BOOL *stop) {
        block(tag, token, [self sentenceRangeForRange:token], stop);
    });
}

- (NSLinguisticTag)tagAtIndex:(NSUInteger)charIndex scheme:(NSLinguisticTagScheme)scheme tokenRange:(NSRangePointer)tokenRange sentenceRange:(NSRangePointer)sentenceRange {
    NSRange token = NSMakeRange(NSNotFound, 0);
    NSLinguisticTag tag = [self tagAtIndex:charIndex unit:NSLinguisticTaggerUnitWord scheme:scheme tokenRange:&token];
    if (tokenRange != NULL)
        *tokenRange = token;
    if (sentenceRange != NULL)
        *sentenceRange = token.location != NSNotFound ? [self sentenceRangeForRange:token] : token;
    return tag;
}

- (NSArray *)tagsInRange:(NSRange)range scheme:(NSString *)tagScheme options:(NSLinguisticTaggerOptions)opts tokenRanges:(NSArray **)tokenRanges {
    return [self tagsInRange:range unit:NSLinguisticTaggerUnitWord scheme:tagScheme options:opts tokenRanges:tokenRanges];
}

- (NSString *)dominantLanguage {
    return dominantLanguage(_string);
}

+ (NSString *)dominantLanguageForString:(NSString *)string {
    return dominantLanguage(string);
}

+ (NSLinguisticTag)tagForString:(NSString *)string atIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme orthography:(NSOrthography *)orthography tokenRange:(NSRangePointer)tokenRange {
    NSLinguisticTagger *tagger = [[NSLinguisticTagger alloc] initWithTagSchemes:@[scheme] options:0];
    tagger.string = string;
    return [tagger tagAtIndex:charIndex unit:unit scheme:scheme tokenRange:tokenRange];
}

+ (NSArray *)tagsForString:(NSString *)string range:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(NSOrthography *)orthography tokenRanges:(NSArray **)tokenRanges {
    NSLinguisticTagger *tagger = [[NSLinguisticTagger alloc] initWithTagSchemes:@[scheme] options:0];
    tagger.string = string;
    return [tagger tagsInRange:range unit:unit scheme:scheme options:options tokenRanges:tokenRanges];
}

+ (void)enumerateTagsForString:(NSString *)string range:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(NSOrthography *)orthography usingBlock:(void (^)(NSLinguisticTag, NSRange, BOOL *))block {
    enumerateTags(string, range, unit, scheme, options, block);
}

- (NSArray *)possibleTagsAtIndex:(NSUInteger)charIndex scheme:(NSString *)tagScheme tokenRange:(NSRangePointer)tokenRange sentenceRange:(NSRangePointer)sentenceRange scores:(NSArray **)scores {
    NSLinguisticTag tag = [self tagAtIndex:charIndex scheme:tagScheme tokenRange:tokenRange sentenceRange:sentenceRange];
    if (tag == nil)
        return nil;
    if (scores != NULL)
        *scores = @[@1.0];
    return @[tag];
}

@end

@implementation NSString (NSLinguisticAnalysis)

- (NSArray *)linguisticTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(NSOrthography *)orthography tokenRanges:(NSArray **)tokenRanges {
    return [NSLinguisticTagger tagsForString:self range:range unit:NSLinguisticTaggerUnitWord scheme:scheme
        options:options orthography:orthography tokenRanges:tokenRanges];
}

- (void)enumerateLinguisticTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(NSOrthography *)orthography usingBlock:(void (^)(NSLinguisticTag, NSRange, NSRange, BOOL *))block {
    NSLinguisticTagger *tagger = [[NSLinguisticTagger alloc] initWithTagSchemes:@[scheme] options:0];
    tagger.string = self;
    [tagger enumerateTagsInRange:range scheme:scheme options:options usingBlock:block];
}

@end
