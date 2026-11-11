/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NIBArchive, the binary nib format, read as a keyed unarchiver.
 *
 * The file is the magic "NIBArchive", two 32-bit little-endian numbers
 * (the format's version and the number of 32-bit words after it in the
 * header, 9), then four tables, each given by its count and offset: the
 * objects, the keys, the values and the class names. Numbers inside the
 * tables are variable-length, 7 bits a byte, least significant first, the
 * last byte with its high bit set.
 *
 * An object is its class name's index, its first value's index and its
 * number of values. A key is its length and its bytes. A value is its key's
 * index, a type and its data: a signed 8, 16, 32 or 64-bit integer, false,
 * true, a float, a double, bytes (a length and them), nil, or another
 * object's index (32 bits). A class name is its length, a flag (0x81 when
 * a 32-bit number follows), and its bytes (with a NUL).
 *
 * Object 0 holds the nib's top keys (IB.objectdata). Strings are objects
 * with their UTF-8 text in NS.bytes, numbers NS.intval, NS.dblval or
 * NS.boolval; arrays, sets and dictionaries have NSInlinedValue and their
 * elements (a dictionary's keys and objects in turn) under
 * UINibEncoderEmptyKey. Geometry is bytes: 0x07 then doubles. Every other
 * object is made by its class's initWithCoder:, which reads its keys from
 * here, as from a keyed archive.
 */

#import "NSNibArchiveUnarchiver.h"
#import <Foundation/Foundation.h>
#import <AppKit/NSView.h>

enum {
    ValueInt8 = 0,
    ValueInt16 = 1,
    ValueInt32 = 2,
    ValueInt64 = 3,
    ValueFalse = 4,
    ValueTrue = 5,
    ValueFloat = 6,
    ValueDouble = 7,
    ValueBytes = 8,
    ValueNil = 9,
    ValueObject = 10
};

static NSString *const InlinedKey = @"NSInlinedValue";
static NSString *const ElementKey = @"UINibEncoderEmptyKey";

@interface NSObject (NSKeyedUnarchiverPrivate)
+ (id)allocWithKeyedUnarchiver:(NSKeyedUnarchiver *)keyed;
@end

@implementation NSNibArchiveUnarchiver

+ (BOOL)isNibArchive:(NSData *)data {
    return [data length] >= 10 && memcmp([data bytes], "NIBArchive", 10) == 0;
}

static uint32_t readWord(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/* A variable-length number at *offset; NO past the end. */
static BOOL readFlex(const uint8_t *bytes, NSUInteger length, NSUInteger *offset, NSUInteger *result) {
    NSUInteger value = 0;
    unsigned shift = 0;

    for (;;) {
        if (*offset >= length || shift > 56)
            return NO;
        uint8_t byte = bytes[(*offset)++];
        value |= (NSUInteger)(byte & 0x7f) << shift;
        shift += 7;
        if (byte & 0x80)
            break;
    }
    *result = value;
    return YES;
}

/* The size of a value's data, from its type; NO for an unknown type or
   one past the end. */
static BOOL skipValue(const uint8_t *bytes, NSUInteger length, NSUInteger *offset, uint8_t type) {
    NSUInteger size;

    switch (type) {
    case ValueInt8: size = 1; break;
    case ValueInt16: size = 2; break;
    case ValueInt32: size = 4; break;
    case ValueInt64: size = 8; break;
    case ValueFalse: case ValueTrue: case ValueNil: size = 0; break;
    case ValueFloat: size = 4; break;
    case ValueDouble: size = 8; break;
    case ValueObject: size = 4; break;
    case ValueBytes:
        if (!readFlex(bytes, length, offset, &size))
            return NO;
        break;
    default:
        return NO;
    }
    if (*offset + size > length)
        return NO;
    *offset += size;
    return YES;
}

- (BOOL)_parse {
    if (_length < 14 + 4 + 32 || ![NSNibArchiveUnarchiver isNibArchive:_archive])
        return NO;
    uint32_t headerWords = readWord(_bytes + 14);
    if (headerWords < 9)
        return NO;
    const uint8_t *sections = _bytes + 18;
    uint32_t objectCount = readWord(sections), objectOffset = readWord(sections + 4);
    uint32_t keyCount = readWord(sections + 8), keyOffset = readWord(sections + 12);
    uint32_t valueCount = readWord(sections + 16), valueOffset = readWord(sections + 20);
    uint32_t classCount = readWord(sections + 24), classOffset = readWord(sections + 28);
    if (objectCount == 0 || objectCount > _length || keyCount > _length || valueCount > _length || classCount > _length)
        return NO;

    _objectCount = objectCount;
    _keyCount = keyCount;
    _valueCount = valueCount;
    _classCount = classCount;
    _objectClass = calloc(objectCount, sizeof(uint32_t));
    _objectFirstValue = calloc(objectCount, sizeof(uint32_t));
    _objectValueCount = calloc(objectCount, sizeof(uint32_t));
    _keys = calloc(keyCount ? keyCount : 1, sizeof(NSString *));
    _valueKey = calloc(valueCount ? valueCount : 1, sizeof(uint32_t));
    _valueType = calloc(valueCount ? valueCount : 1, sizeof(uint8_t));
    _valueOffset = calloc(valueCount ? valueCount : 1, sizeof(NSUInteger));
    _classNames = calloc(classCount ? classCount : 1, sizeof(NSString *));

    NSUInteger offset = classOffset, n;
    for (NSUInteger i = 0; i < classCount; i++) {
        NSUInteger nameLength;
        if (!readFlex(_bytes, _length, &offset, &nameLength) || offset >= _length)
            return NO;
        uint8_t flag = _bytes[offset++];
        if (flag == 0x81)
            offset += 4;
        if (offset + nameLength > _length)
            return NO;
        NSUInteger textLength = nameLength;
        while (textLength > 0 && _bytes[offset + textLength - 1] == 0)
            textLength--;
        _classNames[i] = [[NSString alloc] initWithBytes:_bytes + offset length:textLength encoding:NSUTF8StringEncoding];
        offset += nameLength;
    }

    offset = keyOffset;
    for (NSUInteger i = 0; i < keyCount; i++) {
        if (!readFlex(_bytes, _length, &offset, &n) || offset + n > _length)
            return NO;
        _keys[i] = [[NSString alloc] initWithBytes:_bytes + offset length:n encoding:NSUTF8StringEncoding];
        offset += n;
    }

    offset = valueOffset;
    for (NSUInteger i = 0; i < valueCount; i++) {
        if (!readFlex(_bytes, _length, &offset, &n) || n >= keyCount || offset >= _length)
            return NO;
        _valueKey[i] = (uint32_t)n;
        _valueType[i] = _bytes[offset++];
        _valueOffset[i] = offset;
        if (!skipValue(_bytes, _length, &offset, _valueType[i]))
            return NO;
    }

    offset = objectOffset;
    for (NSUInteger i = 0; i < objectCount; i++) {
        NSUInteger classIndex, first, count;
        if (!readFlex(_bytes, _length, &offset, &classIndex) || !readFlex(_bytes, _length, &offset, &first) ||
            !readFlex(_bytes, _length, &offset, &count) || classIndex >= classCount || first + count > valueCount)
            return NO;
        _objectClass[i] = (uint32_t)classIndex;
        _objectFirstValue[i] = (uint32_t)first;
        _objectValueCount[i] = (uint32_t)count;
    }
    return YES;
}

- (id)initForReadingWithData:(NSData *)data {
    _archive = [data retain];
    _bytes = [data bytes];
    _length = [data length];
    _objectStack = [[NSMutableArray alloc] initWithObjects:[NSNumber numberWithUnsignedInteger:0], nil];
    _indexToObject = NSCreateMapTable(NSIntMapKeyCallBacks, NSNonOwnedPointerMapValueCallBacks, 0);
    _objectToIndex = NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks, NSIntMapValueCallBacks, 0);
    _replacementClasses = [NSMutableDictionary new];
    if (![self _parse]) {
        [self release];
        return nil;
    }
    return self;
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _keyCount && _keys; i++)
        [_keys[i] release];
    for (NSUInteger i = 0; i < _classCount && _classNames; i++)
        [_classNames[i] release];
    free(_keys);
    free(_classNames);
    free(_objectClass);
    free(_objectFirstValue);
    free(_objectValueCount);
    free(_valueKey);
    free(_valueType);
    free(_valueOffset);
    [_objectStack release];
    if (_indexToObject)
        NSFreeMapTable(_indexToObject);
    if (_objectToIndex)
        NSFreeMapTable(_objectToIndex);
    [_replacementClasses release];
    [_archive release];
    [super dealloc];
}

- (BOOL)allowsKeyedCoding {
    return YES;
}

#pragma mark Values

/* The value of key in the object being decoded (its last one, if there are
   several), or NSNotFound. */
- (NSUInteger)_valueForKey:(NSString *)key {
    NSUInteger object = [[_objectStack lastObject] unsignedIntegerValue];
    NSUInteger first = _objectFirstValue[object], count = _objectValueCount[object];
    for (NSUInteger i = first + count; i > first; i--)
        if ([_keys[_valueKey[i - 1]] isEqualToString:key])
            return i - 1;
    return NSNotFound;
}

- (int64_t)_integerAt:(NSUInteger)value {
    const uint8_t *p = _bytes + _valueOffset[value];
    switch (_valueType[value]) {
    case ValueInt8: return (int8_t)p[0];
    case ValueInt16: return (int16_t)(p[0] | p[1] << 8);
    case ValueInt32: return (int32_t)readWord(p);
    case ValueInt64: return (int64_t)((uint64_t)readWord(p) | (uint64_t)readWord(p + 4) << 32);
    case ValueTrue: return 1;
    case ValueFalse: return 0;
    case ValueFloat: { float f; memcpy(&f, p, sizeof(f)); return (int64_t)f; }
    case ValueDouble: { double d; memcpy(&d, p, sizeof(d)); return (int64_t)d; }
    default: return 0;
    }
}

- (double)_doubleAt:(NSUInteger)value {
    const uint8_t *p = _bytes + _valueOffset[value];
    switch (_valueType[value]) {
    case ValueFloat: { float f; memcpy(&f, p, sizeof(f)); return f; }
    case ValueDouble: { double d; memcpy(&d, p, sizeof(d)); return d; }
    default: return (double)[self _integerAt:value];
    }
}

/* A bytes value's data (its length through *length). */
- (const uint8_t *)_bytesAt:(NSUInteger)value length:(NSUInteger *)length {
    NSUInteger offset = _valueOffset[value], n = 0;
    readFlex(_bytes, _length, &offset, &n);
    *length = n;
    return _bytes + offset;
}

- (BOOL)containsValueForKey:(NSString *)key {
    return [self _valueForKey:key] != NSNotFound;
}

- (int64_t)decodeInt64ForKey:(NSString *)key {
    NSUInteger value = [self _valueForKey:key];
    return value == NSNotFound ? 0 : [self _integerAt:value];
}

- (BOOL)decodeBoolForKey:(NSString *)key { return [self decodeInt64ForKey:key] != 0; }
- (int)decodeIntForKey:(NSString *)key { return (int)[self decodeInt64ForKey:key]; }
- (int32_t)decodeInt32ForKey:(NSString *)key { return (int32_t)[self decodeInt64ForKey:key]; }
- (NSInteger)decodeIntegerForKey:(NSString *)key { return (NSInteger)[self decodeInt64ForKey:key]; }
- (short)decodeShortForKey:(NSString *)key { return (short)[self decodeInt64ForKey:key]; }
- (char)decodeCharForKey:(NSString *)key { return (char)[self decodeInt64ForKey:key]; }
- (unsigned char)decodeUnsignedCharForKey:(NSString *)key { return (unsigned char)[self decodeInt64ForKey:key]; }

- (double)decodeDoubleForKey:(NSString *)key {
    NSUInteger value = [self _valueForKey:key];
    return value == NSNotFound ? 0 : [self _doubleAt:value];
}

- (float)decodeFloatForKey:(NSString *)key {
    return (float)[self decodeDoubleForKey:key];
}

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)lengthp {
    NSUInteger value = [self _valueForKey:key], length = 0;
    const uint8_t *bytes = NULL;

    if (value != NSNotFound && _valueType[value] == ValueBytes)
        bytes = [self _bytesAt:value length:&length];
    else if (value != NSNotFound && _valueType[value] == ValueObject) {
        /* Bytes kept as an NSData (or a string) object. */
        id object = [self decodeObjectForKey:key];
        if ([object isKindOfClass:[NSData class]]) {
            bytes = [object bytes];
            length = [object length];
        } else if ([object isKindOfClass:[NSString class]]) {
            bytes = (const uint8_t *)[object UTF8String];
            length = strlen((const char *)bytes);
        }
    }
    if (lengthp)
        *lengthp = length;
    return bytes;
}

/* Up to count doubles of key: bytes of 0x07 and doubles, or a string as
   "{{x, y}, {w, h}}" (the numbers in order). The number read. */
- (NSUInteger)_decodeDoubles:(double *)doubles count:(NSUInteger)count forKey:(NSString *)key {
    NSUInteger value = [self _valueForKey:key], found = 0;
    if (value == NSNotFound)
        return 0;
    if (_valueType[value] == ValueBytes) {
        NSUInteger length;
        const uint8_t *bytes = [self _bytesAt:value length:&length];
        if (length > 0 && bytes[0] == 0x07) {
            for (found = 0; found < count && 1 + (found + 1) * 8 <= length; found++)
                memcpy(&doubles[found], bytes + 1 + found * 8, 8);
            return found;
        }
    }
    id object = (_valueType[value] == ValueObject) ? [self decodeObjectForKey:key] : nil;
    if ([object isKindOfClass:[NSString class]]) {
        NSScanner *scanner = [NSScanner scannerWithString:object];
        [scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@"{}, \t\n"]];
        while (found < count && [scanner scanDouble:&doubles[found]])
            found++;
    }
    return found;
}

- (NSPoint)decodePointForKey:(NSString *)key {
    double v[2] = { 0, 0 };
    [self _decodeDoubles:v count:2 forKey:key];
    return NSMakePoint(v[0], v[1]);
}

- (NSSize)decodeSizeForKey:(NSString *)key {
    double v[2] = { 0, 0 };
    [self _decodeDoubles:v count:2 forKey:key];
    return NSMakeSize(v[0], v[1]);
}

- (NSRect)decodeRectForKey:(NSString *)key {
    double v[4] = { 0, 0, 0, 0 };
    [self _decodeDoubles:v count:4 forKey:key];
    return NSMakeRect(v[0], v[1], v[2], v[3]);
}

#pragma mark Objects

/* A value as an object: an object's, or a scalar as an NSNumber (an
   element of an array, for one). */
- (id)_objectForValue:(NSUInteger)value {
    switch (_valueType[value]) {
    case ValueObject: return [self _objectAtIndex:readWord(_bytes + _valueOffset[value])];
    case ValueNil: return nil;
    case ValueTrue: return [NSNumber numberWithBool:YES];
    case ValueFalse: return [NSNumber numberWithBool:NO];
    case ValueFloat: case ValueDouble: return [NSNumber numberWithDouble:[self _doubleAt:value]];
    case ValueBytes: {
        NSUInteger length;
        const uint8_t *bytes = [self _bytesAt:value length:&length];
        return [NSData dataWithBytes:bytes length:length];
    }
    default: return [NSNumber numberWithLongLong:[self _integerAt:value]];
    }
}

/* The values of object under key, in order; a missing one (of a class the
   program lacks) is MissingElement. */
static id MissingElement(void) {
    static id missing;
    if (missing == nil)
        missing = [NSObject new];
    return missing;
}

- (NSArray *)_elementsOfObject:(NSUInteger)object {
    NSMutableArray *elements = [NSMutableArray array];
    NSUInteger first = _objectFirstValue[object], count = _objectValueCount[object];
    for (NSUInteger i = first; i < first + count; i++) {
        if (![_keys[_valueKey[i]] isEqualToString:ElementKey])
            continue;
        id element = [self _objectForValue:i];
        [elements addObject:element ? element : MissingElement()];
    }
    return elements;
}

/* A Foundation value object of the archive (a string, number, data or
   collection), made here; nil when the class is another. */
- (id)_foundationObjectAtIndex:(NSUInteger)object className:(NSString *)className {
    NSUInteger first = _objectFirstValue[object], count = _objectValueCount[object];
    NSUInteger bytesValue = NSNotFound;
    for (NSUInteger i = first; i < first + count; i++)
        if ([_keys[_valueKey[i]] isEqualToString:@"NS.bytes"] && _valueType[i] == ValueBytes)
            bytesValue = i;

    if ([className isEqualToString:@"NSString"] || [className isEqualToString:@"NSMutableString"] ||
        [className isEqualToString:@"NSLocalizableString"]) {
        NSUInteger length = 0;
        const uint8_t *bytes = bytesValue != NSNotFound ? [self _bytesAt:bytesValue length:&length] : NULL;
        NSString *string = [[[NSString alloc] initWithBytes:bytes length:length encoding:NSUTF8StringEncoding] autorelease];
        if (string == nil)
            string = @"";
        return [className isEqualToString:@"NSMutableString"] ? [[string mutableCopy] autorelease] : string;
    }
    if ([className isEqualToString:@"NSData"] || [className isEqualToString:@"NSMutableData"]) {
        NSUInteger length = 0;
        const uint8_t *bytes = bytesValue != NSNotFound ? [self _bytesAt:bytesValue length:&length] : NULL;
        return [className isEqualToString:@"NSMutableData"] ? [NSMutableData dataWithBytes:bytes length:length]
                                                            : [NSData dataWithBytes:bytes length:length];
    }
    if ([className isEqualToString:@"NSNumber"]) {
        for (NSUInteger i = first; i < first + count; i++) {
            NSString *key = _keys[_valueKey[i]];
            if ([key isEqualToString:@"NS.boolval"])
                return [NSNumber numberWithBool:[self _integerAt:i] != 0];
            if ([key isEqualToString:@"NS.dblval"])
                return [NSNumber numberWithDouble:[self _doubleAt:i]];
            if ([key isEqualToString:@"NS.intval"])
                return [NSNumber numberWithLongLong:[self _integerAt:i]];
        }
        return [NSNumber numberWithInt:0];
    }

    BOOL array = [className isEqualToString:@"NSArray"] || [className isEqualToString:@"NSMutableArray"];
    BOOL set = [className isEqualToString:@"NSSet"] || [className isEqualToString:@"NSMutableSet"];
    BOOL dictionary = [className isEqualToString:@"NSDictionary"] || [className isEqualToString:@"NSMutableDictionary"];
    if (!array && !set && !dictionary)
        return nil;
    /* Missing elements are left out (a dictionary's with their pair). */
    NSMutableArray *elements = [NSMutableArray arrayWithArray:[self _elementsOfObject:object]];
    BOOL mutable = [className hasPrefix:@"NSMutable"];
    if (!dictionary)
        [elements removeObjectIdenticalTo:MissingElement()];
    if (array)
        return mutable ? [NSMutableArray arrayWithArray:elements] : [NSArray arrayWithArray:elements];
    if (set)
        return mutable ? [NSMutableSet setWithArray:elements] : [NSSet setWithArray:elements];
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    for (NSUInteger i = 0; i + 1 < [elements count]; i += 2) {
        id key = [elements objectAtIndex:i], value = [elements objectAtIndex:i + 1];
        if (key != MissingElement() && value != MissingElement())
            [result setObject:value forKey:key];
    }
    return mutable ? result : [NSDictionary dictionaryWithDictionary:result];
}

- (BOOL)_object:(NSUInteger)object hasKey:(NSString *)key {
    NSUInteger first = _objectFirstValue[object], count = _objectValueCount[object];
    for (NSUInteger i = first; i < first + count; i++)
        if ([_keys[_valueKey[i]] isEqualToString:key])
            return YES;
    return NO;
}

- (Class)_classNamed:(NSString *)className {
    Class class = [_replacementClasses objectForKey:className];
    if (class == Nil)
        class = [NSKeyedUnarchiver classForClassName:className];
    if (class == Nil)
        class = NSClassFromString(className);
    if (class == Nil && [_delegate respondsToSelector:@selector(unarchiver:cannotDecodeObjectOfClassName:originalClasses:)])
        class = [_delegate unarchiver:self cannotDecodeObjectOfClassName:className originalClasses:[NSArray arrayWithObject:className]];
    return class;
}

- (id)_objectAtIndex:(NSUInteger)object {
    if (object >= _objectCount)
        return nil;
    id result = NSMapGet(_indexToObject, (void *)object);
    if (result != nil)
        return result;

    NSString *className = _classNames[_objectClass[object]];
    result = [self _foundationObjectAtIndex:object className:className];
    if (result != nil) {
        NSMapInsert(_indexToObject, (void *)object, result);
        NSMapInsert(_objectToIndex, result, (void *)object);
        /* Kept for the archive's life, as the object graph is. */
        [[result retain] autorelease];
        return result;
    }

    Class class = [self _classNamed:className];
    if (class == Nil) {
        /* A view of a class the program lacks (a framework it does not
           link) is a plain view, so the views around it still load. */
        if ([self _object:object hasKey:@"NSFrame"] || [self _object:object hasKey:@"NSFrameSize"] ||
            [self _object:object hasKey:@"NSSuperview"]) {
            NSLog(@"Unknown class %@ in Interface Builder file, using NSView instead.", className);
            class = [NSView class];
        } else {
            NSLog(@"Unknown class %@ in Interface Builder file.", className);
            return nil;
        }
    }

    /* As NSKeyedUnarchiver does: known before its initWithCoder: (objects
       may refer to each other), replaced by what initWithCoder: and
       awakeAfterUsingCoder: return. */
    [_objectStack addObject:[NSNumber numberWithUnsignedInteger:object]];
    result = [class allocWithKeyedUnarchiver:self];
    NSMapInsert(_indexToObject, (void *)object, result);
    NSMapInsert(_objectToIndex, result, (void *)object);
    /* A class put in for one the program lacks may not be NSCoding. */
    result = [result respondsToSelector:@selector(initWithCoder:)] ? [result initWithCoder:self] : [result init];
    NSMapInsert(_indexToObject, (void *)object, result);
    NSMapInsert(_objectToIndex, result, (void *)object);
    result = [result awakeAfterUsingCoder:self];
    [result autorelease];
    if ([_delegate respondsToSelector:@selector(unarchiver:didDecodeObject:)])
        result = [_delegate unarchiver:self didDecodeObject:result];
    NSMapInsert(_indexToObject, (void *)object, result);
    NSMapInsert(_objectToIndex, result, (void *)object);
    [_objectStack removeLastObject];
    return result;
}

- (id)decodeObjectForKey:(NSString *)key {
    NSUInteger value = [self _valueForKey:key];
    return value == NSNotFound ? nil : [self _objectForValue:value];
}

- (id)decodeObjectOfClass:(Class)cls forKey:(NSString *)key {
    return [self decodeObjectForKey:key];
}

- (id)decodeObjectOfClasses:(NSSet *)classes forKey:(NSString *)key {
    return [self decodeObjectForKey:key];
}

- (id)decodeTopLevelObjectForKey:(NSString *)key error:(NSError **)error {
    if (error)
        *error = nil;
    return [self decodeObjectForKey:key];
}

- (void)replaceObject:object withObject:replacement {
    NSUInteger index = (NSUInteger)NSMapGet(_objectToIndex, object);
    if (NSMapGet(_indexToObject, (void *)index) != object)
        return;
    if ([_delegate respondsToSelector:@selector(unarchiver:willReplaceObject:withObject:)])
        [_delegate unarchiver:self willReplaceObject:object withObject:replacement];
    NSMapInsert(_indexToObject, (void *)index, replacement);
    NSMapInsert(_objectToIndex, replacement, (void *)index);
}

- (void)setClass:(Class)class forClassName:(NSString *)className {
    if (class)
        [_replacementClasses setObject:class forKey:className];
}

- (Class)classForClassName:(NSString *)className {
    return [_replacementClasses objectForKey:className];
}

- (void)finishDecoding {
}

@end
