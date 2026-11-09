/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <AppKit/NSTextInputContext.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>

NSString *const NSTextInputContextKeyboardSelectionDidChangeNotification=@"NSTextInputContextKeyboardSelectionDidChangeNotification";

/* The keyboard layout in use: the only input source there is. */
static NSString *const kKeyboardInputSource=@"org.minkar.keylayout.US";

static NSTextInputContext *sActive;

@implementation NSTextInputContext

+(NSTextInputContext *)currentInputContext {
   NSResponder *responder=[[NSApp keyWindow] firstResponder];

   if([responder respondsToSelector:@selector(inputContext)])
    return [(NSView *)responder inputContext];
   return sActive;
}

-initWithClient:(id<NSTextInputClient>)client {
   self=[super init];
   if(self!=nil)
    _client=client;
   return self;
}

-init {
   return [self initWithClient:nil];
}

-(void)dealloc {
   if(sActive==self)
    sActive=nil;
   [_allowedInputSourceLocales release];
   [super dealloc];
}

-(id<NSTextInputClient>)client {
   return _client;
}

-(BOOL)acceptsGlyphInfo {
   return _acceptsGlyphInfo;
}

-(void)setAcceptsGlyphInfo:(BOOL)value {
   _acceptsGlyphInfo=value;
}

-(NSArray *)allowedInputSourceLocales {
   return _allowedInputSourceLocales;
}

-(void)setAllowedInputSourceLocales:(NSArray *)locales {
   locales=[locales copy];
   [_allowedInputSourceLocales release];
   _allowedInputSourceLocales=locales;
}

-(NSArray *)keyboardInputSources {
   return [NSArray arrayWithObject:kKeyboardInputSource];
}

-(NSString *)selectedKeyboardInputSource {
   return kKeyboardInputSource;
}

-(void)setSelectedKeyboardInputSource:(NSString *)source {
}

+(NSString *)localizedNameForInputSource:(NSString *)source {
   return [source isEqualToString:kKeyboardInputSource]?@"U.S.":nil;
}

-(void)activate {
   sActive=self;
}

-(void)deactivate {
   if(sActive==self)
    sActive=nil;
}

/* No input method takes the event: the client handles its keys. */
-(BOOL)handleEvent:(NSEvent *)event {
   return NO;
}

-(void)discardMarkedText {
   if([_client respondsToSelector:@selector(hasMarkedText)] && [_client hasMarkedText])
    [_client unmarkText];
}

-(void)invalidateCharacterCoordinates {
}

@end
