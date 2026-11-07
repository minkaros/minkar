/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSHashTable.h>

@class NSURL, NSData, NSError, NSXMLParser, NSDictionary<KeyType, ObjectType>, NSMutableArray<ObjectType>, NSMutableDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

@protocol NSXMLParserDelegate <NSObject>
@optional

- (void)parserDidStartDocument:(NSXMLParser *)parser;
- (void)parserDidEndDocument:(NSXMLParser *)parser;

- (void)parser:(NSXMLParser *)parser foundNotationDeclarationWithName:(NSString *)name publicID:(nullable NSString *)publicID systemID:(nullable NSString *)systemID;
- (void)parser:(NSXMLParser *)parser foundUnparsedEntityDeclarationWithName:(NSString *)name publicID:(nullable NSString *)publicID systemID:(nullable NSString *)systemID notationName:(nullable NSString *)notationName;
- (void)parser:(NSXMLParser *)parser foundAttributeDeclarationWithName:(NSString *)attributeName forElement:(NSString *)elementName type:(nullable NSString *)type defaultValue:(nullable NSString *)defaultValue;
- (void)parser:(NSXMLParser *)parser foundElementDeclarationWithName:(NSString *)elementName model:(NSString *)model;
- (void)parser:(NSXMLParser *)parser foundInternalEntityDeclarationWithName:(NSString *)name value:(nullable NSString *)value;
- (void)parser:(NSXMLParser *)parser foundExternalEntityDeclarationWithName:(NSString *)name publicID:(nullable NSString *)publicID systemID:(nullable NSString *)systemID;

- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(nullable NSString *)namespaceURI qualifiedName:(nullable NSString *)qName attributes:(NSDictionary<NSString *, NSString *> *)attributeDict;
- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(nullable NSString *)namespaceURI qualifiedName:(nullable NSString *)qName;

- (void)parser:(NSXMLParser *)parser didStartMappingPrefix:(NSString *)prefix toURI:(NSString *)namespaceURI;
- (void)parser:(NSXMLParser *)parser didEndMappingPrefix:(NSString *)prefix;

- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string;
- (void)parser:(NSXMLParser *)parser foundIgnorableWhitespace:(NSString *)whitespaceString;
- (void)parser:(NSXMLParser *)parser foundProcessingInstructionWithTarget:(NSString *)target data:(nullable NSString *)data;
- (void)parser:(NSXMLParser *)parser foundComment:(NSString *)comment;
- (void)parser:(NSXMLParser *)parser foundCDATA:(NSData *)CDATABlock;
- (nullable NSData *)parser:(NSXMLParser *)parser resolveExternalEntityName:(NSString *)name systemID:(nullable NSString *)systemID;

- (void)parser:(NSXMLParser *)parser parseErrorOccurred:(NSError *)parseError;
- (void)parser:(NSXMLParser *)parser validationErrorOccurred:(NSError *)validationError;

@end

@interface NSXMLParser : NSObject {
    NSData *_data;
    id _delegate;
    BOOL _shouldProcessNamespaces;
    BOOL _shouldReportNamespacePrefixes;
    BOOL _shouldResolveExternalEntities;
    NSError *_parserError;
    NSString *_systemID;
    NSString *_publicID;
    NSInteger _columnNumber;
    NSInteger _lineNumber;

    // parsing state
    const uint8_t *_bytes;
    NSUInteger _length;
    NSRange _range;

    NSMutableDictionary *_entityRefContents;

    int _state;
    unichar _charRef;
    NSMutableArray *_elementNameStack;
    NSString *_currentAttributeName;
    NSMutableDictionary *_currentAttributes;
}

- (nullable instancetype)initWithContentsOfURL:(NSURL *)url;
- (instancetype)initWithData:(NSData *)data;
// The former misspelled name, kept for the code that used it.
- (nullable instancetype)initWithContentsofURL:(NSURL *)url;

@property (nullable, assign) id<NSXMLParserDelegate> delegate;
@property BOOL shouldProcessNamespaces;
@property BOOL shouldReportNamespacePrefixes;
@property BOOL shouldResolveExternalEntities;

- (BOOL)parse;
- (void)abortParsing;

@property (nullable, readonly, copy) NSError *parserError;

@property (nullable, readonly, copy) NSString *publicID;
@property (nullable, readonly, copy) NSString *systemID;
@property (readonly) NSInteger lineNumber;
@property (readonly) NSInteger columnNumber;

@end

NS_ASSUME_NONNULL_END
