/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFFileDescriptor on dispatch sources: a read and a write source on the
 * descriptor, resumed while their callback is enabled. When one fires, it
 * marks its event, is suspended again (the callback is enabled once), and
 * signals the descriptor's run loop source, waking the run loops it is
 * scheduled in; that source runs the callback on the run loop's thread.
 * A descriptor is freed once invalidated and released (its sources hold
 * it until then).
 */

#include <CoreFoundation/CFFileDescriptor.h>
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct __CFFileDescriptor {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CFFileDescriptorNativeDescriptor fd;
    Boolean closeOnInvalidate;
    Boolean valid;
    CFFileDescriptorCallBack callout;
    CFFileDescriptorContext context;
    CFOptionFlags enabled;            /* callbacks wanted */
    CFOptionFlags fired;              /* events seen, not yet called back */
    dispatch_source_t readSource, writeSource;
    Boolean readResumed, writeResumed;
    CFRunLoopSourceRef runLoopSource;
    CFMutableArrayRef runLoops;       /* where runLoopSource is scheduled */
};

static CFTypeID fileDescriptorTypeID = 0;

static void invalidateLocked(CFFileDescriptorRef f, void **releaseInfo, void (**release)(void *));

static void fileDescriptorFinalize(CFTypeRef cf)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;
    void *info = NULL;
    void (*release)(void *) = NULL;

    pthread_mutex_lock(&f->lock);
    invalidateLocked(f, &info, &release);
    pthread_mutex_unlock(&f->lock);
    if (release)
        release(info);
    if (f->runLoops)
        CFRelease(f->runLoops);
    pthread_mutex_destroy(&f->lock);
}

static CFStringRef fileDescriptorDescription(CFTypeRef cf)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<CFFileDescriptor %p [%p]>{valid = %s, fd = %d}"),
                                    cf, CFGetAllocator(cf), f->valid ? "Yes" : "No", f->fd);
}

static const CFRuntimeClass fileDescriptorClass = {
    0, "CFFileDescriptor", NULL, NULL, fileDescriptorFinalize, NULL, NULL, NULL, fileDescriptorDescription
};

CFTypeID CFFileDescriptorGetTypeID(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ fileDescriptorTypeID = _CFRuntimeRegisterClass(&fileDescriptorClass); });
    return fileDescriptorTypeID;
}

/* The run loop source told to run, and its run loops woken. */
static void signalRunLoops(CFFileDescriptorRef f)
{
    CFRunLoopSourceRef source = NULL;
    CFArrayRef runLoops = NULL;

    pthread_mutex_lock(&f->lock);
    if (f->valid && f->runLoopSource) {
        source = (CFRunLoopSourceRef)CFRetain(f->runLoopSource);
        runLoops = CFArrayCreateCopy(kCFAllocatorDefault, f->runLoops);
    }
    pthread_mutex_unlock(&f->lock);
    if (source == NULL)
        return;
    CFRunLoopSourceSignal(source);
    for (CFIndex i = 0; i < CFArrayGetCount(runLoops); i++)
        CFRunLoopWakeUp((CFRunLoopRef)CFArrayGetValueAtIndex(runLoops, i));
    CFRelease(runLoops);
    CFRelease(source);
}

/* A dispatch source fired: its event marked, the source suspended until
   the callback is enabled again. */
static void eventFired(CFFileDescriptorRef f, CFOptionFlags event)
{
    pthread_mutex_lock(&f->lock);
    if (!f->valid || !(f->enabled & event)) {
        pthread_mutex_unlock(&f->lock);
        return;
    }
    f->enabled &= ~event;
    f->fired |= event;
    if (event == kCFFileDescriptorReadCallBack && f->readResumed) {
        dispatch_suspend(f->readSource);
        f->readResumed = false;
    } else if (event == kCFFileDescriptorWriteCallBack && f->writeResumed) {
        dispatch_suspend(f->writeSource);
        f->writeResumed = false;
    }
    pthread_mutex_unlock(&f->lock);
    signalRunLoops(f);
}

static dispatch_queue_t eventQueue(void)
{
    static dispatch_queue_t queue;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ queue = dispatch_queue_create("org.minkar.CFFileDescriptor", DISPATCH_QUEUE_SERIAL); });
    return queue;
}

CFFileDescriptorRef CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd,
                                           Boolean closeOnInvalidate, CFFileDescriptorCallBack callout,
                                           const CFFileDescriptorContext *context)
{
    if (fd < 0)
        return NULL;
    CFIndex size = sizeof(struct __CFFileDescriptor) - sizeof(CFRuntimeBase);
    CFFileDescriptorRef f = (CFFileDescriptorRef)_CFRuntimeCreateInstance(allocator, CFFileDescriptorGetTypeID(), size, NULL);
    if (f == NULL)
        return NULL;
    pthread_mutex_init(&f->lock, NULL);
    f->fd = fd;
    f->closeOnInvalidate = closeOnInvalidate;
    f->valid = true;
    f->callout = callout;
    if (context) {
        f->context = *context;
        if (context->retain && context->info)
            f->context.info = context->retain(context->info);
    }
    f->runLoops = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    /* Each source holds f until it is cancelled (by the invalidation): a
       handler already on its way never finds f freed. */
    dispatch_queue_t queue = eventQueue();
    f->readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, queue);
    f->writeSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, (uintptr_t)fd, 0, queue);
    if (f->readSource == NULL || f->writeSource == NULL) {
        if (f->readSource)
            dispatch_release(f->readSource);
        if (f->writeSource)
            dispatch_release(f->writeSource);
        f->readSource = f->writeSource = NULL;
        CFRelease(f);
        return NULL;
    }
    dispatch_source_set_event_handler(f->readSource, ^{ eventFired(f, kCFFileDescriptorReadCallBack); });
    dispatch_source_set_event_handler(f->writeSource, ^{ eventFired(f, kCFFileDescriptorWriteCallBack); });
    CFRetain(f);
    CFRetain(f);
    dispatch_source_set_cancel_handler(f->readSource, ^{ CFRelease(f); });
    dispatch_source_set_cancel_handler(f->writeSource, ^{ CFRelease(f); });
    return f;
}

CFFileDescriptorNativeDescriptor CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f)
{
    return f->fd;
}

void CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context)
{
    if (context)
        *context = f->context;
}

void CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes)
{
    pthread_mutex_lock(&f->lock);
    if (f->valid) {
        f->enabled |= callBackTypes & (kCFFileDescriptorReadCallBack | kCFFileDescriptorWriteCallBack);
        if ((f->enabled & kCFFileDescriptorReadCallBack) && !f->readResumed) {
            dispatch_resume(f->readSource);
            f->readResumed = true;
        }
        if ((f->enabled & kCFFileDescriptorWriteCallBack) && !f->writeResumed) {
            dispatch_resume(f->writeSource);
            f->writeResumed = true;
        }
    }
    pthread_mutex_unlock(&f->lock);
}

void CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes)
{
    pthread_mutex_lock(&f->lock);
    if (f->valid) {
        f->enabled &= ~callBackTypes;
        f->fired &= ~callBackTypes;
        if (!(f->enabled & kCFFileDescriptorReadCallBack) && f->readResumed) {
            dispatch_suspend(f->readSource);
            f->readResumed = false;
        }
        if (!(f->enabled & kCFFileDescriptorWriteCallBack) && f->writeResumed) {
            dispatch_suspend(f->writeSource);
            f->writeResumed = false;
        }
    }
    pthread_mutex_unlock(&f->lock);
}

/* Under the lock: the sources cancelled (resumed first: a suspended
   source is not cancelled), the run loop source invalidated later by
   the caller's release of it, the descriptor closed if asked. */
static void invalidateLocked(CFFileDescriptorRef f, void **releaseInfo, void (**release)(void *))
{
    if (!f->valid)
        return;
    f->valid = false;
    dispatch_source_t sources[2] = { f->readSource, f->writeSource };
    Boolean resumed[2] = { f->readResumed, f->writeResumed };
    for (int i = 0; i < 2; i++) {
        if (sources[i] == NULL)
            continue;
        dispatch_source_cancel(sources[i]);
        if (!resumed[i])
            dispatch_resume(sources[i]);
        dispatch_release(sources[i]);
    }
    f->readSource = f->writeSource = NULL;
    f->readResumed = f->writeResumed = false;
    if (f->closeOnInvalidate)
        close(f->fd);
    *releaseInfo = f->context.info;
    *release = f->context.release;
    f->context.info = NULL;
}

void CFFileDescriptorInvalidate(CFFileDescriptorRef f)
{
    void *info = NULL;
    void (*release)(void *) = NULL;
    CFRunLoopSourceRef source = NULL;

    CFRetain(f);
    pthread_mutex_lock(&f->lock);
    invalidateLocked(f, &info, &release);
    source = f->runLoopSource;
    f->runLoopSource = NULL;
    pthread_mutex_unlock(&f->lock);
    if (source) {
        CFRunLoopSourceInvalidate(source);
        CFRelease(source);
    }
    if (release && info)
        release(info);
    CFRelease(f);
}

Boolean CFFileDescriptorIsValid(CFFileDescriptorRef f)
{
    return f->valid;
}

/* The run loop source's part. */

static void sourceSchedule(void *info, CFRunLoopRef runLoop, CFStringRef mode)
{
    CFFileDescriptorRef f = info;
    (void)mode;
    pthread_mutex_lock(&f->lock);
    if (!CFArrayContainsValue(f->runLoops, CFRangeMake(0, CFArrayGetCount(f->runLoops)), runLoop))
        CFArrayAppendValue(f->runLoops, runLoop);
    pthread_mutex_unlock(&f->lock);
}

/* A run loop it leaves stays in the list: it may still be scheduled there
   in another mode, and waking it once more does no harm. */
static void sourceCancel(void *info, CFRunLoopRef runLoop, CFStringRef mode)
{
    (void)info; (void)runLoop; (void)mode;
}

static void sourcePerform(void *info)
{
    CFFileDescriptorRef f = info;
    CFOptionFlags fired;
    void *calloutInfo;

    CFRetain(f);
    pthread_mutex_lock(&f->lock);
    fired = f->valid ? f->fired : 0;
    f->fired = 0;
    calloutInfo = f->context.info;
    pthread_mutex_unlock(&f->lock);
    if (fired && f->callout)
        f->callout(f, fired, calloutInfo);
    CFRelease(f);
}

CFRunLoopSourceRef CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order)
{
    CFRunLoopSourceRef source = NULL;

    pthread_mutex_lock(&f->lock);
    if (f->valid) {
        if (f->runLoopSource == NULL) {
            /* The source holds f weakly: it goes with f's invalidation. */
            CFRunLoopSourceContext context = { 0, f, NULL, NULL, NULL, NULL, NULL,
                                               sourceSchedule, sourceCancel, sourcePerform };
            f->runLoopSource = CFRunLoopSourceCreate(allocator, order, &context);
        }
        source = (CFRunLoopSourceRef)CFRetain(f->runLoopSource);
    }
    pthread_mutex_unlock(&f->lock);
    return source;
}
