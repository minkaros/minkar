/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Tokens of text and their tags (API 19): token types, scripts and the
 * dominant language. Minkar has no statistical models: lexical classes,
 * names and lemmas have no tags.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRange.h>

@class NSArray<ObjectType>, NSOrthography, NSValue;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSLinguisticTagScheme NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeTokenType;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeLexicalClass;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeNameType;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeNameTypeOrLexicalClass;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeLemma;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeLanguage;
FOUNDATION_EXPORT NSLinguisticTagScheme const NSLinguisticTagSchemeScript;

typedef NSString *NSLinguisticTag NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagWord;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagPunctuation;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagWhitespace;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOther;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagNoun;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagVerb;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagAdjective;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagAdverb;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagPronoun;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagDeterminer;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagParticle;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagPreposition;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagNumber;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagConjunction;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagInterjection;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagClassifier;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagIdiom;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOtherWord;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagSentenceTerminator;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOpenQuote;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagCloseQuote;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOpenParenthesis;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagCloseParenthesis;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagWordJoiner;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagDash;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOtherPunctuation;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagParagraphBreak;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOtherWhitespace;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagPersonalName;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagPlaceName;
FOUNDATION_EXPORT NSLinguisticTag const NSLinguisticTagOrganizationName;

typedef NS_ENUM(NSInteger, NSLinguisticTaggerUnit) {
    NSLinguisticTaggerUnitWord,
    NSLinguisticTaggerUnitSentence,
    NSLinguisticTaggerUnitParagraph,
    NSLinguisticTaggerUnitDocument
};

typedef NS_OPTIONS(NSUInteger, NSLinguisticTaggerOptions) {
    NSLinguisticTaggerOmitWords = 1 << 0,
    NSLinguisticTaggerOmitPunctuation = 1 << 1,
    NSLinguisticTaggerOmitWhitespace = 1 << 2,
    NSLinguisticTaggerOmitOther = 1 << 3,
    NSLinguisticTaggerJoinNames = 1 << 4
};

@interface NSLinguisticTagger : NSObject {
    NSArray *_schemes;
    NSString *_string;
}

- (instancetype)initWithTagSchemes:(NSArray<NSLinguisticTagScheme> *)tagSchemes options:(NSUInteger)opts NS_DESIGNATED_INITIALIZER;

@property (readonly, copy) NSArray<NSLinguisticTagScheme> *tagSchemes;
@property (nullable, retain) NSString *string;

+ (NSArray<NSLinguisticTagScheme> *)availableTagSchemesForUnit:(NSLinguisticTaggerUnit)unit language:(NSString *)language;
+ (NSArray<NSLinguisticTagScheme> *)availableTagSchemesForLanguage:(NSString *)language;

- (void)setOrthography:(nullable NSOrthography *)orthography range:(NSRange)range;
- (nullable NSOrthography *)orthographyAtIndex:(NSUInteger)charIndex effectiveRange:(nullable NSRangePointer)effectiveRange;
- (void)stringEditedInRange:(NSRange)newRange changeInLength:(NSInteger)delta;

- (NSRange)tokenRangeAtIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit;
- (NSRange)sentenceRangeForRange:(NSRange)range;

- (void)enumerateTagsInRange:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options usingBlock:(void (NS_NOESCAPE ^)(NSLinguisticTag _Nullable tag, NSRange tokenRange, BOOL *stop))block;
- (nullable NSLinguisticTag)tagAtIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme tokenRange:(nullable NSRangePointer)tokenRange;
- (NSArray<NSLinguisticTag> *)tagsInRange:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options tokenRanges:(NSArray<NSValue *> * _Nullable * _Nullable)tokenRanges;

- (void)enumerateTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)tagScheme options:(NSLinguisticTaggerOptions)opts usingBlock:(void (NS_NOESCAPE ^)(NSLinguisticTag _Nullable tag, NSRange tokenRange, NSRange sentenceRange, BOOL *stop))block;
- (nullable NSLinguisticTag)tagAtIndex:(NSUInteger)charIndex scheme:(NSLinguisticTagScheme)scheme tokenRange:(nullable NSRangePointer)tokenRange sentenceRange:(nullable NSRangePointer)sentenceRange;
- (NSArray<NSString *> *)tagsInRange:(NSRange)range scheme:(NSString *)tagScheme options:(NSLinguisticTaggerOptions)opts tokenRanges:(NSArray<NSValue *> * _Nullable * _Nullable)tokenRanges;

@property (nullable, readonly, copy) NSString *dominantLanguage;
+ (nullable NSString *)dominantLanguageForString:(NSString *)string;

+ (nullable NSLinguisticTag)tagForString:(NSString *)string atIndex:(NSUInteger)charIndex unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme orthography:(nullable NSOrthography *)orthography tokenRange:(nullable NSRangePointer)tokenRange;
+ (NSArray<NSLinguisticTag> *)tagsForString:(NSString *)string range:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(nullable NSOrthography *)orthography tokenRanges:(NSArray<NSValue *> * _Nullable * _Nullable)tokenRanges;
+ (void)enumerateTagsForString:(NSString *)string range:(NSRange)range unit:(NSLinguisticTaggerUnit)unit scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(nullable NSOrthography *)orthography usingBlock:(void (NS_NOESCAPE ^)(NSLinguisticTag _Nullable tag, NSRange tokenRange, BOOL *stop))block;

- (nullable NSArray<NSString *> *)possibleTagsAtIndex:(NSUInteger)charIndex scheme:(NSString *)tagScheme tokenRange:(nullable NSRangePointer)tokenRange sentenceRange:(nullable NSRangePointer)sentenceRange scores:(NSArray<NSValue *> * _Nullable * _Nullable)scores;

@end

@interface NSString (NSLinguisticAnalysis)
- (NSArray<NSLinguisticTag> *)linguisticTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(nullable NSOrthography *)orthography tokenRanges:(NSArray<NSValue *> * _Nullable * _Nullable)tokenRanges;
- (void)enumerateLinguisticTagsInRange:(NSRange)range scheme:(NSLinguisticTagScheme)scheme options:(NSLinguisticTaggerOptions)options orthography:(nullable NSOrthography *)orthography usingBlock:(void (NS_NOESCAPE ^)(NSLinguisticTag _Nullable tag, NSRange tokenRange, NSRange sentenceRange, BOOL *stop))block;
@end

NS_ASSUME_NONNULL_END
