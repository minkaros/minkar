/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SMAppService (API 22), ahead of Minkar's API because apps ask for it: an
 * app registers itself, a helper of its bundle, or one of its agents or
 * daemons as a launchd job. A registration is a property list in the
 * user's (or, for a daemon, the system's) LaunchAgents/LaunchDaemons, as
 * launchd reads them; a helper, agent or daemon is also started now.
 * SHORTCUT(Atlas): loginwindow does not yet load the user's agents at
 * login, so a registration starts again only from the next session that
 * does.
 */

#import <ServiceManagement/SMAppService.h>
#import <ServiceManagement/ServiceManagement.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

typedef NS_ENUM(NSInteger, SMServiceKind) {
    SMServiceKindMainApp,
    SMServiceKindLoginItem,
    SMServiceKindAgent,
    SMServiceKindDaemon,
};

@interface SMAppService () {
    SMServiceKind _kind;
    NSString *_name;   // a helper's identifier, or an agent's or daemon's property list
}
@end

static NSError *serviceError(NSInteger code, NSString *reason) {
    return [NSError errorWithDomain:(__bridge NSString *)kSMErrorDomainFramework code:code
                           userInfo:@{ NSLocalizedFailureReasonErrorKey: reason }];
}

@implementation SMAppService

- (instancetype)initWithKind:(SMServiceKind)kind name:(NSString *)name {
    if ((self = [super init]) != nil) {
        _kind = kind;
        _name = [name copy];
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

+ (SMAppService *)mainAppService {
    return [[[self alloc] initWithKind:SMServiceKindMainApp name:nil] autorelease];
}

+ (SMAppService *)loginItemServiceWithIdentifier:(NSString *)identifier {
    return [[[self alloc] initWithKind:SMServiceKindLoginItem name:identifier] autorelease];
}

+ (SMAppService *)agentServiceWithPlistName:(NSString *)plistName {
    return [[[self alloc] initWithKind:SMServiceKindAgent name:plistName] autorelease];
}

+ (SMAppService *)daemonServiceWithPlistName:(NSString *)plistName {
    return [[[self alloc] initWithKind:SMServiceKindDaemon name:plistName] autorelease];
}

#pragma mark - The job

/* The job this service stands for, from the main bundle; nil when the
   bundle has no such helper or property list. */
- (NSDictionary *)job {
    NSBundle *main = [NSBundle mainBundle];
    NSString *contents = [[main bundlePath] stringByAppendingPathComponent:@"Contents"];

    switch (_kind) {
    case SMServiceKindMainApp:
        if ([main bundleIdentifier] == nil || [main executablePath] == nil)
            return nil;
        return @{ @"Label": [main bundleIdentifier], @"ProgramArguments": @[ [main executablePath] ],
                  @"RunAtLoad": @YES, @"LimitLoadToSessionType": @"Aqua" };

    case SMServiceKindLoginItem: {
        NSString *items = [contents stringByAppendingPathComponent:@"Library/LoginItems"];

        for (NSString *entry in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:items error:NULL]) {
            NSBundle *helper = [NSBundle bundleWithPath:[items stringByAppendingPathComponent:entry]];

            if ([[helper bundleIdentifier] isEqualToString:_name] && [helper executablePath] != nil)
                return @{ @"Label": _name, @"ProgramArguments": @[ [helper executablePath] ],
                          @"RunAtLoad": @YES, @"LimitLoadToSessionType": @"Aqua" };
        }
        return nil;
    }

    case SMServiceKindAgent:
    case SMServiceKindDaemon: {
        NSString *folder = (_kind == SMServiceKindAgent) ? @"Library/LaunchAgents" : @"Library/LaunchDaemons";
        NSString *path = [[contents stringByAppendingPathComponent:folder] stringByAppendingPathComponent:_name];
        NSMutableDictionary *job = [NSMutableDictionary dictionaryWithContentsOfFile:path];
        NSString *program = [job objectForKey:@"BundleProgram"];

        if ([job objectForKey:@"Label"] == nil)
            return nil;
        // BundleProgram is relative to the app's bundle.
        if (program != nil) {
            NSString *absolute = [[main bundlePath] stringByAppendingPathComponent:program];
            NSMutableArray *arguments = [[[job objectForKey:@"ProgramArguments"] mutableCopy] autorelease];

            [job removeObjectForKey:@"BundleProgram"];
            [job setObject:absolute forKey:@"Program"];
            if ([arguments count] > 0)
                [arguments replaceObjectAtIndex:0 withObject:absolute];
            else
                arguments = [NSMutableArray arrayWithObject:absolute];
            [job setObject:arguments forKey:@"ProgramArguments"];
        }
        return job;
    }
    }
    return nil;
}

/* Where the registration lives: the user's LaunchAgents, or the system's
   LaunchDaemons. */
- (NSString *)registrationPathForLabel:(NSString *)label {
    NSString *folder = (_kind == SMServiceKindDaemon) ? @"/Library/LaunchDaemons"
        : [NSHomeDirectory() stringByAppendingPathComponent:@"Library/LaunchAgents"];

    return [[folder stringByAppendingPathComponent:label] stringByAppendingPathExtension:@"plist"];
}

#pragma mark - Registering

- (BOOL)registerAndReturnError:(NSError **)error {
    NSDictionary *job = [self job];
    NSString *label = [job objectForKey:@"Label"];
    NSString *path;
    NSError *problem = nil;

    if (job == nil) {
        if (error != NULL)
            *error = serviceError(kSMErrorJobPlistNotFound, @"The app's bundle has no such service.");
        return NO;
    }
    path = [self registrationPathForLabel:label];
    if (![[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent]
                                   withIntermediateDirectories:YES attributes:nil error:&problem] ||
        ![job writeToFile:path atomically:YES]) {
        if (error != NULL)
            *error = (_kind == SMServiceKindDaemon && geteuid() != 0)
                ? serviceError(kSMErrorAuthorizationFailure, @"Registering a daemon needs an administrator.")
                : (problem ?: serviceError(kSMErrorInternalFailure, @"The registration could not be written."));
        return NO;
    }
    // The running app is not started again; its helpers and agents are.
    if (_kind != SMServiceKindMainApp) {
        CFErrorRef cfError = NULL;

        // The session type is for launchctl at login; launchd takes no
        // such key in a job submitted to it (EINVAL).
        NSMutableDictionary *submitted = [[job mutableCopy] autorelease];

        [submitted removeObjectForKey:@"LimitLoadToSessionType"];
        SMJobRemove(kSMDomainUserLaunchd, (__bridge CFStringRef)label, NULL, false, NULL);
        if (!SMJobSubmit(_kind == SMServiceKindDaemon ? kSMDomainSystemLaunchd : kSMDomainUserLaunchd,
                         (__bridge CFDictionaryRef)submitted, NULL, &cfError)) {
            if (error != NULL)
                *error = [(NSError *)cfError autorelease];
            else if (cfError != NULL)
                CFRelease(cfError);
            return NO;
        }
    }
    return YES;
}

- (BOOL)unregisterAndReturnError:(NSError **)error {
    NSDictionary *job = [self job];
    NSString *label = [job objectForKey:@"Label"];
    NSString *path;

    if (job == nil) {
        if (error != NULL)
            *error = serviceError(kSMErrorJobPlistNotFound, @"The app's bundle has no such service.");
        return NO;
    }
    path = [self registrationPathForLabel:label];
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        if (error != NULL)
            *error = serviceError(kSMErrorJobNotFound, @"The service is not registered.");
        return NO;
    }
    if (![[NSFileManager defaultManager] removeItemAtPath:path error:error])
        return NO;
    if (_kind != SMServiceKindMainApp)
        SMJobRemove(kSMDomainUserLaunchd, (__bridge CFStringRef)label, NULL, false, NULL);
    return YES;
}

- (void)unregisterWithCompletionHandler:(void (^)(NSError *error))handler {
    NSError *error = nil;

    [self unregisterAndReturnError:&error];
    if (handler != nil)
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            handler(error);
        });
}

#pragma mark - Status

- (SMAppServiceStatus)status {
    NSDictionary *job = [self job];

    if (job == nil)
        return SMAppServiceStatusNotFound;
    return [[NSFileManager defaultManager] fileExistsAtPath:[self registrationPathForLabel:[job objectForKey:@"Label"]]]
        ? SMAppServiceStatusEnabled : SMAppServiceStatusNotRegistered;
}

/* A helper registered the older way (SMLoginItemSetEnabled): a launchd job
   labeled with its identifier. */
+ (SMAppServiceStatus)statusForLegacyURL:(NSURL *)url {
    NSString *identifier = [[NSBundle bundleWithURL:url] bundleIdentifier];
    CFDictionaryRef job;

    if (identifier == nil)
        return SMAppServiceStatusNotFound;
    job = SMJobCopyDictionary(kSMDomainUserLaunchd, (__bridge CFStringRef)identifier);
    if (job == NULL)
        return SMAppServiceStatusNotRegistered;
    CFRelease(job);
    return SMAppServiceStatusEnabled;
}

/* Login items are set in the system's Preferences. */
+ (void)openSystemSettingsLoginItems {
    char *arguments[] = { "/usr/bin/open", "-b", "org.minkar.systempreferences", NULL };
    pid_t pid;

    if (posix_spawn(&pid, arguments[0], NULL, NULL, arguments, environ) == 0)
        waitpid(pid, NULL, 0);
}

@end
