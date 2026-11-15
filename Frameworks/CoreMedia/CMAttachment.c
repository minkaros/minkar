/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Attachments on CoreMedia's bearers (their own store) and on CoreVideo's
 * buffers (CVBuffer's).
 */

#include "CMInternal.h"
#include <CoreVideo/CoreVideo.h>

#define MAX_BEARER_TYPES 8
static CFTypeID bearerTypes[MAX_BEARER_TYPES];
static int bearerTypeCount;
static pthread_mutex_t typesLock = PTHREAD_MUTEX_INITIALIZER;

void _CMRegisterBearerType(CFTypeID type) {
    pthread_mutex_lock(&typesLock);
    if (bearerTypeCount < MAX_BEARER_TYPES)
        bearerTypes[bearerTypeCount++] = type;
    pthread_mutex_unlock(&typesLock);
}

void _CMAttachmentStoreInit(CMAttachmentStore *store) {
    pthread_mutex_init(&store->lock, NULL);
    store->propagated = NULL;
    store->unpropagated = NULL;
}

void _CMAttachmentStoreFinalize(CMAttachmentStore *store) {
    if (store->propagated != NULL)
        CFRelease(store->propagated);
    if (store->unpropagated != NULL)
        CFRelease(store->unpropagated);
    pthread_mutex_destroy(&store->lock);
}

static CMAttachmentStore *storeOf(CMAttachmentBearerRef target) {
    if (target == NULL)
        return NULL;
    CFTypeID type = CFGetTypeID(target);
    pthread_mutex_lock(&typesLock);
    for (int i = 0; i < bearerTypeCount; i++)
        if (bearerTypes[i] == type) {
            pthread_mutex_unlock(&typesLock);
            return &((CMBearerHeader *)target)->attachments;
        }
    pthread_mutex_unlock(&typesLock);
    return NULL;
}

static Boolean isCVBuffer(CMAttachmentBearerRef target) {
    return target != NULL && CFGetTypeID(target) == CVPixelBufferGetTypeID();
}

static CFMutableDictionaryRef *dictionaryOf(CMAttachmentStore *s, CMAttachmentMode mode) {
    return mode == kCMAttachmentMode_ShouldPropagate ? &s->propagated : &s->unpropagated;
}

void CMSetAttachment(CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode mode) {
    if (key == NULL || value == NULL)
        return;
    if (isCVBuffer(target)) {
        CVBufferSetAttachment((CVBufferRef)target, key, value, (CVAttachmentMode)mode);
        return;
    }
    CMAttachmentStore *s = storeOf(target);
    if (s == NULL)
        return;
    pthread_mutex_lock(&s->lock);
    /* A key is in one mode at a time. */
    CFMutableDictionaryRef *other = dictionaryOf(s, mode == kCMAttachmentMode_ShouldPropagate
                                                        ? kCMAttachmentMode_ShouldNotPropagate
                                                        : kCMAttachmentMode_ShouldPropagate);
    if (*other != NULL)
        CFDictionaryRemoveValue(*other, key);
    CFMutableDictionaryRef *d = dictionaryOf(s, mode);
    if (*d == NULL)
        *d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(*d, key, value);
    pthread_mutex_unlock(&s->lock);
}

CFTypeRef CMGetAttachment(CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode *modeOut) {
    if (key == NULL)
        return NULL;
    if (isCVBuffer(target)) {
        CVAttachmentMode m = kCVAttachmentMode_ShouldPropagate;
        CFTypeRef v = CVBufferGetAttachment((CVBufferRef)target, key, &m);
        if (v != NULL && modeOut != NULL)
            *modeOut = (CMAttachmentMode)m;
        return v;
    }
    CMAttachmentStore *s = storeOf(target);
    if (s == NULL)
        return NULL;
    CFTypeRef v = NULL;
    pthread_mutex_lock(&s->lock);
    if (s->propagated != NULL && (v = CFDictionaryGetValue(s->propagated, key)) != NULL) {
        if (modeOut != NULL)
            *modeOut = kCMAttachmentMode_ShouldPropagate;
    } else if (s->unpropagated != NULL && (v = CFDictionaryGetValue(s->unpropagated, key)) != NULL) {
        if (modeOut != NULL)
            *modeOut = kCMAttachmentMode_ShouldNotPropagate;
    }
    pthread_mutex_unlock(&s->lock);
    return v;
}

void CMRemoveAttachment(CMAttachmentBearerRef target, CFStringRef key) {
    if (isCVBuffer(target)) {
        CVBufferRemoveAttachment((CVBufferRef)target, key);
        return;
    }
    CMAttachmentStore *s = storeOf(target);
    if (s == NULL || key == NULL)
        return;
    pthread_mutex_lock(&s->lock);
    if (s->propagated != NULL)
        CFDictionaryRemoveValue(s->propagated, key);
    if (s->unpropagated != NULL)
        CFDictionaryRemoveValue(s->unpropagated, key);
    pthread_mutex_unlock(&s->lock);
}

void CMRemoveAllAttachments(CMAttachmentBearerRef target) {
    if (isCVBuffer(target)) {
        CVBufferRemoveAllAttachments((CVBufferRef)target);
        return;
    }
    CMAttachmentStore *s = storeOf(target);
    if (s == NULL)
        return;
    pthread_mutex_lock(&s->lock);
    if (s->propagated != NULL)
        CFDictionaryRemoveAllValues(s->propagated);
    if (s->unpropagated != NULL)
        CFDictionaryRemoveAllValues(s->unpropagated);
    pthread_mutex_unlock(&s->lock);
}

CFDictionaryRef CMCopyDictionaryOfAttachments(CFAllocatorRef allocator, CMAttachmentBearerRef target,
                                              CMAttachmentMode mode) {
    CFDictionaryRef found = NULL;
    if (isCVBuffer(target)) {
        found = CVBufferGetAttachments((CVBufferRef)target, (CVAttachmentMode)mode);
        return found != NULL && CFDictionaryGetCount(found) > 0 ? CFDictionaryCreateCopy(allocator, found) : NULL;
    }
    CMAttachmentStore *s = storeOf(target);
    if (s == NULL)
        return NULL;
    pthread_mutex_lock(&s->lock);
    CFMutableDictionaryRef d = *dictionaryOf(s, mode);
    if (d != NULL && CFDictionaryGetCount(d) > 0)
        found = CFDictionaryCreateCopy(allocator, d);
    pthread_mutex_unlock(&s->lock);
    return found;
}

static void setOne(const void *key, const void *value, void *context) {
    const struct { CMAttachmentBearerRef target; CMAttachmentMode mode; } *c = context;
    if (CFGetTypeID(key) == CFStringGetTypeID())
        CMSetAttachment(c->target, key, value, c->mode);
}

void CMSetAttachments(CMAttachmentBearerRef target, CFDictionaryRef attachments, CMAttachmentMode mode) {
    struct { CMAttachmentBearerRef target; CMAttachmentMode mode; } c = { target, mode };
    if (attachments != NULL)
        CFDictionaryApplyFunction(attachments, setOne, &c);
}

/* The source's propagated attachments on the destination, propagated too. */
void CMPropagateAttachments(CMAttachmentBearerRef source, CMAttachmentBearerRef destination) {
    CFDictionaryRef d = CMCopyDictionaryOfAttachments(NULL, source, kCMAttachmentMode_ShouldPropagate);
    if (d == NULL)
        return;
    CMSetAttachments(destination, d, kCMAttachmentMode_ShouldPropagate);
    CFRelease(d);
}
