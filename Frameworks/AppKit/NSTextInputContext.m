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

#import <AppKit/NSKeyboardBindingManager.h>
#import <AppKit/NSKeyboardBinding.h>
#import <AppKit/NSEvent.h>
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
/* Minkar: a key goes to the client as the reference platform's context
   sends it with no input method: what its key binding names as commands
   (doCommandBySelector:), else its characters as text
   (insertText:replacementRange:); function keys and control characters
   that nothing binds are not text. It did nothing, so clients that read
   their keys through the context (TextMate's text view) got none. */
-(BOOL)handleEvent:(NSEvent *)event {
   if([event type]!=NSEventTypeKeyDown || _client==nil)
    return NO;

   NSKeyboardBinding *binding=[[NSKeyboardBindingManager defaultKeyBindingManager] keyBindingWithString:[event charactersIgnoringModifiers] modifierFlags:[event modifierFlags]];
   NSArray           *selectorNames=[binding selectorNames];

   if(selectorNames!=nil){
    for(NSString *name in selectorNames)
     [_client doCommandBySelector:NSSelectorFromString(name)];
    return YES;
   }

   // A key held with Command is no text: the client is told "noop:", as
   // the reference platform's context does (Command-R typed an r).
   if([event modifierFlags]&NSEventModifierFlagCommand){
    [_client doCommandBySelector:@selector(noop:)];
    return YES;
   }

   NSString       *characters=[event characters];
   NSUInteger      i,length=[characters length];
   NSMutableString *text=[NSMutableString string];

   for(i=0;i<length;i++){
    unichar c=[characters characterAtIndex:i];

    if((c>=NSUpArrowFunctionKey && c<=NSModeSwitchFunctionKey) || c<' ' || c==0x7f)
     continue;
    [text appendString:[NSString stringWithCharacters:&c length:1]];
   }
   if([text length]>0)
    [_client insertText:text replacementRange:NSMakeRange(NSNotFound,0)];
   return YES;
}

-(void)discardMarkedText {
   if([_client respondsToSelector:@selector(hasMarkedText)] && [_client hasMarkedText])
    [_client unmarkText];
}

-(void)invalidateCharacterCoordinates {
}

@end
