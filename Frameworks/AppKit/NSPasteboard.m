/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSPasteboard.h>
#import <CoreServices/CoreServices.h>
#import <AppKit/NSRaise.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <string.h>
#include "Pasteboard.h"		/* MIG, generated from Pasteboard/Pasteboard.defs */
#include "PasteboardProvider.h"	/* MIG, from Pasteboard/PasteboardProvider.defs: */
#include "PasteboardProviderServer.h"	/* its client and its server */
#import <AppKit/NSApplication.h>
#import <AppKit/NSPasteboardItem.h>
#import <AppKit/NSWorkspace.h>
#include <CoreServices/CoreServices.h>

NSString *const NSPasteboardURLReadingFileURLsOnlyKey = @"NSPasteboardURLReadingFileURLsOnlyKey";
NSString *const NSPasteboardURLReadingContentsConformToTypesKey = @"NSPasteboardURLReadingContentsConformToTypesKey";

/* An object read for readObjectsForClasses:options: kept or not, by the
   URL options: only file URLs, only files of these types. */
static BOOL passesURLOptions(id object, NSDictionary *options) {
   if (![object isKindOfClass:[NSURL class]] || [options count] == 0)
      return YES;
   NSURL *url = object;
   NSArray *types = [options objectForKey:NSPasteboardURLReadingContentsConformToTypesKey];
   if ([[options objectForKey:NSPasteboardURLReadingFileURLsOnlyKey] boolValue] && ![url isFileURL])
      return NO;
   if ([types count] > 0) {
      if (![url isFileURL])
         return NO;
      NSString *uti = [[NSWorkspace sharedWorkspace] typeOfFile:[url path] error:NULL];
      if (uti == nil)
         return NO;
      for (NSString *want in types)
         if (UTTypeConformsTo((CFStringRef)uti, (CFStringRef)want))
            return YES;
      return NO;
   }
   return YES;
}

/*
 * Copyright (c) 2026 The Minkar contributors (the pasteboard server's
 * client below, the uniform type identifiers and the API of 10.6 and
 * later); same license as above.
 *
 * NSPasteboard keeps nothing itself: every pasteboard lives in the
 * pasteboard server (pboard, CoreServices/pboard), the caller's user's
 * own, so other processes of the same user see what this one writes.
 * Types are stored as uniform type identifiers; the older NS*PboardType
 * names are taken as the same types, and types lists both.
 *
 * Data given later: the types an owner declares (pasteboard:
 * provideDataForType:), an item's types with a data provider and the
 * types a writer marks NSPasteboardWritingPromised are promised to the
 * server with this process's provider port, a Mach port on the main run
 * loop. A reader in another process asks that port for the data, and
 * waits at most ten seconds (PasteboardProvider.defs); one in this
 * process gets it directly.
 */

NSString *const NSPasteboardTypeString = @"public.utf8-plain-text";
NSString *const NSPasteboardTypePDF = @"com.adobe.pdf";
NSString *const NSPasteboardTypeTIFF = @"public.tiff";
NSString *const NSPasteboardTypePNG = @"public.png";
NSString *const NSPasteboardTypeRTF = @"public.rtf";
NSString *const NSPasteboardTypeRTFD = @"com.apple.flat-rtfd";
NSString *const NSPasteboardTypeHTML = @"public.html";
NSString *const NSPasteboardTypeTabularText = @"public.utf8-tab-separated-values-text";
NSString *const NSPasteboardTypeFont = @"com.apple.cocoa.pasteboard.character-formatting";
NSString *const NSPasteboardTypeRuler = @"com.apple.cocoa.pasteboard.paragraph-formatting";
NSString *const NSPasteboardTypeColor = @"com.apple.cocoa.pasteboard.color";
NSString *const NSPasteboardTypeSound = @"com.apple.cocoa.pasteboard.sound";
NSString *const NSPasteboardTypeMultipleTextSelection = @"com.apple.cocoa.pasteboard.multiple-text-selection";
NSString *const NSPasteboardTypeTextFinderOptions = @"com.apple.cocoa.pasteboard.find-panel-search-options";
NSString *const NSPasteboardTypeURL = @"public.url";
NSString *const NSPasteboardTypeFileURL = @"public.file-url";

NSString * const NSColorPboardType=@"NSColorPboardType";
NSString * const NSFileContentsPboardType=@"NSFileContentsPboardType";
NSString * const NSFilenamesPboardType=@"NSFilenamesPboardType";
// The reference's value.
NSString * const NSFilesPromisePboardType=@"Apple files promise pasteboard type";
NSString * const NSFontPboardType=@"NSFontPboardType";
NSString * const NSPDFPboardType=@"NSPDFPboardType";
NSString * const NSPICTPboardType=@"NSPICTPboardType";
NSString * const NSPostScriptPboardType=@"NSPostScriptPboardType";
NSString * const NSRTFDPboardType=@"NSRTFDPboardType";
NSString * const NSRTFPboardType=@"NSRTFPboardType";
NSString * const NSRulerPboardType=@"NSRulerPboardType";
NSString * const NSStringPboardType=@"NSStringPboardType";
NSString * const NSTabularTextPboardType=@"NSTabularTextPboardType";
NSString * const NSTIFFPboardType=@"NSTIFFPboardType";
NSString * const NSURLPboardType=@"NSURLPboardType";
NSString * const NSHTMLPboardType=@"Apple HTML pasteboard type";

NSString * const NSDragPboard=@"Apple CFPasteboard drag";
NSString * const NSFindPboard=@"Apple CFPasteboard find";
NSString * const NSFontPboard=@"Apple CFPasteboard font";
NSString * const NSGeneralPboard=@"Apple CFPasteboard general";
NSString * const NSRulerPboard=@"Apple CFPasteboard ruler";

NSString * const NSPasteboardNameGeneral=@"Apple CFPasteboard general";
NSString * const NSPasteboardNameFont=@"Apple CFPasteboard font";
NSString * const NSPasteboardNameRuler=@"Apple CFPasteboard ruler";
NSString * const NSPasteboardNameFind=@"Apple CFPasteboard find";
NSString * const NSPasteboardNameDrag=@"Apple CFPasteboard drag";

/* The older names and the types they are. */
static NSDictionary *legacyTypes(void) {
   static NSDictionary *map;
   if (map == nil)
      map = [[NSDictionary alloc] initWithObjectsAndKeys:
         NSPasteboardTypeString, NSStringPboardType,
         NSPasteboardTypePDF, NSPDFPboardType,
         NSPasteboardTypeTIFF, NSTIFFPboardType,
         NSPasteboardTypeRTF, NSRTFPboardType,
         NSPasteboardTypeRTFD, NSRTFDPboardType,
         NSPasteboardTypeHTML, NSHTMLPboardType,
         NSPasteboardTypeTabularText, NSTabularTextPboardType,
         NSPasteboardTypeFont, NSFontPboardType,
         NSPasteboardTypeRuler, NSRulerPboardType,
         NSPasteboardTypeColor, NSColorPboardType,
         NSPasteboardTypeURL, NSURLPboardType,
         nil];
   return map;
}

static NSString *canonicalType(NSString *type) {
   NSString *uti = [legacyTypes() objectForKey:type];
   return uti != nil ? uti : type;
}

static NSString *legacyNameOf(NSString *uti) {
   NSArray *names = [legacyTypes() allKeysForObject:uti];
   return [names count] > 0 ? [names objectAtIndex:0] : nil;
}

/* ---- The server ---- */

static mach_port_t sServer = MACH_PORT_NULL;

static mach_port_t serverPort(BOOL again) {
   @synchronized([NSPasteboard class]) {
      if (again && sServer != MACH_PORT_NULL) {
         mach_port_deallocate(mach_task_self(), sServer);
         sServer = MACH_PORT_NULL;
      }
      if (sServer == MACH_PORT_NULL &&
          bootstrap_look_up(bootstrap_port, "org.minkar.pboard", &sServer) != KERN_SUCCESS)
         sServer = MACH_PORT_NULL;
      return sServer;
   }
}

/* A call, again once with a new lookup if the server went away. */
#define PB_CALL(call) ({ \
   kern_return_t _kr = MACH_SEND_INVALID_DEST; \
   for (int _try = 0; _try < 2; _try++) { \
      mach_port_t server = serverPort(_try > 0); \
      if (server == MACH_PORT_NULL) { _kr = MACH_SEND_INVALID_DEST; continue; } \
      _kr = (call); \
      if (_kr != MACH_SEND_INVALID_DEST && _kr != MIG_SERVER_DIED) break; \
   } \
   _kr; })

static BOOL nameFits(NSString *name, char *out) {
   return name != nil && [name getCString:out maxLength:sizeof(pb_name_t)
                                  encoding:NSUTF8StringEncoding];
}

@interface NSPasteboard () {
   NSMutableDictionary *_promised;   /* type -> the writer that gives it */
}
-(BOOL)_provideLocally:(NSString *)type;
-(NSInteger)_ownChangeCount;
-(NSData *)_serverDataForType:(NSString *)type result:(int32_t *)result;
@end

@interface NSPasteboardItem (NSPasteboardPromises)
-(id)_dataProviderForType:(NSPasteboardType)type;
-(id)_storedValueForType:(NSPasteboardType)type;
@end

/* ---- This process's provider port ---- */

static mach_port_t sProviderPort = MACH_PORT_NULL;
static CFAbsoluteTime sLastProvided;

/* When this process last gave promised data (a drag's source waits for
   the drop's answer while it does). */
CFAbsoluteTime _NSPasteboardLastProvidedTime(void);
CFAbsoluteTime _NSPasteboardLastProvidedTime(void) {
   return sLastProvided;
}

static void providerReceived(CFMachPortRef port, void *message, CFIndex size, void *info) {
   union {
      mig_reply_error_t error;
      uint8_t bytes[1024];
   } reply;

   @autoreleasepool {
      PasteboardProvider_server((mach_msg_header_t *)message, &reply.error.Head);
   }
   if (reply.error.Head.msgh_remote_port != MACH_PORT_NULL && reply.error.RetCode != MIG_NO_REPLY)
      mach_msg(&reply.error.Head, MACH_SEND_MSG | MACH_SEND_TIMEOUT, reply.error.Head.msgh_size, 0,
         MACH_PORT_NULL, 1000, MACH_PORT_NULL);
}

static mach_port_t providerPort(void) {
   @synchronized([NSPasteboard class]) {
      if (sProviderPort == MACH_PORT_NULL) {
         CFMachPortRef port = CFMachPortCreate(NULL, providerReceived, NULL, NULL);
         if (port == NULL)
            return MACH_PORT_NULL;
         CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL, port, 0);
         CFRunLoopRef main = CFRunLoopGetMain();
         /* Also while the main thread tracks a drag or runs a panel. */
         CFRunLoopAddSource(main, source, kCFRunLoopCommonModes);
         for (NSString *mode in [NSArray arrayWithObjects:NSDefaultRunLoopMode,
                NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode, nil])
            CFRunLoopAddSource(main, source, (CFStringRef)mode);
         CFRelease(source);
         sProviderPort = CFMachPortGetPort(port);
      }
      return sProviderPort;
   }
}

@implementation NSPasteboard

static NSMutableDictionary *sPasteboards;

+(NSPasteboard *)generalPasteboard {
   return [self pasteboardWithName:NSPasteboardNameGeneral];
}

+(NSPasteboard *)pasteboardWithName:(NSString *)name {
   if (name == nil)
      return nil;
   @synchronized(self) {
      if (sPasteboards == nil)
         sPasteboards = [[NSMutableDictionary alloc] init];
      NSPasteboard *p = [sPasteboards objectForKey:name];
      if (p == nil) {
         p = [[[self alloc] init] autorelease];
         p->_name = [name copy];
         p->_declaredTypes = [[NSMutableArray alloc] init];
         [sPasteboards setObject:p forKey:name];
      }
      return p;
   }
}

+(NSPasteboard *)pasteboardWithUniqueName {
   return [self pasteboardWithName:[NSString stringWithFormat:@"Minkar unique pasteboard %@",
      [[NSProcessInfo processInfo] globallyUniqueString]]];
}

-(void)dealloc {
   [_promised release];
   [_owner release];
   [_name release];
   [_declaredTypes release];
   [super dealloc];
}

-(NSString *)name {
   return _name;
}

-(void)releaseGlobally {
   pb_name_t n;
   if (nameFits(_name, n))
      PB_CALL(pb_release(server, n));
   @synchronized([NSPasteboard class]) {
      [[self retain] autorelease];
      [sPasteboards removeObjectForKey:_name];
   }
}

-(NSInteger)changeCount {
   pb_name_t n;
   int32_t count = 0;
   if (!nameFits(_name, n) || PB_CALL(pb_change_count(server, n, &count)) != KERN_SUCCESS)
      return 0;
   return count;
}

/* The change count of this process's last clear or declare. */
-(NSInteger)_ownChangeCount {
   return _changeCount;
}

/* Whether this process still owns what it declared last. */
-(BOOL)_declaredStillCurrent {
   return [_declaredTypes count] > 0 && [self changeCount] == _changeCount;
}

-(NSInteger)clearContents {
   pb_name_t n;
   int32_t count = 0;
   if (!nameFits(_name, n) || PB_CALL(pb_clear(server, n, &count)) != KERN_SUCCESS)
      return 0;
   id previous = _owner;
   _owner = nil;
   _changeCount = count;
   [_declaredTypes removeAllObjects];
   [_promised removeAllObjects];
   if (previous != nil && [previous respondsToSelector:@selector(pasteboardChangedOwner:)])
      [previous pasteboardChangedOwner:self];
   [previous release];
   return count;
}

/* Types given later, promised to the server with this process's port. */
-(void)_promiseTypes:(NSArray *)types {
   NSMutableData *kinds = [NSMutableData data];
   pb_name_t n;
   int32_t result = -1;

   for (NSString *t in types) {
      const char *c = [canonicalType(t) UTF8String];
      if (c != NULL && strlen(c) < sizeof(pb_name_t))
         [kinds appendBytes:c length:strlen(c) + 1];
   }
   mach_port_t port = providerPort();
   if ([kinds length] == 0 || !nameFits(_name, n) || port == MACH_PORT_NULL ||
       mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
      return;
   /* The send right goes with the message; not sent, it is still ours. */
   if (PB_CALL(pb_promise(server, n, (int32_t)_changeCount, (uint8_t *)[kinds bytes],
       (mach_msg_type_number_t)[kinds length], port, &result)) == MACH_SEND_INVALID_DEST)
      mach_port_deallocate(mach_task_self(), port);
}

/* The types of an owner that gives data later. */
-(void)_promiseOwnerTypes:(NSArray *)types {
   if ([_owner respondsToSelector:@selector(pasteboard:provideDataForType:)])
      [self _promiseTypes:types];
}

-(NSInteger)declareTypes:(NSArray *)types owner:(id)owner {
   NSInteger count = [self clearContents];
   _owner = [owner retain];
   for (NSString *t in types)
      if (![_declaredTypes containsObject:canonicalType(t)])
         [_declaredTypes addObject:canonicalType(t)];
   [self _promiseOwnerTypes:types];
   return count;
}

-(NSInteger)addTypes:(NSArray *)types owner:(id)owner {
   if (![self _declaredStillCurrent] && [_declaredTypes count] == 0)
      _changeCount = [self changeCount];
   if (owner != nil && owner != _owner) {
      [_owner release];
      _owner = [owner retain];
   }
   for (NSString *t in types)
      if (![_declaredTypes containsObject:canonicalType(t)])
         [_declaredTypes addObject:canonicalType(t)];
   if (owner != nil)
      [self _promiseOwnerTypes:types];
   return _changeCount;
}

-(NSArray *)_serverTypes {
   pb_name_t n;
   uint8_t *list = NULL;
   mach_msg_type_number_t len = 0;
   int32_t count = 0;
   NSMutableArray *types = [NSMutableArray array];

   if (!nameFits(_name, n) ||
       PB_CALL(pb_types(server, n, &list, &len, &count)) != KERN_SUCCESS)
      return types;
   for (size_t at = 0; at < len; ) {
      size_t l = strnlen((const char *)list + at, len - at);
      NSString *t = [[NSString alloc] initWithBytes:list + at length:l
                                           encoding:NSUTF8StringEncoding];
      if (t != nil)
         [types addObject:t];
      [t release];
      at += l + 1;
   }
   if (list != NULL)
      vm_deallocate(mach_task_self(), (vm_address_t)list, len);
   return types;
}

/* The types as identifiers: written ones, and declared ones still ours. */
-(NSArray *)_canonicalTypes {
   NSMutableArray *types = [NSMutableArray arrayWithArray:[self _serverTypes]];
   if ([self _declaredStillCurrent])
      for (NSString *t in _declaredTypes)
         if (![types containsObject:t])
            [types addObject:t];
   return types;
}

-(NSArray *)types {
   NSMutableArray *result = [NSMutableArray array];
   for (NSString *t in [self _canonicalTypes]) {
      [result addObject:t];
      NSString *old = legacyNameOf(t);
      if (old != nil && ![result containsObject:old])
         [result addObject:old];
   }
   return result;
}

-(NSString *)availableTypeFromArray:(NSArray *)types {
   NSArray *have = [self _canonicalTypes];
   for (NSString *t in types)
      if ([have containsObject:canonicalType(t)])
         return t;
   return nil;
}

/* The type's data in the server; result: 0 there, 1 promised, -1 neither. */
-(NSData *)_serverDataForType:(NSString *)type result:(int32_t *)out {
   pb_name_t n, k;
   uint8_t *bytes = NULL;
   mach_msg_type_number_t len = 0;
   int32_t result = -1;

   if (out != NULL)
      *out = -1;
   if (!nameFits(_name, n) || !nameFits(canonicalType(type), k) ||
       PB_CALL(pb_get_data(server, n, k, &bytes, &len, &result)) != KERN_SUCCESS)
      return nil;
   if (out != NULL)
      *out = result;
   if (result != 0)
      return nil;
   NSData *data = [NSData dataWithBytes:bytes length:len];
   if (bytes != NULL)
      vm_deallocate(mach_task_self(), (vm_address_t)bytes, len);
   return data;
}

-(void)_writeValue:(id)value forType:(NSString *)type {
   if ([value isKindOfClass:[NSData class]])
      [self setData:value forType:type];
   else if ([value isKindOfClass:[NSString class]])
      [self setString:value forType:type];
   else if (value != nil)
      [self setPropertyList:value forType:type];
}

/* A type this process promised or declared and has not written: its
   writer, item's data provider or owner gives it now. */
-(BOOL)_provideLocally:(NSString *)type {
   NSString *t = canonicalType(type);
   if (![self _declaredStillCurrent])
      return NO;
   id writer = [[[_promised objectForKey:t] retain] autorelease];
   if (writer != nil) {
      if ([writer isKindOfClass:[NSPasteboardItem class]]) {
         id provider = [writer _dataProviderForType:t];
         [provider pasteboard:self item:writer provideDataForType:t];
         /* Given to the item rather than to the pasteboard. */
         if ([self _serverDataForType:t result:NULL] == nil)
            [self _writeValue:[writer _storedValueForType:t] forType:t];
      } else
         [self _writeValue:[writer pasteboardPropertyListForType:t] forType:t];
      return YES;
   }
   if (_owner != nil && [_declaredTypes containsObject:t] &&
       [_owner respondsToSelector:@selector(pasteboard:provideDataForType:)]) {
      [_owner pasteboard:self provideDataForType:type];
      return YES;
   }
   return NO;
}

/* A type another process promised: asked of it, through its port. */
-(BOOL)_askProviderForType:(NSString *)type {
   pb_name_t n, k;
   mach_port_t port = MACH_PORT_NULL;
   int32_t count = 0, result = -1;

   if (!nameFits(_name, n) || !nameFits(canonicalType(type), k) ||
       PB_CALL(pb_provider(server, n, &port, &count)) != KERN_SUCCESS || port == MACH_PORT_NULL)
      return NO;
   /* This process's own port: what it promised is no longer current. */
   if (port != sProviderPort &&
       pbp_provide(port, n, k, count, &result) != KERN_SUCCESS)
      result = -1;
   mach_port_deallocate(mach_task_self(), port);
   return result == 0;
}

-(NSData *)dataForType:(NSString *)type {
   int32_t result;
   NSData *data = [self _serverDataForType:type result:&result];

   if (data == nil && ([self _provideLocally:type] || (result == 1 && [self _askProviderForType:type])))
      data = [self _serverDataForType:type result:NULL];
   return data;
}

-(NSString *)stringForType:(NSString *)type {
   NSData *data = [self dataForType:type];
   if (data == nil)
      return nil;
   return [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
}

-(id)propertyListForType:(NSString *)type {
   NSData *data = [self dataForType:type];
   if (data == nil)
      return nil;
   return [NSPropertyListSerialization propertyListWithData:data
      options:NSPropertyListImmutable format:NULL error:NULL];
}

-(BOOL)setData:(NSData *)data forType:(NSString *)type {
   pb_name_t n, k;
   int32_t result = -1;

   if (data == nil || !nameFits(_name, n) || !nameFits(canonicalType(type), k))
      return NO;
   if (PB_CALL(pb_set_data(server, n, (int32_t)_changeCount, k, (uint8_t *)(uintptr_t)[data bytes],
       (mach_msg_type_number_t)[data length], &result)) != KERN_SUCCESS)
      return NO;
   return result == 0;
}

-(BOOL)setString:(NSString *)string forType:(NSString *)type {
   return [self setData:[string dataUsingEncoding:NSUTF8StringEncoding] forType:type];
}

-(BOOL)setPropertyList:(id)plist forType:(NSString *)type {
   NSData *data = [NSPropertyListSerialization dataWithPropertyList:plist
      format:NSPropertyListXMLFormat_v1_0 options:0 error:NULL];
   return [self setData:data forType:type];
}

/* ---- Objects (10.6 and later) ---- */

/* Whether a writer gives this type only when it is read: an item's type
   with a data provider, or one the writer marks promised. */
static BOOL givenLater(id object, NSString *type, NSPasteboard *pasteboard) {
   if ([object isKindOfClass:[NSPasteboardItem class]])
      return [object _dataProviderForType:type] != nil && [object _storedValueForType:type] == nil;
   return [object respondsToSelector:@selector(writingOptionsForType:pasteboard:)] &&
      ([object writingOptionsForType:type pasteboard:pasteboard] & NSPasteboardWritingPromised);
}

-(BOOL)writeObjects:(NSArray *)objects {
   BOOL wrote = NO;
   NSMutableArray *later = [NSMutableArray array];
   for (id object in objects) {
      if (![object respondsToSelector:@selector(writableTypesForPasteboard:)])
         continue;
      NSArray *types = [object writableTypesForPasteboard:self];
      [self addTypes:types owner:nil];
      for (NSString *type in types) {
         if (givenLater(object, type, self)) {
            if (_promised == nil)
               _promised = [[NSMutableDictionary alloc] init];
            [_promised setObject:object forKey:canonicalType(type)];
            [later addObject:type];
            wrote = YES;
            continue;
         }
         id plist = [object pasteboardPropertyListForType:type];
         if ([plist isKindOfClass:[NSData class]])
            wrote |= [self setData:plist forType:type];
         else if ([plist isKindOfClass:[NSString class]])
            wrote |= [self setString:plist forType:type];
         else if (plist != nil)
            wrote |= [self setPropertyList:plist forType:type];
      }
   }
   [self _promiseTypes:later];
   return wrote;
}

-(NSArray *)readObjectsForClasses:(NSArray *)classArray options:(NSDictionary *)options {
   NSMutableArray *objects = [NSMutableArray array];
   NSArray *have = [self _canonicalTypes];
   for (Class c in classArray) {
      if (![c respondsToSelector:@selector(readableTypesForPasteboard:)])
         continue;
      for (NSString *type in [c readableTypesForPasteboard:self]) {
         if (![have containsObject:canonicalType(type)])
            continue;
         NSPasteboardReadingOptions how = NSPasteboardReadingAsData;
         if ([c respondsToSelector:@selector(readingOptionsForType:pasteboard:)])
            how = [c readingOptionsForType:type pasteboard:self];
         id plist = (how & NSPasteboardReadingAsString) ? (id)[self stringForType:type] :
            (how & NSPasteboardReadingAsPropertyList) ? [self propertyListForType:type] :
            (id)[self dataForType:type];
         id object = plist != nil ? [[[c alloc] initWithPasteboardPropertyList:plist
                                                                      ofType:type] autorelease] : nil;
         if (object != nil && !passesURLOptions(object, options))
            object = nil;
         if (object != nil) {
            [objects addObject:object];
            break;
         }
      }
      if ([objects count] > 0)
         break;
   }
   return [objects count] > 0 ? objects : nil;
}

-(BOOL)canReadObjectForClasses:(NSArray *)classArray options:(NSDictionary *)options {
   return [self readObjectsForClasses:classArray options:options] != nil;
}

/* Minkar: whether one of its types is one of these, or conforms to one
   (by the type identifiers' conformance). */
-(BOOL)canReadItemWithDataConformingToTypes:(NSArray *)types {
   for(NSString *have in [self types])
    for(NSString *want in types)
     if([have isEqualToString:want] || UTTypeConformsTo((CFStringRef)have,(CFStringRef)want))
      return YES;
   return NO;
}

@end

/* ---- Foundation's classes on the pasteboard ---- */

@implementation NSString (NSPasteboardSupport)

-(NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
   return [NSArray arrayWithObject:NSPasteboardTypeString];
}

-(id)pasteboardPropertyListForType:(NSString *)type {
   return [canonicalType(type) isEqualToString:NSPasteboardTypeString] ? self : nil;
}

+(NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
   return [NSArray arrayWithObjects:NSPasteboardTypeString, NSPasteboardTypeURL,
      NSPasteboardTypeFileURL, nil];
}

+(NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard {
   return NSPasteboardReadingAsString;
}

-(id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSString *)type {
   if (![propertyList isKindOfClass:[NSString class]]) {
      [self release];
      return nil;
   }
   return [self initWithString:propertyList];
}

@end

@implementation NSURL (NSPasteboardSupport)

-(NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
   if ([self isFileURL])
      return [NSArray arrayWithObjects:NSPasteboardTypeFileURL, NSPasteboardTypeURL,
         NSPasteboardTypeString, nil];
   return [NSArray arrayWithObjects:NSPasteboardTypeURL, NSPasteboardTypeString, nil];
}

-(id)pasteboardPropertyListForType:(NSString *)type {
   NSString *t = canonicalType(type);
   if ([t isEqualToString:NSPasteboardTypeString] && [self isFileURL])
      return [self path];
   if ([t isEqualToString:NSPasteboardTypeURL] || [t isEqualToString:NSPasteboardTypeFileURL] ||
       [t isEqualToString:NSPasteboardTypeString])
      return [self absoluteString];
   return nil;
}

+(NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
   return [NSArray arrayWithObjects:NSPasteboardTypeFileURL, NSPasteboardTypeURL, nil];
}

+(NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard {
   return NSPasteboardReadingAsString;
}

-(id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSString *)type {
   if (![propertyList isKindOfClass:[NSString class]]) {
      [self release];
      return nil;
   }
   return [self initWithString:propertyList];
}

+(NSURL *)URLFromPasteboard:(NSPasteboard *)pasteboard {
   NSArray *urls = [pasteboard readObjectsForClasses:[NSArray arrayWithObject:self] options:nil];
   return [urls count] > 0 ? [urls objectAtIndex:0] : nil;
}

-(void)writeToPasteboard:(NSPasteboard *)pasteboard {
   [pasteboard writeObjects:[NSArray arrayWithObject:self]];
}

@end

/* ---- What another process asks of this one ---- */

kern_return_t pbps_provide(mach_port_t provider, pb_name_t name, pb_name_t kind, int32_t count,
    int32_t *result) {
   NSString *pasteboardName = [NSString stringWithUTF8String:name];
   NSString *type = [NSString stringWithUTF8String:kind];
   NSPasteboard *pasteboard = nil;

   *result = -1;
   sLastProvided = CFAbsoluteTimeGetCurrent();
   if (pasteboardName != nil)
      @synchronized([NSPasteboard class]) {
         pasteboard = [[[sPasteboards objectForKey:pasteboardName] retain] autorelease];
      }
   if (pasteboard != nil && type != nil && [pasteboard _ownChangeCount] == count &&
       [pasteboard _provideLocally:type] && [pasteboard _serverDataForType:type result:NULL] != nil)
      *result = 0;
   sLastProvided = CFAbsoluteTimeGetCurrent();
   return KERN_SUCCESS;
}
