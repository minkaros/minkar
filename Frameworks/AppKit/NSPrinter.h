/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>

#import <Foundation/NSArray.h>

@class NSString, NSDictionary;

typedef NSString *NSPrinterTypeName;
typedef NSString *NSPrinterPaperName;

typedef enum {
    NSPrinterTableOK = 0,
    NSPrinterTableNotFound = 1,
    NSPrinterTableError = 2,
} NSPrinterTableStatus;

/* A printer (PrintCore's PMPrinter, CUPS's queue): its name, type and
   papers; its description table ("PPD") when it has one. */
@interface NSPrinter : NSObject <NSCopying, NSCoding> {
    void *_printer;     /* PMPrinter, retained */
}

+ (NSArray<NSString *> *)printerNames;
+ (NSArray<NSPrinterTypeName> *)printerTypes;
+ (NSPrinter *)printerWithName:(NSString *)name;
+ (NSPrinter *)printerWithType:(NSPrinterTypeName)type;

@property (readonly, copy) NSString *name;
@property (readonly, copy) NSPrinterTypeName type;
@property (readonly) NSInteger languageLevel;
@property (readonly, copy) NSDictionary *deviceDescription;

- (NSSize)pageSizeForPaper:(NSPrinterPaperName)paperName;

- (NSPrinterTableStatus)statusForTable:(NSString *)tableName;
- (BOOL)isKey:(NSString *)key inTable:(NSString *)table;
- (NSString *)stringForKey:(NSString *)key inTable:(NSString *)table;
- (NSArray *)stringListForKey:(NSString *)key inTable:(NSString *)table;
- (BOOL)booleanForKey:(NSString *)key inTable:(NSString *)table;
- (float)floatForKey:(NSString *)key inTable:(NSString *)table;
- (int)intForKey:(NSString *)key inTable:(NSString *)table;
- (NSRect)rectForKey:(NSString *)key inTable:(NSString *)table;
- (NSSize)sizeForKey:(NSString *)key inTable:(NSString *)table;

@end

/* Minkar: the PMPrinter (for NSPrintInfo). */
@interface NSPrinter (MinkarPrintCore)
+ (NSPrinter *)_printerWithPMPrinter:(void *)printer;
- (void *)_PMPrinter;
@end
