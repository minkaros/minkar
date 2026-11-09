/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The process's Apple event handlers, by event class and ID (either may be
 * typeWildCard), and sending: an event addressed to this process goes to
 * its handler, with a reply event for the answer.
 */

#include "AE.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define paramErr (-50)
#define memFullErr (-108)
#define procNotFound (-600)
/* The current process's serial number: {0, kCurrentProcess}. */
#define kCurrentProcess 2

typedef struct Handler {
    AEEventClass eventClass;
    AEEventID eventID;
    AEEventHandlerUPP handler;
    SRefCon refcon;
} Handler;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Handler *sHandlers;
static long sCount, sCapacity;

AEEventHandlerUPP NewAEEventHandlerUPP(AEEventHandlerProcPtr userRoutine) {
    return userRoutine;
}

void DisposeAEEventHandlerUPP(AEEventHandlerUPP userUPP) {
}

OSErr InvokeAEEventHandlerUPP(const AppleEvent *theAppleEvent, AppleEvent *reply, SRefCon handlerRefcon, AEEventHandlerUPP userUPP) {
    return userUPP(theAppleEvent, reply, handlerRefcon);
}

static long findExact(AEEventClass c, AEEventID i) {
    for (long n = 0; n < sCount; n++)
        if (sHandlers[n].eventClass == c && sHandlers[n].eventID == i)
            return n;
    return -1;
}

OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler,
                            SRefCon handlerRefcon, Boolean isSysHandler) {
    if (handler == NULL)
        return paramErr;
    pthread_mutex_lock(&sLock);
    long at = findExact(theAEEventClass, theAEEventID);
    if (at < 0) {
        if (sCount == sCapacity) {
            long n = sCapacity ? sCapacity * 2 : 8;
            Handler *grown = (Handler *)realloc(sHandlers, (size_t)n * sizeof(Handler));
            if (grown == NULL) {
                pthread_mutex_unlock(&sLock);
                return memFullErr;
            }
            sHandlers = grown;
            sCapacity = n;
        }
        at = sCount++;
    }
    sHandlers[at] = (Handler){ theAEEventClass, theAEEventID, handler, handlerRefcon };
    pthread_mutex_unlock(&sLock);
    return noErr;
}

OSErr AERemoveEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler, Boolean isSysHandler) {
    OSErr err = errAEHandlerNotFound;
    pthread_mutex_lock(&sLock);
    long at = findExact(theAEEventClass, theAEEventID);
    if (at >= 0 && (handler == NULL || sHandlers[at].handler == handler)) {
        memmove(sHandlers + at, sHandlers + at + 1, (size_t)(sCount - at - 1) * sizeof(Handler));
        sCount--;
        err = noErr;
    }
    pthread_mutex_unlock(&sLock);
    return err;
}

/* The exact handler, else one for the class and any ID, any class and the
   ID, or anything. */
static Boolean lookup(AEEventClass c, AEEventID i, Handler *out) {
    const AEEventClass classes[4] = { c, c, typeWildCard, typeWildCard };
    const AEEventID ids[4] = { i, typeWildCard, i, typeWildCard };
    Boolean found = false;
    pthread_mutex_lock(&sLock);
    for (int k = 0; k < 4 && !found; k++) {
        long at = findExact(classes[k], ids[k]);
        if (at >= 0) {
            *out = sHandlers[at];
            found = true;
        }
    }
    pthread_mutex_unlock(&sLock);
    return found;
}

OSErr AEGetEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP *handler,
                        SRefCon *handlerRefcon, Boolean isSysHandler) {
    Handler h;
    pthread_mutex_lock(&sLock);
    long at = findExact(theAEEventClass, theAEEventID);
    if (at >= 0)
        h = sHandlers[at];
    pthread_mutex_unlock(&sLock);
    if (at < 0)
        return errAEHandlerNotFound;
    if (handler != NULL)
        *handler = h.handler;
    if (handlerRefcon != NULL)
        *handlerRefcon = h.refcon;
    return noErr;
}

OSErr AEDispatchAppleEvent(const AppleEvent *theAppleEvent, AppleEvent *reply) {
    AEEventClass c = 0;
    AEEventID i = 0;
    Handler h;

    if (AEGetAttributePtr(theAppleEvent, keyEventClassAttr, typeType, NULL, &c, sizeof(c), NULL) != noErr ||
        AEGetAttributePtr(theAppleEvent, keyEventIDAttr, typeType, NULL, &i, sizeof(i), NULL) != noErr)
        return errAENotAppleEvent;
    if (!lookup(c, i, &h))
        return errAEEventNotHandled;
    return h.handler(theAppleEvent, reply, h.refcon);
}

/* Whether the address is this process: its pid, the current process
   serial number, or its bundle identifier. */
static Boolean addressedHere(const AppleEvent *event) {
    AEDesc target;
    Boolean here = false;

    if (AEGetAttributeDesc(event, keyAddressAttr, typeWildCard, &target) != noErr)
        return true;
    if (target.descriptorType == typeKernelProcessID) {
        pid_t pid = 0;
        AEGetDescData(&target, &pid, sizeof(pid));
        here = (pid == getpid());
    } else if (target.descriptorType == typeProcessSerialNumber) {
        UInt32 psn[2] = { 0, 0 };
        AEGetDescData(&target, psn, sizeof(psn));
        here = (psn[0] == 0 && psn[1] == kCurrentProcess);
    } else if (target.descriptorType == typeApplicationBundleID) {
        CFBundleRef main = CFBundleGetMainBundle();
        CFStringRef mine = main != NULL ? CFBundleGetIdentifier(main) : NULL;
        Size n = AEGetDescDataSize(&target);
        char *bytes = (char *)malloc((size_t)n + 1);
        if (bytes != NULL && mine != NULL) {
            AEGetDescData(&target, bytes, n);
            bytes[n] = 0;
            CFStringRef wanted = CFStringCreateWithCString(NULL, bytes, kCFStringEncodingUTF8);
            here = wanted != NULL && CFStringCompare(wanted, mine, 0) == kCFCompareEqualTo;
            if (wanted != NULL)
                CFRelease(wanted);
        }
        free(bytes);
    } else if (target.descriptorType == typeNull) {
        here = true;
    }
    AEDisposeDesc(&target);
    return here;
}

OSStatus AESendMessage(const AppleEvent *event, AppleEvent *reply, AESendMode sendMode, long timeOutInTicks) {
    AppleEvent answer;
    AEReturnID returnID = 0;
    AETransactionID transactionID = 0;
    OSErr err;

    if (reply != NULL)
        AEInitializeDesc(reply);
    if (event == NULL || event->descriptorType != typeAppleEvent)
        return errAENotAppleEvent;
    if (!addressedHere(event))
        return procNotFound;
    AEGetAttributePtr(event, keyReturnIDAttr, typeSInt16, NULL, &returnID, sizeof(returnID), NULL);
    AEGetAttributePtr(event, keyTransactionIDAttr, typeSInt32, NULL, &transactionID, sizeof(transactionID), NULL);
    AECreateAppleEvent(kCoreEventClass, kAEAnswer, NULL, returnID, transactionID, &answer);
    err = AEDispatchAppleEvent(event, &answer);
    if (err != noErr) {
        SInt32 number = err;
        if (AESizeOfParam(&answer, keyErrorNumber, NULL, NULL) != noErr)
            AEPutParamPtr(&answer, keyErrorNumber, typeSInt32, &number, sizeof(number));
    }
    if ((sendMode & kAEWaitReply) == kAEWaitReply && reply != NULL)
        *reply = answer;
    else
        AEDisposeDesc(&answer);
    return (err == errAEEventNotHandled) ? err : noErr;
}
