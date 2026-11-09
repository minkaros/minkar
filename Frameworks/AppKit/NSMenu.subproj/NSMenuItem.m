/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSEvent.h>
#import <Foundation/NSKeyedArchiver.h>

@implementation NSMenuItem

+(NSMenuItem *)separatorItem {
   return [[[self alloc] initWithTitle:nil action:NULL keyEquivalent:nil] autorelease];
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_title forKey:@"NSTitle"];
   [coder encodeObject:_keyEquivalent forKey:@"NSKeyEquiv"];
   [coder encodeInt:_keyEquivalentModifierMask forKey:@"NSKeyEquivModMask"];
   [coder encodeObject:_submenu forKey:@"NSSubmenu"];
   [coder encodeInt:_tag forKey:@"NSTag"];
   [coder encodeObject: NSStringFromSelector(_action) forKey: @"NSAction"];
   [coder encodeObject: _target forKey: @"NSTarget"];
   [coder encodeObject:_image forKey: @"NSImage"];
   [coder encodeBool:_hidden forKey: @"NSIsHidden"];
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    NSString          *title=[keyed decodeObjectForKey:@"NSTitle"];
    NSString          *keyEquivalent=[keyed decodeObjectForKey:@"NSKeyEquiv"];

    SEL action = NULL;
    NSString *actionString = [coder decodeObjectForKey: @"NSAction"];
    if (actionString) {
        action = NSSelectorFromString(actionString);
    }
    [self initWithTitle:title action:action keyEquivalent:keyEquivalent];
    id target = [coder decodeObjectForKey: @"NSTarget"];
    [self setTarget: target];

    /* Left out when it is the default, Command. */
    if([keyed containsValueForKey:@"NSKeyEquivModMask"])
     [self setKeyEquivalentModifierMask:[keyed decodeIntForKey:@"NSKeyEquivModMask"]];
    [self setSubmenu:[keyed decodeObjectForKey:@"NSSubmenu"]];
    _tag=[keyed decodeIntForKey:@"NSTag"];
    _hidden = [keyed decodeBoolForKey:@"NSIsHidden"];
    _image = [[coder decodeObjectForKey:@"NSImage"] retain];
   if([keyed decodeBoolForKey:@"NSIsSeparator"]){
     [_title release];
     _title=nil;
    }
   }
   else {
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];
   }

   return self;
}

-initWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)keyEquivalent {
    _title= [title copy];
   _target=nil;
   _action=action;
   _keyEquivalent=[keyEquivalent copy];
   _keyEquivalentModifierMask=NSCommandKeyMask; // the API's default
   _mnemonic=@"";
   _mnemonicLocation=0;
   _submenu=nil;
   _tag=-1;
   _indentationLevel = 0;
   _enabled=YES;
   _hidden=NO;

   return self;
}

-(void)dealloc {
    [_title release];
   [_atitle release];
   [_keyEquivalent release];
   [_submenu release];
   [_image release];
   [_onStateImage release];
   [_mixedStateImage release];
   [_offStateImage release];
   [_representedObject release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
    NSMenuItem *copy=NSCopyObject(self, 0, zone);

    copy->_title=[_title copyWithZone:zone];
    copy->_atitle=[_atitle copyWithZone:zone];
    copy->_submenu=[_submenu copyWithZone:zone];
    copy->_keyEquivalent=[_keyEquivalent copyWithZone:zone];;
    copy->_image=[_image retain];
    copy->_onStateImage=[_onStateImage retain];
    copy->_mixedStateImage=[_mixedStateImage retain];
    copy->_offStateImage=[_offStateImage retain];
    copy->_representedObject=[_representedObject retain];
    return copy;
}

-(NSMenu *)menu {
   return _menu;
}

-(void)_setMenu:(NSMenu *)menu {
   _menu=menu;
   _submenu.supermenu=menu;
}

-(NSString *)title {
   return _title;
}

-(NSAttributedString *)attributedTitle {
   return _atitle;
}

-(NSString *)mnemonic {
   return _mnemonic;
}

-(NSUInteger)mnemonicLocation {
   return _mnemonicLocation;
}

-(id)target {
   return _target;
}

-(SEL)action {
   return _action;
}

-(NSInteger)indentationLevel
{
    return _indentationLevel;
}

-(NSInteger)tag {
   return _tag;
}

-(NSControlStateValue)state {
    return _state;
}

-(NSString *)keyEquivalent {
   return _keyEquivalent;
}

-(NSEventModifierFlags)keyEquivalentModifierMask {
   return _keyEquivalentModifierMask;
}

-(NSImage *)image {
    return _image;
}

-(NSImage *)onStateImage {
    return _onStateImage;
}

-(NSImage *)offStateImage {
    return _offStateImage;
}

-(NSImage *)mixedStateImage {
    return _mixedStateImage;
}

-(id)representedObject {
    return _representedObject;
}

-(BOOL)hasSubmenu {
   return (_submenu!=nil)?YES:NO;
}

-(NSMenu *)submenu {
   return _submenu;
}

-(BOOL)isSeparatorItem {
   return _title==nil;
}

-(BOOL)isEnabled {
   return _enabled;
}

-(BOOL)isHidden {
    return _hidden;
}

-(void)setTitle:(NSString *)title {
    title=[title copy];
    [_title release];
    _title=title;
    [_menu itemChanged:self];
}

-(void)setAttributedTitle:(NSAttributedString *)title {
   title=[title copy];
   [_atitle release];
   _atitle=title;
    [self setTitle: [title string]];
}

-(void)setTitleWithMnemonic:(NSString *)mnemonic {
   mnemonic=[mnemonic copy];
   [_mnemonic release];
   _mnemonic=mnemonic;
}

-(void)setMnemonicLocation:(NSUInteger)location {
   _mnemonicLocation=location;
}

-(void)setTarget:(id)target {
   _target=target;
}

-(void)setAction:(SEL)action {
   _action=action;
}

-(void)setIndentationLevel:(NSInteger)indentationLevel
{
    _indentationLevel = indentationLevel;
}

-(void)setTag:(NSInteger)tag {
   _tag=tag;
}

-(void)setState:(NSControlStateValue)state {
    _state = state;
    [_menu itemChanged:self];
}

-(void)setKeyEquivalent:(NSString *)keyEquivalent {
   keyEquivalent=[keyEquivalent copy];
   [_keyEquivalent release];
   _keyEquivalent=keyEquivalent;
   [_menu itemChanged:self];
}

-(void)setKeyEquivalentModifierMask:(NSEventModifierFlags)mask {
   _keyEquivalentModifierMask=mask;
   [_menu itemChanged:self];
}

-(void)setImage:(NSImage *)image {
   image=[image retain];
    [_image release];
    _image = image;
    [_menu itemChanged:self];
}

-(void)setOnStateImage:(NSImage *)image {
    [_onStateImage release];
    _onStateImage = [image retain];
}

-(void)setOffStateImage:(NSImage *)image {
    [_offStateImage release];
    _offStateImage = [image retain];
}

-(void)setMixedStateImage:(NSImage *)image {
    [_mixedStateImage release];
    _mixedStateImage = [image retain];
}

-(void)setRepresentedObject:(id)object {
    [_representedObject release];
    _representedObject = [object retain];
}

-(void)setSubmenu:(NSMenu *)submenu {
    submenu=[submenu retain];
   [_submenu release];
   _submenu=submenu;
    [submenu setSupermenu:_menu];
    [_menu itemChanged:self];
}

-(void)setEnabled:(BOOL)flag {
   if(_enabled==flag)
    return;
   _enabled=flag;
   [_menu itemChanged:self];
}

-(void)setHidden:(BOOL)flag {
   if(_hidden==flag)
    return;
   _hidden=flag;
   [_menu itemChanged:self];
}

/* The names the reference platform shows for keys that are not
   characters, by the key equivalent's character. */
static NSString *keyGlyph(unichar ch) {
   switch(ch){
    case '\r': return @"\u21A9";              // return
    case 3: return @"Enter";                   // enter (no glyph in the font)
    case '\t': return @"\u21E5";              // tab
    case 0x19: return @"\u21E4";               // back tab
    case 8: case 0x7F: return @"\u232B";       // delete
    case 0x1B: return @"\u238B";               // escape
    case ' ': return @"Space";
    case NSDeleteFunctionKey: return @"\u2326";
    case NSUpArrowFunctionKey: return @"\u2191";
    case NSDownArrowFunctionKey: return @"\u2193";
    case NSLeftArrowFunctionKey: return @"\u2190";
    case NSRightArrowFunctionKey: return @"\u2192";
    case NSHomeFunctionKey: return @"\u2196";
    case NSEndFunctionKey: return @"\u2198";
    case NSPageUpFunctionKey: return @"\u21DE";
    case NSPageDownFunctionKey: return @"\u21DF";
   }
   if(ch>=NSF1FunctionKey && ch<=NSF35FunctionKey)
    return [NSString stringWithFormat:@"F%d",ch-NSF1FunctionKey+1];
   return nil;
}

/* The key equivalent as menus show it: the modifiers as glyphs in the
   reference platform's order (Control, Option, Shift, Command), then the
   key, uppercase. An uppercase letter implies Shift. */
-(NSString *)_keyEquivalentDescription {
   NSString        *key=_keyEquivalent;
   NSUInteger       mask=_keyEquivalentModifierMask;
   NSMutableString *result=[NSMutableString string];

   if([key length]==0)
    return @"";
   if(![key isEqualToString:[key lowercaseString]])
    mask|=NSShiftKeyMask;
   if(mask&NSControlKeyMask)
    [result appendString:@"\u2303"];
   if(mask&NSAlternateKeyMask)
    [result appendString:@"\u2325"];
   if(mask&NSShiftKeyMask)
    [result appendString:@"\u21E7"];
   if(mask&NSCommandKeyMask)
    [result appendString:@"\u2318"];

   NSString *glyph=([key length]==1)?keyGlyph([key characterAtIndex:0]):nil;
   [result appendString:glyph?glyph:[key uppercaseString]];
   return result;
}

-(int)DBusItemID {
    return _DBusItemID;
}

-(void)setDBusItemID:(int)tag {
    _DBusItemID = tag;
}

-(NSString *)description {
    return [NSString stringWithFormat:@"<%@[0x%x]: title: %@ action: %@ hasSubmenu: %@>",
        [self class],self,[self title],NSStringFromSelector(_action),([self hasSubmenu] ? @"YES" : @"NO")];
}

@end
