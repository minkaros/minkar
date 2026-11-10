// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// GLib's default main context on the main thread's run loops. Each time
// the run loop is about to sleep, GLib's sources are prepared and queried
// for the file descriptors to watch and how long to wait; a helper thread
// polls them (with a pipe of its own to be interrupted), and wakes the run
// loop when one is ready or the time is up. The main thread then checks
// and dispatches GLib's sources. When the run loop wakes for something
// else (an event, a timer), the poll is cut short at the next sleep, so
// sources the main thread added meanwhile are seen: GLib does not wake its
// own thread for those.
//
// Foundation's NSRunLoop (what AppKit runs) is not built on CFRunLoop
// here: it gets an input source on a pipe the poll thread writes to when
// it is done, asked for its events before each wait (the "about to sleep"
// step). CFRunLoop gets a source and an observer, for
// programs that run it.
//
// GLib is not run by the run loop while one of its callbacks is running (a
// modal loop started from one does not dispatch GLib's sources); the API's
// synchronous calls made from one run it nested (_WebGLibRunLoopWait).

#import "WebGLibRunLoop.h"
#import <Foundation/NSSelectInputSource.h>
#import <Foundation/NSSocket.h>

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum { PollIdle, PollRunning, PollDone } PollState;

static GMainContext *context;
static CFRunLoopRef mainRunLoop;
static CFRunLoopSourceRef pollDoneSource;
static gboolean dispatching;

/* Shared with the poll thread, under lock: the state, and while it is
   PollRunning, pfds and pollTimeout belong to the poll thread. */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static PollState pollState = PollIdle;
static struct pollfd *pfds;      /* GLib's descriptors, then the wake pipe */
static gint pollTimeout;
static int wakePipe[2];
/* Written by the poll thread when it is done (NSRunLoop's input source). */
static int donePipe[2];

/* The main thread's: the iteration being run. */
static GPollFD *fds;
static gint fdsCapacity;
static gint nfds;
static gint maxPriority;

static void *pollThread(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&lock);
    for (;;) {
        while (pollState != PollRunning)
            pthread_cond_wait(&changed, &lock);
        gint count = nfds + 1, timeout = pollTimeout;
        pthread_mutex_unlock(&lock);

        int result;
        do
            result = poll(pfds, count, timeout);
        while (result < 0 && errno == EINTR);
        if (pfds[count - 1].revents) {
            char drain[64];
            while (read(wakePipe[0], drain, sizeof(drain)) > 0) { }
        }

        pthread_mutex_lock(&lock);
        pollState = PollDone;
        pthread_cond_broadcast(&changed);
        char byte = 0;
        (void)!write(donePipe[1], &byte, 1);
        CFRunLoopSourceSignal(pollDoneSource);
        CFRunLoopWakeUp(mainRunLoop);
    }
    return NULL;
}

/* A finished poll's results to GLib, and what is ready dispatched. */
static gboolean finishIteration(void)
{
    for (gint i = 0; i < nfds; i++)
        fds[i].revents = pfds[i].revents;
    pthread_mutex_lock(&lock);
    pollState = PollIdle;
    pthread_mutex_unlock(&lock);

    if (!g_main_context_check(context, maxPriority, fds, nfds))
        return FALSE;
    dispatching = TRUE;
    g_main_context_dispatch(context);
    dispatching = FALSE;
    return TRUE;
}

/* Prepare GLib's sources and give their descriptors to the poll thread. */
static void startIteration(void)
{
    gboolean ready = g_main_context_prepare(context, &maxPriority);
    gint timeout = -1;
    while ((nfds = g_main_context_query(context, maxPriority, &timeout, fds, fdsCapacity)) > fdsCapacity) {
        fdsCapacity = nfds;
        fds = g_renew(GPollFD, fds, fdsCapacity);
    }
    if (ready)
        timeout = 0;

    pfds = realloc(pfds, sizeof(struct pollfd) * (size_t)(nfds + 1));
    for (gint i = 0; i < nfds; i++) {
        pfds[i].fd = fds[i].fd;
        pfds[i].events = (short)fds[i].events;
        pfds[i].revents = 0;
    }
    pfds[nfds] = (struct pollfd){ .fd = wakePipe[0], .events = POLLIN };

    pthread_mutex_lock(&lock);
    pollTimeout = timeout;
    pollState = PollRunning;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
}

/* The poll thread's wake-up: its results, once ready. */
static void pollDonePerform(void *info)
{
    (void)info;
    if (dispatching)
        return;
    pthread_mutex_lock(&lock);
    PollState state = pollState;
    pthread_mutex_unlock(&lock);
    if (state == PollDone)
        finishIteration();
}

/* Before the run loop sleeps: a poll still running is cut short (the main
   thread may have changed GLib's sources), and a new one started. YES when
   GLib's sources were dispatched. */
static BOOL aboutToWait(void)
{
    if (dispatching)
        return NO;

    pthread_mutex_lock(&lock);
    if (pollState == PollRunning) {
        char byte = 0;
        (void)!write(wakePipe[1], &byte, 1);
        while (pollState == PollRunning)
            pthread_cond_wait(&changed, &lock);
    }
    PollState state = pollState;
    pthread_mutex_unlock(&lock);

    BOOL dispatched = NO;
    if (state == PollDone)
        dispatched = finishIteration();
    startIteration();
    return dispatched;
}

static void beforeWaiting(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    (void)observer; (void)activity; (void)info;
    aboutToWait();
}

/* NSRunLoop's part: its waits watch donePipe, and before each one it asks
   its select sources for their events (none, if theirs is not ready):
   that is the "about to sleep" step. */
@interface _WebGLibInputSource : NSSelectInputSource
@end

@implementation _WebGLibInputSource
- (NSUInteger)processImmediateEvents:(NSUInteger)selectEvent {
    NSUInteger handled = 0;
    if (selectEvent & NSSelectReadEvent) {
        char drain[64];
        while (read(donePipe[0], drain, sizeof(drain)) > 0) { }
        handled = NSSelectReadEvent;
    }
    aboutToWait();
    return handled;
}
@end


void _WebGLibRunLoopStart(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        context = g_main_context_default();
        if (!g_main_context_acquire(context))
            NSLog(@"WebKit: GLib's main context is owned by another thread");
        mainRunLoop = CFRunLoopGetMain();

        if (pipe(wakePipe) != 0 || pipe(donePipe) != 0)
            abort();
        for (int i = 0; i < 2; i++) {
            int both[2] = { wakePipe[i], donePipe[i] };
            for (int j = 0; j < 2; j++) {
                fcntl(both[j], F_SETFL, fcntl(both[j], F_GETFL) | O_NONBLOCK);
                fcntl(both[j], F_SETFD, FD_CLOEXEC);
            }
        }

        CFRunLoopSourceContext sourceContext = { .perform = pollDonePerform };
        pollDoneSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
        CFRunLoopAddSource(mainRunLoop, pollDoneSource, kCFRunLoopCommonModes);
        CFRunLoopObserverRef observer = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting, true, 0,
                                                                beforeWaiting, NULL);
        CFRunLoopAddObserver(mainRunLoop, observer, kCFRunLoopCommonModes);

        NSSocket *socket = [[NSSocket alloc] initWithFileDescriptor:donePipe[0]];
        _WebGLibInputSource *input = [[_WebGLibInputSource alloc] initWithSocket:socket];
        [input setSelectEventMask:NSSelectReadEvent];
        NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
        for (NSString *mode in @[ NSDefaultRunLoopMode, @"NSModalPanelRunLoopMode", @"NSEventTrackingRunLoopMode" ])
            [runLoop addInputSource:input forMode:mode];

        pthread_t thread;
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&thread, &attributes, pollThread, NULL) != 0)
            abort();
        pthread_attr_destroy(&attributes);

        /* The first iteration now: the run loop may not sleep soon. */
        startIteration();
    });
}

static gboolean stopWaiting(gpointer data)
{
    (void)data;
    return G_SOURCE_REMOVE;
}

BOOL _WebGLibRunLoopWait(BOOL (^done)(void), NSTimeInterval timeout)
{
    _WebGLibRunLoopStart();
    NSDate *limit = [NSDate dateWithTimeIntervalSinceNow:timeout];
    while (!done()) {
        if (limit.timeIntervalSinceNow <= 0)
            return NO;
        if (dispatching) {
            /* Nested in GLib's dispatch: one of its iterations, woken at
               least every 50 ms to look at done again. */
            GSource *tick = g_timeout_source_new(50);
            g_source_set_callback(tick, stopWaiting, NULL, NULL);
            g_source_attach(tick, context);
            g_main_context_iteration(context, TRUE);
            g_source_destroy(tick);
            g_source_unref(tick);
        } else {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                     beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }
    }
    return YES;
}
