/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSMachPort: a Mach port as an NSPort. A message's components go as one
 * Mach message: each NSData as out-of-line memory, each NSPort as a send
 * right, the sender's port as the reply port and the msgid as the
 * message's id. Received, they come back as an NSPortMessage for the
 * delegate's handlePortMessage: (or the raw message for handleMachMessage:),
 * through a CFMachPort in the run loops the port is scheduled in. One
 * NSMachPort stands for each Mach port name in the process.
 */

#include <dispatch/dispatch.h>
#import <Foundation/NSMachPort.h>
#import <Foundation/NSPortMessage.h>
#import <Foundation/NSData.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSRunLoop-InputSource.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSException.h>
#import <CoreFoundation/CFMachPort.h>
#import <CoreFoundation/CFRunLoop.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>

@interface NSRunLoop (CFRunLoop)
- (CFRunLoopRef)getCFRunLoop;
@end

static NSMutableDictionary *portsByName;   // NSNumber (port name) -> NSMachPort

static NSMutableDictionary *allPorts(void) {
   static dispatch_once_t once;

   dispatch_once(&once,^{ portsByName=[NSMutableDictionary new]; });
   return portsByName;
}

@implementation NSMachPort {
   CFMachPortRef _cfPort;
   CFRunLoopSourceRef _cfSource;
}

+(NSPort *)portWithMachPort:(uint32_t)machPort {
   return [[self alloc] initWithMachPort:machPort];
}

+(NSPort *)portWithMachPort:(uint32_t)machPort options:(NSMachPortOptions)options {
   return [[self alloc] initWithMachPort:machPort options:options];
}

-init {
   mach_port_t port=MACH_PORT_NULL;

   if(mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE,&port)!=KERN_SUCCESS)
    return nil;
   if(mach_port_insert_right(mach_task_self(),port,port,MACH_MSG_TYPE_MAKE_SEND)!=KERN_SUCCESS){
    mach_port_mod_refs(mach_task_self(),port,MACH_PORT_RIGHT_RECEIVE,-1);
    return nil;
   }
   return [self initWithMachPort:port options:NSMachPortDeallocateSendRight|NSMachPortDeallocateReceiveRight];
}

-initWithMachPort:(uint32_t)machPort {
   return [self initWithMachPort:machPort options:NSMachPortDeallocateSendRight];
}

// The same object for the same port name: a reply port that arrives twice
// is one port, and its connection is found again.
-initWithMachPort:(uint32_t)machPort options:(NSMachPortOptions)options {
   NSNumber *key=[NSNumber numberWithUnsignedInt:machPort];

   @synchronized([NSMachPort class]){
    NSMachPort *existing=[allPorts() objectForKey:key];

    if(existing!=nil && existing->_valid)
     return existing;
    _machPort=machPort;
    _options=options;
    _valid=(machPort!=MACH_PORT_NULL);
    if(_valid)
     [allPorts() setObject:self forKey:key];
   }
   return self;
}

-(void)dealloc {
   if(_cfSource!=NULL)
    CFRelease(_cfSource);
   if(_cfPort!=NULL){
    CFMachPortInvalidate(_cfPort);
    CFRelease(_cfPort);
   }
}

-(uint32_t)machPort {
   return _machPort;
}

-(NSUInteger)hash {
   return _machPort;
}

-(BOOL)isEqual:(id)other {
   return [other isKindOfClass:[NSMachPort class]] && ((NSMachPort *)other)->_machPort==_machPort;
}

-(id)delegate {
   return _delegate;
}

-(void)setDelegate:(id)delegate {
   _delegate=delegate;
}

-(BOOL)isValid {
   return _valid;
}

-(void)invalidate {
   if(!_valid)
    return;
   _valid=NO;
   @synchronized([NSMachPort class]){
    [allPorts() removeObjectForKey:[NSNumber numberWithUnsignedInt:_machPort]];
   }
   if(_cfPort!=NULL)
    CFMachPortInvalidate(_cfPort);
   if(_options&NSMachPortDeallocateReceiveRight)
    mach_port_mod_refs(mach_task_self(),_machPort,MACH_PORT_RIGHT_RECEIVE,-1);
   if(_options&NSMachPortDeallocateSendRight)
    mach_port_deallocate(mach_task_self(),_machPort);
   [[NSNotificationCenter defaultCenter] postNotificationName:NSPortDidBecomeInvalidNotification object:self];
}

/* A received message: its components (the out-of-line memory as NSData,
 * now copied and freed; the port rights as NSMachPorts, now ours) for the
 * delegate. */
static void receive(CFMachPortRef cfPort,void *msg,CFIndex size,void *info) {
   NSMachPort         *self=(__bridge NSMachPort *)info;
   mach_msg_header_t  *header=(mach_msg_header_t *)msg;
   id                  delegate=self->_delegate;

   @autoreleasepool {
    if([delegate respondsToSelector:@selector(handleMachMessage:)]){
     [delegate handleMachMessage:msg];
     return;
    }

    NSMutableArray *components=[NSMutableArray array];
    char           *end=(char *)header+header->msgh_size;
    char           *next=(char *)(header+1);

    if(header->msgh_bits&MACH_MSGH_BITS_COMPLEX){
     mach_msg_body_t *body=(mach_msg_body_t *)next;
     mach_msg_size_t  i;

     next=(char *)(body+1);
     for(i=0;i<body->msgh_descriptor_count;i++){
      mach_msg_type_descriptor_t *any=(mach_msg_type_descriptor_t *)next;

      switch(any->type){
       case MACH_MSG_OOL_DESCRIPTOR: {
        mach_msg_ool_descriptor_t *ool=(mach_msg_ool_descriptor_t *)next;

        [components addObject:[NSData dataWithBytes:ool->address length:ool->size]];
        if(ool->size>0)
         vm_deallocate(mach_task_self(),(vm_address_t)ool->address,ool->size);
        next+=sizeof(*ool);
        break;
       }
       case MACH_MSG_PORT_DESCRIPTOR: {
        mach_msg_port_descriptor_t *port=(mach_msg_port_descriptor_t *)next;

        [components addObject:[NSMachPort portWithMachPort:port->name]];
        next+=sizeof(*port);
        break;
       }
       default:
        // Not one of ours: nothing more of it can be read.
        next=end;
        i=body->msgh_descriptor_count;
        break;
      }
     }
    }
    if(next<end)
     [components addObject:[NSData dataWithBytes:next length:end-next]];

    NSPort        *reply=(header->msgh_remote_port!=MACH_PORT_NULL)?[NSMachPort portWithMachPort:header->msgh_remote_port]:nil;
    NSPortMessage *message=[[NSPortMessage alloc] initWithSendPort:reply receivePort:self components:components];

    [message setMsgid:header->msgh_id];
    if([delegate respondsToSelector:@selector(handlePortMessage:)])
     [delegate handlePortMessage:message];
   }
}

-(void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
   if(!_valid)
    return;
   if(_cfPort==NULL){
    CFMachPortContext context={ 0,(__bridge void *)self,NULL,NULL,NULL };
    Boolean           shouldFree=false;

    _cfPort=CFMachPortCreateWithPort(kCFAllocatorDefault,_machPort,receive,&context,&shouldFree);
    if(_cfPort==NULL)
     return;
    _cfSource=CFMachPortCreateRunLoopSource(kCFAllocatorDefault,_cfPort,0);
   }
   CFRunLoopAddSource([runLoop getCFRunLoop],_cfSource,NSRunLoopCFMode(mode));
}

-(void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
   if(_cfSource!=NULL)
    CFRunLoopRemoveSource([runLoop getCFRunLoop],_cfSource,NSRunLoopCFMode(mode));
}

-(NSUInteger)reservedSpaceLength {
   return 0;
}

-(BOOL)sendBeforeDate:(NSDate *)date components:(NSMutableArray *)components from:(NSPort *)fromPort reserved:(NSUInteger)reserved {
   return [self sendBeforeDate:date msgid:0 components:components from:fromPort reserved:reserved];
}

// How a port right goes in a message: made from our receive right, or a
// copy of the send right we were given.
static mach_msg_type_name_t disposition(mach_port_t name) {
   mach_port_type_t type=0;

   if(mach_port_type(mach_task_self(),name,&type)==KERN_SUCCESS && (type&MACH_PORT_TYPE_RECEIVE))
    return MACH_MSG_TYPE_MAKE_SEND;
   return MACH_MSG_TYPE_COPY_SEND;
}

/* One Mach message: a descriptor for each component, the reply port made
 * a send right for the receiver. Sent with the time left until the date;
 * NO when it could not be sent then (or the port is dead). */
-(BOOL)sendBeforeDate:(NSDate *)date msgid:(NSUInteger)msgid components:(NSMutableArray *)components from:(NSPort *)fromPort reserved:(NSUInteger)reserved {
   NSUInteger          count=[components count],i;
   size_t              size=sizeof(mach_msg_header_t)+sizeof(mach_msg_body_t)+count*sizeof(mach_msg_ool_descriptor_t);
   mach_msg_header_t  *header=calloc(1,size);
   mach_msg_body_t    *body=(mach_msg_body_t *)(header+1);
   char               *next=(char *)(body+1);
   mach_port_t         reply=[fromPort isKindOfClass:[NSMachPort class]]?[(NSMachPort *)fromPort machPort]:MACH_PORT_NULL;
   NSTimeInterval      wait=(date!=nil)?[date timeIntervalSinceNow]:0;
   mach_msg_timeout_t  timeout=(wait<=0)?0:(wait>4000000)?4000000000U:(mach_msg_timeout_t)(wait*1000);
   kern_return_t       result;

   if(!_valid){
    free(header);
    return NO;
   }
   for(i=0;i<count;i++){
    id component=[components objectAtIndex:i];

    if([component isKindOfClass:[NSMachPort class]]){
     mach_msg_port_descriptor_t *port=(mach_msg_port_descriptor_t *)next;

     port->name=[(NSMachPort *)component machPort];
     port->disposition=disposition(port->name);
     port->type=MACH_MSG_PORT_DESCRIPTOR;
     next+=sizeof(*port);
    }
    else if([component isKindOfClass:[NSData class]]){
     mach_msg_ool_descriptor_t *ool=(mach_msg_ool_descriptor_t *)next;

     ool->address=(void *)[(NSData *)component bytes];
     ool->size=(mach_msg_size_t)[(NSData *)component length];
     ool->deallocate=FALSE;
     ool->copy=MACH_MSG_VIRTUAL_COPY;
     ool->type=MACH_MSG_OOL_DESCRIPTOR;
     next+=sizeof(*ool);
    }
    else {
     free(header);
     [NSException raise:NSInvalidArgumentException format:@"-[NSMachPort sendBeforeDate:...]: a component is neither data nor a port: %@",component];
    }
   }
   body->msgh_descriptor_count=(mach_msg_size_t)count;
   header->msgh_bits=MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,(reply!=MACH_PORT_NULL)?disposition(reply):0)|MACH_MSGH_BITS_COMPLEX;
   header->msgh_size=(mach_msg_size_t)(next-(char *)header);
   header->msgh_remote_port=_machPort;
   header->msgh_local_port=reply;
   header->msgh_id=(mach_msg_id_t)msgid;

   result=mach_msg(header,MACH_SEND_MSG|MACH_SEND_TIMEOUT,header->msgh_size,0,MACH_PORT_NULL,timeout,MACH_PORT_NULL);
   free(header);
   if(result==MACH_SEND_INVALID_DEST)
    [self invalidate];
   return result==MACH_MSG_SUCCESS;
}

@end
