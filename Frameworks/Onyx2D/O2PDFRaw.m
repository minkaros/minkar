/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Onyx2D/O2PDFRaw.h>
#import <Onyx2D/O2PDFDocument.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFString.h>
#import <Onyx2D/O2PDFObject_R.h>
#import <Onyx2D/O2PDFObject_Name.h>
#import <Onyx2D/O2PDFObject_Integer.h>
#import <Onyx2D/O2PDFObject_Real.h>
#import <Onyx2D/O2PDFObject_Boolean.h>
#import <Onyx2D/O2PDFxref.h>
#import <Foundation/NSMapTable.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSNull.h>
#import <string.h>
#import <stdio.h>

@interface O2PDFObject_R (O2PDFRaw)
- (int)_number;
- (int)_generation;
@end
@implementation O2PDFObject_R (O2PDFRaw)
- (int)_number { return _number; }
- (int)_generation { return _generation; }
@end

@interface O2PDFDictionary (O2PDFRaw)
- (O2PDFObject *)_rawObjectForKey:(const char *)key;
- (NSMapTable *)_table;
@end
@implementation O2PDFDictionary (O2PDFRaw)
- (O2PDFObject *)_rawObjectForKey:(const char *)key { return NSMapGet(_table, key); }
- (NSMapTable *)_table { return _table; }
@end

static void appendCString(NSMutableData *data, const char *s) {
    [data appendBytes:s length:strlen(s)];
}

static void appendName(NSMutableData *data, const char *name) {
    appendCString(data, "/");
    for (const unsigned char *c = (const unsigned char *)name; *c; c++) {
        char escaped[4];

        if (*c <= ' ' || *c >= 0x7F || *c == '#' || strchr("()<>[]{}/%", *c) != NULL) {
            snprintf(escaped, sizeof(escaped), "#%02X", *c);
            appendCString(data, escaped);
        } else
            [data appendBytes:c length:1];
    }
}

void O2PDFAppendRawObject(NSMutableData *data, O2PDFObject *object) {
    char buffer[64];

    switch ([object objectTypeNoParsing]) {
    case O2PDFObjectType_R:
        snprintf(buffer, sizeof(buffer), "%d %d R", [(O2PDFObject_R *)object _number], [(O2PDFObject_R *)object _generation]);
        appendCString(data, buffer);
        return;
    case kO2PDFObjectTypeNull:
        appendCString(data, "null");
        return;
    case kO2PDFObjectTypeBoolean: {
        O2PDFBoolean value;
        [object checkForType:kO2PDFObjectTypeBoolean value:&value];
        appendCString(data, value ? "true" : "false");
        return;
    }
    case kO2PDFObjectTypeInteger: {
        O2PDFInteger value;
        [object checkForType:kO2PDFObjectTypeInteger value:&value];
        snprintf(buffer, sizeof(buffer), "%d", (int)value);
        appendCString(data, buffer);
        return;
    }
    case kO2PDFObjectTypeReal: {
        O2PDFReal value;
        [object checkForType:kO2PDFObjectTypeReal value:&value];
        snprintf(buffer, sizeof(buffer), "%.6g", (double)value);
        appendCString(data, buffer);
        return;
    }
    case kO2PDFObjectTypeName:
        appendName(data, [(O2PDFObject_Name *)object name]);
        return;
    case kO2PDFObjectTypeString: {
        O2PDFString *string = (O2PDFString *)object;
        const unsigned char *bytes = [string bytes];

        appendCString(data, "<");
        for (unsigned i = 0; i < [string length]; i++) {
            snprintf(buffer, sizeof(buffer), "%02X", bytes[i]);
            appendCString(data, buffer);
        }
        appendCString(data, ">");
        return;
    }
    case kO2PDFObjectTypeArray: {
        O2PDFArray *array = (O2PDFArray *)object;

        appendCString(data, "[");
        for (unsigned i = 0; i < [array count]; i++) {
            if (i > 0)
                appendCString(data, " ");
            O2PDFAppendRawObject(data, [array objectAtIndex:i]);
        }
        appendCString(data, "]");
        return;
    }
    case kO2PDFObjectTypeDictionary: {
        NSData *entries = O2PDFDictionaryCopyRawEncoding((O2PDFDictionary *)object, nil);

        [data appendData:entries];
        [entries release];
        return;
    }
    default:
        /* A stream is always written indirectly: not here. */
        appendCString(data, "null");
        return;
    }
}

NSData *O2PDFDictionaryCopyRawEncoding(O2PDFDictionary *dictionary, NSArray *excludedKeys) {
    NSMutableData *data = [[NSMutableData alloc] init];
    NSMapEnumerator state = NSEnumerateMapTable([dictionary _table]);
    const char *key;
    O2PDFObject *value;

    appendCString(data, "<<");
    while (NSNextMapEnumeratorPair(&state, (void **)&key, (void **)&value)) {
        if ([excludedKeys containsObject:[NSString stringWithUTF8String:key]])
            continue;
        appendCString(data, " ");
        appendName(data, key);
        appendCString(data, " ");
        O2PDFAppendRawObject(data, value);
    }
    NSEndMapTableEnumeration(&state);
    appendCString(data, " >>");
    return data;
}

/* The page tree's leaves in order, from node: index counts down to the
   page asked for. */
static BOOL findPage(O2PDFDictionary *node, unsigned *index, int *number, int *generation, int depth) {
    O2PDFObject *kidsObject = [[node _rawObjectForKey:"Kids"] realObject];
    O2PDFArray *kids;

    if (depth > 64 || ![kidsObject checkForType:kO2PDFObjectTypeArray value:&kids])
        return NO;
    for (unsigned i = 0; i < [kids count]; i++) {
        O2PDFObject *raw = [kids objectAtIndex:i];
        O2PDFDictionary *kid;
        const char *type;

        if (![[raw realObject] checkForType:kO2PDFObjectTypeDictionary value:&kid])
            continue;
        if ([kid getNameForKey:"Type" value:&type] && strcmp(type, "Pages") == 0) {
            if (findPage(kid, index, number, generation, depth + 1))
                return YES;
            continue;
        }
        if (*index == 0) {
            if ([raw objectTypeNoParsing] != O2PDFObjectType_R)
                return NO;
            *number = [(O2PDFObject_R *)raw _number];
            *generation = [(O2PDFObject_R *)raw _generation];
            return YES;
        }
        (*index)--;
    }
    return NO;
}

BOOL O2PDFDocumentGetPageReference(O2PDFDocument *document, unsigned pageIndex, int *number, int *generation) {
    O2PDFDictionary *pages;

    if (![[document catalog] getDictionaryForKey:"Pages" value:&pages])
        return NO;
    return findPage(pages, &pageIndex, number, generation, 0);
}

NSArray *O2PDFDictionaryCopyAnnotationReferences(O2PDFDictionary *page) {
    O2PDFObject *annotsObject = [[page _rawObjectForKey:"Annots"] realObject];
    O2PDFArray *annots;
    NSMutableArray *result = [[NSMutableArray alloc] init];

    if ([annotsObject checkForType:kO2PDFObjectTypeArray value:&annots])
        for (unsigned i = 0; i < [annots count]; i++) {
            O2PDFObject *raw = [annots objectAtIndex:i];

            if ([raw objectTypeNoParsing] == O2PDFObjectType_R)
                [result addObject:[NSArray arrayWithObjects:[NSNumber numberWithInt:[(O2PDFObject_R *)raw _number]],
                                                            [NSNumber numberWithInt:[(O2PDFObject_R *)raw _generation]], nil]];
            else
                [result addObject:[NSNull null]];
        }
    return result;
}

O2PDFDictionary *O2PDFDocumentGetTrailer(O2PDFDocument *document) {
    return [[document xref] trailer];
}
