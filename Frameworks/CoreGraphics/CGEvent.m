/*
 * Event services: CGEvent, event taps, event sources
 * Copyright (C) 2024-2026 Zoe Knox <zoe@ravynsoft.com>
 * Modifications Copyright (c) 2026 The Minkar contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * The events are the new window server's (WindowServer plan, WS3): a
 * CGEventRef holds an SLSEvent, CGEventPost and the taps go through
 * SkyLight. With the legacy server running (_CGUsesSkyLight() false) taps
 * and posting do nothing; that server goes in WS4.
 */

#import <CoreFoundation/CFRuntime.h>
#import <CoreFoundation/CFMachPort.h>
#import <CoreFoundation/CFString.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGEvent.h>
#import <CoreGraphics/CGDirectDisplay.h>
#include <SkyLight/SkyLight.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

extern int _CGUsesSkyLight(void);

struct __CGEvent {
    CFRuntimeBase _base;
    SLSEvent e;
    CGEventSourceRef source;
    int64_t userData;
    int64_t sourcePID;
    int64_t targetPID;
    double pressure;
    UniChar unicode[8];
    UniCharCount unicodeLength;
};

struct __CGEventSource {
    CFRuntimeBase _base;
    CGEventSourceStateID state;
    CGEventSourceKeyboardType keyboardType;
    int64_t userData;
    double pixelsPerLine;
    CFTimeInterval suppressionInterval;
    CGEventFilterMask filter[kCGNumberOfEventSuppressionStates];
};

static CFTypeID eventTypeID, sourceTypeID;
static pthread_once_t registerOnce = PTHREAD_ONCE_INIT;

static void finalizeEvent(CFTypeRef cf) {
    struct __CGEvent *ev = (struct __CGEvent *)(uintptr_t)cf;
    if(ev->source)
        CFRelease(ev->source);
}

static const CFRuntimeClass eventClass = {
    0, "CGEvent", NULL, NULL, finalizeEvent, NULL, NULL, NULL, NULL
};
static const CFRuntimeClass sourceClass = {
    0, "CGEventSource", NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static void registerClasses(void) {
    eventTypeID = _CFRuntimeRegisterClass(&eventClass);
    sourceTypeID = _CFRuntimeRegisterClass(&sourceClass);
}

static uint64_t nanoseconds(void) {
    static mach_timebase_info_data_t tb;
    if(tb.denom == 0)
        mach_timebase_info(&tb);
    return mach_absolute_time() * tb.numer / tb.denom;
}

static struct __CGEvent *newEvent(CGEventSourceRef source, CGEventType type) {
    pthread_once(&registerOnce, registerClasses);
    struct __CGEvent *ev = (struct __CGEvent *)(uintptr_t)_CFRuntimeCreateInstance(kCFAllocatorDefault,
        eventTypeID, sizeof(struct __CGEvent) - sizeof(CFRuntimeBase), NULL);
    if(ev == NULL)
        return NULL;
    memset(&ev->e, 0, sizeof(*ev) - sizeof(CFRuntimeBase));
    ev->e.type = type;
    ev->e.timestamp = nanoseconds();
    ev->sourcePID = getpid();
    if(source) {
        ev->source = (CGEventSourceRef)CFRetain(source);
        ev->userData = source->userData;
    }
    return ev;
}

static CGEventRef eventFromSLS(const SLSEvent *e) {
    struct __CGEvent *ev = newEvent(NULL, e->type);
    if(ev)
        ev->e = *e;
    return ev;
}

/* ---- Working with events ---- */

CFTypeID CGEventGetTypeID(void) {
    pthread_once(&registerOnce, registerClasses);
    return eventTypeID;
}

CGEventRef CGEventCreate(CGEventSourceRef source) {
    struct __CGEvent *ev = newEvent(source, kCGEventNull);
    SLSPoint p = { 0, 0 };
    if(ev == NULL)
        return NULL;
    if(_CGUsesSkyLight() && SLSGetCursorPosition(&p) == kSLSErrorSuccess) {
        ev->e.x = p.x;
        ev->e.y = p.y;
    }
    return ev;
}

/* The event's bytes, for CGEventCreateFromData: its SLSEvent, then the
 * fields kept beside it. */
struct eventData {
    uint32_t magic;
    uint32_t version;
    SLSEvent e;
    int64_t userData, sourcePID, targetPID;
    double pressure;
};
enum { kEventDataMagic = 0x43474576 /* 'CGEv' */ };

CFDataRef CGEventCreateData(CFAllocatorRef allocator, CGEventRef event) {
    struct eventData d;
    if(event == NULL)
        return NULL;
    memset(&d, 0, sizeof(d));
    d.magic = kEventDataMagic;
    d.version = 1;
    d.e = event->e;
    d.userData = event->userData;
    d.sourcePID = event->sourcePID;
    d.targetPID = event->targetPID;
    d.pressure = event->pressure;
    return CFDataCreate(allocator, (const UInt8 *)&d, sizeof(d));
}

CGEventRef CGEventCreateFromData(CFAllocatorRef allocator, CFDataRef data) {
    struct eventData d;
    if(data == NULL || CFDataGetLength(data) != sizeof(d))
        return NULL;
    CFDataGetBytes(data, CFRangeMake(0, sizeof(d)), (UInt8 *)&d);
    if(d.magic != kEventDataMagic || d.version != 1)
        return NULL;
    struct __CGEvent *ev = (struct __CGEvent *)eventFromSLS(&d.e);
    if(ev) {
        ev->userData = d.userData;
        ev->sourcePID = d.sourcePID;
        ev->targetPID = d.targetPID;
        ev->pressure = d.pressure;
    }
    return ev;
}

CGEventRef CGEventCreateMouseEvent(CGEventSourceRef source, CGEventType mouseType, CGPoint mouseCursorPosition, CGMouseButton mouseButton) {
    struct __CGEvent *ev = newEvent(source, mouseType);
    if(ev == NULL)
        return NULL;
    ev->e.x = mouseCursorPosition.x;
    ev->e.y = mouseCursorPosition.y;
    ev->e.buttonNumber = mouseButton;
    switch(mouseType) {
        case kCGEventLeftMouseDown: case kCGEventLeftMouseUp:
        case kCGEventRightMouseDown: case kCGEventRightMouseUp:
        case kCGEventOtherMouseDown: case kCGEventOtherMouseUp:
            ev->e.clickCount = 1;
            ev->pressure = (mouseType == kCGEventLeftMouseUp || mouseType == kCGEventRightMouseUp ||
                mouseType == kCGEventOtherMouseUp) ? 0 : 1;
            break;
        default:
            break;
    }
    return ev;
}

CGEventRef CGEventCreateKeyboardEvent(CGEventSourceRef source, CGKeyCode virtualKey, bool keyDown) {
    struct __CGEvent *ev = newEvent(source, keyDown ? kCGEventKeyDown : kCGEventKeyUp);
    if(ev)
        ev->e.keyCode = virtualKey;	/* the server's keymap gives the characters */
    return ev;
}

CGEventRef CGEventCreateScrollWheelEvent(CGEventSourceRef source, CGScrollEventUnit units, uint32_t wheelCount, int32_t wheel1, ...) {
    struct __CGEvent *ev = newEvent(source, kCGEventScrollWheel);
    if(ev == NULL)
        return NULL;
    ev->e.scrollY = wheel1;
    if(wheelCount > 1) {
        va_list ap;
        va_start(ap, wheel1);
        ev->e.scrollX = va_arg(ap, int32_t);
        va_end(ap);
    }
    if(units == kCGScrollEventUnitPixel) {
        /* SLSEvent counts lines. */
        double perLine = source ? source->pixelsPerLine : 10;
        ev->e.scrollY /= perLine;
        ev->e.scrollX /= perLine;
    }
    if(_CGUsesSkyLight()) {
        SLSPoint p;
        if(SLSGetCursorPosition(&p) == kSLSErrorSuccess) {
            ev->e.x = p.x;
            ev->e.y = p.y;
        }
    }
    return ev;
}

CGEventRef CGEventCreateCopy(CGEventRef event) {
    if(event == NULL)
        return NULL;
    struct __CGEvent *copy = newEvent(event->source, event->e.type);
    if(copy) {
        CGEventSourceRef source = copy->source;
        memcpy(&copy->e, &event->e, sizeof(*copy) - sizeof(CFRuntimeBase));
        copy->source = source;
    }
    return copy;
}

CGEventSourceRef CGEventCreateSourceFromEvent(CGEventRef event) {
    if(event == NULL)
        return NULL;
    if(event->source)
        return (CGEventSourceRef)CFRetain(event->source);
    return CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
}

void CGEventSetSource(CGEventRef event, CGEventSourceRef source) {
    if(event == NULL)
        return;
    if(source)
        CFRetain(source);
    if(event->source)
        CFRelease(event->source);
    event->source = source;
}

CGEventType CGEventGetType(CGEventRef event) {
    return event ? event->e.type : kCGEventNull;
}

void CGEventSetType(CGEventRef event, CGEventType type) {
    if(event)
        event->e.type = type;
}

CGEventTimestamp CGEventGetTimestamp(CGEventRef event) {
    return event ? event->e.timestamp : 0;
}

void CGEventSetTimestamp(CGEventRef event, CGEventTimestamp timestamp) {
    if(event)
        event->e.timestamp = timestamp;
}

CGPoint CGEventGetLocation(CGEventRef event) {
    return event ? CGPointMake(event->e.x, event->e.y) : CGPointZero;
}

/* From the main display's bottom left, as AppKit counts. */
CGPoint CGEventGetUnflippedLocation(CGEventRef event) {
    if(event == NULL)
        return CGPointZero;
    CGFloat height = CGDisplayBounds(CGMainDisplayID()).size.height;
    return CGPointMake(event->e.x, height - event->e.y);
}

void CGEventSetLocation(CGEventRef event, CGPoint location) {
    if(event) {
        event->e.x = location.x;
        event->e.y = location.y;
    }
}

CGEventFlags CGEventGetFlags(CGEventRef event) {
    return event ? event->e.flags : 0;
}

void CGEventSetFlags(CGEventRef event, CGEventFlags flags) {
    if(event)
        event->e.flags = (uint32_t)flags;
}

void CGEventKeyboardGetUnicodeString(CGEventRef event, UniCharCount maxStringLength, UniCharCount *actualStringLength, UniChar *unicodeString) {
    UniCharCount n = 0;

    if(event != NULL && event->unicodeLength > 0) {
        n = event->unicodeLength;
        if(unicodeString)
            memcpy(unicodeString, event->unicode, (n < maxStringLength ? n : maxStringLength) * sizeof(UniChar));
    } else if(event != NULL && event->e.characters[0] != '\0') {
        CFStringRef s = CFStringCreateWithCString(NULL, event->e.characters, kCFStringEncodingUTF8);
        if(s) {
            n = CFStringGetLength(s);
            if(unicodeString)
                CFStringGetCharacters(s, CFRangeMake(0, n < maxStringLength ? n : maxStringLength), unicodeString);
            CFRelease(s);
        }
    }
    if(actualStringLength)
        *actualStringLength = n;
}

void CGEventKeyboardSetUnicodeString(CGEventRef event, UniCharCount stringLength, const UniChar *unicodeString) {
    if(event == NULL)
        return;
    if(stringLength > 8)
        stringLength = 8;
    memcpy(event->unicode, unicodeString, stringLength * sizeof(UniChar));
    event->unicodeLength = stringLength;
    /* What the event carries to the server and AppKit. */
    memset(event->e.characters, 0, sizeof(event->e.characters));
    CFStringRef s = CFStringCreateWithCharacters(NULL, unicodeString, stringLength);
    if(s) {
        CFStringGetCString(s, event->e.characters, sizeof(event->e.characters), kCFStringEncodingUTF8);
        memcpy(event->e.charactersIgnoringModifiers, event->e.characters, sizeof(event->e.characters));
        CFRelease(s);
    }
}

int64_t CGEventGetIntegerValueField(CGEventRef event, CGEventField field) {
    if(event == NULL)
        return 0;
    switch(field) {
        case kCGMouseEventClickState: return event->e.clickCount;
        case kCGMouseEventButtonNumber: return event->e.buttonNumber;
        case kCGMouseEventDeltaX: return (int64_t)event->e.dx;
        case kCGMouseEventDeltaY: return (int64_t)event->e.dy;
        case kCGKeyboardEventAutorepeat: return event->e.isRepeat;
        case kCGKeyboardEventKeycode: return event->e.keyCode;
        case kCGScrollWheelEventDeltaAxis1: return (int64_t)event->e.scrollY;
        case kCGScrollWheelEventDeltaAxis2: return (int64_t)event->e.scrollX;
        case kCGEventSourceUserData: return event->userData;
        case kCGEventSourceUnixProcessID: return event->sourcePID;
        case kCGEventTargetUnixProcessID: return event->targetPID;
        case kCGMouseEventWindowUnderMousePointer:
        case kCGMouseEventWindowUnderMousePointerThatCanHandleThisEvent:
            return event->e.window;
        default: return 0;
    }
}

void CGEventSetIntegerValueField(CGEventRef event, CGEventField field, int64_t value) {
    if(event == NULL)
        return;
    switch(field) {
        case kCGMouseEventClickState: event->e.clickCount = (int32_t)value; break;
        case kCGMouseEventButtonNumber: event->e.buttonNumber = (uint32_t)value; break;
        case kCGMouseEventDeltaX: event->e.dx = value; break;
        case kCGMouseEventDeltaY: event->e.dy = value; break;
        case kCGKeyboardEventAutorepeat: event->e.isRepeat = value != 0; break;
        case kCGKeyboardEventKeycode: event->e.keyCode = (uint16_t)value; break;
        case kCGScrollWheelEventDeltaAxis1: event->e.scrollY = value; break;
        case kCGScrollWheelEventDeltaAxis2: event->e.scrollX = value; break;
        case kCGEventSourceUserData: event->userData = value; break;
        case kCGEventTargetUnixProcessID: event->targetPID = value; break;
        default: break;
    }
}

double CGEventGetDoubleValueField(CGEventRef event, CGEventField field) {
    if(event == NULL)
        return 0;
    switch(field) {
        case kCGMouseEventPressure: return event->pressure;
        case kCGMouseEventDeltaX: return event->e.dx;
        case kCGMouseEventDeltaY: return event->e.dy;
        case kCGScrollWheelEventFixedPtDeltaAxis1: return event->e.scrollY;
        case kCGScrollWheelEventFixedPtDeltaAxis2: return event->e.scrollX;
        default: return (double)CGEventGetIntegerValueField(event, field);
    }
}

void CGEventSetDoubleValueField(CGEventRef event, CGEventField field, double value) {
    if(event == NULL)
        return;
    switch(field) {
        case kCGMouseEventPressure: event->pressure = value; break;
        case kCGMouseEventDeltaX: event->e.dx = value; break;
        case kCGMouseEventDeltaY: event->e.dy = value; break;
        case kCGScrollWheelEventFixedPtDeltaAxis1: event->e.scrollY = value; break;
        case kCGScrollWheelEventFixedPtDeltaAxis2: event->e.scrollX = value; break;
        default: CGEventSetIntegerValueField(event, field, (int64_t)value); break;
    }
}

/* ---- Event taps ---- */

/* A tap of this process: its CFMachPort's info, and the CGEventTapProxy
 * its callback is given. */
struct tap {
    struct tap *next;
    uint32_t tapID;
    CGEventTapLocation location;
    CGEventTapOptions options;
    CGEventTapCallBack callback;
    void *userInfo;
    mach_port_t port;
    bool enabled;
};

static pthread_mutex_t tapsLock = PTHREAD_MUTEX_INITIALIZER;
static struct tap *taps;

static struct tap *tapForPort(CFMachPortRef port) {
    mach_port_t name = port ? CFMachPortGetPort(port) : MACH_PORT_NULL;
    struct tap *t;

    pthread_mutex_lock(&tapsLock);
    for(t = taps; t != NULL && t->port != name; t = t->next)
        ;
    pthread_mutex_unlock(&tapsLock);
    return t;
}

/* An event from the server: given to the callback, and for a filtering
 * tap its answer sent back. */
static void tapCallBack(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    SLSTapEventMessage *m = (SLSTapEventMessage *)msg;
    struct tap *t = (struct tap *)info;
    mach_port_t reply = m->header.msgh_remote_port;

    if(size < (CFIndex)sizeof(*m) || m->header.msgh_id != kSLSTapEventMessageID)
        return;
    if(m->event.type == kSLSEventTapDisabledByTimeout)
        t->enabled = false;
    CGEventRef event = eventFromSLS(&m->event);
    CGEventRef result = t->callback((CGEventTapProxy)t, (CGEventType)m->event.type, event,
        t->userInfo);
    if(reply != MACH_PORT_NULL && MACH_MSGH_BITS_REMOTE(m->header.msgh_bits) ==
        MACH_MSG_TYPE_PORT_SEND_ONCE) {
        SLSTapReplyMessage r;
        memset(&r, 0, sizeof(r));
        r.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
        r.header.msgh_size = sizeof(r);
        r.header.msgh_remote_port = reply;
        r.header.msgh_id = kSLSTapReplyMessageID;
        r.drop = result == NULL;
        r.event = result ? result->e : m->event;
        r.event.timestamp = m->event.timestamp;	/* how the server matches it */
        mach_msg(&r.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(r), 0, MACH_PORT_NULL,
            0, MACH_PORT_NULL);
        /* The reply right is used: nothing is left for a later destroy. */
        m->header.msgh_remote_port = MACH_PORT_NULL;
        m->header.msgh_bits &= ~MACH_MSGH_BITS_REMOTE_MASK;
    }
    if(event)
        CFRelease(event);
}

CFMachPortRef CGEventTapCreate(CGEventTapLocation location, CGEventTapPlacement place, CGEventTapOptions options, CGEventMask eventsOfInterest, CGEventTapCallBack callback, void *userInfo) {
    mach_port_t port = MACH_PORT_NULL;
    struct tap *t;
    uint32_t tapID = 0;

    if(callback == NULL || !_CGUsesSkyLight())
        return NULL;
    if(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) != KERN_SUCCESS)
        return NULL;
    if((t = calloc(1, sizeof(*t))) == NULL ||
        SLSEventTapCreate(location, place, options, eventsOfInterest, port, &tapID) !=
        kSLSErrorSuccess) {
        free(t);
        mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
        return NULL;
    }
    t->tapID = tapID;
    t->location = location;
    t->options = options;
    t->callback = callback;
    t->userInfo = userInfo;
    t->port = port;
    t->enabled = true;

    CFMachPortContext context = { 0, t, NULL, NULL, NULL };
    Boolean shouldFree = false;
    CFMachPortRef machPort = CFMachPortCreateWithPort(kCFAllocatorDefault, port, tapCallBack,
        &context, &shouldFree);
    if(machPort == NULL) {
        SLSEventTapRelease(tapID);
        free(t);
        return NULL;
    }
    pthread_mutex_lock(&tapsLock);
    t->next = taps;
    taps = t;
    pthread_mutex_unlock(&tapsLock);
    return machPort;
}

CFMachPortRef CGEventTapCreateForPSN(void *processSerialNumber, CGEventTapPlacement place, CGEventTapOptions options, CGEventMask eventsOfInterest, CGEventTapCallBack callback, void *userInfo) {
    /* Per-process taps are not kept apart yet: a session tap. */
    return CGEventTapCreate(kCGAnnotatedSessionEventTap, place, options, eventsOfInterest,
        callback, userInfo);
}

void CGEventTapEnable(CFMachPortRef tap, bool enable) {
    struct tap *t = tapForPort(tap);
    if(t != NULL && SLSEventTapEnable(t->tapID, enable) == kSLSErrorSuccess)
        t->enabled = enable;
}

bool CGEventTapIsEnabled(CFMachPortRef tap) {
    struct tap *t = tapForPort(tap);
    return t != NULL && t->enabled;
}

/* From a tap's callback: the event enters again after this tap's
 * location. */
void CGEventTapPostEvent(CGEventTapProxy proxy, CGEventRef event) {
    struct tap *t = (struct tap *)proxy;
    if(t == NULL || event == NULL)
        return;
    SLSPostEvent(t->location + 1 > kCGAnnotatedSessionEventTap ? kCGAnnotatedSessionEventTap :
        t->location + 1, &event->e);
}

void CGEventPost(CGEventTapLocation tap, CGEventRef event) {
    if(event != NULL && _CGUsesSkyLight())
        SLSPostEvent(tap, &event->e);
}

void CGEventPostToPSN(void *processSerialNumber, CGEventRef event) {
    CGEventPost(kCGAnnotatedSessionEventTap, event);
}

CGError CGGetEventTapList(uint32_t maxNumberOfTaps, CGEventTapInformation *tapList, uint32_t *eventTapCount) {
    SLSEventTapInfo list[kSLSMaxEventTaps];
    uint32_t n = 0;

    if(eventTapCount == NULL)
        return kCGErrorIllegalArgument;
    if(!_CGUsesSkyLight()) {
        *eventTapCount = 0;
        return kCGErrorSuccess;
    }
    if(SLSGetEventTapList(kSLSMaxEventTaps, list, &n) != kSLSErrorSuccess)
        return kCGErrorFailure;
    if(tapList == NULL) {
        *eventTapCount = n;
        return kCGErrorSuccess;
    }
    if(n > maxNumberOfTaps)
        n = maxNumberOfTaps;
    for(uint32_t i = 0; i < n; i++) {
        tapList[i].eventTapID = list[i].tap;
        tapList[i].tapPoint = list[i].location;
        tapList[i].options = list[i].options;
        tapList[i].eventsOfInterest = list[i].mask;
        tapList[i].tappingProcess = list[i].tappingPID;
        tapList[i].processBeingTapped = list[i].processBeingTapped;
        tapList[i].enabled = list[i].enabled != 0;
        tapList[i].minUsecLatency = list[i].minUsecLatency;
        tapList[i].avgUsecLatency = list[i].avgUsecLatency;
        tapList[i].maxUsecLatency = list[i].maxUsecLatency;
    }
    *eventTapCount = n;
    return kCGErrorSuccess;
}

/* ---- Event sources ---- */

CFTypeID CGEventSourceGetTypeID(void) {
    pthread_once(&registerOnce, registerClasses);
    return sourceTypeID;
}

CGEventSourceRef CGEventSourceCreate(CGEventSourceStateID stateID) {
    pthread_once(&registerOnce, registerClasses);
    struct __CGEventSource *s = (struct __CGEventSource *)(uintptr_t)_CFRuntimeCreateInstance(
        kCFAllocatorDefault, sourceTypeID,
        sizeof(struct __CGEventSource) - sizeof(CFRuntimeBase), NULL);
    if(s == NULL)
        return NULL;
    s->state = stateID;
    s->keyboardType = 0;
    s->userData = 0;
    s->pixelsPerLine = 10;
    s->suppressionInterval = 0.25;
    for(int i = 0; i < kCGNumberOfEventSuppressionStates; i++)
        s->filter[i] = kCGEventFilterMaskPermitLocalMouseEvents |
            kCGEventFilterMaskPermitLocalKeyboardEvents | kCGEventFilterMaskPermitSystemDefinedEvents;
    return s;
}

CGEventSourceKeyboardType CGEventSourceGetKeyboardType(CGEventSourceRef source) {
    return source ? source->keyboardType : 0;
}

void CGEventSourceSetKeyboardType(CGEventSourceRef source, CGEventSourceKeyboardType keyboardType) {
    if(source)
        source->keyboardType = keyboardType;
}

CGEventSourceStateID CGEventSourceGetSourceStateID(CGEventSourceRef source) {
    return source ? source->state : kCGEventSourceStatePrivate;
}

/* SHORTCUT(WS3): the HID state (buttons, keys, flags, times since the
 * last events) is not asked of the server yet. */
bool CGEventSourceButtonState(CGEventSourceStateID stateID, CGMouseButton button) {
    return false;
}

bool CGEventSourceKeyState(CGEventSourceStateID stateID, CGKeyCode key) {
    return false;
}

CGEventFlags CGEventSourceFlagsState(CGEventSourceStateID stateID) {
    return 0;
}

CFTimeInterval CGEventSourceSecondsSinceLastEventType(CGEventSourceStateID stateID, CGEventType eventType) {
    return 0;
}

uint32_t CGEventSourceCounterForEventType(CGEventSourceStateID stateID, CGEventType eventType) {
    return 0;
}

int64_t CGEventSourceGetUserData(CGEventSourceRef source) {
    return source ? source->userData : 0;
}

void CGEventSourceSetUserData(CGEventSourceRef source, int64_t userData) {
    if(source)
        source->userData = userData;
}

CGEventFilterMask CGEventSourceGetLocalEventsFilterDuringSuppressionState(CGEventSourceRef source, CGEventSuppressionState state) {
    return (source && state < kCGNumberOfEventSuppressionStates) ? source->filter[state] : 0;
}

void CGEventSourceSetLocalEventsFilterDuringSuppressionState(CGEventSourceRef source, CGEventFilterMask filter, CGEventSuppressionState state) {
    if(source && state < kCGNumberOfEventSuppressionStates)
        source->filter[state] = filter;
}

CFTimeInterval CGEventSourceGetLocalEventsSuppressionInterval(CGEventSourceRef source) {
    return source ? source->suppressionInterval : 0;
}

void CGEventSourceSetLocalEventsSuppressionInterval(CGEventSourceRef source, CFTimeInterval seconds) {
    if(source)
        source->suppressionInterval = seconds;
}

double CGEventSourceGetPixelsPerLine(CGEventSourceRef source) {
    return source ? source->pixelsPerLine : 10;
}

void CGEventSourceSetPixelsPerLine(CGEventSourceRef source, double pixelsPerLine) {
    if(source && pixelsPerLine > 0)
        source->pixelsPerLine = pixelsPerLine;
}
