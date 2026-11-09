/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What AppKit lacks, as the gallery finds it: classes not there, methods
 * a class does not answer, menu items whose action nothing in the
 * responder chain performs, and the tabs that failed to build. One line
 * each, "missing class NSPopover", sorted, for the tests to compare.
 */

#import <Cocoa/Cocoa.h>

@interface GalleryReport : NSObject
+ (instancetype)sharedReport;
/* A tab, control or panel that raised while it was built or shown. */
- (void)noteFailure:(NSString *)what reason:(NSString *)reason;
/* Classes and methods (the fixed lists in GalleryReport.m). */
- (void)checkAPI;
/* Every item of the menu and its submenus whose action no AppKit class
 * that takes it (the application, windows, views, text, documents, the
 * font manager) implements, and the item's own target does not either. */
- (void)checkMenu:(NSMenu *)menu path:(NSString *)path;
- (NSArray<NSString *> *)lines;
- (NSString *)text;
@end
