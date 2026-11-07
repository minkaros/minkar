/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSPropertyDescription.h>

@class NSEntityDescription;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, NSDeleteRule) {
    NSNoActionDeleteRule,
    NSNullifyDeleteRule,
    NSCascadeDeleteRule,
    NSDenyDeleteRule
};

@interface NSRelationshipDescription : NSPropertyDescription {
    NSDeleteRule _deleteRule;
    NSEntityDescription *_destinationEntity;
    NSRelationshipDescription *_inverseRelationship;
    NSUInteger _maxCount;
    NSUInteger _minCount;
    NSString *_destinationEntityName;
    NSString *_inverseRelationshipName;
    BOOL _ordered;
}

@property (nullable, nonatomic, assign) NSEntityDescription *destinationEntity;
@property (nullable, nonatomic, assign) NSRelationshipDescription *inverseRelationship;
@property NSUInteger maxCount;
@property NSUInteger minCount;
@property NSDeleteRule deleteRule;
@property (getter=isToMany, readonly) BOOL toMany;
@property (getter=isOrdered) BOOL ordered;

@end

NS_ASSUME_NONNULL_END
