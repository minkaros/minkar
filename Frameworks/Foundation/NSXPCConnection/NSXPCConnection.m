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
 * NSXPCConnection over libxpc.
 *
 * A message to a remote object is an XPC dictionary:
 *   "s"  the selector's name
 *   "p"  which exported object: 0 the connection's exportedObject, others
 *        objects sent as proxies (setInterface:forSelector:...)
 *   "a"  the arguments, a keyed archive: key "<index>" for each argument
 *        (scalars by value, objects archived securely), "p<index>" for an
 *        object sent as a proxy
 *   "x"  an XPC array with the XPC objects the arguments hold (endpoints,
 *        xpc_object_t arguments), referred to by index from the archive
 * A method with a reply block is sent with a reply; the reply has "a" and
 * "x" for the block's arguments, or "e" when the call was refused.
 *
 * The argument types come from the protocol's extended type encoding, so
 * a reply block's own arguments are known. Decoding allows the class each
 * argument is declared with (or those set with setClasses:...) and the
 * property list classes.
 *
 * This file is built with OS_OBJECT_USE_OBJC=0: XPC and dispatch objects
 * are plain pointers here, retained and released by hand.
 */

#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSLock.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSProxy.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSUUID.h>
#import <Foundation/NSValue.h>
#import <Foundation/FoundationErrors.h>
#import <objc/runtime.h>
#include <Block.h>
#include <ctype.h>
#include <string.h>

extern const char *_protocol_getMethodTypeEncoding(Protocol *proto, SEL sel,
    BOOL isRequiredMethod, BOOL isInstanceMethod);
extern void NSInvocationBlockForward(void);

@interface NSInvocation (NSXPCConnection)
- (void)invokeUsingIMP:(IMP)imp;
@end

// The start of every block (the blocks ABI).
struct NSXPCBlockLayout {
    void    *isa;
    int32_t  flags;
    int32_t  reserved;
    void    *invoke;
    void    *descriptor;
};

static __thread void *NSXPCCurrentConnection;

static NSError *NSXPCError(NSInteger code) {
    NSString *text = code == NSXPCConnectionInterrupted ? @"The connection was interrupted." :
        code == NSXPCConnectionInvalid ? @"The connection is invalid." :
        @"The reply was not valid.";

    return [NSError errorWithDomain:NSCocoaErrorDomain code:code
        userInfo:@{ NSDebugDescriptionErrorKey : text }];
}

#pragma mark - Type encodings

static const char *NSXPCSkipQuoted(const char *p) {
    const char *end = strchr(p + 1, '"');

    return end ? end + 1 : p + strlen(p);
}

static const char *NSXPCSkipBalanced(const char *p, char open, char close) {
    int depth = 0;

    for (; *p; p++) {
        if (*p == '"') {
            p = NSXPCSkipQuoted(p) - 1;
            continue;
        }
        if (*p == open)
            depth++;
        else if (*p == close && --depth == 0)
            return p + 1;
    }
    return p;
}

static const char *NSXPCSkipQualifiers(const char *p) {
    while (*p && strchr("rnNoORVA", *p) != NULL)
        p++;
    return p;
}

// The end of the type at p (extended encodings: @"Class", @?<signature>).
static const char *NSXPCSkipType(const char *p) {
    p = NSXPCSkipQualifiers(p);
    switch (*p) {
    case '\0':
        return p;
    case '@':
        p++;
        if (*p == '"')
            return NSXPCSkipQuoted(p);
        if (*p == '?') {
            p++;
            if (*p == '<')
                return NSXPCSkipBalanced(p, '<', '>');
        }
        return p;
    case '{':
        return NSXPCSkipBalanced(p, '{', '}');
    case '(':
        return NSXPCSkipBalanced(p, '(', ')');
    case '[':
        return NSXPCSkipBalanced(p, '[', ']');
    case '^':
        return NSXPCSkipType(p + 1);
    case 'b':
        p++;
        while (isdigit((unsigned char)*p))
            p++;
        return p;
    default:
        return p + 1;
    }
}

// The types of an encoding, one string each, without the frame offsets.
static NSArray<NSString *> *NSXPCTypeList(const char *types) {
    NSMutableArray *result = [NSMutableArray array];
    const char     *p = types;

    while (*p) {
        const char *end = NSXPCSkipType(p);

        if (end == p)
            break;
        [result addObject:[[NSString alloc] initWithBytes:p length:end - p encoding:NSUTF8StringEncoding]];
        p = end;
        while (*p == '-' || *p == '+' || isdigit((unsigned char)*p))
            p++;
    }
    return result;
}

// A type without the class names and block signatures, as NSMethodSignature reads it.
static NSString *NSXPCPlainType(NSString *type) {
    const char    *p = [type UTF8String];
    NSMutableData *out = [NSMutableData data];

    while (*p) {
        if (*p == '"') {
            p = NSXPCSkipQuoted(p);
            continue;
        }
        if (*p == '<') {
            p = NSXPCSkipBalanced(p, '<', '>');
            continue;
        }
        [out appendBytes:p length:1];
        p++;
    }
    return [[NSString alloc] initWithData:out encoding:NSUTF8StringEncoding];
}

// The class an object type names (@"NSString" gives NSString), or nil.
static NSString *NSXPCClassName(NSString *type) {
    const char *p = NSXPCSkipQualifiers([type UTF8String]);
    const char *end;

    if (p[0] != '@' || p[1] != '"')
        return nil;
    p += 2;
    for (end = p; *end && *end != '"' && *end != '<'; end++)
        ;
    if (end == p)
        return nil;
    return [[NSString alloc] initWithBytes:p length:end - p encoding:NSUTF8StringEncoding];
}

// The signature of a block type (@?<v@?@"NSString"> gives v@?@"NSString"), or nil.
static NSString *NSXPCBlockSignature(NSString *type) {
    const char *p = NSXPCSkipQualifiers([type UTF8String]);
    const char *end;

    if (strncmp(p, "@?<", 3) != 0)
        return nil;
    end = NSXPCSkipBalanced(p + 2, '<', '>');
    if (end - p < 5)
        return nil;
    return [[NSString alloc] initWithBytes:p + 3 length:end - p - 4 encoding:NSUTF8StringEncoding];
}

static NSMethodSignature *NSXPCSignature(NSArray<NSString *> *types) {
    NSMutableString *plain = [NSMutableString string];

    for (NSString *type in types)
        [plain appendString:NSXPCPlainType(type)];
    return [NSMethodSignature signatureWithObjCTypes:[plain UTF8String]];
}

static BOOL NSXPCIsXPCObjectType(NSString *type) {
    return [type rangeOfString:@"OS_xpc_object"].location != NSNotFound;
}

#pragma mark - Methods of an interface

// One method of a protocol, with what its arguments may hold.
@interface _NSXPCMethod : NSObject {
@public
    NSMethodSignature   *_signature;
    NSArray<NSString *> *_types;
    NSUInteger           _replyIndex;
    NSMethodSignature   *_replySignature;
    NSArray<NSString *> *_replyTypes;
    NSMutableDictionary *_classes;      // "a<index>" / "r<index>": NSSet
    NSMutableDictionary *_interfaces;   // same keys: NSXPCInterface
    NSMutableDictionary *_xpcTypes;     // same keys: NSValue of xpc_type_t
}
@end

@implementation _NSXPCMethod

static NSString *NSXPCArgumentKey(NSUInteger index, BOOL ofReply) {
    return [NSString stringWithFormat:@"%c%lu", ofReply ? 'r' : 'a', (unsigned long)index];
}

// The property list classes, allowed in every object argument.
static NSSet *NSXPCBaseClasses(void) {
    static NSSet *classes;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        classes = [NSSet setWithObjects:[NSString class], [NSNumber class], [NSData class],
            [NSDate class], [NSArray class], [NSDictionary class], [NSSet class], [NSNull class],
            [NSURL class], [NSUUID class], [NSError class], [NSXPCListenerEndpoint class], nil];
    });
    return classes;
}

- (NSSet *)classesForArgument:(NSUInteger)index ofReply:(BOOL)ofReply {
    NSSet        *set = _classes[NSXPCArgumentKey(index, ofReply)];
    NSArray      *types = ofReply ? _replyTypes : _types;
    NSMutableSet *result = [NSMutableSet setWithSet:NSXPCBaseClasses()];

    if (set != nil) {
        [result unionSet:set];
    } else if (index + 1 < [types count]) {
        Class cls = NSClassFromString(NSXPCClassName(types[index + 1]));

        if (cls != Nil)
            [result addObject:cls];
    }
    return result;
}

- (NSString *)typeOfArgument:(NSUInteger)index ofReply:(BOOL)ofReply {
    NSArray *types = ofReply ? _replyTypes : _types;

    return index + 1 < [types count] ? types[index + 1] : @"";
}

@end

@implementation NSXPCInterface {
    Protocol            *_protocol;
    NSMutableDictionary *_methods;      // selector name: _NSXPCMethod
}

+ (NSXPCInterface *)interfaceWithProtocol:(Protocol *)protocol {
    NSXPCInterface *result = [[self alloc] init];

    result.protocol = protocol;
    return result;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _methods = [NSMutableDictionary dictionary];
    return self;
}

- (Protocol *)protocol {
    return _protocol;
}

- (void)setProtocol:(Protocol *)protocol {
    @synchronized (self) {
        _protocol = protocol;
        [_methods removeAllObjects];
    }
}

// The method sel of the protocol, or nil. NSObject's own methods are never
// called remotely.
- (_NSXPCMethod *)_methodForSelector:(SEL)sel {
    if (sel == NULL || _protocol == nil)
        return nil;
    @synchronized (self) {
        NSString     *name = NSStringFromSelector(sel);
        _NSXPCMethod *method = _methods[name];
        const char   *types;

        if (method != nil)
            return method;
        if (protocol_getMethodDescription(@protocol(NSObject), sel, YES, YES).name != NULL)
            return nil;
        types = _protocol_getMethodTypeEncoding(_protocol, sel, YES, YES);
        if (types == NULL)
            types = _protocol_getMethodTypeEncoding(_protocol, sel, NO, YES);
        if (types == NULL) {
            struct objc_method_description description = protocol_getMethodDescription(_protocol, sel, YES, YES);

            if (description.name == NULL)
                description = protocol_getMethodDescription(_protocol, sel, NO, YES);
            types = description.types;
        }
        if (types == NULL)
            return nil;

        method = [_NSXPCMethod new];
        method->_types = NSXPCTypeList(types);
        method->_signature = NSXPCSignature(method->_types);
        method->_replyIndex = NSNotFound;
        method->_classes = [NSMutableDictionary dictionary];
        method->_interfaces = [NSMutableDictionary dictionary];
        method->_xpcTypes = [NSMutableDictionary dictionary];
        if (method->_signature == nil)
            return nil;
        // The reply block is the last block argument.
        for (NSUInteger i = [method->_types count]; i > 3; i--) {
            NSString *blockTypes = NSXPCBlockSignature(method->_types[i - 1]);

            if (blockTypes != nil) {
                method->_replyIndex = i - 2;            // argument index (self, _cmd first)
                method->_replyTypes = NSXPCTypeList([blockTypes UTF8String]);
                method->_replySignature = NSXPCSignature(method->_replyTypes);
                break;
            }
        }
        _methods[name] = method;
        return method;
    }
}

// Argument arg counts from 0 for the method's own arguments (and for the
// reply block's, with ofReply); invocations count self and _cmd too.
static NSUInteger NSXPCInvocationIndex(NSUInteger arg, BOOL ofReply) {
    return ofReply ? arg + 1 : arg + 2;
}

- (_NSXPCMethod *)_checkedMethodForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _methodForSelector:sel];
    NSUInteger    count;

    if (method == nil)
        [NSException raise:NSInvalidArgumentException
                    format:@"%@ is not a method of protocol %s", NSStringFromSelector(sel), protocol_getName(_protocol)];
    count = ofReply ? [method->_replySignature numberOfArguments] : [method->_signature numberOfArguments];
    if (NSXPCInvocationIndex(arg, ofReply) >= count)
        [NSException raise:NSInvalidArgumentException
                    format:@"%@ has no argument %lu%s", NSStringFromSelector(sel), (unsigned long)arg,
                           ofReply ? " in its reply block" : ""];
    return method;
}

- (void)setClasses:(NSSet *)classes forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];

    @synchronized (self) {
        method->_classes[NSXPCArgumentKey(NSXPCInvocationIndex(arg, ofReply), ofReply)] = [classes copy];
    }
}

- (NSSet *)classesForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];
    NSUInteger    index = NSXPCInvocationIndex(arg, ofReply);

    @synchronized (self) {
        NSSet *set = method->_classes[NSXPCArgumentKey(index, ofReply)];

        if (set != nil)
            return set;
        Class cls = NSClassFromString(NSXPCClassName([method typeOfArgument:index ofReply:ofReply]));
        return cls != Nil ? [NSSet setWithObject:cls] : [NSSet set];
    }
}

- (void)setInterface:(NSXPCInterface *)ifc forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];

    @synchronized (self) {
        method->_interfaces[NSXPCArgumentKey(NSXPCInvocationIndex(arg, ofReply), ofReply)] = ifc;
    }
}

- (NSXPCInterface *)interfaceForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];

    @synchronized (self) {
        return method->_interfaces[NSXPCArgumentKey(NSXPCInvocationIndex(arg, ofReply), ofReply)];
    }
}

- (void)setXPCType:(xpc_type_t)type forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];

    @synchronized (self) {
        method->_xpcTypes[NSXPCArgumentKey(NSXPCInvocationIndex(arg, ofReply), ofReply)] =
            [NSValue valueWithPointer:type];
    }
}

- (xpc_type_t)XPCTypeForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply {
    _NSXPCMethod *method = [self _checkedMethodForSelector:sel argumentIndex:arg ofReply:ofReply];

    @synchronized (self) {
        return [method->_xpcTypes[NSXPCArgumentKey(NSXPCInvocationIndex(arg, ofReply), ofReply)] pointerValue];
    }
}

- (NSXPCInterface *)_interfaceForArgument:(NSUInteger)index method:(_NSXPCMethod *)method ofReply:(BOOL)ofReply {
    @synchronized (self) {
        return method->_interfaces[NSXPCArgumentKey(index, ofReply)];
    }
}

- (xpc_type_t)_XPCTypeForArgument:(NSUInteger)index method:(_NSXPCMethod *)method ofReply:(BOOL)ofReply {
    @synchronized (self) {
        return [method->_xpcTypes[NSXPCArgumentKey(index, ofReply)] pointerValue];
    }
}

- (NSSet *)_classesForArgument:(NSUInteger)index method:(_NSXPCMethod *)method ofReply:(BOOL)ofReply {
    @synchronized (self) {
        return [method classesForArgument:index ofReply:ofReply];
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%s: %p protocol %s>", object_getClassName(self), self,
        _protocol ? protocol_getName(_protocol) : "(none)"];
}

@end

#pragma mark - Coders

// The coders of the arguments: keyed archives, with the XPC objects they
// hold kept aside in an XPC array.
@interface _NSXPCEncoder : NSKeyedArchiver {
@public
    xpc_object_t     _side;
    NSXPCConnection *_connection;
    id <NSObject>    _userInfo;
}
@end

@interface _NSXPCDecoder : NSKeyedUnarchiver {
@public
    xpc_object_t     _side;
    NSXPCConnection *_connection;
    id <NSObject>    _userInfo;
}
@end

@implementation _NSXPCEncoder

- (void)encodeXPCObject:(xpc_object_t)object forKey:(NSString *)key {
    if (object == NULL)
        return;
    xpc_array_append_value(_side, object);
    [self encodeInt64:(int64_t)xpc_array_get_count(_side) forKey:key];
}

- (xpc_object_t)decodeXPCObjectOfType:(xpc_type_t)type forKey:(NSString *)key {
    [NSException raise:NSInvalidArgumentException format:@"decoding with an encoder"];
    return NULL;
}

- (id <NSObject>)userInfo { return _userInfo; }
- (void)setUserInfo:(id <NSObject>)userInfo { _userInfo = userInfo; }
- (NSXPCConnection *)connection { return _connection; }

- (void)dealloc {
    if (_side != NULL)
        xpc_release(_side);
}

@end

@implementation _NSXPCDecoder

- (void)encodeXPCObject:(xpc_object_t)object forKey:(NSString *)key {
    [NSException raise:NSInvalidArgumentException format:@"encoding with a decoder"];
}

// The object is the decoder's: retain it to keep it.
- (xpc_object_t)decodeXPCObjectOfType:(xpc_type_t)type forKey:(NSString *)key {
    int64_t      index = [self decodeInt64ForKey:key];
    xpc_object_t object;

    if (index <= 0 || _side == NULL || (size_t)index > xpc_array_get_count(_side))
        return NULL;
    object = xpc_array_get_value(_side, (size_t)index - 1);
    if (object == NULL || (type != NULL && xpc_get_type(object) != type))
        return NULL;
    return object;
}

- (id <NSObject>)userInfo { return _userInfo; }
- (void)setUserInfo:(id <NSObject>)userInfo { _userInfo = userInfo; }
- (NSXPCConnection *)connection { return _connection; }

- (void)dealloc {
    if (_side != NULL)
        xpc_release(_side);
}

@end

@implementation NSXPCCoder

- (void)encodeXPCObject:(xpc_object_t)xpcObject forKey:(NSString *)key {
    [NSException raise:NSInvalidArgumentException format:@"-[NSXPCCoder encodeXPCObject:forKey:] is abstract"];
}

- (xpc_object_t)decodeXPCObjectOfType:(xpc_type_t)type forKey:(NSString *)key {
    [NSException raise:NSInvalidArgumentException format:@"-[NSXPCCoder decodeXPCObjectOfType:forKey:] is abstract"];
    return NULL;
}

- (id <NSObject>)userInfo { return nil; }
- (void)setUserInfo:(id <NSObject>)userInfo { }
- (NSXPCConnection *)connection { return nil; }

@end

#pragma mark - Endpoints

@interface NSXPCListenerEndpoint ()
- (instancetype)_initWithEndpoint:(xpc_endpoint_t)endpoint;
- (xpc_endpoint_t)_endpoint;
@end

@implementation NSXPCListenerEndpoint {
    xpc_endpoint_t _xendpoint;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithEndpoint:(xpc_endpoint_t)endpoint {
    if (endpoint == NULL)
        return nil;
    if ((self = [super init]) != nil)
        _xendpoint = endpoint;          // the caller's reference
    return self;
}

- (xpc_endpoint_t)_endpoint {
    return _xendpoint;
}

- (void)dealloc {
    if (_xendpoint != NULL)
        xpc_release(_xendpoint);
}

- (void)encodeWithCoder:(NSCoder *)coder {
    if (![coder respondsToSelector:@selector(encodeXPCObject:forKey:)])
        [NSException raise:NSInvalidArgumentException
                    format:@"an NSXPCListenerEndpoint can only be sent over an NSXPCConnection"];
    [(id)coder encodeXPCObject:_xendpoint forKey:@"endpoint"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    xpc_object_t endpoint = NULL;

    if ([coder respondsToSelector:@selector(decodeXPCObjectOfType:forKey:)])
        endpoint = [(id)coder decodeXPCObjectOfType:XPC_TYPE_ENDPOINT forKey:@"endpoint"];
    if (endpoint == NULL)
        return nil;
    return [self _initWithEndpoint:(xpc_endpoint_t)xpc_retain(endpoint)];
}

@end

#pragma mark - Proxies

@interface NSXPCConnection ()
- (instancetype)_initWithConnection:(xpc_connection_t)connection;
- (xpc_connection_t)_connection;
- (void)_sendInvocation:(NSInvocation *)invocation proxyID:(uint64_t)proxyID
              interface:(NSXPCInterface *)interface errorHandler:(void (^)(NSError *))errorHandler
            synchronous:(BOOL)synchronous;
@end

// A remote object: messages to it go over the connection.
@interface _NSXPCDistantObject : NSProxy <NSXPCProxyCreating> {
@public
    NSXPCConnection *_connection;
    uint64_t         _proxyID;
    NSXPCInterface  *_interface;
    void           (^_errorHandler)(NSError *);
    BOOL             _synchronous;
}
@end

@implementation _NSXPCDistantObject

- (instancetype)_initWithConnection:(NSXPCConnection *)connection proxyID:(uint64_t)proxyID
                          interface:(NSXPCInterface *)interface errorHandler:(void (^)(NSError *))handler
                        synchronous:(BOOL)synchronous {
    _connection = connection;
    _proxyID = proxyID;
    _interface = interface;
    _errorHandler = [handler copy];
    _synchronous = synchronous;
    return self;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    _NSXPCMethod *method = [_interface _methodForSelector:selector];

    if (method == nil)
        [NSException raise:NSInvalidArgumentException
                    format:@"%@ is not in the remote object interface %@", NSStringFromSelector(selector), _interface];
    return method->_signature;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    [_connection _sendInvocation:invocation proxyID:_proxyID interface:_interface
                    errorHandler:_errorHandler synchronous:_synchronous];
}

- (BOOL)respondsToSelector:(SEL)selector {
    return [_interface _methodForSelector:selector] != nil;
}

- (BOOL)conformsToProtocol:(Protocol *)protocol {
    Protocol *own = [_interface protocol];

    return own != nil && (protocol_isEqual(own, protocol) || protocol_conformsToProtocol(own, protocol));
}

- (id)remoteObjectProxy {
    return [self remoteObjectProxyWithErrorHandler:_errorHandler];
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [[_NSXPCDistantObject alloc] _initWithConnection:_connection proxyID:_proxyID
        interface:_interface errorHandler:handler synchronous:NO];
}

- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [[_NSXPCDistantObject alloc] _initWithConnection:_connection proxyID:_proxyID
        interface:_interface errorHandler:handler synchronous:YES];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%s: %p proxy %llu on %@>", object_getClassName(self), self,
        (unsigned long long)_proxyID, _connection];
}

@end

#pragma mark - Arguments

@interface NSXPCConnection (NSXPCArguments)
- (uint64_t)_exportObject:(id)object interface:(NSXPCInterface *)interface;
@end

static void NSXPCEncodeArguments(NSInvocation *invocation, NSUInteger first, NSUInteger skip,
    _NSXPCMethod *method, NSXPCInterface *interface, BOOL ofReply, NSXPCConnection *connection,
    _NSXPCEncoder *encoder) {
    NSMethodSignature *signature = [invocation methodSignature];
    NSUInteger         count = [signature numberOfArguments];

    for (NSUInteger i = first; i < count; i++) {
        const char *type = NSXPCSkipQualifiers([signature getArgumentTypeAtIndex:i]);
        NSString   *key = [NSString stringWithFormat:@"%lu", (unsigned long)i];
        NSUInteger  size, align;

        if (i == skip)
            continue;
        NSGetSizeAndAlignment(type, &size, &align);
        switch (type[0]) {
        case '@': {
            __unsafe_unretained id object = nil;
            NSXPCInterface *proxyInterface;

            [invocation getArgument:&object atIndex:i];
            if (type[1] == '?')
                [NSException raise:NSInvalidArgumentException
                            format:@"%@: only the reply block can be a block",
                                   NSStringFromSelector([invocation selector])];
            if (object == nil)
                break;
            if ([interface _XPCTypeForArgument:i method:method ofReply:ofReply] != NULL ||
                NSXPCIsXPCObjectType([method typeOfArgument:i ofReply:ofReply])) {
                [encoder encodeXPCObject:(__bridge xpc_object_t)object forKey:key];
                break;
            }
            proxyInterface = [interface _interfaceForArgument:i method:method ofReply:ofReply];
            if (proxyInterface != nil && ![object isKindOfClass:[_NSXPCDistantObject class]]) {
                uint64_t exported = [connection _exportObject:object interface:proxyInterface];

                [encoder encodeInt64:(int64_t)exported forKey:[@"p" stringByAppendingString:key]];
                break;
            }
            [encoder encodeObject:object forKey:key];
            break;
        }
        case '#': {
            __unsafe_unretained Class cls = Nil;

            [invocation getArgument:&cls atIndex:i];
            if (cls != Nil)
                [encoder encodeObject:NSStringFromClass(cls) forKey:key];
            break;
        }
        case ':': {
            SEL sel = NULL;

            [invocation getArgument:&sel atIndex:i];
            if (sel != NULL)
                [encoder encodeObject:NSStringFromSelector(sel) forKey:key];
            break;
        }
        case '*': {
            char *string = NULL;

            [invocation getArgument:&string atIndex:i];
            if (string != NULL)
                [encoder encodeBytes:(const uint8_t *)string length:strlen(string) + 1 forKey:key];
            break;
        }
        case 'c': case 's': case 'i': case 'l': case 'q': {
            int64_t value = 0;

            [invocation getArgument:&value atIndex:i];     // little-endian: the low bytes
            if (size < 8) {
                int shift = (int)(64 - 8 * size);

                value = (int64_t)((uint64_t)value << shift) >> shift;     // sign-extends
            }
            [encoder encodeInt64:value forKey:key];
            break;
        }
        case 'C': case 'S': case 'I': case 'L': case 'Q': case 'B': {
            uint64_t value = 0;

            [invocation getArgument:&value atIndex:i];     // little-endian: the low bytes
            if (size < 8)
                value &= (1ULL << (8 * size)) - 1;
            [encoder encodeInt64:(int64_t)value forKey:key];
            break;
        }
        case 'f': {
            float value;

            [invocation getArgument:&value atIndex:i];
            [encoder encodeFloat:value forKey:key];
            break;
        }
        case 'd': {
            double value;

            [invocation getArgument:&value atIndex:i];
            [encoder encodeDouble:value forKey:key];
            break;
        }
        case '{': case '[': case '(': {
            NSMutableData *bytes = [NSMutableData dataWithLength:size];

            [invocation getArgument:[bytes mutableBytes] atIndex:i];
            [encoder encodeBytes:[bytes bytes] length:size forKey:key];
            break;
        }
        default:
            [NSException raise:NSInvalidArgumentException
                        format:@"%@: argument %lu of type %s cannot be sent",
                               NSStringFromSelector([invocation selector]), (unsigned long)i, type];
        }
    }
}

@interface NSXPCConnection (NSXPCProxies)
- (id)_proxyForID:(uint64_t)proxyID interface:(NSXPCInterface *)interface;
@end

// Sets the arguments from the archive; NO when one is not valid. Objects
// are kept alive in keep.
static BOOL NSXPCDecodeArguments(NSInvocation *invocation, NSUInteger first, NSUInteger skip,
    _NSXPCMethod *method, NSXPCInterface *interface, BOOL ofReply, NSXPCConnection *connection,
    _NSXPCDecoder *decoder, NSMutableArray *keep) {
    NSMethodSignature *signature = [invocation methodSignature];
    NSUInteger         count = [signature numberOfArguments];

    for (NSUInteger i = first; i < count; i++) {
        const char *type = NSXPCSkipQualifiers([signature getArgumentTypeAtIndex:i]);
        NSString   *key = [NSString stringWithFormat:@"%lu", (unsigned long)i];
        NSUInteger  size, align;

        if (i == skip)
            continue;
        NSGetSizeAndAlignment(type, &size, &align);
        switch (type[0]) {
        case '@': {
            __unsafe_unretained id value = nil;
            NSString       *proxyKey = [@"p" stringByAppendingString:key];
            NSXPCInterface *proxyInterface;
            xpc_type_t      xpcType = [interface _XPCTypeForArgument:i method:method ofReply:ofReply];

            if (type[1] == '?')
                return NO;
            if (xpcType != NULL || NSXPCIsXPCObjectType([method typeOfArgument:i ofReply:ofReply])) {
                value = (__bridge id)[decoder decodeXPCObjectOfType:xpcType forKey:key];
                if (value == nil && [decoder containsValueForKey:key])
                    return NO;
                [invocation setArgument:&value atIndex:i];
                break;
            }
            if ([decoder containsValueForKey:proxyKey]) {
                id proxy;

                proxyInterface = [interface _interfaceForArgument:i method:method ofReply:ofReply];
                if (proxyInterface == nil)
                    return NO;
                proxy = [connection _proxyForID:(uint64_t)[decoder decodeInt64ForKey:proxyKey]
                                      interface:proxyInterface];
                [keep addObject:proxy];
                value = proxy;
            } else if ([decoder containsValueForKey:key]) {
                id object = [decoder decodeObjectOfClasses:[interface _classesForArgument:i method:method ofReply:ofReply]
                                                    forKey:key];

                if (object == nil || [decoder error] != nil)
                    return NO;
                [keep addObject:object];
                value = object;
            }
            [invocation setArgument:&value atIndex:i];
            break;
        }
        case '#': {
            NSString *name = [decoder decodeObjectOfClass:[NSString class] forKey:key];
            __unsafe_unretained Class cls = name ? NSClassFromString(name) : Nil;

            if (name != nil && cls == Nil)
                return NO;
            [invocation setArgument:&cls atIndex:i];
            break;
        }
        case ':': {
            NSString *name = [decoder decodeObjectOfClass:[NSString class] forKey:key];
            SEL       sel = name ? NSSelectorFromString(name) : NULL;

            [invocation setArgument:&sel atIndex:i];
            break;
        }
        case '*': {
            NSUInteger     length = 0;
            const uint8_t *bytes = [decoder decodeBytesForKey:key returnedLength:&length];
            char          *string = NULL;

            if (bytes != NULL) {
                NSMutableData *copy;

                if (length == 0 || bytes[length - 1] != '\0')
                    return NO;
                copy = [NSMutableData dataWithBytes:bytes length:length];
                [keep addObject:copy];
                string = [copy mutableBytes];
            }
            [invocation setArgument:&string atIndex:i];
            break;
        }
        case 'c': case 's': case 'i': case 'l': case 'q':
        case 'C': case 'S': case 'I': case 'L': case 'Q': case 'B': {
            int64_t value = [decoder decodeInt64ForKey:key];
            uint8_t bytes[8];

            memcpy(bytes, &value, 8);                      // little-endian: the low bytes
            [invocation setArgument:bytes atIndex:i];
            break;
        }
        case 'f': {
            float value = [decoder decodeFloatForKey:key];

            [invocation setArgument:&value atIndex:i];
            break;
        }
        case 'd': {
            double value = [decoder decodeDoubleForKey:key];

            [invocation setArgument:&value atIndex:i];
            break;
        }
        case '{': case '[': case '(': {
            NSUInteger     length = 0;
            const uint8_t *bytes = [decoder decodeBytesForKey:key returnedLength:&length];

            NSMutableData *copy;

            if (bytes == NULL || length != size)
                return NO;
            copy = [NSMutableData dataWithBytes:bytes length:length];
            [keep addObject:copy];
            [invocation setArgument:[copy mutableBytes] atIndex:i];
            break;
        }
        default:
            return NO;
        }
    }
    return YES;
}

static _NSXPCEncoder *NSXPCNewEncoder(NSXPCConnection *connection) {
    _NSXPCEncoder *encoder = [[_NSXPCEncoder alloc] initRequiringSecureCoding:YES];

    encoder->_side = xpc_array_create(NULL, 0);
    encoder->_connection = connection;
    return encoder;
}

// Puts the encoder's archive and XPC objects in message.
static void NSXPCFinishMessage(xpc_object_t message, _NSXPCEncoder *encoder) {
    NSData *data = [encoder encodedData];

    xpc_dictionary_set_data(message, "a", [data bytes], [data length]);
    xpc_dictionary_set_value(message, "x", encoder->_side);
}

static _NSXPCDecoder *NSXPCNewDecoder(NSXPCConnection *connection, xpc_object_t message) {
    size_t         length = 0;
    const void    *bytes = xpc_dictionary_get_data(message, "a", &length);
    xpc_object_t   side = xpc_dictionary_get_value(message, "x");
    _NSXPCDecoder *decoder;

    if (bytes == NULL)
        return nil;
    decoder = [[_NSXPCDecoder alloc] initForReadingFromData:[NSData dataWithBytes:bytes length:length] error:NULL];
    if (decoder == nil)
        return nil;
    if (side != NULL && xpc_get_type(side) == XPC_TYPE_ARRAY)
        decoder->_side = xpc_retain(side);
    decoder->_connection = connection;
    return decoder;
}

#pragma mark - Reply blocks

// What a reply block made for an incoming message does: sends the reply.
@interface _NSXPCReplyContext : NSObject {
@public
    NSXPCConnection *_connection;
    xpc_object_t     _request;
    _NSXPCMethod    *_method;
    NSXPCInterface  *_interface;
    BOOL             _sent;
}
@end

@implementation _NSXPCReplyContext

- (NSMethodSignature *)_blockSignature {
    return _method->_replySignature;
}

- (void)_blockInvoked:(NSInvocation *)invocation {
    xpc_object_t   reply;
    _NSXPCEncoder *encoder;

    @synchronized (self) {
        if (_sent) {
            NSLog(@"NSXPCConnection: the reply block of %@ was called more than once",
                [NSString stringWithUTF8String:xpc_dictionary_get_string(_request, "s") ?: "?"]);
            return;
        }
        _sent = YES;
    }
    reply = xpc_dictionary_create_reply(_request);
    if (reply == NULL)
        return;
    encoder = NSXPCNewEncoder(_connection);
    @try {
        NSXPCEncodeArguments(invocation, 1, NSNotFound, _method, _interface, YES, _connection, encoder);
        NSXPCFinishMessage(reply, encoder);
    } @catch (NSException *exception) {
        NSLog(@"NSXPCConnection: the reply could not be encoded: %@", exception);
        xpc_dictionary_set_uint64(reply, "e", 1);
    }
    xpc_connection_send_message([_connection _connection], reply);
    xpc_release(reply);
}

- (void)dealloc {
    if (_request != NULL)
        xpc_release(_request);
}

@end

// A block that, called, sends its arguments as the reply to request.
static id NSXPCMakeReplyBlock(_NSXPCReplyContext *context) {
    id block = [^{ (void)context; } copy];

    // The block captures only context, right after its header, where
    // NSInvocationBlockForwardRegisters looks for it.
    ((__bridge struct NSXPCBlockLayout *)block)->invoke = (void *)NSInvocationBlockForward;
    return block;
}

#pragma mark - Connections

@implementation NSXPCConnection {
    xpc_connection_t     _xconnection;
    dispatch_queue_t     _queue;
    NSString            *_serviceName;
    NSXPCListenerEndpoint *_endpoint;
    NSXPCInterface      *_exportedInterface;
    id                   _exportedObject;
    NSXPCInterface      *_remoteObjectInterface;
    void               (^_interruptionHandler)(void);
    void               (^_invalidationHandler)(void);
    NSMutableDictionary *_exported;         // proxy id: @[object, interface]
    uint64_t             _nextExportedID;
    BOOL                 _resumed;
    BOOL                 _invalid;
}

+ (NSXPCConnection *)currentConnection {
    return (__bridge NSXPCConnection *)NSXPCCurrentConnection;
}

- (instancetype)init {
    [NSException raise:NSInvalidArgumentException format:@"use -initWithServiceName:, -initWithMachServiceName:options: or -initWithListenerEndpoint:"];
    return nil;
}

- (void)_setUpWithConnection:(xpc_connection_t)connection {
    _xconnection = connection;
    _queue = dispatch_queue_create("org.minkar.NSXPCConnection", DISPATCH_QUEUE_SERIAL);
    _exported = [NSMutableDictionary dictionary];
    _nextExportedID = 1;
    xpc_connection_set_target_queue(_xconnection, _queue);
    // The handler keeps the connection alive until it is invalidated, as
    // a peer connection that its listener's delegate only resumed.
    NSXPCConnection *strongSelf = self;
    xpc_connection_set_event_handler(_xconnection, ^(xpc_object_t event) {
        [strongSelf _handleEvent:event];
    });
}

- (instancetype)initWithServiceName:(NSString *)serviceName {
    if ((self = [super init]) != nil) {
        _serviceName = [serviceName copy];
        [self _setUpWithConnection:xpc_connection_create([serviceName UTF8String], NULL)];
    }
    return self;
}

- (instancetype)initWithMachServiceName:(NSString *)name options:(NSXPCConnectionOptions)options {
    if ((self = [super init]) != nil) {
        uint64_t flags = (options & NSXPCConnectionPrivileged) ? XPC_CONNECTION_MACH_SERVICE_PRIVILEGED : 0;

        _serviceName = [name copy];
        [self _setUpWithConnection:xpc_connection_create_mach_service([name UTF8String], NULL, flags)];
    }
    return self;
}

- (instancetype)initWithListenerEndpoint:(NSXPCListenerEndpoint *)endpoint {
    xpc_connection_t connection;

    if (endpoint == nil || (connection = xpc_connection_create_from_endpoint([endpoint _endpoint])) == NULL)
        return nil;
    if ((self = [super init]) != nil) {
        _endpoint = endpoint;
        [self _setUpWithConnection:connection];
    }
    return self;
}

// A peer connection a listener accepted.
- (instancetype)_initWithConnection:(xpc_connection_t)connection {
    if ((self = [super init]) != nil)
        [self _setUpWithConnection:(xpc_connection_t)xpc_retain(connection)];
    return self;
}

- (void)dealloc {
    if (_xconnection != NULL)
        xpc_release(_xconnection);
    if (_queue != NULL)
        dispatch_release(_queue);
}

- (xpc_connection_t)_connection {
    return _xconnection;
}

- (NSString *)serviceName {
    return _serviceName;
}

- (NSXPCListenerEndpoint *)endpoint {
    @synchronized (self) {
        if (_endpoint == nil)
            _endpoint = [[NSXPCListenerEndpoint alloc] _initWithEndpoint:xpc_endpoint_create(_xconnection)];
        return _endpoint;
    }
}

- (NSXPCInterface *)exportedInterface {
    @synchronized (self) { return _exportedInterface; }
}

- (void)setExportedInterface:(NSXPCInterface *)interface {
    @synchronized (self) { _exportedInterface = interface; }
}

- (id)exportedObject {
    @synchronized (self) { return _exportedObject; }
}

- (void)setExportedObject:(id)object {
    @synchronized (self) { _exportedObject = object; }
}

- (NSXPCInterface *)remoteObjectInterface {
    @synchronized (self) { return _remoteObjectInterface; }
}

- (void)setRemoteObjectInterface:(NSXPCInterface *)interface {
    @synchronized (self) { _remoteObjectInterface = interface; }
}

- (void (^)(void))interruptionHandler {
    @synchronized (self) { return _interruptionHandler; }
}

- (void)setInterruptionHandler:(void (^)(void))handler {
    @synchronized (self) { _interruptionHandler = [handler copy]; }
}

- (void (^)(void))invalidationHandler {
    @synchronized (self) { return _invalidationHandler; }
}

- (void)setInvalidationHandler:(void (^)(void))handler {
    @synchronized (self) { _invalidationHandler = [handler copy]; }
}

- (id)remoteObjectProxy {
    return [self _proxyForID:0 interface:[self remoteObjectInterface] errorHandler:nil synchronous:NO];
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [self _proxyForID:0 interface:[self remoteObjectInterface] errorHandler:handler synchronous:NO];
}

- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [self _proxyForID:0 interface:[self remoteObjectInterface] errorHandler:handler synchronous:YES];
}

- (id)_proxyForID:(uint64_t)proxyID interface:(NSXPCInterface *)interface errorHandler:(void (^)(NSError *))handler
      synchronous:(BOOL)synchronous {
    return [[_NSXPCDistantObject alloc] _initWithConnection:self proxyID:proxyID interface:interface
        errorHandler:handler synchronous:synchronous];
}

- (id)_proxyForID:(uint64_t)proxyID interface:(NSXPCInterface *)interface {
    return [self _proxyForID:proxyID interface:interface errorHandler:nil synchronous:NO];
}

// Objects sent as proxies stay exported while the connection lives.
- (uint64_t)_exportObject:(id)object interface:(NSXPCInterface *)interface {
    @synchronized (self) {
        uint64_t proxyID = _nextExportedID++;

        _exported[@(proxyID)] = @[ object, interface ];
        return proxyID;
    }
}

- (void)resume {
    @synchronized (self) {
        if (_invalid)
            return;
        _resumed = YES;
    }
    xpc_connection_resume(_xconnection);
}

- (void)suspend {
    xpc_connection_suspend(_xconnection);
}

- (void)invalidate {
    BOOL resume;

    @synchronized (self) {
        if (_invalid)
            return;
        _invalid = YES;
        resume = !_resumed;
        _resumed = YES;
    }
    // A connection never resumed still reports its invalidation.
    if (resume)
        xpc_connection_resume(_xconnection);
    xpc_connection_cancel(_xconnection);
}

- (au_asid_t)auditSessionIdentifier {
    return xpc_connection_get_asid(_xconnection);
}

- (pid_t)processIdentifier {
    return xpc_connection_get_pid(_xconnection);
}

- (uid_t)effectiveUserIdentifier {
    return xpc_connection_get_euid(_xconnection);
}

- (gid_t)effectiveGroupIdentifier {
    return xpc_connection_get_egid(_xconnection);
}

- (void)scheduleSendBarrierBlock:(void (^)(void))block {
    xpc_connection_send_barrier(_xconnection, block);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%s: %p%@%@ pid %d>", object_getClassName(self), self,
        _serviceName ? @" service " : @"", _serviceName ?: @"", [self processIdentifier]];
}

#pragma mark Sending

- (void)_sendInvocation:(NSInvocation *)invocation proxyID:(uint64_t)proxyID
              interface:(NSXPCInterface *)interface errorHandler:(void (^)(NSError *))errorHandler
            synchronous:(BOOL)synchronous {
    SEL            selector = [invocation selector];
    _NSXPCMethod  *method = [interface _methodForSelector:selector];
    NSUInteger     replyIndex;
    xpc_object_t   message;
    _NSXPCEncoder *encoder;
    id             replyBlock = nil;

    if (method == nil)
        [NSException raise:NSInvalidArgumentException format:@"%@ is not in the interface %@",
            NSStringFromSelector(selector), interface];
    if (*NSXPCSkipQualifiers([method->_signature methodReturnType]) != 'v')
        [NSException raise:NSInvalidArgumentException
                    format:@"%@: methods called over NSXPCConnection return void; use a reply block",
                           NSStringFromSelector(selector)];
    replyIndex = method->_replyIndex;

    encoder = NSXPCNewEncoder(self);
    NSXPCEncodeArguments(invocation, 2, replyIndex, method, interface, NO, self, encoder);
    message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(message, "s", sel_getName(selector));
    xpc_dictionary_set_uint64(message, "p", proxyID);
    NSXPCFinishMessage(message, encoder);

    if (replyIndex != NSNotFound) {
        __unsafe_unretained id block = nil;

        [invocation getArgument:&block atIndex:replyIndex];
        replyBlock = [block copy];
    }

    if (replyBlock == nil) {
        if (synchronous)
            NSLog(@"NSXPCConnection: %@ has no reply block; sent asynchronously", NSStringFromSelector(selector));
        xpc_connection_send_message(_xconnection, message);
    } else if (synchronous) {
        xpc_object_t reply = xpc_connection_send_message_with_reply_sync(_xconnection, message);

        [self _handleReply:reply method:method interface:interface block:replyBlock errorHandler:errorHandler];
        if (reply != NULL)
            xpc_release(reply);
    } else {
        xpc_connection_send_message_with_reply(_xconnection, message, _queue, ^(xpc_object_t reply) {
            [self _handleReply:reply method:method interface:interface block:replyBlock errorHandler:errorHandler];
        });
    }
    xpc_release(message);
}

- (void)_handleReply:(xpc_object_t)reply method:(_NSXPCMethod *)method interface:(NSXPCInterface *)interface
               block:(id)block errorHandler:(void (^)(NSError *))errorHandler {
    NSInteger      errorCode = 0;
    __attribute__((objc_precise_lifetime)) _NSXPCDecoder *decoder = nil;
    NSInvocation  *invocation;
    // Precise: the objects it keeps must outlive the call, after its last use.
    __attribute__((objc_precise_lifetime)) NSMutableArray *keep = [NSMutableArray array];
    void          *saved = NSXPCCurrentConnection;

    if (reply == NULL || reply == XPC_ERROR_CONNECTION_INVALID)
        errorCode = NSXPCConnectionInvalid;
    else if (reply == XPC_ERROR_CONNECTION_INTERRUPTED)
        errorCode = NSXPCConnectionInterrupted;
    else if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY || xpc_dictionary_get_uint64(reply, "e") != 0)
        errorCode = NSXPCConnectionReplyInvalid;
    else if ((decoder = NSXPCNewDecoder(self, reply)) == nil)
        errorCode = NSXPCConnectionReplyInvalid;

    if (errorCode == 0) {
        __unsafe_unretained id target = block;

        invocation = [NSInvocation invocationWithMethodSignature:method->_replySignature];
        [invocation setArgument:&target atIndex:0];
        if (!NSXPCDecodeArguments(invocation, 1, NSNotFound, method, interface, YES, self, decoder, keep))
            errorCode = NSXPCConnectionReplyInvalid;
        else {
            NSXPCCurrentConnection = (__bridge void *)self;
            [invocation invokeUsingIMP:(IMP)((__bridge struct NSXPCBlockLayout *)block)->invoke];
            NSXPCCurrentConnection = saved;
            return;
        }
    }
    if (errorCode == NSXPCConnectionReplyInvalid)
        NSLog(@"NSXPCConnection: the reply to a message on %@ was not valid", self);
    if (errorHandler != nil)
        errorHandler(NSXPCError(errorCode));
}

#pragma mark Receiving

- (void)_handleEvent:(xpc_object_t)event {
    xpc_type_t type = xpc_get_type(event);

    if (type == XPC_TYPE_DICTIONARY) {
        [self _handleMessage:event];
    } else if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
        void (^handler)(void) = [self interruptionHandler];

        if (handler != nil)
            handler();
    } else if (event == XPC_ERROR_CONNECTION_INVALID) {
        void (^handler)(void);

        @synchronized (self) {
            _invalid = YES;
            handler = _invalidationHandler;
            _invalidationHandler = nil;
            _interruptionHandler = nil;
            [_exported removeAllObjects];
        }
        if (handler != nil)
            handler();
        // Drops the event handler, which keeps the connection alive; later,
        // as the handler may be running now.
        xpc_connection_t connection = (xpc_connection_t)xpc_retain(_xconnection);
        dispatch_async(_queue, ^{
            xpc_connection_set_event_handler(connection, ^(xpc_object_t ignored) { (void)ignored; });
            xpc_release(connection);
        });
    }
}

- (void)_refuseMessage:(xpc_object_t)message reason:(NSString *)reason {
    xpc_object_t reply = xpc_dictionary_create_reply(message);

    NSLog(@"NSXPCConnection: refused a message on %@: %@", self, reason);
    if (reply != NULL) {
        xpc_dictionary_set_uint64(reply, "e", 1);
        xpc_connection_send_message(_xconnection, reply);
        xpc_release(reply);
    }
}

- (void)_handleMessage:(xpc_object_t)message {
    const char     *name = xpc_dictionary_get_string(message, "s");
    uint64_t        proxyID = xpc_dictionary_get_uint64(message, "p");
    id              target;
    NSXPCInterface *interface;
    _NSXPCMethod   *method;
    SEL             selector;
    NSInvocation   *invocation;
    __attribute__((objc_precise_lifetime)) _NSXPCDecoder *decoder;
    // Precise: the objects it keeps must outlive the call, after its last use.
    __attribute__((objc_precise_lifetime)) NSMutableArray *keep = [NSMutableArray array];
    void           *saved = NSXPCCurrentConnection;

    if (name == NULL) {
        [self _refuseMessage:message reason:@"no selector"];
        return;
    }
    @synchronized (self) {
        if (proxyID == 0) {
            target = _exportedObject;
            interface = _exportedInterface;
        } else {
            NSArray *entry = _exported[@(proxyID)];

            target = entry[0];
            interface = entry[1];
        }
    }
    selector = sel_registerName(name);
    if (target == nil || interface == nil) {
        [self _refuseMessage:message reason:@"no exported object"];
        return;
    }
    if ((method = [interface _methodForSelector:selector]) == nil) {
        [self _refuseMessage:message reason:[NSString stringWithFormat:@"%s is not in the exported interface", name]];
        return;
    }
    if (![target respondsToSelector:selector]) {
        [self _refuseMessage:message reason:[NSString stringWithFormat:@"the exported object does not implement %s", name]];
        return;
    }
    if ((decoder = NSXPCNewDecoder(self, message)) == nil) {
        [self _refuseMessage:message reason:@"the arguments are not a valid archive"];
        return;
    }
    invocation = [NSInvocation invocationWithMethodSignature:method->_signature];
    [invocation setSelector:selector];
    [invocation setTarget:target];
    if (!NSXPCDecodeArguments(invocation, 2, method->_replyIndex, method, interface, NO, self, decoder, keep)) {
        [self _refuseMessage:message reason:[NSString stringWithFormat:@"the arguments of %s are not valid", name]];
        return;
    }
    if (method->_replyIndex != NSNotFound) {
        _NSXPCReplyContext *context = [_NSXPCReplyContext new];
        id block;                   // strong: an unretained variable lets ARC free it at once

        context->_connection = self;
        context->_request = xpc_retain(message);
        context->_method = method;
        context->_interface = interface;
        block = NSXPCMakeReplyBlock(context);
        [keep addObject:block];
        [invocation setArgument:&block atIndex:method->_replyIndex];
    }

    NSXPCCurrentConnection = (__bridge void *)self;
    @try {
        [invocation invoke];
    } @catch (NSException *exception) {
        NSLog(@"NSXPCConnection: %s raised %@", name, exception);
    }
    NSXPCCurrentConnection = saved;
}

@end

#pragma mark - Listeners

@implementation NSXPCListener {
    xpc_connection_t _xconnection;
    dispatch_queue_t _queue;
    __weak id <NSXPCListenerDelegate> _delegate;
    BOOL _isServiceListener;
    BOOL _resumed;
}

static NSXPCListener *NSXPCServiceListener;

+ (NSXPCListener *)serviceListener {
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        NSXPCServiceListener = [[NSXPCListener alloc] _initServiceListener];
    });
    return NSXPCServiceListener;
}

+ (NSXPCListener *)anonymousListener {
    return [[self alloc] _initWithConnection:xpc_connection_create(NULL, NULL)];
}

- (instancetype)init {
    [NSException raise:NSInvalidArgumentException format:@"use +serviceListener, +anonymousListener or -initWithMachServiceName:"];
    return nil;
}

- (instancetype)_initServiceListener {
    if ((self = [super init]) != nil)
        _isServiceListener = YES;
    return self;
}

- (instancetype)_initWithConnection:(xpc_connection_t)connection {
    if (connection == NULL)
        return nil;
    if ((self = [super init]) != nil) {
        __weak NSXPCListener *weakSelf = self;

        _xconnection = connection;
        _queue = dispatch_queue_create("org.minkar.NSXPCListener", DISPATCH_QUEUE_SERIAL);
        xpc_connection_set_target_queue(_xconnection, _queue);
        xpc_connection_set_event_handler(_xconnection, ^(xpc_object_t event) {
            if (xpc_get_type(event) == XPC_TYPE_CONNECTION)
                [weakSelf _acceptPeer:(xpc_connection_t)event];
        });
    }
    return self;
}

- (instancetype)initWithMachServiceName:(NSString *)name {
    return [self _initWithConnection:xpc_connection_create_mach_service([name UTF8String], NULL,
        XPC_CONNECTION_MACH_SERVICE_LISTENER)];
}

- (void)dealloc {
    if (_xconnection != NULL) {
        xpc_connection_cancel(_xconnection);
        xpc_release(_xconnection);
    }
    if (_queue != NULL)
        dispatch_release(_queue);
}

- (id <NSXPCListenerDelegate>)delegate {
    return _delegate;
}

- (void)setDelegate:(id <NSXPCListenerDelegate>)delegate {
    _delegate = delegate;
}

- (void)_acceptPeer:(xpc_connection_t)peer {
    NSXPCConnection *connection = [[NSXPCConnection alloc] _initWithConnection:peer];
    id <NSXPCListenerDelegate> delegate = _delegate;
    BOOL accepted = NO;

    if ([delegate respondsToSelector:@selector(listener:shouldAcceptNewConnection:)])
        accepted = [delegate listener:self shouldAcceptNewConnection:connection];
    if (!accepted)
        [connection invalidate];
}

static void NSXPCServiceConnectionHandler(xpc_connection_t peer) {
    [NSXPCServiceListener _acceptPeer:peer];
}

- (NSXPCListenerEndpoint *)endpoint {
    if (_xconnection == NULL)
        return nil;
    return [[NSXPCListenerEndpoint alloc] _initWithEndpoint:xpc_endpoint_create(_xconnection)];
}

- (void)resume {
    if (_isServiceListener) {
        xpc_main(NSXPCServiceConnectionHandler);       // does not return
        return;
    }
    if (!_resumed) {
        _resumed = YES;
        xpc_connection_resume(_xconnection);
    }
}

- (void)suspend {
    if (_xconnection != NULL && _resumed)
        xpc_connection_suspend(_xconnection);
}

- (void)invalidate {
    if (_xconnection != NULL)
        xpc_connection_cancel(_xconnection);
}

@end
