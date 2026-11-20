// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <CoreGraphics/CGPSConverter.h>
#import <Foundation/NSObject.h>
#include <stdlib.h>
#include <string.h>

/* Minkar: there is no PostScript interpreter yet. A converter is made and
   its callbacks kept; a conversion begins and ends the document, telling
   it did not succeed (as the reference does for input it cannot read), a
   message says why, and nothing is written. */
@interface _CGPSConverter : NSObject {
@public
    void *_info;
    CGPSConverterCallbacks _callbacks;
    BOOL _converting;
}
@end

@implementation _CGPSConverter

- (void)dealloc {
    if (_callbacks.releaseInfo != NULL)
        _callbacks.releaseInfo(_info);
    [super dealloc];
}

@end

CGPSConverterRef CGPSConverterCreate(void *info, const CGPSConverterCallbacks *callbacks, CFDictionaryRef options) {
    _CGPSConverter *converter = [[_CGPSConverter alloc] init];

    (void)options;
    converter->_info = info;
    if (callbacks != NULL)
        memcpy(&converter->_callbacks, callbacks, sizeof(converter->_callbacks));
    return (CGPSConverterRef)converter;
}

bool CGPSConverterConvert(CGPSConverterRef ref, CGDataProviderRef provider, CGDataConsumerRef consumer, CFDictionaryRef options) {
    _CGPSConverter *converter = (_CGPSConverter *)ref;

    (void)consumer;
    (void)options;
    if (converter == nil || provider == NULL)
        return false;
    converter->_converting = YES;
    if (converter->_callbacks.beginDocument != NULL)
        converter->_callbacks.beginDocument(converter->_info);
    if (converter->_callbacks.noteMessage != NULL)
        converter->_callbacks.noteMessage(converter->_info, CFSTR("PostScript cannot be converted on this system."));
    if (converter->_callbacks.endDocument != NULL)
        converter->_callbacks.endDocument(converter->_info, false);
    converter->_converting = NO;
    return false;
}

/* Nothing is ever being converted when this is asked. */
bool CGPSConverterAbort(CGPSConverterRef ref) {
    _CGPSConverter *converter = (_CGPSConverter *)ref;

    return (converter != nil && converter->_converting);
}

bool CGPSConverterIsConverting(CGPSConverterRef ref) {
    _CGPSConverter *converter = (_CGPSConverter *)ref;

    return (converter != nil && converter->_converting);
}

CFTypeID CGPSConverterGetTypeID(void) {
    return (CFTypeID)(uintptr_t)[_CGPSConverter class];
}
