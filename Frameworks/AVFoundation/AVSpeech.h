/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Speech synthesis. Minkar has no voices yet: an utterance is cancelled.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVSpeechBoundary) {
    AVSpeechBoundaryImmediate,
    AVSpeechBoundaryWord,
};

typedef NS_ENUM(NSInteger, AVSpeechSynthesisVoiceQuality) {
    AVSpeechSynthesisVoiceQualityDefault = 1,
    AVSpeechSynthesisVoiceQualityEnhanced,
    AVSpeechSynthesisVoiceQualityPremium,
};

typedef NS_ENUM(NSInteger, AVSpeechSynthesisVoiceGender) {
    AVSpeechSynthesisVoiceGenderUnspecified,
    AVSpeechSynthesisVoiceGenderMale,
    AVSpeechSynthesisVoiceGenderFemale,
};

FOUNDATION_EXPORT const float AVSpeechUtteranceMinimumSpeechRate;
FOUNDATION_EXPORT const float AVSpeechUtteranceMaximumSpeechRate;
FOUNDATION_EXPORT const float AVSpeechUtteranceDefaultSpeechRate;

@interface AVSpeechSynthesisVoice : NSObject <NSSecureCoding>
+ (NSArray<AVSpeechSynthesisVoice *> *)speechVoices;
+ (NSString *)currentLanguageCode;
+ (nullable AVSpeechSynthesisVoice *)voiceWithLanguage:(nullable NSString *)languageCode;
+ (nullable AVSpeechSynthesisVoice *)voiceWithIdentifier:(NSString *)identifier;
@property (readonly) NSString *language;
@property (readonly) NSString *identifier;
@property (readonly) NSString *name;
@property (readonly) AVSpeechSynthesisVoiceQuality quality;
@property (readonly) AVSpeechSynthesisVoiceGender gender;
@end

@interface AVSpeechUtterance : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)speechUtteranceWithString:(NSString *)string;
- (instancetype)initWithString:(NSString *)string;
@property (nullable, strong) AVSpeechSynthesisVoice *voice;
@property (readonly) NSString *speechString;
@property float rate;
@property float pitchMultiplier;
@property float volume;
@property NSTimeInterval preUtteranceDelay;
@property NSTimeInterval postUtteranceDelay;
@end

@class AVSpeechSynthesizer;

@protocol AVSpeechSynthesizerDelegate <NSObject>
@optional
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didStartSpeechUtterance:(AVSpeechUtterance *)utterance;
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance;
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didPauseSpeechUtterance:(AVSpeechUtterance *)utterance;
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didContinueSpeechUtterance:(AVSpeechUtterance *)utterance;
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance;
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer willSpeakRangeOfSpeechString:(NSRange)characterRange
                utterance:(AVSpeechUtterance *)utterance;
@end

@interface AVSpeechSynthesizer : NSObject
@property (nullable, weak) id<AVSpeechSynthesizerDelegate> delegate;
@property (readonly, getter=isSpeaking) BOOL speaking;
@property (readonly, getter=isPaused) BOOL paused;
- (void)speakUtterance:(AVSpeechUtterance *)utterance;
- (BOOL)stopSpeakingAtBoundary:(AVSpeechBoundary)boundary;
- (BOOL)pauseSpeakingAtBoundary:(AVSpeechBoundary)boundary;
- (BOOL)continueSpeaking;
@end

NS_ASSUME_NONNULL_END
