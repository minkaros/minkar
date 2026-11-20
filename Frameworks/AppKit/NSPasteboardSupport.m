// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

/* Colors, images and attributed strings read from and written to a
   pasteboard by their types, as on the reference: a color as a keyed
   archive (com.apple.cocoa.pasteboard.color); an image from any image
   type and as TIFF; an attributed string from RTFD, RTF, HTML or plain
   text and as RTFD, RTF and plain text. */

#import <AppKit/NSPasteboard.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSAttributedString.h>
#import <Foundation/Foundation.h>

/* A class cluster's instance made from a property list: the object it
   decodes to, in place of the one allocated. */
static id replacing(id self, id result) {
    [result retain];
    [self release];
    return result;
}

@implementation NSColor (NSPasteboardSupport)

+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [NSArray arrayWithObject:NSPasteboardTypeColor];
}

+ (NSPasteboardReadingOptions)readingOptionsForType:(NSPasteboardType)type pasteboard:(NSPasteboard *)pasteboard {
    return NSPasteboardReadingAsKeyedArchive;
}

- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSPasteboardType)type {
    id color = [propertyList isKindOfClass:[NSData class]] ? [NSKeyedUnarchiver unarchiveObjectWithData:propertyList] : propertyList;
    return replacing(self, [color isKindOfClass:[NSColor class]] ? color : nil);
}

- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [NSArray arrayWithObject:NSPasteboardTypeColor];
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {
    return [NSKeyedArchiver archivedDataWithRootObject:self];
}

@end

@implementation NSImage (NSPasteboardSupport)

+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [self imageUnfilteredTypes];
}

- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSPasteboardType)type {
    if (![propertyList isKindOfClass:[NSData class]]) {
        [self release];
        return nil;
    }
    return [self initWithData:propertyList];
}

- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [NSArray arrayWithObject:NSPasteboardTypeTIFF];
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {
    return [type isEqualToString:NSPasteboardTypeTIFF] ? [self TIFFRepresentation] : nil;
}

@end

@implementation NSAttributedString (NSPasteboardSupport)

+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [NSArray arrayWithObjects:NSPasteboardTypeRTFD, NSPasteboardTypeRTF, NSPasteboardTypeHTML, NSPasteboardTypeString, nil];
}

/* Plain text as a string, the rest as data. */
+ (NSPasteboardReadingOptions)readingOptionsForType:(NSPasteboardType)type pasteboard:(NSPasteboard *)pasteboard {
    return [type isEqualToString:NSPasteboardTypeString] ? NSPasteboardReadingAsString : NSPasteboardReadingAsData;
}

- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSPasteboardType)type {
    if ([propertyList isKindOfClass:[NSString class]])
        return [self initWithString:propertyList];
    if (![propertyList isKindOfClass:[NSData class]]) {
        [self release];
        return nil;
    }
    if ([type isEqualToString:NSPasteboardTypeRTFD])
        return [self initWithRTFD:propertyList documentAttributes:NULL];
    if ([type isEqualToString:NSPasteboardTypeRTF])
        return [self initWithRTF:propertyList documentAttributes:NULL];
    if ([type isEqualToString:NSPasteboardTypeHTML])
        return [self initWithHTML:propertyList documentAttributes:NULL];
    NSString *string = [[[NSString alloc] initWithData:propertyList encoding:NSUTF8StringEncoding] autorelease];
    return [self initWithString:(string != nil) ? string : @""];
}

- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
    return [NSArray arrayWithObjects:NSPasteboardTypeRTFD, NSPasteboardTypeRTF, NSPasteboardTypeString, nil];
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {
    NSRange all = NSMakeRange(0, [self length]);

    if ([type isEqualToString:NSPasteboardTypeRTFD])
        return [self RTFDFromRange:all documentAttributes:[NSDictionary dictionary]];
    if ([type isEqualToString:NSPasteboardTypeRTF])
        return [self RTFFromRange:all documentAttributes:[NSDictionary dictionary]];
    if ([type isEqualToString:NSPasteboardTypeString])
        return [self string];
    return nil;
}

@end
