/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSProxy.h>
#import <Foundation/NSCFTypeID.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSString.h>
#import <Foundation/NSStringFormatter.h>
#import <Foundation/NSAutoreleasePool-private.h>
#import <Foundation/NSRaise.h>
#import <objc/message.h>
#import <objc/runtime.h>

@interface NSInvocation(private)
+(NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature arguments:(void *)arguments;
@end

// objc4's root reference counting (objc-internal.h).
extern id _objc_rootRetain(id object);
extern BOOL _objc_rootReleaseWasZero(id object);
extern NSUInteger _objc_rootRetainCount(id object);
extern id _objc_rootAutorelease(id object);

@implementation NSProxy

+(void)load {
}


+(Class)class {
   return self;
}

/*
 FIXME: should we implement this? The Apple implementation does _not_ throw an exception, so we may not, either
+(BOOL)respondsToSelector:(SEL)selector {
   NSUnimplementedMethod();
   return NO;
}
 */

+allocWithZone:(NSZone *)zone {
   /* Minkar: not NSAllocateObject, which stores the CF type in the word
    * after isa (NSObject's cfinfo): a proxy has no such field, and that
    * word is its subclass's first instance variable. */
   return class_createInstance(self, 0);
}

+alloc {
   return [self allocWithZone:NULL];
}

/* Minkar: CF asks objects for their CF type; a proxy answers it itself
 * instead of forwarding it to what it stands for (or raising, when it
 * stands for nothing yet). */
-(unsigned)_cfTypeID {
   return kNSCFTypeObject;
}

-(void)dealloc {
   NSDeallocateObject((id)self);
}

-(void)finalize {
   // do nothing?
}

-(void)doesNotRecognizeSelector:(SEL)selector {
	[NSException raise:NSInvalidArgumentException
				format:@"%c[%@ %@]: selector not recognized", class_isMetaClass(object_getClass(self))?'+':'-',
	 NSStringFromClass(object_getClass(self)),NSStringFromSelector(selector)];
}


-(void)forwardInvocation:(NSInvocation *)invocation {
   NSInvalidAbstractInvocation();
}

-(id)forwardSelector:(SEL)selector arguments:(void *)arguments {
   NSMethodSignature *signature=[self methodSignatureForSelector:selector];

   if(signature==nil){
    [self doesNotRecognizeSelector:selector];
    return nil;
   }
   else {
    NSInvocation *invocation=[NSInvocation invocationWithMethodSignature:signature arguments:arguments];
   // char          result[[signature methodReturnLength]];
    id              result;

    [self forwardInvocation:invocation];
    [invocation getReturnValue:&result];

   // __builtin_return(result); Can we use __builtin_return like this? It still doesn't seem to work on float/doubles ?
    return result;
   }
}

-(NSUInteger)hash {
   return (NSUInteger)self>>4;
}

-(BOOL)isEqual:object {
   return (self==object);
}

-self {
   return self;
}


-(Class)class {
   return object_getClass(self);
}


-(Class)superclass {
   return class_getSuperclass(object_getClass(self));
}


-(NSZone *)zone {
   return NSZoneFromPointer(self);
}


- performSelector:(SEL)selector
{
#if defined(GCC_RUNTIME_3) || defined(APPLE_RUNTIME_4)
    IMP imp = class_getMethodImplementation(object_getClass(self), selector);
#else
    IMP imp = objc_msg_lookup(self, selector);
#endif
    return ((id (*)(id,SEL))imp)(self, selector);
}


- performSelector:(SEL)selector withObject:object1
{
#if defined(GCC_RUNTIME_3) || defined(APPLE_RUNTIME_4)
    IMP imp = class_getMethodImplementation(object_getClass(self), selector);
#else
    IMP imp = objc_msg_lookup(self, selector);
#endif
    return ((id (*)(id,SEL,id))imp)(self, selector, object1);
}


- performSelector:(SEL)selector withObject:object1 withObject:object2
{
#if defined(GCC_RUNTIME_3) || defined(APPLE_RUNTIME_4)
    IMP imp = class_getMethodImplementation(object_getClass(self), selector);
#else
    IMP imp = objc_msg_lookup(self, selector);
#endif
    return ((id (*)(id,SEL,id,id))imp)(self, selector, object1, object2);
}


-(BOOL)isProxy {
   return YES;
}


-(BOOL)isKindOfClass:(Class)class {
   NSMethodSignature *signature=[self methodSignatureForSelector:_cmd];
   NSInvocation      *invocation=[NSInvocation
     invocationWithMethodSignature:signature];
   BOOL               returnValue;

   [self forwardInvocation:invocation];

   [invocation getReturnValue:&returnValue];

   return returnValue;
}


-(BOOL)isMemberOfClass:(Class)class {
   NSMethodSignature *signature=[self methodSignatureForSelector:_cmd];
   NSInvocation      *invocation=[NSInvocation
     invocationWithMethodSignature:signature];
   BOOL               returnValue;

   [self forwardInvocation:invocation];

   [invocation getReturnValue:&returnValue];

   return returnValue;
}


-(BOOL)conformsToProtocol:(Protocol *)protocol {
   NSMethodSignature *signature=[self methodSignatureForSelector:_cmd];
   NSInvocation      *invocation=[NSInvocation
     invocationWithMethodSignature:signature];
   BOOL               returnValue;

   [self forwardInvocation:invocation];

   [invocation getReturnValue:&returnValue];

   return returnValue;
}

-(BOOL)respondsToSelector:(SEL)selector {
   NSMethodSignature *signature=[self methodSignatureForSelector:_cmd];
   NSInvocation      *invocation=[NSInvocation
     invocationWithMethodSignature:signature];
   BOOL               returnValue;

   [self forwardInvocation:invocation];

   [invocation getReturnValue:&returnValue];

   return returnValue;
}

-autorelease {
   return _objc_rootAutorelease(self);
}


/* Minkar: the runtime's own reference count, as objc4's NSObject keeps
 * it. NSIncrementExtraRefCount calls objc_retain (and NSAutorelease
 * objc_autorelease), which sends the message back to a class that
 * implements it, as NSProxy does: it never returned. */
-(oneway void)release {
   if(_objc_rootReleaseWasZero(self))
    [self dealloc];
}


-retain {
   return _objc_rootRetain(self);
}


-(NSUInteger)retainCount {
   return _objc_rootRetainCount(self);
}


-(NSString *)description {
   return NSStringWithFormat(@"<%@: 0x%0x>",NSStringFromClass(object_getClass(self)),self);
}

-(NSString *)debugDescription {
    return [self description];
}

@end
