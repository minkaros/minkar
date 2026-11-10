// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.
//
// The DOM classes on WebScriptObject (see DOM.h): each property and
// method runs its JavaScript counterpart on the page's object.

#import <WebKit/DOM.h>
#import "WebScriptObjectInternal.h"

/* An object result, or nil (null, undefined, an exception, or another
   kind of value). */
static id objectOf(id value, Class kind)
{
    return [value isKindOfClass:kind] ? value : nil;
}

static NSString *stringOf(id value)
{
    return [value isKindOfClass:[NSString class]] ? value : nil;
}

static NSString *scriptValueOf(id value)
{
    if (value == nil || value == [NSNull null])
        return @"null";
    if ([value isKindOfClass:[WebScriptObject class]])
        return [(WebScriptObject *)value expression];
    if ([value isKindOfClass:[NSNumber class]])
        return [value stringValue];
    return _WebScriptLiteral([value description]);
}

@implementation DOMObject

/* The value of the object's property name. */
- (id)_get:(NSString *)name {
    return [self _run:[NSString stringWithFormat:@"return (%@)[%@];", self.expression, _WebScriptLiteral(name)]];
}

- (void)_set:(NSString *)name to:(id)value {
    [self _run:[NSString stringWithFormat:@"(%@)[%@] = %@;", self.expression, _WebScriptLiteral(name), scriptValueOf(value)]];
}

/* The object's method name called with arguments (strings, numbers,
   null, script objects). */
- (id)_call:(NSString *)name with:(NSArray *)arguments {
    NSMutableArray *values = [NSMutableArray array];
    for (id argument in arguments)
        [values addObject:scriptValueOf(argument)];
    return [self _run:[NSString stringWithFormat:@"var o = %@; return o[%@](%@);", self.expression,
                       _WebScriptLiteral(name), [values componentsJoinedByString:@", "]]];
}

- (BOOL)isEqual:(id)object {
    return [object isKindOfClass:[DOMObject class]] && [[object expression] isEqualToString:self.expression];
}

- (NSUInteger)hash {
    return self.expression.hash;
}

@end

@implementation DOMNode
- (NSString *)nodeName { return stringOf([self _get:@"nodeName"]) ?: @""; }
- (NSString *)nodeValue { return stringOf([self _get:@"nodeValue"]); }
- (void)setNodeValue:(NSString *)value { [self _set:@"nodeValue" to:value]; }
- (unsigned short)nodeType { return [[self _get:@"nodeType"] unsignedShortValue]; }
- (DOMNode *)parentNode { return objectOf([self _get:@"parentNode"], [DOMNode class]); }
- (DOMNodeList *)childNodes {
    DOMNodeList *list = [DOMNodeList new];
    list.webView = self.webView;
    list.expression = [NSString stringWithFormat:@"(%@).childNodes", self.expression];
    return list;
}
- (DOMNode *)firstChild { return objectOf([self _get:@"firstChild"], [DOMNode class]); }
- (DOMNode *)lastChild { return objectOf([self _get:@"lastChild"], [DOMNode class]); }
- (DOMNode *)previousSibling { return objectOf([self _get:@"previousSibling"], [DOMNode class]); }
- (DOMNode *)nextSibling { return objectOf([self _get:@"nextSibling"], [DOMNode class]); }
- (DOMDocument *)ownerDocument { return objectOf([self _get:@"ownerDocument"], [DOMDocument class]); }
- (NSString *)textContent { return stringOf([self _get:@"textContent"]); }
- (void)setTextContent:(NSString *)text { [self _set:@"textContent" to:text]; }
- (BOOL)hasChildNodes { return [[self _call:@"hasChildNodes" with:@[]] boolValue]; }
- (DOMNode *)appendChild:(DOMNode *)child { return objectOf([self _call:@"appendChild" with:@[ child ]], [DOMNode class]); }
- (DOMNode *)removeChild:(DOMNode *)child { return objectOf([self _call:@"removeChild" with:@[ child ]], [DOMNode class]); }
- (DOMNode *)cloneNode:(BOOL)deep { return objectOf([self _call:@"cloneNode" with:@[ @(deep) ]], [DOMNode class]); }
@end

@implementation DOMNodeList
- (unsigned)length { return [[self _get:@"length"] unsignedIntValue]; }
- (DOMNode *)item:(unsigned)index { return objectOf([self _call:@"item" with:@[ @(index) ]], [DOMNode class]); }
@end

@implementation DOMCharacterData
- (NSString *)data { return stringOf([self _get:@"data"]) ?: @""; }
- (void)setData:(NSString *)data { [self _set:@"data" to:data]; }
- (unsigned)length { return [[self _get:@"length"] unsignedIntValue]; }
@end

@implementation DOMText
@end

@implementation DOMElement
- (NSString *)tagName { return stringOf([self _get:@"tagName"]) ?: @""; }
- (NSString *)getAttribute:(NSString *)name { return stringOf([self _call:@"getAttribute" with:@[ name ]]); }
- (void)setAttribute:(NSString *)name value:(NSString *)value { [self _call:@"setAttribute" with:@[ name, value ?: @"" ]]; }
- (void)removeAttribute:(NSString *)name { [self _call:@"removeAttribute" with:@[ name ]]; }
- (BOOL)hasAttribute:(NSString *)name { return [[self _call:@"hasAttribute" with:@[ name ]] boolValue]; }
- (DOMNodeList *)getElementsByTagName:(NSString *)name {
    DOMNodeList *list = [DOMNodeList new];
    list.webView = self.webView;
    list.expression = [NSString stringWithFormat:@"(%@).getElementsByTagName(%@)", self.expression, _WebScriptLiteral(name)];
    return list;
}
@end

@implementation DOMDocumentFragment
@end

@implementation DOMDocument
- (DOMElement *)documentElement { return objectOf([self _get:@"documentElement"], [DOMElement class]); }
- (DOMElement *)body { return objectOf([self _get:@"body"], [DOMElement class]); }
- (NSString *)title { return stringOf([self _get:@"title"]) ?: @""; }
- (void)setTitle:(NSString *)title { [self _set:@"title" to:title]; }
- (DOMElement *)getElementById:(NSString *)elementId {
    return objectOf([self _call:@"getElementById" with:@[ elementId ]], [DOMElement class]);
}
- (DOMNodeList *)getElementsByTagName:(NSString *)name {
    DOMNodeList *list = [DOMNodeList new];
    list.webView = self.webView;
    list.expression = [NSString stringWithFormat:@"(%@).getElementsByTagName(%@)", self.expression, _WebScriptLiteral(name)];
    return list;
}
- (DOMElement *)createElement:(NSString *)tagName {
    return objectOf([self _call:@"createElement" with:@[ tagName ]], [DOMElement class]);
}
- (DOMText *)createTextNode:(NSString *)data {
    return objectOf([self _call:@"createTextNode" with:@[ data ?: @"" ]], [DOMText class]);
}
- (DOMDocumentFragment *)createDocumentFragment {
    return objectOf([self _call:@"createDocumentFragment" with:@[]], [DOMDocumentFragment class]);
}
- (DOMRange *)createRange {
    return objectOf([self _call:@"createRange" with:@[]], [DOMRange class]);
}
- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root whatToShow:(unsigned)whatToShow
                                 filter:(id<DOMNodeFilter>)filter expandEntityReferences:(BOOL)expand {
    if (filter != nil)
        NSLog(@"WebKit: a DOM node filter is not supported; the iterator shows what whatToShow says");
    return objectOf([self _call:@"createNodeIterator" with:@[ root, @(whatToShow) ]], [DOMNodeIterator class]);
}
@end

@implementation DOMRange
- (DOMNode *)startContainer { return objectOf([self _get:@"startContainer"], [DOMNode class]); }
- (int)startOffset { return [[self _get:@"startOffset"] intValue]; }
- (DOMNode *)endContainer { return objectOf([self _get:@"endContainer"], [DOMNode class]); }
- (int)endOffset { return [[self _get:@"endOffset"] intValue]; }
- (BOOL)collapsed { return [[self _get:@"collapsed"] boolValue]; }
- (DOMNode *)commonAncestorContainer { return objectOf([self _get:@"commonAncestorContainer"], [DOMNode class]); }
- (NSString *)text { return [self toString]; }
- (DOMDocumentFragment *)cloneContents {
    return objectOf([self _call:@"cloneContents" with:@[]], [DOMDocumentFragment class]);
}
- (void)selectNode:(DOMNode *)node { [self _call:@"selectNode" with:@[ node ]]; }
- (void)selectNodeContents:(DOMNode *)node { [self _call:@"selectNodeContents" with:@[ node ]]; }
- (NSString *)toString { return stringOf([self _call:@"toString" with:@[]]) ?: @""; }
@end

@implementation DOMNodeIterator
- (DOMNode *)root { return objectOf([self _get:@"root"], [DOMNode class]); }
- (unsigned)whatToShow { return [[self _get:@"whatToShow"] unsignedIntValue]; }
- (DOMNode *)nextNode { return objectOf([self _call:@"nextNode" with:@[]], [DOMNode class]); }
- (DOMNode *)previousNode { return objectOf([self _call:@"previousNode" with:@[]], [DOMNode class]); }
- (void)detach { [self _call:@"detach" with:@[]]; }
@end
