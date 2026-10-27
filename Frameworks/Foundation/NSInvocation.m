/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSInvocation.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSRaise.h>

#import <sys/param.h>
#include <string.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <Foundation/NSException.h>
#import "NSObject/NSInvocationCall.h"

@implementation NSInvocation

-(void)buildFrame {
   NSInteger  i,count=[_signature numberOfArguments];
   NSUInteger align;

   NSGetSizeAndAlignment([_signature methodReturnType],&_returnSize,&align);
   _returnValue=NSZoneCalloc(NULL,MAX(_returnSize, sizeof(NSUInteger)),1);

   _argumentFrameSize=0;
   _argumentSizes=NSZoneCalloc(NULL,count,sizeof(NSUInteger));
   _argumentOffsets=NSZoneCalloc(NULL,count,sizeof(NSUInteger));

   for(i=0;i<count;i++){
    NSUInteger naturalSize;
    NSUInteger promotedSize;

    _argumentOffsets[i]=_argumentFrameSize;

    NSGetSizeAndAlignment([_signature getArgumentTypeAtIndex:i],&naturalSize,&align);
    promotedSize=((naturalSize+sizeof(NSUInteger)-1)/sizeof(NSUInteger))*sizeof(NSUInteger);

    _argumentSizes[i]=naturalSize;
    _argumentFrameSize+=promotedSize;
   }
}

-initWithMethodSignature:(NSMethodSignature *)signature {
   if(signature==nil){
    [self dealloc];
    [NSException raise:NSInvalidArgumentException format:@"nil signature in NSInvocation creation"];
    return nil;
   }

   _signature=[signature retain];

   [self buildFrame];

   _argumentFrame=NSZoneCalloc(NULL,_argumentFrameSize,1);

   return self;
}

-initWithMethodSignature:(NSMethodSignature *)signature arguments:(void *)arguments {
   unsigned       i;
   uint8_t *stackFrame=arguments;

   [self initWithMethodSignature:signature];

   for(i=0;i<_argumentFrameSize;i++)
    _argumentFrame[i]=stackFrame[i];

   return self;
}

-(void)dealloc {
    if (_retainArguments) {
        NSInteger i, count = [_signature numberOfArguments];

        for (i = 0; i < count; ++i) {
            const char *type = [_signature getArgumentTypeAtIndex:i];

            switch (type[0]) {
                case '@': {
                    id object;

                    [self getArgument:&object atIndex:i];
                    [object release];
                    break;
                }

                case '*': {
                    char *ptr;

                    [self getArgument:&ptr atIndex:i];
                    NSZoneFree(NULL, ptr);
                    break;
                }

                default:
                    break;
            }
        }
    }

   NSZoneFree(NULL,_returnValue);
   NSZoneFree(NULL,_argumentSizes);
   NSZoneFree(NULL,_argumentOffsets);
   NSZoneFree(NULL,_argumentFrame);
   [_signature release];
   NSDeallocateObject(self);
   return;
   [super dealloc];
}

static void *bufferForType(void *buffer,const char *type){
   NSUInteger size,align;

   NSGetSizeAndAlignment(type,&size,&align);
   if(buffer!=NULL)
    NSZoneFree(NULL,buffer);

   return NSZoneMalloc(NULL,size);
}

-initWithCoder:(NSCoder *)coder {
   const char *type;
   NSInteger         i,count;
   void       *buffer=NULL;

   _signature=[[coder decodeObject] retain];

   [self buildFrame];

   _argumentFrame=NSZoneCalloc(NULL,_argumentFrameSize,1);

   if([_signature methodReturnLength]>0){
    type=[_signature methodReturnType];
    buffer=bufferForType(buffer,type);
    [coder decodeValueOfObjCType:type at:buffer];
    [self setReturnValue:buffer];
   }

   count=[_signature numberOfArguments];
   for(i=0;i<count;i++){
    type=[_signature getArgumentTypeAtIndex:i];
    buffer=bufferForType(buffer,type);
    [coder decodeValueOfObjCType:type at:buffer];
    [self setArgument:buffer atIndex:i];
   }
   NSZoneFree(NULL,buffer);

   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   const char *type;
   NSInteger         i,count;
   void       *buffer=NULL;

   [coder encodeObject:_signature];

   if([_signature methodReturnLength]>0){
    type=[_signature methodReturnType];
    buffer=bufferForType(buffer,type);
    [self getReturnValue:buffer];
    [coder encodeValueOfObjCType:type at:buffer];
   }

   count=[_signature numberOfArguments];
   for(i=0;i<count;i++){
    type=[_signature getArgumentTypeAtIndex:i];
    buffer=bufferForType(buffer,type);
    [self getArgument:buffer atIndex:i];
    [coder encodeValueOfObjCType:type at:buffer];
   }
}

+(NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature {
   return [[[self allocWithZone:NULL] initWithMethodSignature:signature] autorelease];
}

+(NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature arguments:(void *)arguments {
   return [[[self allocWithZone:NULL] initWithMethodSignature:signature arguments:arguments] autorelease];
}

-(NSMethodSignature *)methodSignature {
   return _signature;
}

static void byteCopy(void *src,void *dst,NSUInteger length){
   NSInteger i;

   for(i=0;i<length;i++)
    ((char *)dst)[i]=((char *)src)[i];
}

-(void)getReturnValue:(void *)pointerToValue {
   byteCopy(_returnValue,pointerToValue,_returnSize);
}

-(void)setReturnValue:(void *)pointerToValue {
   byteCopy(pointerToValue,_returnValue,_returnSize);
}


// Arguments are kept as their own bytes, each in a zeroed slot of a
// multiple of 8 bytes; -invoke widens them as the calling convention wants.
// Minkar: small values were sign-extended here (unsigned ones too), and a
// struct whose size is not a multiple of 8 raised.
-(void)getArgument:(void *)pointerToValue atIndex:(NSInteger)index
{
    if (index < 0 || (NSUInteger)index >= [_signature numberOfArguments])
        [NSException raise:NSRangeException format:@"-[NSInvocation getArgument:atIndex:] index %ld out of range", (long)index];
    byteCopy(_argumentFrame + _argumentOffsets[index], pointerToValue, _argumentSizes[index]);
}

- (void)setArgument:(void *)pointerToValue atIndex:(NSInteger)index
{
    if (index < 0 || (NSUInteger)index >= [_signature numberOfArguments])
        [NSException raise:NSRangeException format:@"-[NSInvocation setArgument:atIndex:] index %ld out of range", (long)index];
    byteCopy(pointerToValue, _argumentFrame + _argumentOffsets[index], _argumentSizes[index]);
}


-(void)retainArguments {
    if (!_retainArguments) {
        NSInteger i, count = [_signature numberOfArguments];

        _retainArguments=YES;
        for (i = 0; i < count; ++i) {
            const char *type = [_signature getArgumentTypeAtIndex:i];

            switch (type[0]) {
                case '@': {
                    id object;

                    [self getArgument:&object atIndex:i];
                    [object retain];
                    break;
                }

                case '*': {
                    char *ptr, *copy;
                    size_t length;

                    [self getArgument:&ptr atIndex:i];
                    if (ptr == NULL)
                        break;
                    length = strlen(ptr);
                    copy = NSZoneCalloc(NULL, length+1, 1);
                    byteCopy(ptr, copy, length);
                    [self setArgument:&copy atIndex:i];
                    break;
                }

                default:
                    break;
            }
        }
    }
}

-(BOOL)argumentsRetained {
   return _retainArguments;
}

-(SEL)selector {
   SEL result;

   [self getArgument:&result atIndex:1];

   return result;
}

-(void)setSelector:(SEL)selector {
   [self setArgument:&selector atIndex:1];
}

-target {
   id result;

   [self getArgument:&result atIndex:0];

   return result;
}

-(void)setTarget:target {
   [self setArgument:&target atIndex:0];
}


- (void)invokeWithTarget:target
{
    [self setTarget: target];
    [self invoke];
}

-(NSUInteger)_argumentOffsetAtIndex:(NSUInteger)index {
    if (index >= [_signature numberOfArguments])
        return NSNotFound;
    return _argumentOffsets[index];
}

/*
 * -invoke: the System V x86-64 calling convention, by hand.
 *
 * Each argument is classified from its type in the signature: integers and
 * pointers go in the six integer registers, float and double in the eight
 * vector ones; a struct or union of up to 16 bytes is split into eightbytes,
 * each INTEGER or SSE by what it holds, and goes in registers if there are
 * enough left for all of it; anything else, and whatever does not fit, goes
 * on the stack. A struct returned in memory takes a hidden pointer in the
 * first register. NSInvocationCall (assembly) loads the registers and calls.
 * Minkar: this replaces a msgSendv that could not call anything correctly.
 */

enum { kClassNone = 0, kClassInteger = 1, kClassSSE = 2 };

static const char *skipQualifiers(const char *type) {
    while (*type && strchr("rnNoORVA", *type) != NULL)
        type++;
    return type;
}

static void markClass(int classes[2], NSUInteger offset, int cls) {
    NSUInteger word = offset / 8;

    if (word > 1)
        return;
    if (classes[word] == kClassInteger || cls == kClassInteger)
        classes[word] = kClassInteger;
    else
        classes[word] = cls;
}

// Marks the classes of the eightbytes a value of this type covers at
// offset. NO when the value must go in memory.
static BOOL classifyType(const char *type, NSUInteger offset, int classes[2]) {
    NSUInteger size, align;

    type = skipQualifiers(type);
    NSGetSizeAndAlignment(type, &size, &align);
    if (align == 0)
        align = 1;
    if (offset % align != 0)
        return NO;
    switch (*type) {
    case '{':
    case '(': {
        BOOL       isUnion = (*type == '(');
        const char *p = type + 1;
        NSUInteger at = offset;

        while (*p && *p != '=' && *p != '}' && *p != ')')
            p++;
        if (*p != '=')
            return size == 0;
        p++;
        while (*p && *p != '}' && *p != ')') {
            NSUInteger fieldSize, fieldAlign;
            const char *next;

            if (*p == '"') {                // a field name
                p = strchr(p + 1, '"');
                if (p == NULL)
                    return NO;
                p++;
                continue;
            }
            next = NSGetSizeAndAlignment(p, &fieldSize, &fieldAlign);
            if (fieldAlign == 0)
                fieldAlign = 1;
            if (!isUnion)
                at = (at + fieldAlign - 1) / fieldAlign * fieldAlign;
            if (!classifyType(p, isUnion ? offset : at, classes))
                return NO;
            if (!isUnion)
                at += fieldSize;
            if (next == p)
                return NO;
            p = next;
        }
        return YES;
    }
    case '[': {
        char       *end;
        unsigned long count = strtoul(type + 1, &end, 10);
        NSUInteger elementSize, elementAlign;

        NSGetSizeAndAlignment(end, &elementSize, &elementAlign);
        for (unsigned long k = 0; k < count; k++)
            if (!classifyType(end, offset + k * elementSize, classes))
                return NO;
        return YES;
    }
    case 'f':
    case 'd':
        markClass(classes, offset, kClassSSE);
        return YES;
    case 'D':                               // long double: x87, in memory
        return NO;
    default:
        markClass(classes, offset, kClassInteger);
        if (size > 8)
            markClass(classes, offset + 8, kClassInteger);
        return YES;
    }
}

// The 8-byte register image of a scalar argument of this type, widened
// as clang expects (signed types sign-extended, unsigned ones zero-extended).
static uint64_t widenScalar(const char *type, const uint8_t *bytes) {
    int8_t   c;
    uint8_t  uc;
    int16_t  sh;
    uint16_t us;
    int32_t  in;
    uint32_t ui;
    uint64_t word = 0;

    switch (*skipQualifiers(type)) {
    case 'c': memcpy(&c, bytes, 1); return (uint64_t)(int64_t)c;
    case 'C':
    case 'B': memcpy(&uc, bytes, 1); return uc;
    case 's': memcpy(&sh, bytes, 2); return (uint64_t)(int64_t)sh;
    case 'S': memcpy(&us, bytes, 2); return us;
    case 'i':
    case 'l': memcpy(&in, bytes, 4); return (uint64_t)(int64_t)in;
    case 'I':
    case 'L':
    case 'f': memcpy(&ui, bytes, 4); return ui;
    default:  memcpy(&word, bytes, 8); return word;
    }
}

// Where each eightbyte of each argument goes: a register or a stack word.
enum { kInGPR = 0, kInXMM = 1, kOnStack = 2 };

typedef struct {
    uint8_t  where;
    uint8_t  scalar;            // a lone scalar: widen it in a register
    uint16_t slot;              // register number or stack word
} NSArgumentWord;

// YES when the return value goes in memory (a hidden pointer in the first
// register); otherwise classes[] says which eightbytes come in rax/rdx and
// which in xmm0/xmm1.
-(BOOL)_returnInMemory:(int *)classes {
    const char *type = skipQualifiers([_signature methodReturnType]);

    classes[0] = classes[1] = kClassNone;
    if (*type == 'D')
        [NSException raise:NSInvalidArgumentException format:@"NSInvocation: long double return values are not supported"];
    if (*type == 'v' || _returnSize == 0)
        return NO;
    return _returnSize > 16 || !classifyType(type, 0, classes);
}

// Places every argument as the caller of a method with this signature
// does. words[first[i] + w] is eightbyte w of argument i.
-(void)_placeArguments:(BOOL)hiddenReturn words:(NSArgumentWord *)words first:(NSUInteger *)first
    stackWords:(NSUInteger *)stackWordsOut {
    NSUInteger count = [_signature numberOfArguments], gprs = hiddenReturn ? 1 : 0, xmms = 0;
    NSUInteger stackWords = 0, next = 0, i;

    for (i = 0; i < count; i++) {
        const char *type = skipQualifiers([_signature getArgumentTypeAtIndex:i]);
        NSUInteger  size = _argumentSizes[i], nwords = (size + 7) / 8, align, ints = 0, sses = 0, w;
        int         classes[2] = { kClassNone, kClassNone };
        BOOL        inMemory, scalar = (*type != '{' && *type != '(' && *type != '[');

        first[i] = next;
        if (*type == 'D')
            [NSException raise:NSInvalidArgumentException format:@"NSInvocation: long double arguments are not supported"];
        if (size == 0)
            continue;
        NSGetSizeAndAlignment(type, NULL, &align);
        inMemory = size > 16 || !classifyType(type, 0, classes);
        if (!inMemory) {
            for (w = 0; w < nwords; w++) {
                if (classes[w] == kClassSSE)
                    sses++;
                else
                    ints++;
            }
            inMemory = gprs + ints > 6 || xmms + sses > 8;
        }
        if (inMemory && align > 8 && stackWords % 2 != 0)
            stackWords++;
        for (w = 0; w < nwords; w++, next++) {
            words[next].scalar = scalar;
            if (inMemory) {
                words[next].where = kOnStack;
                words[next].slot = (uint16_t)stackWords++;
            } else if (classes[w] == kClassSSE) {
                words[next].where = kInXMM;
                words[next].slot = (uint16_t)xmms++;
            } else {
                words[next].where = kInGPR;
                words[next].slot = (uint16_t)gprs++;
            }
        }
    }
    *stackWordsOut = stackWords;
}

- (void)invoke
{
    id                    target = [self target];
    SEL                   selector = [self selector];
    NSUInteger            count = [_signature numberOfArguments], stackWords, i;
    NSArgumentWord       *words;
    NSUInteger           *first;
    NSInvocationCallFrame frame;
    uint64_t             *stack;
    int                   returnClasses[2];
    BOOL                  returnInMemory = [self _returnInMemory:returnClasses];
    void                 *imp;

    memset(_returnValue, 0, MAX(_returnSize, sizeof(NSUInteger)));
    if (target == nil)
        return;                             // messages to nil return zero

    words = NSZoneCalloc(NULL, _argumentFrameSize / 8 + 1, sizeof(NSArgumentWord));
    first = NSZoneCalloc(NULL, count + 1, sizeof(NSUInteger));
    [self _placeArguments:returnInMemory words:words first:first stackWords:&stackWords];

    memset(&frame, 0, sizeof(frame));
    stack = NSZoneCalloc(NULL, stackWords + 1, sizeof(uint64_t));
    if (returnInMemory)
        frame.gpr[0] = (uint64_t)(uintptr_t)_returnValue;
    for (i = 0; i < count; i++) {
        const char    *type = [_signature getArgumentTypeAtIndex:i];
        const uint8_t *bytes = _argumentFrame + _argumentOffsets[i];
        NSUInteger     size = _argumentSizes[i];

        for (NSUInteger w = 0; w < (size + 7) / 8; w++) {
            NSArgumentWord *place = &words[first[i] + w];
            uint64_t        word = 0;

            if (place->scalar && size <= 8)
                word = widenScalar(type, bytes);
            else
                memcpy(&word, bytes + 8 * w, MIN(8, size - 8 * w));
            if (place->where == kInGPR)
                frame.gpr[place->slot] = word;
            else if (place->where == kInXMM)
                frame.xmm[place->slot] = word;
            else
                stack[place->slot] = word;
        }
    }
    frame.stack = stack;
    frame.stackCount = stackWords;

    // class_getMethodImplementation gives _objc_msgForward for a selector
    // the class does not implement, so forwarding works as usual.
    if (returnInMemory)
        imp = (void *)class_getMethodImplementation_stret(object_getClass(target), selector);
    else
        imp = (void *)class_getMethodImplementation(object_getClass(target), selector);
    NSInvocationCall(imp, &frame);
    NSZoneFree(NULL, stack);
    NSZoneFree(NULL, words);
    NSZoneFree(NULL, first);

    if (!returnInMemory) {
        uint64_t regs[4] = { frame.rax, frame.rdx, frame.xmm0, frame.xmm1 };
        [self _setReturnFromRegisters:regs classes:returnClasses];
    }
}

// The return value from rax, rdx, xmm0, xmm1 (regs[0..3]) by its classes.
-(void)_setReturnFromRegisters:(const uint64_t *)regs classes:(const int *)classes {
    NSUInteger nextInt = 0, nextSSE = 0;
    uint8_t    image[16];

    if (_returnSize == 0 || _returnSize > 16)
        return;
    for (NSUInteger w = 0; w < (_returnSize + 7) / 8; w++) {
        uint64_t word = (classes[w] == kClassSSE) ? regs[2 + nextSSE++] : regs[nextInt++];

        memcpy(image + 8 * w, &word, 8);
    }
    memcpy(_returnValue, image, _returnSize);
}

// The other way round, for a forwarded message's caller.
-(void)_storeReturnInRegisters:(uint64_t *)regs classes:(const int *)classes {
    NSUInteger nextInt = 0, nextSSE = 0;
    uint8_t    image[16] = { 0 };

    regs[0] = regs[1] = regs[2] = regs[3] = 0;
    if (_returnSize == 0 || _returnSize > 16)
        return;
    memcpy(image, _returnValue, _returnSize);
    for (NSUInteger w = 0; w < (_returnSize + 7) / 8; w++) {
        uint64_t word;

        memcpy(&word, image + 8 * w, 8);
        if (classes[w] == kClassSSE)
            regs[2 + nextSSE++] = word;
        else
            regs[nextInt++] = word;
    }
}

// Sets every argument from a call's registers (regs[0..5] the integer
// ones, regs[6..13] the vector ones) and stack words.
-(void)_setArgumentsFromRegisters:(const uint64_t *)regs stack:(const uint64_t *)stack
    hiddenReturn:(BOOL)hiddenReturn {
    NSUInteger      count = [_signature numberOfArguments], stackWords, i;
    NSArgumentWord *words = NSZoneCalloc(NULL, _argumentFrameSize / 8 + 1, sizeof(NSArgumentWord));
    NSUInteger     *first = NSZoneCalloc(NULL, count + 1, sizeof(NSUInteger));

    [self _placeArguments:hiddenReturn words:words first:first stackWords:&stackWords];
    for (i = 0; i < count; i++) {
        NSUInteger size = _argumentSizes[i];

        for (NSUInteger w = 0; w < (size + 7) / 8; w++) {
            NSArgumentWord *place = &words[first[i] + w];
            uint64_t        word;

            if (place->where == kInGPR)
                word = regs[place->slot];
            else if (place->where == kInXMM)
                word = regs[6 + place->slot];
            else
                word = stack[place->slot];
            memcpy(_argumentFrame + _argumentOffsets[i] + 8 * w, &word, MIN(8, size - 8 * w));
        }
    }
    NSZoneFree(NULL, words);
    NSZoneFree(NULL, first);
}


-(id)description
{
   return [NSString stringWithFormat:@"<%@ with signature %@>", [super description], [_signature description]];
}

@end

/*
 * Forwarding (NSInvocationCall-x86_64.s calls these from the handlers that
 * NSObject installs with objc_setForwardHandler).
 */
extern id NSObjCGetFastForwardTarget(id object, SEL selector);

@interface NSInvocation (NSInvocationForwarding)
-(BOOL)_returnInMemory:(int *)classes;
-(void)_setArgumentsFromRegisters:(const uint64_t *)regs stack:(const uint64_t *)stack
    hiddenReturn:(BOOL)hiddenReturn;
-(void)_storeReturnInRegisters:(uint64_t *)regs classes:(const int *)classes;
@end

void *NSInvocationFastTarget(const uint64_t *regs, uint64_t structReturn);
void NSInvocationForwardRegisters(const uint64_t *regs, const uint64_t *stack, uint64_t *returnRegs,
    uint64_t structReturn);

// forwardingTargetForSelector:'s answer, or NULL.
void *NSInvocationFastTarget(const uint64_t *regs, uint64_t structReturn) {
    id  object = (id)(uintptr_t)regs[structReturn ? 1 : 0];
    SEL selector = (SEL)(uintptr_t)regs[structReturn ? 2 : 1];

    return NSObjCGetFastForwardTarget(object, selector);
}

void NSInvocationForwardRegisters(const uint64_t *regs, const uint64_t *stack, uint64_t *returnRegs,
    uint64_t structReturn) {
    id                 object = (id)(uintptr_t)regs[structReturn ? 1 : 0];
    SEL                selector = (SEL)(uintptr_t)regs[structReturn ? 2 : 1];
    NSMethodSignature *signature = [object methodSignatureForSelector:selector];
    NSInvocation      *invocation;
    int                classes[2];
    BOOL               inMemory;

    memset(returnRegs, 0, 4 * sizeof(uint64_t));
    if (signature == nil) {
        [object doesNotRecognizeSelector:selector];
        return;
    }
    invocation = [NSInvocation invocationWithMethodSignature:signature];
    inMemory = [invocation _returnInMemory:classes];
    [invocation _setArgumentsFromRegisters:regs stack:stack hiddenReturn:(BOOL)structReturn];
    [object forwardInvocation:invocation];
    if (inMemory || structReturn) {
        void *buffer = (void *)(uintptr_t)regs[0];

        if (structReturn && buffer != NULL)
            [invocation getReturnValue:buffer];
        returnRegs[0] = regs[0];
    } else {
        [invocation _storeReturnInRegisters:returnRegs classes:classes];
    }
}
