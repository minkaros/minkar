/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSConnection: Distributed Objects over Mach ports (the message format is
 * in NSConnectionInternal.h).
 *
 * A connection is a receive port and a send port. A server registers its
 * receive port under a name; each client that sends to it gets a child
 * connection on the server's side (the server's receive port, the
 * client's port to reply to), with the server's root object. Each
 * connection keeps the objects it has sent by reference (by id) and the
 * proxies for those it was sent. A message to a proxy is sent as a
 * request, and the caller waits for its reply running its run loop in
 * NSConnectionReplyMode, where requests that come meanwhile (calls back)
 * are served too. The receive port is scheduled in the run loops the
 * connection is added to, in its request modes and the reply mode.
 */

#import <Foundation/NSNull.h>
#include <dispatch/dispatch.h>
#import <Foundation/NSConnection.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSMachPort.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSPortMessage.h>
#import <Foundation/NSPortNameServer.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSObjCRuntime.h>
#import "NSConnectionInternal.h"
#include <objc/runtime.h>
#include <string.h>
#include <stdlib.h>

NSString * const NSConnectionReplyMode=@"NSConnectionReplyMode";
NSString * const NSConnectionDidDieNotification=@"NSConnectionDidDieNotification";
NSString * const NSConnectionDidInitializeNotification=@"NSConnectionDidInitializeNotification";
NSString * const NSFailedAuthenticationException=@"NSFailedAuthenticationException";
NSString * const NSPortTimeoutException=@"NSPortTimeoutException";
NSString * const NSInvalidSendPortException=@"NSInvalidSendPortException";
NSString * const NSInvalidReceivePortException=@"NSInvalidReceivePortException";
NSString * const NSPortSendException=@"NSPortSendException";
NSString * const NSPortReceiveException=@"NSPortReceiveException";

@interface NSObject (NSConnectionDelegate)
-(BOOL)connection:(NSConnection *)parent shouldMakeNewConnection:(NSConnection *)child;
-(BOOL)makeNewConnection:(NSConnection *)child sender:(NSConnection *)parent;
@end

// NSDODebug=1 in the environment logs each message sent and received.
static BOOL debugging(void) {
   static int on=-1;

   if(on<0)
    on=(getenv("NSDODebug")!=NULL);
   return on;
}

static NSMutableArray *allConnections(void) {
   static NSMutableArray *connections;
   static dispatch_once_t once;

   dispatch_once(&once,^{ connections=[NSMutableArray new]; });
   return connections;
}

/* A method's types, element by element: its qualifiers (r n N o O R V)
 * and its type, the return's first. */
typedef struct {
   NSString *qualifiers;
   NSString *type;
} Element;

static NSArray *elementsOfTypes(const char *types) {
   NSMutableArray *result=[NSMutableArray array];
   const char     *next=types;

   while(next!=NULL && *next!='\0'){
    const char *start=next;

    while(*next!='\0' && strchr("rnNoORVA",*next)!=NULL)
     next++;
    NSString   *qualifiers=[[NSString alloc] initWithBytes:start length:next-start encoding:NSASCIIStringEncoding];
    const char *typeStart=next;
    const char *end=NSGetSizeAndAlignment(typeStart,NULL,NULL);

    if(end==typeStart)
     break;
    [result addObject:@[ qualifiers,[[NSString alloc] initWithBytes:typeStart length:end-typeStart encoding:NSASCIIStringEncoding] ]];
    next=end;
    while((*next>='0' && *next<='9') || *next=='+' || *next=='-')
     next++;
   }
   return result;
}

static NSString *qualifiersOf(NSArray *element) { return [element objectAtIndex:0]; }
static const char *typeOf(NSArray *element) { return [[element objectAtIndex:1] UTF8String]; }

// The types without qualifiers, for an NSMethodSignature.
static NSMethodSignature *signatureForElements(NSArray *elements) {
   NSMutableString *types=[NSMutableString string];

   for(NSArray *element in elements)
    [types appendString:[element objectAtIndex:1]];
   return [NSMethodSignature signatureWithObjCTypes:[types UTF8String]];
}

// A pointer passed in, out or both: const pointers only in, out pointers
// only out, others both ways.
static BOOL isPointer(const char *type) { return type[0]=='^'; }
static BOOL sendsIn(NSString *qualifiers) { return [qualifiers rangeOfString:@"o"].location==NSNotFound; }
static BOOL comesBack(NSString *qualifiers) {
   return [qualifiers rangeOfString:@"r"].location==NSNotFound && [qualifiers rangeOfString:@"n"].location==NSNotFound;
}
static BOOL isBycopy(NSString *qualifiers) { return [qualifiers rangeOfString:@"O"].location!=NSNotFound; }
static BOOL isByref(NSString *qualifiers) { return [qualifiers rangeOfString:@"R"].location!=NSNotFound; }
static BOOL isOneway(NSString *qualifiers) { return [qualifiers rangeOfString:@"V"].location!=NSNotFound; }

static NSUInteger sizeOfType(const char *type) {
   NSUInteger size=0;

   NSGetSizeAndAlignment(type,&size,NULL);
   return size;
}

@implementation NSConnection {
   NSMutableDictionary *_localByID;      // NSNumber -> object sent by reference
   NSMutableDictionary *_idByLocal;      // NSValue (the object's address) -> NSNumber
   NSMutableDictionary *_proxyByID;      // NSNumber -> NSDistantObject
   NSMutableDictionary *_replies;        // sequence -> NSPortCoder positioned at the reply's body
   NSMutableDictionary *_typesBySelector;
   NSMutableArray      *_runLoops;
   id                   _rootObject;
   NSConnection        *_parent;
   uint32_t             _nextID;
   uint32_t             _nextSequence;
}

+(NSArray *)allConnections {
   @synchronized(allConnections()){
    return [allConnections() copy];
   }
}

// One for each thread, as on the reference platform.
+(NSConnection *)defaultConnection {
   NSMutableDictionary *thread=[[NSThread currentThread] threadDictionary];
   NSConnection        *result=[thread objectForKey:@"NSDefaultConnection"];

   if(result==nil){
    result=[[NSConnection alloc] init];
    [thread setObject:result forKey:@"NSDefaultConnection"];
   }
   return result;
}

+currentConversation {
   return nil;
}

+(NSConnection *)_existingWithReceivePort:(NSPort *)receivePort sendPort:(NSPort *)sendPort {
   @synchronized(allConnections()){
    for(NSConnection *connection in allConnections())
     if(connection->_isValid && [connection->_receivePort isEqual:receivePort] && [connection->_sendPort isEqual:sendPort])
      return connection;
   }
   return nil;
}

-init {
   NSPort *port=[NSMachPort port];

   return [self initWithReceivePort:port sendPort:port];
}

// The connection there is between the ports, or a new one. A connection
// that has its own receive port is added to the current run loop.
-initWithReceivePort:(NSPort *)receivePort sendPort:(NSPort *)sendPort {
   if(receivePort==nil)
    receivePort=[NSMachPort port];
   if(sendPort==nil)
    sendPort=receivePort;

   NSConnection *existing=[NSConnection _existingWithReceivePort:receivePort sendPort:sendPort];

   if(existing!=nil)
    return existing;

   _receivePort=receivePort;
   _sendPort=sendPort;
   _isValid=YES;
   _replyTimeout=1e10;
   _requestTimeout=1e10;
   _requestModes=[NSMutableArray arrayWithObject:NSDefaultRunLoopMode];
   _statistics=[NSMutableDictionary dictionary];
   _localByID=[NSMutableDictionary dictionary];
   _idByLocal=[NSMutableDictionary dictionary];
   _proxyByID=[NSMutableDictionary dictionary];
   _replies=[NSMutableDictionary dictionary];
   _typesBySelector=[NSMutableDictionary dictionary];
   _runLoops=[NSMutableArray array];
   _nextID=1;
   _nextSequence=1;

   @synchronized(allConnections()){
    [allConnections() addObject:self];
   }
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_portDidBecomeInvalid:) name:NSPortDidBecomeInvalidNotification object:_sendPort];
   if(_receivePort!=_sendPort)
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_portDidBecomeInvalid:) name:NSPortDidBecomeInvalidNotification object:_receivePort];

   if([_receivePort delegate]==nil){
    [_receivePort setDelegate:self];
    [self addRunLoop:[NSRunLoop currentRunLoop]];
   }
   [[NSNotificationCenter defaultCenter] postNotificationName:NSConnectionDidInitializeNotification object:self];
   return self;
}

+(NSConnection *)connectionWithReceivePort:(NSPort *)receivePort sendPort:(NSPort *)sendPort {
   return [[self alloc] initWithReceivePort:receivePort sendPort:sendPort];
}

// A client's connection to the port registered under the name, with a
// receive port of its own for the replies: the one it made before for
// that port, if it is still valid.
+(NSConnection *)connectionWithRegisteredName:(NSString *)name host:(NSString *)hostName usingNameServer:(NSPortNameServer *)nameServer {
   NSPort *port=[(nameServer!=nil?nameServer:[NSPortNameServer systemDefaultPortNameServer]) portForName:name host:hostName];

   if(port==nil)
    return nil;
   @synchronized(allConnections()){
    for(NSConnection *connection in allConnections())
     if(connection->_isValid && connection->_parent==nil && [connection->_sendPort isEqual:port] && ![connection->_receivePort isEqual:port])
      return connection;
   }
   return [self connectionWithReceivePort:[NSMachPort port] sendPort:port];
}

+(NSConnection *)connectionWithRegisteredName:(NSString *)name host:(NSString *)hostName {
   return [self connectionWithRegisteredName:name host:hostName usingNameServer:[NSPortNameServer systemDefaultPortNameServer]];
}

+(NSDistantObject *)rootProxyForConnectionWithRegisteredName:(NSString *)name host:(NSString *)hostName usingNameServer:(NSPortNameServer *)nameServer {
   return [[self connectionWithRegisteredName:name host:hostName usingNameServer:nameServer] rootProxy];
}

+(NSDistantObject *)rootProxyForConnectionWithRegisteredName:(NSString *)name host:(NSString *)hostName {
   return [[self connectionWithRegisteredName:name host:hostName] rootProxy];
}

-(void)dealloc {
   [[NSNotificationCenter defaultCenter] removeObserver:self];
}

-delegate { return _delegate; }
-(void)setDelegate:delegate { _delegate=delegate; }
-(BOOL)isValid { return _isValid; }
-(BOOL)independentConversationQueueing { return _independentConversationQueueing; }
-(void)setIndependentConversationQueueing:(BOOL)flag { _independentConversationQueueing=flag; }
-(BOOL)multipleThreadsEnabled { return _multipleThreadsEnabled; }
-(void)enableMultipleThreads { _multipleThreadsEnabled=YES; }
-(NSTimeInterval)replyTimeout { return _replyTimeout; }
-(void)setReplyTimeout:(NSTimeInterval)seconds { _replyTimeout=seconds; }
-(NSTimeInterval)requestTimeout { return _requestTimeout; }
-(void)setRequestTimeout:(NSTimeInterval)seconds { _requestTimeout=seconds; }
-(NSPort *)sendPort { return _sendPort; }
-(NSPort *)receivePort { return _receivePort; }
-(NSArray *)requestModes { return [_requestModes copy]; }
-(NSDictionary *)statistics { return [_statistics copy]; }

-rootObject {
   return (_parent!=nil)?[_parent rootObject]:_rootObject;
}

-(void)setRootObject:rootObject {
   _rootObject=rootObject;
}

-(NSArray *)localObjects {
   return [_localByID allValues];
}

-(NSArray *)remoteObjects {
   return [_proxyByID allValues];
}

-(void)_scheduleInRunLoop:(NSRunLoop *)runLoop {
   for(NSString *mode in _requestModes)
    [_receivePort scheduleInRunLoop:runLoop forMode:mode];
   [_receivePort scheduleInRunLoop:runLoop forMode:NSConnectionReplyMode];
}

-(void)addRunLoop:(NSRunLoop *)runLoop {
   if([_runLoops indexOfObjectIdenticalTo:runLoop]!=NSNotFound)
    return;
   [_runLoops addObject:runLoop];
   [self _scheduleInRunLoop:runLoop];
}

-(void)removeRunLoop:(NSRunLoop *)runLoop {
   if([_runLoops indexOfObjectIdenticalTo:runLoop]==NSNotFound)
    return;
   for(NSString *mode in _requestModes)
    [_receivePort removeFromRunLoop:runLoop forMode:mode];
   [_receivePort removeFromRunLoop:runLoop forMode:NSConnectionReplyMode];
   [_runLoops removeObjectIdenticalTo:runLoop];
}

-(void)addRequestMode:(NSString *)mode {
   if([_requestModes containsObject:mode])
    return;
   [_requestModes addObject:mode];
   for(NSRunLoop *runLoop in _runLoops)
    [_receivePort scheduleInRunLoop:runLoop forMode:mode];
}

-(void)removeRequestMode:(NSString *)mode {
   if(![_requestModes containsObject:mode])
    return;
   [_requestModes removeObject:mode];
   for(NSRunLoop *runLoop in _runLoops)
    [_receivePort removeFromRunLoop:runLoop forMode:mode];
}

-(void)_runForever {
   @autoreleasepool {
    [self addRunLoop:[NSRunLoop currentRunLoop]];
   }
   while(_isValid){
    @autoreleasepool {
     [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
    }
   }
}

-(void)runInNewThread {
   for(NSRunLoop *runLoop in [_runLoops copy])
    [self removeRunLoop:runLoop];
   [NSThread detachNewThreadSelector:@selector(_runForever) toTarget:self withObject:nil];
}

-(BOOL)registerName:(NSString *)name withNameServer:(NSPortNameServer *)nameServer {
   if(name==nil)
    return NO;
   if(![(nameServer!=nil?nameServer:[NSPortNameServer systemDefaultPortNameServer]) registerPort:_receivePort name:name])
    return NO;
   [self addRunLoop:[NSRunLoop currentRunLoop]];
   return YES;
}

-(BOOL)registerName:(NSString *)name {
   return [self registerName:name withNameServer:[NSPortNameServer systemDefaultPortNameServer]];
}

-(void)invalidate {
   if(!_isValid)
    return;
   _isValid=NO;
   @synchronized(allConnections()){
    [allConnections() removeObjectIdenticalTo:self];
   }
   if([_receivePort delegate]==self){
    for(NSRunLoop *runLoop in [_runLoops copy])
     [self removeRunLoop:runLoop];
   }
   [[NSNotificationCenter defaultCenter] postNotificationName:NSConnectionDidDieNotification object:self];
}

-(void)_portDidBecomeInvalid:(NSNotification *)note {
   [self invalidate];
}

/* Objects by reference. */

-(uint32_t)_exportLocalObject:(id)object {
   NSValue  *key=[NSValue valueWithPointer:(__bridge const void *)object];
   NSNumber *wireID=[_idByLocal objectForKey:key];

   if(wireID==nil){
    wireID=[NSNumber numberWithUnsignedInt:_nextID++];
    [_idByLocal setObject:wireID forKey:key];
    [_localByID setObject:object forKey:wireID];
   }
   return [wireID unsignedIntValue];
}

-(id)_localObjectForWireID:(uint32_t)wireID {
   return [_localByID objectForKey:[NSNumber numberWithUnsignedInt:wireID]];
}

-(NSDistantObject *)_proxyForWireID:(uint32_t)wireID {
   NSNumber        *key=[NSNumber numberWithUnsignedInt:wireID];
   NSDistantObject *proxy=[_proxyByID objectForKey:key];

   if(proxy==nil){
    proxy=[[NSDistantObject alloc] initWithWireID:wireID connection:self];
    [_proxyByID setObject:proxy forKey:key];
   }
   return proxy;
}

/* Sending and waiting. */

-(void)_send:(NSPortCoder *)coder {
   if(!_isValid)
    [NSException raise:NSInvalidSendPortException format:@"NSConnection %p is no longer valid",self];

   NSMutableArray *components=[[coder encodedComponents] mutableCopy];

   if(debugging())
    NSLog(@"NSConnection %p sends %@ (%lu ports)",self,[components objectAtIndex:0],(unsigned long)[components count]-1);

   if(![_sendPort sendBeforeDate:[NSDate dateWithTimeIntervalSinceNow:_requestTimeout] msgid:0 components:components from:_receivePort reserved:0]){
    if(![_sendPort isValid]){
     [self invalidate];
     [NSException raise:NSInvalidSendPortException format:@"NSConnection: the other side is gone"];
    }
    [NSException raise:NSPortTimeoutException format:@"NSConnection: the request could not be sent in %g seconds",_requestTimeout];
   }
}

-(NSPortCoder *)_newMessage:(uint32_t)kind sequence:(uint32_t)sequence {
   NSPortCoder *coder=[[NSPortCoder alloc] initForSendingWithConnection:self];

   [coder encodeUInt32:kind];
   [coder encodeUInt32:sequence];
   return coder;
}

// The reply's body: the run loop runs in the reply mode until it comes,
// serving the requests that come meanwhile.
-(NSPortCoder *)_waitForReply:(uint32_t)sequence {
   NSNumber   *key=[NSNumber numberWithUnsignedInt:sequence];
   NSDate     *limit=[NSDate dateWithTimeIntervalSinceNow:_replyTimeout];
   NSRunLoop  *runLoop=[NSRunLoop currentRunLoop];
   NSConnection *owner=([_receivePort delegate]==self || _parent==nil)?self:_parent;

   if([owner->_runLoops indexOfObjectIdenticalTo:runLoop]==NSNotFound)
    [_receivePort scheduleInRunLoop:runLoop forMode:NSConnectionReplyMode];

   NSPortCoder *reply;

   while((reply=[_replies objectForKey:key])==nil){
    if(!_isValid)
     [NSException raise:NSInvalidReceivePortException format:@"NSConnection: the connection died before its reply came"];
    if([limit timeIntervalSinceNow]<=0)
     [NSException raise:NSPortTimeoutException format:@"NSConnection: no reply in %g seconds",_replyTimeout];
    @autoreleasepool {
     [runLoop runMode:NSConnectionReplyMode beforeDate:limit];
    }
   }
   [_replies removeObjectForKey:key];
   return reply;
}

-(void)_raiseForReply:(NSPortCoder *)reply {
   uint32_t status=[reply decodeUInt32];

   if(status==NSDOReplyException){
    NSString *name=[reply decodeString];
    NSString *reason=[reply decodeString];

    [[NSException exceptionWithName:(name!=nil)?name:NSGenericException reason:reason userInfo:nil] raise];
   }
   if(status==NSDOReplyNoTarget)
    [NSException raise:NSInvalidArgumentException format:@"NSConnection: the object is no longer there"];
}

-(NSDistantObject *)rootProxy {
   uint32_t     sequence=_nextSequence++;
   NSPortCoder *request=[self _newMessage:NSDOMessageRoot sequence:sequence];

   @try {
    [self _send:request];
    NSPortCoder *reply=[self _waitForReply:sequence];

    [self _raiseForReply:reply];
    id root=[reply decodeObject];

    return root;
   }
   @catch(NSException *exception){
    if([[exception name] isEqualToString:NSInvalidSendPortException] || [[exception name] isEqualToString:NSPortTimeoutException])
     return nil;
    @throw;
   }
}

/* Proxies' messages. */

-(NSMethodSignature *)_methodSignatureForSelector:(SEL)selector proxy:(NSDistantObject *)proxy {
   NSString *name=NSStringFromSelector(selector);
   NSString *types=[_typesBySelector objectForKey:name];
   Protocol *protocol=[(id)proxy _protocol];

   if(types==nil && protocol!=nil){
    struct objc_method_description description=protocol_getMethodDescription(protocol,selector,YES,YES);

    if(description.types==NULL)
     description=protocol_getMethodDescription(protocol,selector,NO,YES);
    if(description.types!=NULL)
     types=[NSString stringWithUTF8String:description.types];
   }
   if(types==nil){
    uint32_t     sequence=_nextSequence++;
    NSPortCoder *request=[self _newMessage:NSDOMessageSignature sequence:sequence];

    [request encodeUInt32:[proxy _wireID]];
    [request encodeCString:sel_getName(selector)];
    [self _send:request];
    NSPortCoder *reply=[self _waitForReply:sequence];

    [self _raiseForReply:reply];
    types=[reply decodeString];
   }
   if(types==nil)
    return nil;
   [_typesBySelector setObject:types forKey:name];
   return signatureForElements(elementsOfTypes([types UTF8String]));
}

/* An argument out: objects by copy or reference as qualified, pointers as
 * whether they are there and, when they go in, what they point to. */
static void encodeArgument(NSPortCoder *coder,NSArray *element,void *address) {
   NSString   *qualifiers=qualifiersOf(element);
   const char *type=typeOf(element);

   [coder setBycopy:isBycopy(qualifiers) byref:isByref(qualifiers)];
   if(type[0]=='@')
    [coder encodeObject:*(__unsafe_unretained id *)address];
   else if(isPointer(type)){
    void *pointer=*(void **)address;

    [coder encodeUInt32:(pointer!=NULL)];
    if(pointer!=NULL && sendsIn(qualifiers)){
     if(type[1]=='@')
      [coder encodeObject:*(__unsafe_unretained id *)pointer];
     else
      [coder encodeValueOfObjCType:type+1 at:pointer];
    }
   }
   else
    [coder encodeValueOfObjCType:type at:address];
   [coder setBycopy:NO byref:NO];
}

-(void)_forwardInvocation:(NSInvocation *)invocation forProxy:(NSDistantObject *)proxy {
   NSMethodSignature *signature=[invocation methodSignature];
   SEL                selector=[invocation selector];
   NSString          *types=[_typesBySelector objectForKey:NSStringFromSelector(selector)];
   NSUInteger         count=[signature numberOfArguments],i;

   if(types==nil){
    NSMutableString *plain=[NSMutableString string]; [plain appendString:[NSString stringWithUTF8String:[signature methodReturnType]]];

    for(i=0;i<count;i++)
     [plain appendString:[NSString stringWithUTF8String:[signature getArgumentTypeAtIndex:i]]];
    types=plain;
   }

   NSArray     *elements=elementsOfTypes([types UTF8String]);
   NSArray     *returned=[elements objectAtIndex:0];
   BOOL         oneway=isOneway(qualifiersOf(returned));
   uint32_t     sequence=_nextSequence++;
   NSPortCoder *request=[self _newMessage:NSDOMessageRequest sequence:sequence];

   [request encodeUInt32:[proxy _wireID]];
   [request encodeUInt32:oneway?NSDORequestOneway:0];
   [request encodeCString:sel_getName(selector)];
   [request encodeCString:[types UTF8String]];
   // An argument's element is the next one: the return's is first.
   for(i=2;i<count && i+1<[elements count];i++){
    NSUInteger size=sizeOfType(typeOf([elements objectAtIndex:i+1]));
    char       buffer[size>0?size:1];

    [invocation getArgument:buffer atIndex:i];
    encodeArgument(request,[elements objectAtIndex:i+1],buffer);
   }
   [self _send:request];
   if(oneway)
    return;

   NSPortCoder *reply=[self _waitForReply:sequence];

   [self _raiseForReply:reply];

   const char *returnType=typeOf(returned);

   if(returnType[0]=='@'){
    __autoreleasing id object=[reply decodeObject];
    __unsafe_unretained id value=object;

    [invocation setReturnValue:&value];
   }
   else if(returnType[0]!='v'){
    NSUInteger size=[signature methodReturnLength];
    char       buffer[size>0?size:1];

    [reply decodeValueOfObjCType:returnType at:buffer];
    [invocation setReturnValue:buffer];
   }

   // What out and inout pointers point to, as the method left it.
   for(i=2;i<count && i+1<[elements count];i++){
    NSArray    *element=[elements objectAtIndex:i+1];
    const char *type=typeOf(element);
    void       *pointer;

    if(!isPointer(type) || !comesBack(qualifiersOf(element)))
     continue;
    [invocation getArgument:&pointer atIndex:i];
    if(pointer==NULL)
     continue;
    if(type[1]=='@'){
     __autoreleasing id object=[reply decodeObject];

     *(__unsafe_unretained id *)pointer=object;
    }
    else
     [reply decodeValueOfObjCType:type+1 at:pointer];
   }
}

/* Received messages. */

// The connection a message is for: this one, or the child made for the
// port it is to be answered on.
-(NSConnection *)_connectionForReplyPort:(NSPort *)replyPort {
   if(replyPort==nil || [replyPort isEqual:_sendPort])
    return self;

   NSConnection *child=[NSConnection _existingWithReceivePort:_receivePort sendPort:replyPort];

   if(child!=nil)
    return child;
   child=[[NSConnection alloc] initWithReceivePort:_receivePort sendPort:replyPort];
   child->_parent=self;
   [child->_requestModes setArray:_requestModes];
   child->_replyTimeout=_replyTimeout;
   child->_requestTimeout=_requestTimeout;
   if([_delegate respondsToSelector:@selector(connection:shouldMakeNewConnection:)] && ![_delegate connection:self shouldMakeNewConnection:child]){
    [child invalidate];
    return nil;
   }
   if([_delegate respondsToSelector:@selector(makeNewConnection:sender:)] && ![_delegate makeNewConnection:child sender:self]){
    [child invalidate];
    return nil;
   }
   return child;
}

-(void)handlePortMessage:(NSPortMessage *)message {
   NSConnection *connection=[self _connectionForReplyPort:[message sendPort]];

   if(connection!=nil)
    [connection _handleComponents:[message components]];
}

-(void)_replyValue:(uint32_t)sequence body:(void (^)(NSPortCoder *))body {
   NSPortCoder *reply=[self _newMessage:NSDOMessageReply sequence:sequence];

   [reply encodeUInt32:NSDOReplyValue];
   if(body!=nil)
    body(reply);
   [self _send:reply];
}

-(void)_replyStatus:(uint32_t)status sequence:(uint32_t)sequence exception:(NSException *)exception {
   NSPortCoder *reply=[self _newMessage:NSDOMessageReply sequence:sequence];

   [reply encodeUInt32:status];
   if(status==NSDOReplyException){
    [reply encodeCString:[[exception name] UTF8String]];
    [reply encodeCString:[[exception reason] UTF8String]];
   }
   [self _send:reply];
}

-(void)_handleComponents:(NSArray *)components {
   NSPortCoder *coder=[[NSPortCoder alloc] initForReceivingWithConnection:self components:components];
   uint32_t     kind,sequence;

   if(debugging())
    NSLog(@"NSConnection %p receives %@",self,components);

   @try {
    kind=[coder decodeUInt32];
    sequence=[coder decodeUInt32];
   }
   @catch(NSException *exception){
    return;   // not a Distributed Objects message
   }

   switch(kind){
    case NSDOMessageReply:
     [_replies setObject:coder forKey:[NSNumber numberWithUnsignedInt:sequence]];
     break;

    case NSDOMessageRoot: {
     id root=[self rootObject];

     @try {
      [self _replyValue:sequence body:^(NSPortCoder *reply){ [reply encodeObject:root]; }];
     }
     @catch(NSException *exception){
     }
     break;
    }

    case NSDOMessageSignature: {
     uint32_t  wireID=[coder decodeUInt32];
     NSString *name=[coder decodeString];
     id        target=(wireID==0)?[self rootObject]:[self _localObjectForWireID:wireID];
     SEL       selector=NSSelectorFromString(name);
     NSString *types=nil;

     if(target!=nil){
      Method method=class_getInstanceMethod(object_getClass(target),selector);

      if(method!=NULL && method_getTypeEncoding(method)!=NULL)
       types=[NSString stringWithUTF8String:method_getTypeEncoding(method)];
      else {
       NSMethodSignature *signature=[target methodSignatureForSelector:selector];

       if(signature!=nil){
        NSMutableString *plain=[NSMutableString string]; [plain appendString:[NSString stringWithUTF8String:[signature methodReturnType]]];
        NSUInteger       i;

        for(i=0;i<[signature numberOfArguments];i++)
         [plain appendString:[NSString stringWithUTF8String:[signature getArgumentTypeAtIndex:i]]];
        types=plain;
       }
      }
     }
     @try {
      if(target==nil)
       [self _replyStatus:NSDOReplyNoTarget sequence:sequence exception:nil];
      else if(types==nil)
       [self _replyStatus:NSDOReplyException sequence:sequence exception:[NSException exceptionWithName:NSInvalidArgumentException reason:[NSString stringWithFormat:@"%@ does not respond to %@",[target class],name] userInfo:nil]];
      else
       [self _replyValue:sequence body:^(NSPortCoder *reply){ [reply encodeCString:[types UTF8String]]; }];
     }
     @catch(NSException *exception){
     }
     break;
    }

    case NSDOMessageRequest:
     [self _handleRequest:coder sequence:sequence];
     break;
   }
}

-(void)_handleRequest:(NSPortCoder *)coder sequence:(uint32_t)sequence {
   uint32_t        wireID=0,flags=0;
   NSString       *name=nil,*types=nil;
   NSArray        *elements=nil;
   id              target=nil;
   NSInvocation   *invocation=nil;
   NSMutableArray *kept=[NSMutableArray array];        // the objects arguments point to
   NSMutableArray *pointees=[NSMutableArray array];    // NSMutableData for each pointer argument, or NSNull
   NSException    *raised=nil;

   @try {
    wireID=[coder decodeUInt32];
    flags=[coder decodeUInt32];
    name=[coder decodeString];
    types=[coder decodeString];
    elements=elementsOfTypes([types UTF8String]);
    target=(wireID==0)?[self rootObject]:[self _localObjectForWireID:wireID];
    if(target==nil){
     if(!(flags&NSDORequestOneway))
      [self _replyStatus:NSDOReplyNoTarget sequence:sequence exception:nil];
     return;
    }

    NSMethodSignature *signature=signatureForElements(elements);
    NSUInteger         i;

    invocation=[NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:target];
    [invocation setSelector:NSSelectorFromString(name)];
    for(i=2;i+1<[elements count];i++){
     NSArray    *element=[elements objectAtIndex:i+1];
     const char *type=typeOf(element);

     if(type[0]=='@'){
      id object=[coder decodeObject];
      __unsafe_unretained id value=object;

      if(object!=nil)
       [kept addObject:object];
      [invocation setArgument:&value atIndex:i];
      [pointees addObject:[NSNull null]];
     }
     else if(isPointer(type)){
      NSMutableData *pointee=nil;
      void          *pointer=NULL;

      if([coder decodeUInt32]){
       pointee=[NSMutableData dataWithLength:MAX(sizeOfType(type+1),sizeof(void *))];
       pointer=[pointee mutableBytes];
       if(sendsIn(qualifiersOf(element))){
        if(type[1]=='@'){
         id object=[coder decodeObject];

         if(object!=nil)
          [kept addObject:object];
         *(__unsafe_unretained id *)pointer=object;
        }
        else
         [coder decodeValueOfObjCType:type+1 at:pointer];
       }
      }
      [invocation setArgument:&pointer atIndex:i];
      [pointees addObject:(pointee!=nil)?pointee:[NSNull null]];
     }
     else {
      NSUInteger size=sizeOfType(type);
      char       buffer[size>0?size:1];

      [coder decodeValueOfObjCType:type at:buffer];
      [invocation setArgument:buffer atIndex:i];
      [pointees addObject:[NSNull null]];
     }
    }
    [invocation invoke];
   }
   @catch(NSException *exception){
    raised=exception;
    if(debugging())
     NSLog(@"NSConnection %p: %@ raised %@: %@",self,name,[exception name],[exception reason]);
   }

   if(flags&NSDORequestOneway)
    return;

   @try {
    if(raised!=nil || invocation==nil || [elements count]==0){
     [self _replyStatus:NSDOReplyException sequence:sequence exception:(raised!=nil)?raised:[NSException exceptionWithName:NSPortReceiveException reason:@"NSConnection: a request that could not be read" userInfo:nil]];
     return;
    }
    NSArray    *returned=[elements objectAtIndex:0];
    const char *returnType=typeOf(returned);

    [self _replyValue:sequence body:^(NSPortCoder *reply){
     NSUInteger i;

     if(returnType[0]=='@'){
      __unsafe_unretained id value=nil;

      [invocation getReturnValue:&value];
      [reply setBycopy:isBycopy(qualifiersOf(returned)) byref:isByref(qualifiersOf(returned))];
      [reply encodeObject:value];
      [reply setBycopy:NO byref:NO];
     }
     else if(returnType[0]!='v'){
      NSUInteger size=[[invocation methodSignature] methodReturnLength];
      char       buffer[size>0?size:1];

      [invocation getReturnValue:buffer];
      [reply encodeValueOfObjCType:returnType at:buffer];
     }
     for(i=2;i+1<[elements count];i++){
      NSArray       *element=[elements objectAtIndex:i+1];
      const char    *type=typeOf(element);
      NSMutableData *pointee=[pointees objectAtIndex:i-2];

      if(!isPointer(type) || ![pointee isKindOfClass:[NSMutableData class]] || !comesBack(qualifiersOf(element)))
       continue;
      if(type[1]=='@')
       [reply encodeObject:*(__unsafe_unretained id *)[pointee mutableBytes]];
      else
       [reply encodeValueOfObjCType:type+1 at:[pointee mutableBytes]];
     }
    }];
   }
   @catch(NSException *exception){
   }
}

@end
