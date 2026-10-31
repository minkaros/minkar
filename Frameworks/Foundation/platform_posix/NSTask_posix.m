/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * NSTask on POSIX: posix_spawn, and a dispatch source for the child's
 * exit.
 *
 * Minkar: this was fork() and execve(), with Objective-C messages in the
 * child (not safe in a process with threads); the child's standard input,
 * output and error were closed when not set, though the API has them
 * inherited; and every child of the process was reaped by the run loop
 * (wait3), so the status of a child some other code had started was lost.
 * Each task now reaps only its own child.
 */

#ifdef PLATFORM_IS_POSIX
#import "NSTask_posix.h"
#import "NSFileHandle_posix.h"
#import <Foundation/NSRaiseException.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

void waitForTaskChildProcess(void) {
}

static int descriptorOf(id handleOrPipe, BOOL reading) {
    if ([handleOrPipe isKindOfClass:[NSPipe class]])
        handleOrPipe = reading ? [handleOrPipe fileHandleForReading] : [handleOrPipe fileHandleForWriting];
    if ([handleOrPipe isKindOfClass:[NSFileHandle class]])
        return [(NSFileHandle *)handleOrPipe fileDescriptor];
    return -1;
}

@implementation NSTask_posix

-(void)dealloc {
    if (_exitSource != NULL) {
        dispatch_source_cancel(_exitSource);
        dispatch_release(_exitSource);
    }
    [_terminationHandler release];
    [_launchThread release];
    [super dealloc];
}

-(int)processIdentifier {
    return _processID;
}

-(void (^)(NSTask *))terminationHandler {
    @synchronized(self) {
        return [[_terminationHandler retain] autorelease];
    }
}

-(void)setTerminationHandler:(void (^)(NSTask *))handler {
    @synchronized(self) {
        [_terminationHandler release];
        _terminationHandler = [handler copy];
    }
}

-(NSQualityOfService)qualityOfService {
    return _qualityOfService;
}

-(void)setQualityOfService:(NSQualityOfService)qos {
    _qualityOfService = qos;
}

static NSError *launchError(NSString *path, int error) {
    NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:
        path ? path : @"", NSFilePathErrorKey,
        [NSError errorWithDomain:NSPOSIXErrorDomain code:error userInfo:nil], NSUnderlyingErrorKey, nil];
    NSInteger code = (error == ENOENT || error == ENOTDIR) ? NSFileNoSuchFileError
                   : (error == EACCES || error == EPERM) ? NSFileReadNoPermissionError : NSFileReadUnknownError;
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:info];
}

-(void)launch {
    NSError *error = nil;
    if (![self launchAndReturnError:&error])
        [NSException raise:NSInvalidArgumentException format:@"launch path not accessible: %@", launchPath];
}

-(BOOL)launchAndReturnError:(NSError **)error {
    if (_launched)
        [NSException raise:NSInvalidArgumentException format:@"task already launched"];
    if (launchPath == nil)
        [NSException raise:NSInvalidArgumentException format:@"must provide launch path"];

    const char *path = [launchPath fileSystemRepresentation];
    if (access(path, X_OK) != 0) {
        if (error) *error = launchError(launchPath, errno);
        return NO;
    }

    // Everything the child needs, made before it starts.
    NSArray *args = arguments ? arguments : [NSArray array];
    NSUInteger count = [args count];
    char **argv = calloc(count + 2, sizeof(char *));
    argv[0] = strdup(path);
    for (NSUInteger i = 0; i < count; i++)
        argv[i + 1] = strdup([[[args objectAtIndex:i] description] UTF8String]);

    char **envp = environ;
    char **ownEnv = NULL;
    if (environment != nil) {
        NSUInteger n = 0;
        ownEnv = calloc([environment count] + 1, sizeof(char *));
        for (NSString *key in environment) {
            id value = [environment objectForKey:key];
            ownEnv[n++] = strdup([[NSString stringWithFormat:@"%@=%@", key, value ? value : @""] UTF8String]);
        }
        envp = ownEnv;
    }

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attributes);

    // Standard input, output and error: the handle or pipe given, else the
    // parent's. Every other descriptor is closed in the child.
    int in = descriptorOf(standardInput, YES);
    int out = descriptorOf(standardOutput, NO);
    int err = descriptorOf(standardError, NO);
    if (in >= 0) posix_spawn_file_actions_adddup2(&actions, in, STDIN_FILENO);
    else posix_spawn_file_actions_addinherit_np(&actions, STDIN_FILENO);
    if (out >= 0) posix_spawn_file_actions_adddup2(&actions, out, STDOUT_FILENO);
    else posix_spawn_file_actions_addinherit_np(&actions, STDOUT_FILENO);
    if (err >= 0) posix_spawn_file_actions_adddup2(&actions, err, STDERR_FILENO);
    else posix_spawn_file_actions_addinherit_np(&actions, STDERR_FILENO);
    if (currentDirectoryPath != nil)
        posix_spawn_file_actions_addchdir_np(&actions, [currentDirectoryPath fileSystemRepresentation]);

    sigset_t all, none;
    sigfillset(&all);
    sigemptyset(&none);
    posix_spawnattr_setsigdefault(&attributes, &all);
    posix_spawnattr_setsigmask(&attributes, &none);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_CLOEXEC_DEFAULT);

    pid_t pid = 0;
    int result = posix_spawn(&pid, path, &actions, &attributes, argv, envp);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    for (NSUInteger i = 0; argv[i] != NULL; i++)
        free(argv[i]);
    free(argv);
    if (ownEnv != NULL) {
        for (NSUInteger i = 0; ownEnv[i] != NULL; i++)
            free(ownEnv[i]);
        free(ownEnv);
    }

    if (result != 0) {
        if (error) *error = launchError(launchPath, result);
        return NO;
    }

    _processID = pid;
    _launched = YES;
    _launchThread = [[NSThread currentThread] retain];

    // The child has its own copies of the pipes' far ends.
    if ([standardInput isKindOfClass:[NSPipe class]])
        [[standardInput fileHandleForReading] closeFile];
    if ([standardOutput isKindOfClass:[NSPipe class]])
        [[standardOutput fileHandleForWriting] closeFile];
    if ([standardError isKindOfClass:[NSPipe class]])
        [[standardError fileHandleForWriting] closeFile];

    // The task lives until its child has exited and been reported.
    [self retain];
    _exitSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid, DISPATCH_PROC_EXIT,
                                         dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
    dispatch_source_set_event_handler(_exitSource, ^{ [self _childExited]; });
    dispatch_resume(_exitSource);
    return YES;
}

-(void)_childExited {
    int status = 0;
    pid_t pid;
    do {
        pid = waitpid(_processID, &status, 0);
    } while (pid < 0 && errno == EINTR);

    @synchronized(self) {
        if (_exited)
            return;
        if (pid == _processID && WIFSIGNALED(status)) {
            _terminationReason = NSTaskTerminationReasonUncaughtSignal;
            _terminationStatus = WTERMSIG(status);
        } else {
            _terminationReason = NSTaskTerminationReasonExit;
            _terminationStatus = pid == _processID ? WEXITSTATUS(status) : -1;
        }
        _exited = YES;
    }
    dispatch_source_cancel(_exitSource);

    void (^handler)(NSTask *) = [self terminationHandler];
    if (handler != nil) {
        handler(self);
        [self setTerminationHandler:nil];
    }
    // The notification is posted on the thread that launched the task: for
    // the main thread, through dispatch's main queue (its run loop runs it).
    NSThread *thread = _launchThread;
    if (thread == [NSThread mainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self _postTermination]; });
    } else if (thread != nil && ![thread isFinished]) {
        @try {
            [self performSelector:@selector(_postTermination) onThread:thread withObject:nil waitUntilDone:NO];
        } @catch (NSException *e) {
            [self _postTermination];   // that thread has no run loop
        }
    } else {
        [self _postTermination];
    }
}

-(void)_postTermination {
    [[NSNotificationCenter defaultCenter] postNotificationName:NSTaskDidTerminateNotification object:self];
    [self release];
}

-(BOOL)isRunning {
    @synchronized(self) {
        return _launched && !_exited;
    }
}

-(void)_signal:(int)sig {
    if (!_launched)
        [NSException raise:NSInvalidArgumentException format:@"task not launched"];
    if ([self isRunning])
        kill(_processID, sig);
}

-(void)terminate {
    [self _signal:SIGTERM];
}

-(void)interrupt {
    [self _signal:SIGINT];
}

-(BOOL)suspend {
    if (![self isRunning])
        return NO;
    if (_suspendCount++ == 0)
        kill(_processID, SIGSTOP);
    return YES;
}

-(BOOL)resume {
    if (![self isRunning] || _suspendCount == 0)
        return NO;
    if (--_suspendCount == 0)
        kill(_processID, SIGCONT);
    return YES;
}

-(int)terminationStatus {
    if (!_launched)
        [NSException raise:NSInvalidArgumentException format:@"task not launched"];
    if ([self isRunning])
        [NSException raise:NSInvalidArgumentException format:@"task still running"];
    return _terminationStatus;
}

-(NSTaskTerminationReason)terminationReason {
    if (!_launched)
        [NSException raise:NSInvalidArgumentException format:@"task not launched"];
    if ([self isRunning])
        [NSException raise:NSInvalidArgumentException format:@"task still running"];
    return _terminationReason;
}

// The run loop of the calling thread runs meanwhile, as the API has it.
-(void)waitUntilExit {
    if (!_launched)
        [NSException raise:NSInvalidArgumentException format:@"task not launched"];
    while ([self isRunning]) {
        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        [pool release];
    }
}

-(void)blockAndWaitUntilExit {
    while ([self isRunning])
        usleep(10000);
}

@end
#endif
