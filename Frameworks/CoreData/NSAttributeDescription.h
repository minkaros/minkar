/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSPropertyDescription.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, NSAttributeType) {
    NSUndefinedAttributeType = 0,
    NSInteger16AttributeType = 100,
    NSInteger32AttributeType = 200,
    NSInteger64AttributeType = 300,
    NSDecimalAttributeType = 400,
    NSDoubleAttributeType = 500,
    NSFloatAttributeType = 600,
    NSStringAttributeType = 700,
    NSBooleanAttributeType = 800,
    NSDateAttributeType = 900,
    NSBinaryDataAttributeType = 1000,
    NSUUIDAttributeType API_AVAILABLE(macos(10.13)) = 1100,
    NSURIAttributeType API_AVAILABLE(macos(10.13)) = 1200,
    NSTransformableAttributeType = 1800,
    NSObjectIDAttributeType = 2000,
};

@interface NSAttributeDescription : NSPropertyDescription {
    NSAttributeType _attributeType;
    NSString *_valueClassName;
    id _defaultValue;
    NSString *_valueTransformerName;
    BOOL _allowsExternalBinaryDataStorage;
    BOOL _preservesValueInHistoryOnDeletion;
}

@property NSAttributeType attributeType;
@property (nullable, copy) NSString *attributeValueClassName;
@property (nullable, retain) id defaultValue;
@property (nullable, copy) NSString *valueTransformerName;
@property BOOL allowsExternalBinaryDataStorage;
@property BOOL preservesValueInHistoryOnDeletion API_AVAILABLE(macos(10.15));

@end

NS_ASSUME_NONNULL_END
