/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AVFoundation/AVSpeech.h>

const float AVSpeechUtteranceMinimumSpeechRate = 0.0f;
const float AVSpeechUtteranceMaximumSpeechRate = 1.0f;
const float AVSpeechUtteranceDefaultSpeechRate = 0.5f;

@implementation AVSpeechSynthesisVoice

+ (BOOL)supportsSecureCoding {
    return YES;
}

/* No voices are installed. */
+ (NSArray<AVSpeechSynthesisVoice *> *)speechVoices {
    return @[];
}

+ (NSString *)currentLanguageCode {
    NSString *language = [[NSLocale preferredLanguages] firstObject];
    return language ?: @"en-US";
}

+ (AVSpeechSynthesisVoice *)voiceWithLanguage:(NSString *)languageCode {
    return nil;
}

+ (AVSpeechSynthesisVoice *)voiceWithIdentifier:(NSString *)identifier {
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

@end

@implementation AVSpeechUtterance

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)speechUtteranceWithString:(NSString *)string {
    return [[self alloc] initWithString:string];
}

- (instancetype)initWithString:(NSString *)string {
    if ((self = [super init]) != nil) {
        _speechString = [string copy];
        _rate = AVSpeechUtteranceDefaultSpeechRate;
        _pitchMultiplier = 1.0f;
        _volume = 1.0f;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    AVSpeechUtterance *copy = [[AVSpeechUtterance alloc] initWithString:_speechString];
    copy.voice = _voice;
    copy.rate = _rate;
    copy.pitchMultiplier = _pitchMultiplier;
    copy.volume = _volume;
    copy.preUtteranceDelay = _preUtteranceDelay;
    copy.postUtteranceDelay = _postUtteranceDelay;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_speechString forKey:@"speechString"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithString:[coder decodeObjectOfClass:[NSString class] forKey:@"speechString"] ?: @""];
}

@end

@implementation AVSpeechSynthesizer

/* With no voice to speak it, the utterance is cancelled. */
- (void)speakUtterance:(AVSpeechUtterance *)utterance {
    dispatch_async(dispatch_get_main_queue(), ^{
        id<AVSpeechSynthesizerDelegate> delegate = self.delegate;
        if ([delegate respondsToSelector:@selector(speechSynthesizer:didCancelSpeechUtterance:)])
            [delegate speechSynthesizer:self didCancelSpeechUtterance:utterance];
    });
}

- (BOOL)stopSpeakingAtBoundary:(AVSpeechBoundary)boundary {
    return NO;
}

- (BOOL)pauseSpeakingAtBoundary:(AVSpeechBoundary)boundary {
    return NO;
}

- (BOOL)continueSpeaking {
    return NO;
}

@end
