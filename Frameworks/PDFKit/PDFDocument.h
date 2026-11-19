/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>
#import <PDFKit/PDFPage.h>

@class PDFOutline, PDFSelection;
@protocol PDFDocumentDelegate;

NS_ASSUME_NONNULL_BEGIN

PDFKIT_EXTERN NSNotificationName const PDFDocumentDidUnlockNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidBeginFindNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidEndFindNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidBeginPageFindNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidEndPageFindNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidFindMatchNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidBeginWriteNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidEndWriteNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidBeginPageWriteNotification;
PDFKIT_EXTERN NSNotificationName const PDFDocumentDidEndPageWriteNotification;

typedef NSString *PDFDocumentAttribute NS_STRING_ENUM;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentTitleAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentAuthorAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentSubjectAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentCreatorAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentProducerAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentCreationDateAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentModificationDateAttribute;
PDFKIT_EXTERN PDFDocumentAttribute const PDFDocumentKeywordsAttribute;

typedef NSString *PDFDocumentWriteOption NS_STRING_ENUM;
PDFKIT_EXTERN PDFDocumentWriteOption const PDFDocumentOwnerPasswordOption;
PDFKIT_EXTERN PDFDocumentWriteOption const PDFDocumentUserPasswordOption;

typedef NS_ENUM(NSInteger, PDFDocumentPermissions) {
    kPDFDocumentPermissionsNone = 0,
    kPDFDocumentPermissionsUser = 1,
    kPDFDocumentPermissionsOwner = 2
};

/* A PDF document: its pages (made as the pageClass), outline and
   attributes. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFDocument : NSObject <NSCopying>
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithURL:(NSURL *)url;
- (nullable instancetype)initWithData:(NSData *)data;

@property (nonatomic, readonly, nullable) NSURL *documentURL;
@property (nonatomic, readonly, nullable) CGPDFDocumentRef documentRef;
@property (nonatomic, copy, nullable) NSDictionary *documentAttributes;
@property (nonatomic, readonly) NSInteger majorVersion;
@property (nonatomic, readonly) NSInteger minorVersion;
@property (nonatomic, readonly) BOOL isEncrypted;
@property (nonatomic, readonly) BOOL isLocked;
- (BOOL)unlockWithPassword:(NSString *)password;
@property (nonatomic, readonly) BOOL allowsPrinting;
@property (nonatomic, readonly) BOOL allowsCopying;
@property (nonatomic, readonly) BOOL allowsDocumentChanges;
@property (nonatomic, readonly) BOOL allowsDocumentAssembly;
@property (nonatomic, readonly) BOOL allowsContentAccessibility;
@property (nonatomic, readonly) BOOL allowsCommenting;
@property (nonatomic, readonly) BOOL allowsFormFieldEntry;
@property (nonatomic, readonly) PDFDocumentPermissions permissionsStatus;

/* Its pages' text, a line feed between pages. */
@property (nonatomic, readonly, nullable) NSString *string;
@property (nonatomic, weak, nullable) id<PDFDocumentDelegate> delegate;

@property (nonatomic, strong, nullable) PDFOutline *outlineRoot;
/* The outline item a selection is under: the last whose destination comes
   before its start. */
- (nullable PDFOutline *)outlineItemForSelection:(PDFSelection *)selection;

@property (nonatomic, readonly) NSUInteger pageCount;
- (nullable PDFPage *)pageAtIndex:(NSUInteger)index;
- (NSUInteger)indexForPage:(PDFPage *)page;
- (void)insertPage:(PDFPage *)page atIndex:(NSUInteger)index;
- (void)removePageAtIndex:(NSUInteger)index;
- (void)exchangePageAtIndex:(NSUInteger)indexA withPageAtIndex:(NSUInteger)indexB;
@property (nonatomic, readonly) Class pageClass;

/* Each match of string in the pages' text (not across pages), in order;
   options as NSString's (case insensitive, backwards...). */
- (NSArray<PDFSelection *> *)findString:(NSString *)string withOptions:(NSStringCompareOptions)options;
/* The next match after selection (before it, backwards), or the first;
   nil past the last (no wrapping). */
- (nullable PDFSelection *)findString:(NSString *)string fromSelection:(nullable PDFSelection *)selection withOptions:(NSStringCompareOptions)options;
/* Finding, told by notifications (and the delegate): begin, each page,
   each match, end. */
- (void)beginFindString:(NSString *)string withOptions:(NSStringCompareOptions)options;
- (void)beginFindStrings:(NSArray<NSString *> *)strings withOptions:(NSStringCompareOptions)options;
@property (nonatomic, readonly) BOOL isFinding;
- (void)cancelFindString;

@property (nonatomic, readonly, nullable) PDFSelection *selectionForEntireDocument;
- (nullable PDFSelection *)selectionFromPage:(PDFPage *)startPage atPoint:(PDFPoint)startPoint toPage:(PDFPage *)endPage atPoint:(PDFPoint)endPoint;
- (nullable PDFSelection *)selectionFromPage:(PDFPage *)startPage atCharacterIndex:(NSUInteger)startCharacter toPage:(PDFPage *)endPage atCharacterIndex:(NSUInteger)endCharacter;

- (nullable NSData *)dataRepresentation;
- (nullable NSData *)dataRepresentationWithOptions:(NSDictionary *)options;
- (BOOL)writeToFile:(NSString *)path;
- (BOOL)writeToFile:(NSString *)path withOptions:(nullable NSDictionary<PDFDocumentWriteOption, id> *)options;
- (BOOL)writeToURL:(NSURL *)url;
- (BOOL)writeToURL:(NSURL *)url withOptions:(nullable NSDictionary<PDFDocumentWriteOption, id> *)options;

@end

@protocol PDFDocumentDelegate <NSObject>
@optional
- (void)documentDidUnlock:(NSNotification *)notification;
- (void)documentDidBeginDocumentFind:(NSNotification *)notification;
- (void)documentDidEndDocumentFind:(NSNotification *)notification;
- (void)documentDidBeginPageFind:(NSNotification *)notification;
- (void)documentDidEndPageFind:(NSNotification *)notification;
- (void)documentDidFindMatch:(NSNotification *)notification;
- (void)didMatchString:(PDFSelection *)instance;
- (Class)classForPage;
@end

NS_ASSUME_NONNULL_END
