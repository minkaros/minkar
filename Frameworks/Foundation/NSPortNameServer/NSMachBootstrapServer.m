/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSMachBootstrapServer: port names in launchd's bootstrap namespace, the
 * system's default name server. A name registered here is found by every
 * process of the session (a host other than the local one finds nothing).
 */

#include <dispatch/dispatch.h>
#import <Foundation/NSPortNameServer.h>
#import <Foundation/NSMachPort.h>
#import <Foundation/NSString.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

@implementation NSMachBootstrapServer

+(id)sharedInstance {
   static NSMachBootstrapServer *shared;
   static dispatch_once_t        once;

   dispatch_once(&once,^{ shared=[[NSMachBootstrapServer alloc] init]; });
   return shared;
}

static BOOL localHost(NSString *host) {
   return host==nil || [host length]==0 || [host isEqualToString:@"localhost"];
}

-(NSPort *)portForName:(NSString *)name {
   return [self portForName:name host:nil];
}

-(NSPort *)portForName:(NSString *)name host:(NSString *)host {
   mach_port_t port=MACH_PORT_NULL;

   if(name==nil || !localHost(host) || [name length]>=sizeof(name_t))
    return nil;
   if(bootstrap_look_up(bootstrap_port,(char *)[name UTF8String],&port)!=KERN_SUCCESS || port==MACH_PORT_NULL)
    return nil;
   return [NSMachPort portWithMachPort:port];
}

-(BOOL)registerPort:(NSPort *)port name:(NSString *)name {
   if(![port isKindOfClass:[NSMachPort class]] || name==nil || [name length]>=sizeof(name_t))
    return NO;
   return bootstrap_register(bootstrap_port,(char *)[name UTF8String],[(NSMachPort *)port machPort])==KERN_SUCCESS;
}

// The bootstrap namespace has no call to take a name back: the name goes
// when the port's receive right does.
-(BOOL)removePortForName:(NSString *)name {
   return NO;
}

-(NSPort *)servicePortWithName:(NSString *)name {
   mach_port_t port=MACH_PORT_NULL;

   if(name==nil || [name length]>=sizeof(name_t))
    return nil;
   if(bootstrap_check_in(bootstrap_port,(char *)[name UTF8String],&port)!=KERN_SUCCESS)
    return nil;
   return [NSMachPort portWithMachPort:port options:NSMachPortDeallocateReceiveRight];
}

@end
