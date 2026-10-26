/* Modifications Copyright (c) 2026 The Minkar contributors */
#import <runtime/objc.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSInvocation.h>
#include <Foundation/NSMethodSignature.h>
#include <Foundation/NSException.h>
#include <stdio.h>

#define NSABISizeofRegisterReturn 8
#define NSABIasm_jmp_objc_msgSend __asm__("jmp _objc_msgSend")
#define NSABIasm_jmp_objc_msgSend_stret __asm__("jmp _objc_msgSend_stret")

static void OBJCRaiseException(const char *name,const char *format,...) {
    va_list arguments;
    
    va_start(arguments,format);
    
    fprintf(stderr,"ObjC:%s:",name);
    vfprintf(stderr,format,arguments);
    fprintf(stderr,"\n");
    fflush(stderr);
    va_end(arguments);
}


#if !FOUNDATION_DISALLOW_FORWARDING
@interface NSObject(fastforwarding)
-forwardingTargetForSelector:(SEL)selector;
@end

@interface NSInvocation(_private)
+(NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature arguments:(void *)arguments;
@end

id NSObjCGetFastForwardTarget(id object,SEL selector){
   id check=nil;

   if([object respondsToSelector:@selector(forwardingTargetForSelector:)])
    if((check=[object forwardingTargetForSelector:selector])==object)
     check=nil;

   return check;
}
#endif

/* Minkar: the forward handlers are NSInvocationForward and
 * NSInvocationForward_stret (NSInvocationCall-x86_64.s), which save the
 * message's registers and build the NSInvocation from them. The variadic
 * NSObjCForward that was here read the arguments with va_arg, which a
 * forwarded message does not set up: floats were lost, structs raised and
 * the return value never reached the caller. */


