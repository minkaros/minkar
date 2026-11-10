// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The older WebView's DOM API (a subset): nodes, documents, elements,
// text, ranges and node iterators. Each object is a WebScriptObject naming
// the page's own object, so reading and changing one reads and changes the
// page (synchronously, as WebScriptObject does).

#import <WebKit/WebView.h>

NS_ASSUME_NONNULL_BEGIN

@class DOMNode, DOMDocument, DOMNodeList, DOMElement, DOMDocumentFragment, DOMRange, DOMNodeIterator;

/* Node types. */
enum {
    DOM_ELEMENT_NODE = 1,
    DOM_ATTRIBUTE_NODE = 2,
    DOM_TEXT_NODE = 3,
    DOM_CDATA_SECTION_NODE = 4,
    DOM_ENTITY_REFERENCE_NODE = 5,
    DOM_ENTITY_NODE = 6,
    DOM_PROCESSING_INSTRUCTION_NODE = 7,
    DOM_COMMENT_NODE = 8,
    DOM_DOCUMENT_NODE = 9,
    DOM_DOCUMENT_TYPE_NODE = 10,
    DOM_DOCUMENT_FRAGMENT_NODE = 11,
    DOM_NOTATION_NODE = 12
};

/* What a node iterator shows. */
enum {
    DOM_SHOW_ALL = 0xFFFFFFFF,
    DOM_SHOW_ELEMENT = 0x00000001,
    DOM_SHOW_ATTRIBUTE = 0x00000002,
    DOM_SHOW_TEXT = 0x00000004,
    DOM_SHOW_CDATA_SECTION = 0x00000008,
    DOM_SHOW_ENTITY_REFERENCE = 0x00000010,
    DOM_SHOW_ENTITY = 0x00000020,
    DOM_SHOW_PROCESSING_INSTRUCTION = 0x00000040,
    DOM_SHOW_COMMENT = 0x00000080,
    DOM_SHOW_DOCUMENT = 0x00000100,
    DOM_SHOW_DOCUMENT_TYPE = 0x00000200,
    DOM_SHOW_DOCUMENT_FRAGMENT = 0x00000400,
    DOM_SHOW_NOTATION = 0x00000800
};

/* A node filter (not supported by Minkar's iterators: they show what
   whatToShow says). */
@protocol DOMNodeFilter <NSObject>
- (short)acceptNode:(DOMNode *)n;
@end

@interface DOMObject : WebScriptObject
@end

@interface DOMNode : DOMObject
@property (readonly, copy) NSString *nodeName;
@property (nullable, copy) NSString *nodeValue;
@property (readonly) unsigned short nodeType;
@property (nullable, readonly, strong) DOMNode *parentNode;
@property (readonly, strong) DOMNodeList *childNodes;
@property (nullable, readonly, strong) DOMNode *firstChild;
@property (nullable, readonly, strong) DOMNode *lastChild;
@property (nullable, readonly, strong) DOMNode *previousSibling;
@property (nullable, readonly, strong) DOMNode *nextSibling;
@property (nullable, readonly, strong) DOMDocument *ownerDocument;
@property (nullable, copy) NSString *textContent;
- (BOOL)hasChildNodes;
- (nullable DOMNode *)appendChild:(DOMNode *)newChild;
- (nullable DOMNode *)removeChild:(DOMNode *)oldChild;
- (nullable DOMNode *)cloneNode:(BOOL)deep;
@end

@interface DOMNodeList : DOMObject
@property (readonly) unsigned length;
- (nullable DOMNode *)item:(unsigned)index;
@end

@interface DOMCharacterData : DOMNode
@property (copy) NSString *data;
@property (readonly) unsigned length;
@end

@interface DOMText : DOMCharacterData
@end

@interface DOMElement : DOMNode
@property (readonly, copy) NSString *tagName;
- (nullable NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name value:(NSString *)value;
- (void)removeAttribute:(NSString *)name;
- (BOOL)hasAttribute:(NSString *)name;
- (DOMNodeList *)getElementsByTagName:(NSString *)name;
@end

@interface DOMDocumentFragment : DOMNode
@end

@interface DOMDocument : DOMNode
@property (nullable, readonly, strong) DOMElement *documentElement;
@property (nullable, readonly, strong) DOMElement *body;
@property (copy) NSString *title;
- (nullable DOMElement *)getElementById:(NSString *)elementId;
- (DOMNodeList *)getElementsByTagName:(NSString *)tagname;
- (nullable DOMElement *)createElement:(NSString *)tagName;
- (DOMText *)createTextNode:(NSString *)data;
- (DOMDocumentFragment *)createDocumentFragment;
- (DOMRange *)createRange;
- (nullable DOMNodeIterator *)createNodeIterator:(DOMNode *)root whatToShow:(unsigned)whatToShow
                                           filter:(nullable id<DOMNodeFilter>)filter
                          expandEntityReferences:(BOOL)expandEntityReferences;
@end

@interface DOMRange : DOMObject
@property (readonly, strong) DOMNode *startContainer;
@property (readonly) int startOffset;
@property (readonly, strong) DOMNode *endContainer;
@property (readonly) int endOffset;
@property (readonly) BOOL collapsed;
@property (readonly, strong) DOMNode *commonAncestorContainer;
@property (readonly, copy) NSString *text;
- (DOMDocumentFragment *)cloneContents;
- (void)selectNode:(DOMNode *)refNode;
- (void)selectNodeContents:(DOMNode *)refNode;
- (NSString *)toString;
@end

@interface DOMNodeIterator : DOMObject
@property (readonly, strong) DOMNode *root;
@property (readonly) unsigned whatToShow;
- (nullable DOMNode *)nextNode;
- (nullable DOMNode *)previousNode;
- (void)detach;
@end

NS_ASSUME_NONNULL_END
