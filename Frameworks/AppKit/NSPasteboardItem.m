/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSPasteboardItem.h>

@implementation NSPasteboardItem

-init {
   _values=[[NSMutableDictionary alloc] init];
   _types=[[NSMutableArray alloc] init];
   _providers=[[NSMutableDictionary alloc] init];
   return self;
}

-(void)dealloc {
   [_values release];
   [_types release];
   [_providers release];
   [super dealloc];
}

-(NSArray *)types {
   return [[_types copy] autorelease];
}

-(NSPasteboardType)availableTypeFromArray:(NSArray *)types {
   for(NSPasteboardType type in types)
    if([_types containsObject:type])
     return type;
   return nil;
}

-(void)_addType:(NSPasteboardType)type {
   if(![_types containsObject:type])
    [_types addObject:type];
}

-(BOOL)setDataProvider:(id<NSPasteboardItemDataProvider>)dataProvider forTypes:(NSArray *)types {
   if(dataProvider==nil)
    return NO;
   for(NSPasteboardType type in types){
    [self _addType:type];
    [_providers setObject:dataProvider forKey:type];
   }
   return YES;
}

-(BOOL)_setValue:(id)value forType:(NSPasteboardType)type {
   if(value==nil || type==nil)
    return NO;
   [self _addType:type];
   [_values setObject:value forKey:type];
   [_providers removeObjectForKey:type];
   return YES;
}

-(BOOL)setData:(NSData *)data forType:(NSPasteboardType)type { return [self _setValue:data forType:type]; }
-(BOOL)setString:(NSString *)string forType:(NSPasteboardType)type { return [self _setValue:[[string copy] autorelease] forType:type]; }
-(BOOL)setPropertyList:(id)propertyList forType:(NSPasteboardType)type { return [self _setValue:propertyList forType:type]; }

/* A provided type asks its provider once, when first read. */
-(id)_valueForType:(NSPasteboardType)type {
   id value=[_values objectForKey:type];
   id provider=[_providers objectForKey:type];

   if(value==nil && provider!=nil){
    [provider pasteboard:nil item:self provideDataForType:type];
    value=[_values objectForKey:type];
   }
   return value;
}

-(NSData *)dataForType:(NSPasteboardType)type {
   id value=[self _valueForType:type];

   if([value isKindOfClass:[NSData class]])
    return value;
   if([value isKindOfClass:[NSString class]])
    return [value dataUsingEncoding:NSUTF8StringEncoding];
   return (value!=nil)?[NSPropertyListSerialization dataWithPropertyList:value format:NSPropertyListXMLFormat_v1_0 options:0 error:NULL]:nil;
}

-(NSString *)stringForType:(NSPasteboardType)type {
   id value=[self _valueForType:type];

   if([value isKindOfClass:[NSString class]])
    return value;
   if([value isKindOfClass:[NSData class]])
    return [[[NSString alloc] initWithData:value encoding:NSUTF8StringEncoding] autorelease];
   return nil;
}

-(id)propertyListForType:(NSPasteboardType)type {
   id value=[self _valueForType:type];

   if([value isKindOfClass:[NSData class]])
    return [NSPropertyListSerialization propertyListWithData:value options:0 format:NULL error:NULL];
   return value;
}

#pragma mark NSPasteboardWriting

-(NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
   return [self types];
}

-(id)pasteboardPropertyListForType:(NSPasteboardType)type {
   return [self _valueForType:type];
}

#pragma mark NSPasteboardReading

+(NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
   return [pasteboard types];
}

-(id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSPasteboardType)type {
   self=[self init];
   [self _setValue:propertyList forType:type];
   return self;
}

@end

@interface NSPasteboard (NSPasteboardItemsPrivate)
-(NSArray *)_canonicalTypes;
@end

@implementation NSPasteboard (NSPasteboardItems)

/* One item with the types as written (the older aliases that -types adds
   are not the item's). */
-(NSArray *)pasteboardItems {
   NSPasteboardItem *item=[[[NSPasteboardItem alloc] init] autorelease];
   NSArray          *types=[self _canonicalTypes];

   if([types count]==0)
    return [NSArray array];
   for(NSPasteboardType type in types){
    NSData *data=[self dataForType:type];

    if(data!=nil)
     [item setData:data forType:type];
   }
   return [NSArray arrayWithObject:item];
}

@end
