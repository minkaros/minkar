/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreFoundation's Objective-C classes made at run time, for a program
 * that loads CoreFoundation but not Foundation (a Swift program on the
 * Dispatch or CoreFoundation overlays retains blocks and CF objects as
 * Objective-C objects). They are made with the runtime's functions as
 * CoreFoundation initializes (__CFInitialize): a compiled class in this
 * image would be set up by the runtime while the image is still
 * initializing.
 *
 * Blocks as Objective-C objects. A block's isa is one of the blocks
 * runtime's _NSConcrete*Block symbols (libsystem_blocks): storage the size
 * of a class (32 pointers), left zeroed. Here each one becomes a real
 * class, built in place as a subclass of NSBlock, so blocks answer
 * messages: retain, release and copy (which ARC and Swift send all the
 * time), description, isKindOfClass:. Without this a message to a block
 * jumped through a zeroed class.
 *
 * They are CoreFoundation's, as on the reference platform.
 *
 * CF's objects: until Foundation gives them its classes (__NSCFType and
 * the rest), the instances of every type are __CFType, which answers
 * retain, release, hash, isEqual: and description with CF's functions.
 * Objects made before Foundation loads keep it.
 */

#include <Block.h>
#include <CoreFoundation/CFBase.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <string.h>

extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteMallocBlock[32];
extern void *_NSConcreteAutoBlock[32];
extern void *_NSConcreteFinalizingBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern void *_NSConcreteWeakBlockVariable[32];

/* objc4's in-place class construction (objc-internal.h). */
OBJC_EXPORT Class objc_initializeClassPair(Class superclass, const char *name,
                                          Class cls, Class metacls);

CF_PRIVATE void __CFMakeObjCClasses(void);
extern uintptr_t __CFRuntimeCoreObjCClass;
uintptr_t __CFRuntimeCoreObjCClass = 0;
extern uintptr_t __CFRuntimeDefaultObjCClass;

#define BLOCK_CLASSES 6
static uintptr_t blockMetaclasses[BLOCK_CLASSES][32];	/* OBJC_MAX_CLASS_SIZE each */
static Class blockSuperclass;	/* NSObject: autorelease */


static int
isHeapBlock(id block)
{
	return object_getClass(block) == (Class)(void *)_NSConcreteMallocBlock;
}

/* Heap blocks count references in the blocks runtime; stack and global
 * blocks live as long as their frame or the program, and ignore both. */
static id
blockRetain(id self, SEL _cmd)
{
	(void)_cmd;
	return isHeapBlock(self) ? (id)_Block_copy(self) : self;
}

static void
blockRelease(id self, SEL _cmd)
{
	(void)_cmd;
	if (isHeapBlock(self))
		_Block_release(self);
}

static id
blockAutorelease(id self, SEL _cmd)
{
	if (!isHeapBlock(self))
		return self;
	struct objc_super sup = { self, blockSuperclass };
	return ((id (*)(struct objc_super *, SEL))objc_msgSendSuper)(&sup, _cmd);
}

static uintptr_t
blockRetainCount(id self, SEL _cmd)
{
	(void)self; (void)_cmd;
	return UINTPTR_MAX;	/* not meaningful for blocks */
}

/* A stack block is copied to the heap; a heap block gains a reference; a
 * global block is itself. */
static id
blockCopy(id self, SEL _cmd)
{
	(void)_cmd;
	return (id)_Block_copy(self);
}

static id
blockCopyWithZone(id self, SEL _cmd, void *zone)
{
	(void)_cmd; (void)zone;
	return (id)_Block_copy(self);
}

static void
blockInvoke(id self, SEL _cmd)
{
	(void)_cmd;
	((void (^)(void))(void *)self)();
}

static void
buildBlockClass(Class block, void *storage, const char *name, int index)
{
	if (objc_lookUpClass(name) != Nil)
		return;
	Class cls = objc_initializeClassPair(block, name, (Class)storage,
	    (Class)(void *)blockMetaclasses[index]);
	if (cls != Nil)
		objc_registerClassPair(cls);
}

/* -- CF's objects -- */

static id
cfRetain(id self, SEL _cmd)
{
	(void)_cmd;
	CFRetain((CFTypeRef)self);
	return self;
}

static void
cfRelease(id self, SEL _cmd)
{
	(void)_cmd;
	CFRelease((CFTypeRef)self);
}

static uintptr_t
cfRetainCount(id self, SEL _cmd)
{
	(void)_cmd;
	return (uintptr_t)CFGetRetainCount((CFTypeRef)self);
}

static void
cfDealloc(id self, SEL _cmd)
{
	(void)self; (void)_cmd;	/* CF frees it when its count reaches zero */
}

static uintptr_t
cfHash(id self, SEL _cmd)
{
	(void)_cmd;
	return (uintptr_t)CFHash((CFTypeRef)self);
}

static BOOL
cfIsEqual(id self, SEL _cmd, id other)
{
	(void)_cmd;
	if (other == self)
		return YES;
	if (other == nil)
		return NO;
	return CFEqual((CFTypeRef)self, (CFTypeRef)other);
}

static id
cfDescription(id self, SEL _cmd)
{
	(void)_cmd;
	CFStringRef description = CFCopyDescription((CFTypeRef)self);
	if (description == NULL)
		return nil;
	struct objc_super sup = { (id)description, blockSuperclass };
	return ((id (*)(struct objc_super *, SEL))objc_msgSendSuper)(&sup, sel_registerName("autorelease"));
}

static void
makeCFTypeClass(void)
{
	Class type;

	if (objc_lookUpClass("__CFType") != Nil)
		return;
	type = objc_allocateClassPair(blockSuperclass, "__CFType", 0);
	if (type == Nil)
		return;
	class_addMethod(type, sel_registerName("retain"), (IMP)cfRetain, "@@:");
	class_addMethod(type, sel_registerName("release"), (IMP)cfRelease, "Vv@:");
	class_addMethod(type, sel_registerName("retainCount"), (IMP)cfRetainCount, "Q@:");
	class_addMethod(type, sel_registerName("dealloc"), (IMP)cfDealloc, "v@:");
	class_addMethod(type, sel_registerName("hash"), (IMP)cfHash, "Q@:");
	class_addMethod(type, sel_registerName("isEqual:"), (IMP)cfIsEqual, "c@:@");
	class_addMethod(type, sel_registerName("description"), (IMP)cfDescription, "@@:");
	class_addMethod(type, sel_registerName("copyWithZone:"), (IMP)cfRetain, "@@:^v");
	objc_registerClassPair(type);
	__CFRuntimeCoreObjCClass = (uintptr_t)type;
	if (__CFRuntimeDefaultObjCClass == 0)
		__CFRuntimeDefaultObjCClass = (uintptr_t)type;
}

/* -- Blocks -- */

static void
makeBlockClasses(void)
{
	Class block;

	if (objc_lookUpClass("NSBlock") != Nil)
		return;
	block = objc_allocateClassPair(blockSuperclass, "NSBlock", 0);
	if (block == Nil)
		return;
	class_addMethod(block, sel_registerName("retain"), (IMP)blockRetain, "@@:");
	class_addMethod(block, sel_registerName("release"), (IMP)blockRelease, "Vv@:");
	class_addMethod(block, sel_registerName("autorelease"), (IMP)blockAutorelease, "@@:");
	class_addMethod(block, sel_registerName("retainCount"), (IMP)blockRetainCount, "Q@:");
	class_addMethod(block, sel_registerName("copy"), (IMP)blockCopy, "@@:");
	class_addMethod(block, sel_registerName("copyWithZone:"), (IMP)blockCopyWithZone, "@@:^v");
	class_addMethod(block, sel_registerName("invoke"), (IMP)blockInvoke, "v@:");
	objc_registerClassPair(block);

	buildBlockClass(block, _NSConcreteStackBlock, "__NSStackBlock__", 0);
	buildBlockClass(block, _NSConcreteMallocBlock, "__NSMallocBlock__", 1);
	buildBlockClass(block, _NSConcreteAutoBlock, "__NSAutoBlock__", 2);
	buildBlockClass(block, _NSConcreteFinalizingBlock, "__NSFinalizingBlock__", 3);
	buildBlockClass(block, _NSConcreteGlobalBlock, "__NSGlobalBlock__", 4);
	buildBlockClass(block, _NSConcreteWeakBlockVariable, "__NSBlockVariable__", 5);
}

CF_PRIVATE void
__CFMakeObjCClasses(void)
{
	blockSuperclass = objc_lookUpClass("NSObject");
	if (blockSuperclass == Nil)
		return;
	makeCFTypeClass();
	makeBlockClasses();
}
