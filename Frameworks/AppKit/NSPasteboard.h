/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>

typedef NSString *NSPasteboardType;
typedef NSString *NSPasteboardName;

// Pasteboard types: uniform type identifiers (10.6 and later); the older
// NS*PboardType names below are taken as the same types.
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeString;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypePDF;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeTIFF;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypePNG;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeRTF;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeRTFD;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeHTML;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeTabularText;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeFont;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeRuler;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeColor;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeSound;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeMultipleTextSelection;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeTextFinderOptions;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeURL;
APPKIT_EXPORT NSPasteboardType const NSPasteboardTypeFileURL;

// Old Pasteboard Types
APPKIT_EXPORT NSString *const NSColorPboardType;
APPKIT_EXPORT NSString *const NSFileContentsPboardType;
APPKIT_EXPORT NSString *const NSFilenamesPboardType;
APPKIT_EXPORT NSString *const NSFontPboardType;
APPKIT_EXPORT NSString *const NSPDFPboardType;
APPKIT_EXPORT NSString *const NSPICTPboardType;
APPKIT_EXPORT NSString *const NSPostScriptPboardType;
APPKIT_EXPORT NSString *const NSRTFDPboardType;
APPKIT_EXPORT NSString *const NSRTFPboardType;
APPKIT_EXPORT NSString *const NSRulerPboardType;
APPKIT_EXPORT NSString *const NSStringPboardType;
APPKIT_EXPORT NSString *const NSTabularTextPboardType;
APPKIT_EXPORT NSString *const NSTIFFPboardType;
APPKIT_EXPORT NSString *const NSURLPboardType;
APPKIT_EXPORT NSString *const NSHTMLPboardType;

APPKIT_EXPORT NSPasteboardName const NSPasteboardNameGeneral;
APPKIT_EXPORT NSPasteboardName const NSPasteboardNameFont;
APPKIT_EXPORT NSPasteboardName const NSPasteboardNameRuler;
APPKIT_EXPORT NSPasteboardName const NSPasteboardNameFind;
APPKIT_EXPORT NSPasteboardName const NSPasteboardNameDrag;

APPKIT_EXPORT NSString *const NSDragPboard;
APPKIT_EXPORT NSString *const NSFindPboard;
APPKIT_EXPORT NSString *const NSFontPboard;
APPKIT_EXPORT NSString *const NSGeneralPboard;
APPKIT_EXPORT NSString *const NSRulerPboard;

typedef NSUInteger NSPasteboardWritingOptions;
enum {
    NSPasteboardWritingPromised = 1 << 9,
};

typedef NSUInteger NSPasteboardReadingOptions;
enum {
    NSPasteboardReadingAsData = 0,
    NSPasteboardReadingAsString = 1 << 0,
    NSPasteboardReadingAsPropertyList = 1 << 1,
    NSPasteboardReadingAsKeyedArchive = 1 << 2,
};

@class NSPasteboard;

@protocol NSPasteboardWriting <NSObject>
- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard;
- (id)pasteboardPropertyListForType:(NSPasteboardType)type;
@optional
- (NSPasteboardWritingOptions)writingOptionsForType:(NSPasteboardType)type
    pasteboard:(NSPasteboard *)pasteboard;
@end

@protocol NSPasteboardReading <NSObject>
+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard;
@optional
+ (NSPasteboardReadingOptions)readingOptionsForType:(NSPasteboardType)type
    pasteboard:(NSPasteboard *)pasteboard;
- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSPasteboardType)type;
@end

@interface NSPasteboard : NSObject {
    NSString *_name;
    NSInteger _changeCount;        /* of our last clear or declare */
    id _owner;                     /* retained while it owns the pasteboard */
    NSMutableArray *_declaredTypes;  /* declared, maybe not written yet */
}

+ (NSPasteboard *)generalPasteboard;
+ (NSPasteboard *)pasteboardWithName:(NSPasteboardName)name;
+ (NSPasteboard *)pasteboardWithUniqueName;

@property(readonly, copy) NSPasteboardName name;
@property(readonly) NSInteger changeCount;

- (void)releaseGlobally;

- (NSInteger)clearContents;
- (BOOL)writeObjects:(NSArray *)objects;
- (NSArray *)readObjectsForClasses:(NSArray *)classArray options:(NSDictionary *)options;
- (BOOL)canReadObjectForClasses:(NSArray *)classArray options:(NSDictionary *)options;

- (NSArray *)types;
- (NSPasteboardType)availableTypeFromArray:(NSArray *)types;
- (NSData *)dataForType:(NSPasteboardType)type;
- (NSString *)stringForType:(NSPasteboardType)type;
- (id)propertyListForType:(NSPasteboardType)type;

- (NSInteger)declareTypes:(NSArray *)types owner:(id)owner;
- (NSInteger)addTypes:(NSArray *)types owner:(id)owner;
- (BOOL)setData:(NSData *)data forType:(NSPasteboardType)type;
- (BOOL)setString:(NSString *)string forType:(NSPasteboardType)type;
- (BOOL)setPropertyList:(id)plist forType:(NSPasteboardType)type;

@end

@interface NSObject (NSPasteboard)
- (void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSPasteboardType)type;
- (void)pasteboardChangedOwner:(NSPasteboard *)sender;
@end

@interface NSString (NSPasteboardSupport) <NSPasteboardWriting, NSPasteboardReading>
@end

@interface NSURL (NSPasteboardSupport) <NSPasteboardWriting, NSPasteboardReading>
+ (NSURL *)URLFromPasteboard:(NSPasteboard *)pasteboard;
- (void)writeToPasteboard:(NSPasteboard *)pasteboard;
@end
