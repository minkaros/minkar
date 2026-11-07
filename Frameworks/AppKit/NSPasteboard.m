/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSPasteboard.h>
#import <AppKit/NSRaise.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <string.h>
#include "Pasteboard.h"		/* MIG, generated from Pasteboard/Pasteboard.defs */

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
   if (previous != nil && [previous respondsToSelector:@selector(pasteboardChangedOwner:)])
      [previous pasteboardChangedOwner:self];
   [previous release];
   return count;
}

-(NSInteger)declareTypes:(NSArray *)types owner:(id)owner {
   NSInteger count = [self clearContents];
   _owner = [owner retain];
   for (NSString *t in types)
      if (![_declaredTypes containsObject:canonicalType(t)])
         [_declaredTypes addObject:canonicalType(t)];
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

-(NSData *)_serverDataForType:(NSString *)type {
   pb_name_t n, k;
   uint8_t *bytes = NULL;
   mach_msg_type_number_t len = 0;
   int32_t result = -1;

   if (!nameFits(_name, n) || !nameFits(canonicalType(type), k) ||
       PB_CALL(pb_get_data(server, n, k, &bytes, &len, &result)) != KERN_SUCCESS || result != 0)
      return nil;
   NSData *data = [NSData dataWithBytes:bytes length:len];
   if (bytes != NULL)
      vm_deallocate(mach_task_self(), (vm_address_t)bytes, len);
   return data;
}

-(NSData *)dataForType:(NSString *)type {
   NSData *data = [self _serverDataForType:type];
   /* Declared and not written yet: the owner gives it now. */
   if (data == nil && _owner != nil && [self _declaredStillCurrent] &&
       [_declaredTypes containsObject:canonicalType(type)] &&
       [_owner respondsToSelector:@selector(pasteboard:provideDataForType:)]) {
      [_owner pasteboard:self provideDataForType:type];
      data = [self _serverDataForType:type];
   }
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

-(BOOL)writeObjects:(NSArray *)objects {
   BOOL wrote = NO;
   for (id object in objects) {
      if (![object respondsToSelector:@selector(writableTypesForPasteboard:)])
         continue;
      NSArray *types = [object writableTypesForPasteboard:self];
      [self addTypes:types owner:nil];
      for (NSString *type in types) {
         id plist = [object pasteboardPropertyListForType:type];
         if ([plist isKindOfClass:[NSData class]])
            wrote |= [self setData:plist forType:type];
         else if ([plist isKindOfClass:[NSString class]])
            wrote |= [self setString:plist forType:type];
         else if (plist != nil)
            wrote |= [self setPropertyList:plist forType:type];
      }
   }
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
