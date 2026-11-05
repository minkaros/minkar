/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class EditorDocumentWindow;

@protocol EditorDocumentWindowDelegate <NSObject>
- (void)documentWindowDidClose:(EditorDocumentWindow *)document;
@end

/* One document: a text file (or a new one) in a window. */
@interface EditorDocumentWindow : NSObject
@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly, nullable, copy) NSURL *fileURL;
@property (nonatomic, readonly, getter=isEdited) BOOL edited;
@property (nonatomic, weak, nullable) id<EditorDocumentWindowDelegate> delegate;

/* A new, untitled document (numbered). */
- (instancetype)initUntitled:(NSUInteger)number;
/* A file's text; nil when it cannot be read as text. */
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (void)show;
/* Saves to its file (or asks where, untitled); NO if not saved. */
- (BOOL)save;
- (BOOL)saveAs;
/* Closes, asking about unsaved changes; NO if the user cancelled. */
- (BOOL)close;
@end

NS_ASSUME_NONNULL_END
