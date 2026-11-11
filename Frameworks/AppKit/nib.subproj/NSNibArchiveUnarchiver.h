/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The binary nib format ("NIBArchive", what Interface Builder compiles
 * nibs to since API 10.8, and Minkar's xib compiler too) read as a keyed
 * unarchiver, so the nib classes decode from it as from a keyed archive.
 */

#import <Foundation/NSKeyedUnarchiver.h>

@interface NSNibArchiveUnarchiver : NSKeyedUnarchiver {
    NSData *_archive;
    const uint8_t *_bytes;
    NSUInteger _length;

    /* The archive's tables. */
    NSUInteger _objectCount, _keyCount, _valueCount, _classCount;
    uint32_t *_objectClass, *_objectFirstValue, *_objectValueCount;
    NSString **_keys;
    uint32_t *_valueKey;
    uint8_t *_valueType;
    NSUInteger *_valueOffset;
    NSString **_classNames;

    NSMutableArray *_objectStack;     /* indexes of the objects being decoded */
    NSMapTable *_indexToObject;
    NSMapTable *_objectToIndex;
    NSMutableDictionary *_replacementClasses;
}

/* YES when data is a binary nib. */
+ (BOOL)isNibArchive:(NSData *)data;

/* nil when data is not a well-formed binary nib. */
- (id)initForReadingWithData:(NSData *)data;

@end
