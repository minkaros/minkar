/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Descriptors keep their contents in a storage block of their own: bytes
 * for plain data, keyed items for lists, records and Apple events (whose
 * attributes are a second set of keyed items). Copies are deep.
 */

#include "AE.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define paramErr (-50)
#define memFullErr (-108)

enum { kindData, kindList, kindRecord, kindEvent };

typedef struct AEItem {
    AEKeyword key;
    AEDesc desc;
} AEItem;

typedef struct AEStorage {
    int kind;
    Size size;
    void *bytes;
    long count, capacity;
    AEItem *items;
    long attributeCount, attributeCapacity;
    AEItem *attributes;
} AEStorage;

#define STORAGE(d) ((AEStorage *)(void *)(d)->dataHandle)

static AEStorage *newStorage(int kind) {
    return (AEStorage *)calloc(1, sizeof(AEStorage));
}

static void freeItems(AEItem *items, long count) {
    for (long i = 0; i < count; i++)
        AEDisposeDesc(&items[i].desc);
    free(items);
}

static OSErr copyItems(const AEItem *from, long count, AEItem **to, long *capacity) {
    *to = NULL;
    *capacity = 0;
    if (count == 0)
        return noErr;
    *to = (AEItem *)calloc((size_t)count, sizeof(AEItem));
    if (*to == NULL)
        return memFullErr;
    *capacity = count;
    for (long i = 0; i < count; i++) {
        (*to)[i].key = from[i].key;
        OSErr err = AEDuplicateDesc(&from[i].desc, &(*to)[i].desc);
        if (err != noErr)
            return err;
    }
    return noErr;
}

void AEInitializeDesc(AEDesc *desc) {
    if (desc != NULL) {
        desc->descriptorType = typeNull;
        desc->dataHandle = NULL;
    }
}

OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result) {
    if (result == NULL)
        return paramErr;
    AEInitializeDesc(result);
    if (typeCode == typeNull && dataSize == 0)
        return noErr;
    if (typeCode == typeAEList || typeCode == typeAERecord || typeCode == typeAppleEvent)
        return errAEWrongDataType;
    AEStorage *s = newStorage(kindData);
    if (s == NULL)
        return memFullErr;
    if (dataSize > 0) {
        s->bytes = malloc((size_t)dataSize);
        if (s->bytes == NULL) {
            free(s);
            return memFullErr;
        }
        if (dataPtr != NULL)
            memcpy(s->bytes, dataPtr, (size_t)dataSize);
        else
            memset(s->bytes, 0, (size_t)dataSize);
        s->size = dataSize;
    }
    result->descriptorType = typeCode;
    result->dataHandle = (AEDataStorage)(void *)s;
    return noErr;
}

OSErr AEDisposeDesc(AEDesc *theAEDesc) {
    if (theAEDesc == NULL)
        return noErr;
    AEStorage *s = STORAGE(theAEDesc);
    if (s != NULL) {
        free(s->bytes);
        freeItems(s->items, s->count);
        freeItems(s->attributes, s->attributeCount);
        free(s);
    }
    AEInitializeDesc(theAEDesc);
    return noErr;
}

OSErr AEDuplicateDesc(const AEDesc *theAEDesc, AEDesc *result) {
    if (theAEDesc == NULL || result == NULL)
        return paramErr;
    AEStorage *from = STORAGE(theAEDesc);
    AEInitializeDesc(result);
    if (from == NULL) {
        result->descriptorType = theAEDesc->descriptorType;
        return noErr;
    }
    AEStorage *s = newStorage(from->kind);
    if (s == NULL)
        return memFullErr;
    s->kind = from->kind;
    if (from->size > 0) {
        s->bytes = malloc((size_t)from->size);
        if (s->bytes == NULL) {
            free(s);
            return memFullErr;
        }
        memcpy(s->bytes, from->bytes, (size_t)from->size);
        s->size = from->size;
    }
    result->descriptorType = theAEDesc->descriptorType;
    result->dataHandle = (AEDataStorage)(void *)s;
    OSErr err = copyItems(from->items, from->count, &s->items, &s->capacity);
    if (err == noErr) {
        s->count = from->count;
        err = copyItems(from->attributes, from->attributeCount, &s->attributes, &s->attributeCapacity);
        if (err == noErr)
            s->attributeCount = from->attributeCount;
    }
    if (err != noErr)
        AEDisposeDesc(result);
    return err;
}

Size AEGetDescDataSize(const AEDesc *theAEDesc) {
    AEStorage *s = (theAEDesc != NULL) ? STORAGE(theAEDesc) : NULL;
    return (s != NULL && s->kind == kindData) ? s->size : 0;
}

OSErr AEGetDescData(const AEDesc *theAEDesc, void *dataPtr, Size maximumSize) {
    if (theAEDesc == NULL)
        return paramErr;
    AEStorage *s = STORAGE(theAEDesc);
    if (s != NULL && s->kind != kindData)
        return errAEWrongDataType;
    if (s != NULL && dataPtr != NULL && maximumSize > 0)
        memcpy(dataPtr, s->bytes, (size_t)(s->size < maximumSize ? s->size : maximumSize));
    return noErr;
}

OSErr AEReplaceDescData(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *theAEDesc) {
    AEDesc fresh;
    OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &fresh);
    if (err != noErr)
        return err;
    AEDisposeDesc(theAEDesc);
    *theAEDesc = fresh;
    return noErr;
}

Boolean AECheckIsRecord(const AEDesc *theDesc) {
    AEStorage *s = (theDesc != NULL) ? STORAGE(theDesc) : NULL;
    return s != NULL && (s->kind == kindRecord || s->kind == kindEvent);
}

/* ---- Lists and records ---- */

OSErr AECreateList(const void *factoringPtr, Size factoredSize, Boolean isRecord, AEDescList *resultList) {
    if (resultList == NULL)
        return paramErr;
    AEInitializeDesc(resultList);
    AEStorage *s = newStorage(0);
    if (s == NULL)
        return memFullErr;
    s->kind = isRecord ? kindRecord : kindList;
    resultList->descriptorType = isRecord ? typeAERecord : typeAEList;
    resultList->dataHandle = (AEDataStorage)(void *)s;
    return noErr;
}

static AEStorage *container(const AEDesc *desc, Boolean keyed) {
    AEStorage *s = (desc != NULL) ? STORAGE(desc) : NULL;
    if (s == NULL || s->kind == kindData)
        return NULL;
    if (keyed && s->kind == kindList)
        return NULL;
    return s;
}

static OSErr growItems(AEItem **items, long *capacity, long needed) {
    if (needed <= *capacity)
        return noErr;
    long n = (*capacity > 0) ? *capacity * 2 : 4;
    while (n < needed)
        n *= 2;
    AEItem *grown = (AEItem *)realloc(*items, (size_t)n * sizeof(AEItem));
    if (grown == NULL)
        return memFullErr;
    *items = grown;
    *capacity = n;
    return noErr;
}

OSErr AECountItems(const AEDescList *theAEDescList, long *theCount) {
    AEStorage *s = container(theAEDescList, false);
    if (theCount == NULL)
        return paramErr;
    if (s == NULL) {
        *theCount = 0;
        return errAEWrongDataType;
    }
    *theCount = s->count;
    return noErr;
}

static OSErr putItem(AEItem **items, long *count, long *capacity, long index, AEKeyword key, const AEDesc *desc) {
    AEDesc copy;
    OSErr err = AEDuplicateDesc(desc, &copy);
    if (err != noErr)
        return err;
    if (index >= 1 && index <= *count) {
        AEDisposeDesc(&(*items)[index - 1].desc);
        (*items)[index - 1].desc = copy;
        if (key != 0)
            (*items)[index - 1].key = key;
        return noErr;
    }
    if (index != 0 && index != -1 && index != *count + 1) {
        AEDisposeDesc(&copy);
        return errAEIllegalIndex;
    }
    err = growItems(items, capacity, *count + 1);
    if (err != noErr) {
        AEDisposeDesc(&copy);
        return err;
    }
    (*items)[*count].key = key;
    (*items)[*count].desc = copy;
    (*count)++;
    return noErr;
}

OSErr AEPutDesc(AEDescList *theAEDescList, long index, const AEDesc *theAEDesc) {
    AEStorage *s = container(theAEDescList, false);
    if (s == NULL || theAEDesc == NULL)
        return errAEWrongDataType;
    return putItem(&s->items, &s->count, &s->capacity, index, (s->kind == kindList) ? 0 : '****', theAEDesc);
}

OSErr AEPutPtr(AEDescList *theAEDescList, long index, DescType typeCode, const void *dataPtr, Size dataSize) {
    AEDesc desc;
    OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &desc);
    if (err == noErr) {
        err = AEPutDesc(theAEDescList, index, &desc);
        AEDisposeDesc(&desc);
    }
    return err;
}

static OSErr copyOut(const AEDesc *found, DescType desiredType, DescType *typeCode, void *dataPtr, Size maximumSize, Size *actualSize) {
    AEDesc coerced;
    OSErr err = AECoerceDesc(found, desiredType, &coerced);
    if (err != noErr)
        return err;
    if (typeCode != NULL)
        *typeCode = coerced.descriptorType;
    if (actualSize != NULL)
        *actualSize = AEGetDescDataSize(&coerced);
    err = AEGetDescData(&coerced, dataPtr, maximumSize);
    AEDisposeDesc(&coerced);
    return err;
}

OSErr AEGetNthDesc(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword, AEDesc *result) {
    AEStorage *s = container(theAEDescList, false);
    if (result != NULL)
        AEInitializeDesc(result);
    if (s == NULL)
        return errAEWrongDataType;
    if (index < 1 || index > s->count)
        return errAEIllegalIndex;
    if (theAEKeyword != NULL)
        *theAEKeyword = (s->kind == kindList) ? '****' : s->items[index - 1].key;
    return AECoerceDesc(&s->items[index - 1].desc, desiredType, result);
}

OSErr AEGetNthPtr(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword,
                  DescType *typeCode, void *dataPtr, Size maximumSize, Size *actualSize) {
    AEStorage *s = container(theAEDescList, false);
    if (s == NULL)
        return errAEWrongDataType;
    if (index < 1 || index > s->count)
        return errAEIllegalIndex;
    if (theAEKeyword != NULL)
        *theAEKeyword = (s->kind == kindList) ? '****' : s->items[index - 1].key;
    return copyOut(&s->items[index - 1].desc, desiredType, typeCode, dataPtr, maximumSize, actualSize);
}

OSErr AESizeOfNthItem(const AEDescList *theAEDescList, long index, DescType *typeCode, Size *dataSize) {
    AEStorage *s = container(theAEDescList, false);
    if (s == NULL)
        return errAEWrongDataType;
    if (index < 1 || index > s->count)
        return errAEIllegalIndex;
    if (typeCode != NULL)
        *typeCode = s->items[index - 1].desc.descriptorType;
    if (dataSize != NULL)
        *dataSize = AEGetDescDataSize(&s->items[index - 1].desc);
    return noErr;
}

static void removeItem(AEItem *items, long *count, long at) {
    AEDisposeDesc(&items[at].desc);
    memmove(items + at, items + at + 1, (size_t)(*count - at - 1) * sizeof(AEItem));
    (*count)--;
}

OSErr AEDeleteItem(AEDescList *theAEDescList, long index) {
    AEStorage *s = container(theAEDescList, false);
    if (s == NULL)
        return errAEWrongDataType;
    if (index < 1 || index > s->count)
        return errAEIllegalIndex;
    removeItem(s->items, &s->count, index - 1);
    return noErr;
}

static long findKey(const AEItem *items, long count, AEKeyword key) {
    for (long i = 0; i < count; i++)
        if (items[i].key == key)
            return i;
    return -1;
}

static OSErr putKeyed(AEItem **items, long *count, long *capacity, AEKeyword key, const AEDesc *desc) {
    long at = findKey(*items, *count, key);
    return putItem(items, count, capacity, (at >= 0) ? at + 1 : *count + 1, key, desc);
}

OSErr AEPutKeyDesc(AERecord *theAERecord, AEKeyword theAEKeyword, const AEDesc *theAEDesc) {
    AEStorage *s = container(theAERecord, true);
    if (s == NULL || theAEDesc == NULL)
        return errAEWrongDataType;
    return putKeyed(&s->items, &s->count, &s->capacity, theAEKeyword, theAEDesc);
}

OSErr AEPutKeyPtr(AERecord *theAERecord, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize) {
    AEDesc desc;
    OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &desc);
    if (err == noErr) {
        err = AEPutKeyDesc(theAERecord, theAEKeyword, &desc);
        AEDisposeDesc(&desc);
    }
    return err;
}

OSErr AEGetKeyDesc(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result) {
    AEStorage *s = container(theAERecord, true);
    if (result != NULL)
        AEInitializeDesc(result);
    if (s == NULL)
        return errAEWrongDataType;
    long at = findKey(s->items, s->count, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    return AECoerceDesc(&s->items[at].desc, desiredType, result);
}

OSErr AEGetKeyPtr(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType desiredType, DescType *typeCode,
                  void *dataPtr, Size maximumSize, Size *actualSize) {
    AEStorage *s = container(theAERecord, true);
    if (s == NULL)
        return errAEWrongDataType;
    long at = findKey(s->items, s->count, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    return copyOut(&s->items[at].desc, desiredType, typeCode, dataPtr, maximumSize, actualSize);
}

OSErr AESizeOfKeyDesc(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize) {
    AEStorage *s = container(theAERecord, true);
    if (s == NULL)
        return errAEWrongDataType;
    long at = findKey(s->items, s->count, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    if (typeCode != NULL)
        *typeCode = s->items[at].desc.descriptorType;
    if (dataSize != NULL)
        *dataSize = AEGetDescDataSize(&s->items[at].desc);
    return noErr;
}

OSErr AEDeleteKeyDesc(AERecord *theAERecord, AEKeyword theAEKeyword) {
    AEStorage *s = container(theAERecord, true);
    if (s == NULL)
        return errAEWrongDataType;
    long at = findKey(s->items, s->count, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    removeItem(s->items, &s->count, at);
    return noErr;
}

/* ---- Apple events ---- */

static AEReturnID sNextReturnID = 1;

OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc *target,
                         AEReturnID returnID, AETransactionID transactionID, AppleEvent *result) {
    if (result == NULL)
        return paramErr;
    AEInitializeDesc(result);
    AEStorage *s = newStorage(0);
    if (s == NULL)
        return memFullErr;
    s->kind = kindEvent;
    result->descriptorType = typeAppleEvent;
    result->dataHandle = (AEDataStorage)(void *)s;
    if (returnID == kAutoGenerateReturnID) {
        returnID = sNextReturnID++;
        if (sNextReturnID <= 0)
            sNextReturnID = 1;
    }
    AEPutAttributePtr(result, keyEventClassAttr, typeType, &theAEEventClass, sizeof(theAEEventClass));
    AEPutAttributePtr(result, keyEventIDAttr, typeType, &theAEEventID, sizeof(theAEEventID));
    AEPutAttributePtr(result, keyReturnIDAttr, typeSInt16, &returnID, sizeof(returnID));
    AEPutAttributePtr(result, keyTransactionIDAttr, typeSInt32, &transactionID, sizeof(transactionID));
    if (target != NULL && target->descriptorType != typeNull)
        AEPutAttributeDesc(result, keyAddressAttr, target);
    return noErr;
}

static AEStorage *event(const AppleEvent *e) {
    AEStorage *s = (e != NULL) ? STORAGE(e) : NULL;
    return (s != NULL && s->kind == kindEvent) ? s : NULL;
}

OSErr AEPutParamDesc(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, const AEDesc *theAEDesc) {
    return AEPutKeyDesc(theAppleEvent, theAEKeyword, theAEDesc);
}

OSErr AEPutParamPtr(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize) {
    return AEPutKeyPtr(theAppleEvent, theAEKeyword, typeCode, dataPtr, dataSize);
}

OSErr AEGetParamDesc(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result) {
    return AEGetKeyDesc(theAppleEvent, theAEKeyword, desiredType, result);
}

OSErr AEGetParamPtr(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType *actualType,
                    void *dataPtr, Size maximumSize, Size *actualSize) {
    return AEGetKeyPtr(theAppleEvent, theAEKeyword, desiredType, actualType, dataPtr, maximumSize, actualSize);
}

OSErr AESizeOfParam(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize) {
    return AESizeOfKeyDesc(theAppleEvent, theAEKeyword, typeCode, dataSize);
}

OSErr AEDeleteParam(AppleEvent *theAppleEvent, AEKeyword theAEKeyword) {
    return AEDeleteKeyDesc(theAppleEvent, theAEKeyword);
}

OSErr AEPutAttributeDesc(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, const AEDesc *theAEDesc) {
    AEStorage *s = event(theAppleEvent);
    if (s == NULL)
        return errAENotAppleEvent;
    if (theAEDesc == NULL)
        return paramErr;
    return putKeyed(&s->attributes, &s->attributeCount, &s->attributeCapacity, theAEKeyword, theAEDesc);
}

OSErr AEPutAttributePtr(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize) {
    AEDesc desc;
    OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &desc);
    if (err == noErr) {
        err = AEPutAttributeDesc(theAppleEvent, theAEKeyword, &desc);
        AEDisposeDesc(&desc);
    }
    return err;
}

OSErr AEGetAttributeDesc(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result) {
    AEStorage *s = event(theAppleEvent);
    if (result != NULL)
        AEInitializeDesc(result);
    if (s == NULL)
        return errAENotAppleEvent;
    long at = findKey(s->attributes, s->attributeCount, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    return AECoerceDesc(&s->attributes[at].desc, desiredType, result);
}

OSErr AEGetAttributePtr(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType *typeCode,
                        void *dataPtr, Size maximumSize, Size *actualSize) {
    AEStorage *s = event(theAppleEvent);
    if (s == NULL)
        return errAENotAppleEvent;
    long at = findKey(s->attributes, s->attributeCount, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    return copyOut(&s->attributes[at].desc, desiredType, typeCode, dataPtr, maximumSize, actualSize);
}

OSErr AESizeOfAttribute(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize) {
    AEStorage *s = event(theAppleEvent);
    if (s == NULL)
        return errAENotAppleEvent;
    long at = findKey(s->attributes, s->attributeCount, theAEKeyword);
    if (at < 0)
        return errAEDescNotFound;
    if (typeCode != NULL)
        *typeCode = s->attributes[at].desc.descriptorType;
    if (dataSize != NULL)
        *dataSize = AEGetDescDataSize(&s->attributes[at].desc);
    return noErr;
}

/* ---- Coercions ---- */

static Boolean isInteger(DescType t) {
    return t == typeSInt16 || t == typeSInt32 || t == typeUInt16 || t == typeUInt32 || t == typeSInt64 || t == typeUInt64;
}

static Boolean isFloat(DescType t) {
    return t == typeIEEE32BitFloatingPoint || t == typeIEEE64BitFloatingPoint;
}

static Boolean isText(DescType t) {
    return t == typeChar || t == typeUTF8Text || t == typeUnicodeText || t == typeUTF16ExternalRepresentation;
}

static Boolean isBoolean(DescType t) {
    return t == typeBoolean || t == typeTrue || t == typeFalse;
}

/* A number from plain data: integers, floats and booleans. */
static Boolean numberOf(DescType t, const void *p, Size n, double *value, int64_t *integer, Boolean *exact) {
    *exact = true;
    switch (t) {
    case typeSInt16: if (n < 2) return false; *integer = *(const int16_t *)p; break;
    case typeUInt16: if (n < 2) return false; *integer = *(const uint16_t *)p; break;
    case typeSInt32: if (n < 4) return false; *integer = *(const int32_t *)p; break;
    case typeUInt32: if (n < 4) return false; *integer = *(const uint32_t *)p; break;
    case typeSInt64: if (n < 8) return false; *integer = *(const int64_t *)p; break;
    case typeUInt64: if (n < 8) return false; *integer = (int64_t)*(const uint64_t *)p; break;
    case typeTrue: *integer = 1; break;
    case typeFalse: *integer = 0; break;
    case typeBoolean: if (n < 1) return false; *integer = (*(const uint8_t *)p) != 0; break;
    case typeIEEE32BitFloatingPoint:
        if (n < 4) return false;
        *value = *(const float *)p;
        *exact = false;
        return true;
    case typeIEEE64BitFloatingPoint:
        if (n < 8) return false;
        *value = *(const double *)p;
        *exact = false;
        return true;
    default:
        return false;
    }
    *value = (double)*integer;
    return true;
}

static CFStringRef stringOf(DescType t, const void *p, Size n) {
    switch (t) {
    case typeChar: return CFStringCreateWithBytes(NULL, p, n, kCFStringEncodingMacRoman, false);
    case typeUTF8Text: return CFStringCreateWithBytes(NULL, p, n, kCFStringEncodingUTF8, false);
    case typeUnicodeText: return CFStringCreateWithBytes(NULL, p, n, kCFStringEncodingUTF16, false);
    case typeUTF16ExternalRepresentation: return CFStringCreateWithBytes(NULL, p, n, kCFStringEncodingUTF16, true);
    default: return NULL;
    }
}

static OSErr descOfString(CFStringRef string, DescType toType, AEDesc *result) {
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    Boolean external = false;
    switch (toType) {
    case typeChar: encoding = kCFStringEncodingMacRoman; break;
    case typeUnicodeText: encoding = kCFStringEncodingUTF16; break;
    case typeUTF16ExternalRepresentation: encoding = kCFStringEncodingUTF16; external = true; break;
    default: break;
    }
    CFIndex length = CFStringGetLength(string), used = 0;
    if (CFStringGetBytes(string, CFRangeMake(0, length), encoding, 0, external, NULL, 0, &used) != length)
        return errAECoercionFail;
    UInt8 *bytes = (UInt8 *)malloc((size_t)used + 1);
    if (bytes == NULL)
        return memFullErr;
    CFStringGetBytes(string, CFRangeMake(0, length), encoding, 0, external, bytes, used, &used);
    OSErr err = AECreateDesc(toType, bytes, used, result);
    free(bytes);
    return err;
}

static OSErr descOfNumber(double value, int64_t integer, Boolean exact, DescType toType, AEDesc *result) {
    switch (toType) {
    case typeSInt16: {
        if (!exact) { if (value != floor(value)) return errAECoercionFail; integer = (int64_t)value; }
        if (integer < INT16_MIN || integer > INT16_MAX) return errAECoercionFail;
        int16_t v = (int16_t)integer; return AECreateDesc(toType, &v, sizeof(v), result);
    }
    case typeUInt16: {
        if (!exact) { if (value != floor(value)) return errAECoercionFail; integer = (int64_t)value; }
        if (integer < 0 || integer > UINT16_MAX) return errAECoercionFail;
        uint16_t v = (uint16_t)integer; return AECreateDesc(toType, &v, sizeof(v), result);
    }
    case typeSInt32: {
        if (!exact) { if (value != floor(value)) return errAECoercionFail; integer = (int64_t)value; }
        if (integer < INT32_MIN || integer > INT32_MAX) return errAECoercionFail;
        int32_t v = (int32_t)integer; return AECreateDesc(toType, &v, sizeof(v), result);
    }
    case typeUInt32: {
        if (!exact) { if (value != floor(value)) return errAECoercionFail; integer = (int64_t)value; }
        if (integer < 0 || integer > UINT32_MAX) return errAECoercionFail;
        uint32_t v = (uint32_t)integer; return AECreateDesc(toType, &v, sizeof(v), result);
    }
    case typeSInt64: case typeUInt64: {
        if (!exact) { if (value != floor(value)) return errAECoercionFail; integer = (int64_t)value; }
        return AECreateDesc(toType, &integer, sizeof(integer), result);
    }
    case typeIEEE32BitFloatingPoint: { float v = (float)value; return AECreateDesc(toType, &v, sizeof(v), result); }
    case typeIEEE64BitFloatingPoint: return AECreateDesc(toType, &value, sizeof(value), result);
    case typeBoolean: {
        if (value != 0 && value != 1) return errAECoercionFail;
        Boolean v = value != 0; return AECreateDesc(toType, &v, 1, result);
    }
    case typeTrue: case typeFalse:
        if ((toType == typeTrue) != (value != 0)) return errAECoercionFail;
        return AECreateDesc(toType, NULL, 0, result);
    default:
        if (isText(toType)) {
            char buffer[64];
            if (exact) snprintf(buffer, sizeof(buffer), "%lld", (long long)integer);
            else snprintf(buffer, sizeof(buffer), "%.17g", value);
            CFStringRef s = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingASCII);
            OSErr err = descOfString(s, toType, result);
            CFRelease(s);
            return err;
        }
        return errAECoercionFail;
    }
}

OSErr AECoerceDesc(const AEDesc *theAEDesc, DescType toType, AEDesc *result) {
    if (theAEDesc == NULL || result == NULL)
        return paramErr;
    AEInitializeDesc(result);
    DescType from = theAEDesc->descriptorType;
    AEStorage *s = STORAGE(theAEDesc);

    if (toType == from || toType == typeWildCard)
        return AEDuplicateDesc(theAEDesc, result);
    if (s != NULL && s->kind != kindData) {
        /* A record takes any type (object specifiers are records), and
           anything goes into a list of one. */
        if (s->kind == kindRecord && toType != typeAEList && toType != typeAppleEvent) {
            OSErr err = AEDuplicateDesc(theAEDesc, result);
            if (err == noErr)
                result->descriptorType = toType;
            return err;
        }
        if (toType == typeAERecord && s->kind != kindList) {
            OSErr err = AEDuplicateDesc(theAEDesc, result);
            if (err == noErr) {
                STORAGE(result)->kind = kindRecord;
                freeItems(STORAGE(result)->attributes, STORAGE(result)->attributeCount);
                STORAGE(result)->attributes = NULL;
                STORAGE(result)->attributeCount = STORAGE(result)->attributeCapacity = 0;
                result->descriptorType = typeAERecord;
            }
            return err;
        }
    }
    if (toType == typeAEList) {
        OSErr err = AECreateList(NULL, 0, false, result);
        if (err == noErr)
            err = AEPutDesc(result, 0, theAEDesc);
        return err;
    }
    if (s != NULL && s->kind != kindData)
        return errAECoercionFail;
    return AECoercePtr(from, s != NULL ? s->bytes : NULL, s != NULL ? s->size : 0, toType, result);
}

OSErr AECoercePtr(DescType typeCode, const void *dataPtr, Size dataSize, DescType toType, AEDesc *result) {
    double value = 0;
    int64_t integer = 0;
    Boolean exact = true;

    if (result == NULL)
        return paramErr;
    AEInitializeDesc(result);
    if (toType == typeCode || toType == typeWildCard)
        return AECreateDesc(typeCode, dataPtr, dataSize, result);
    if (toType == typeAEList) {
        AEDesc item;
        OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &item);
        if (err == noErr) {
            err = AECoerceDesc(&item, typeAEList, result);
            AEDisposeDesc(&item);
        }
        return err;
    }
    if (numberOf(typeCode, dataPtr, dataSize, &value, &integer, &exact))
        return descOfNumber(value, integer, exact, toType, result);
    if (isText(typeCode)) {
        CFStringRef string = stringOf(typeCode, dataPtr, dataSize);
        OSErr err = errAECoercionFail;
        if (string == NULL)
            return errAECoercionFail;
        if (isText(toType))
            err = descOfString(string, toType, result);
        else if (isBoolean(toType)) {
            if (CFStringCompare(string, CFSTR("true"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                err = descOfNumber(1, 1, true, toType, result);
            else if (CFStringCompare(string, CFSTR("false"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                err = descOfNumber(0, 0, true, toType, result);
        } else if (isInteger(toType) || isFloat(toType)) {
            char buffer[128], *end = NULL;
            if (CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                double v = strtod(buffer, &end);
                while (end != NULL && (*end == ' ' || *end == '\t'))
                    end++;
                if (end != buffer && end != NULL && *end == '\0')
                    err = descOfNumber(v, (int64_t)v, v == floor(v) && fabs(v) < 9.2e18, toType, result);
            }
        } else if (toType == typeType || toType == typeEnumerated || toType == typeKeyword) {
            char buffer[8];
            if (CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingMacRoman) && strlen(buffer) == 4) {
                FourCharCode code = ((FourCharCode)(UInt8)buffer[0] << 24) | ((FourCharCode)(UInt8)buffer[1] << 16) |
                                    ((FourCharCode)(UInt8)buffer[2] << 8) | (FourCharCode)(UInt8)buffer[3];
                err = AECreateDesc(toType, &code, sizeof(code), result);
            }
        } else if (toType == typeFileURL) {
            /* A POSIX path. */
            CFURLRef url = CFURLCreateWithFileSystemPath(NULL, string, kCFURLPOSIXPathStyle, false);
            if (url != NULL) {
                CFStringRef absolute = CFURLGetString(url);
                char buffer[4096];
                if (CFStringGetCString(absolute, buffer, sizeof(buffer), kCFStringEncodingUTF8))
                    err = AECreateDesc(typeFileURL, buffer, (Size)strlen(buffer), result);
                CFRelease(url);
            }
        }
        CFRelease(string);
        return err;
    }
    if ((typeCode == typeType || typeCode == typeEnumerated || typeCode == typeKeyword) && dataSize == 4 &&
        (toType == typeType || toType == typeEnumerated || toType == typeKeyword || isText(toType))) {
        if (!isText(toType))
            return AECreateDesc(toType, dataPtr, dataSize, result);
        FourCharCode code = *(const FourCharCode *)dataPtr;
        char buffer[5] = { (char)(code >> 24), (char)(code >> 16), (char)(code >> 8), (char)code, 0 };
        CFStringRef s = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingMacRoman);
        OSErr err = s != NULL ? descOfString(s, toType, result) : errAECoercionFail;
        if (s != NULL)
            CFRelease(s);
        return err;
    }
    if (typeCode == typeFileURL && isText(toType)) {
        CFURLRef url = CFURLCreateWithBytes(NULL, dataPtr, dataSize, kCFStringEncodingUTF8, NULL);
        OSErr err = errAECoercionFail;
        if (url != NULL) {
            CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
            if (path != NULL) {
                err = descOfString(path, toType, result);
                CFRelease(path);
            }
            CFRelease(url);
        }
        return err;
    }
    if (typeCode == typeNull && isText(toType))
        return descOfString(CFSTR(""), toType, result);
    return errAECoercionFail;
}
