/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Find (FilesFind.h).
 */

#import "FilesFind.h"
#import "FilesPreferences.h"
#include <os/log.h>
#include <stdatomic.h>

static const NSUInteger kMaxResults = 5000;

@interface FilesFind () <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@end

@implementation FilesFind {
    NSURL *_folder;
    void (^_reveal)(NSURL *);
    NSWindow *_window;
    NSSearchField *_field;
    NSTableView *_table;
    NSTextField *_status;
    NSMutableArray<NSURL *> *_results;
    atomic_uint _generation;      /* the search running; older ones stop */
}

static NSMutableArray<FilesFind *> *openFinds;

+ (void)showInFolder:(NSURL *)folder reveal:(void (^)(NSURL *))reveal
{
    FilesFind *find = [[FilesFind alloc] initWithFolder:folder reveal:reveal];
    if (openFinds == nil)
        openFinds = [NSMutableArray array];
    [openFinds addObject:find];
    [find->_window makeKeyAndOrderFront:nil];
    [find->_window makeFirstResponder:find->_field];
}

- (instancetype)initWithFolder:(NSURL *)folder reveal:(void (^)(NSURL *))reveal
{
    if ((self = [super init]) == nil)
        return nil;
    _folder = [folder copy];
    _reveal = [reveal copy];
    _results = [NSMutableArray array];

    NSRect frame = NSMakeRect(0, 0, 620, 420);
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered defer:NO];
    NSString *name = [[folder path] isEqualToString:@"/"] ? @"Minkar"
        : [[NSFileManager defaultManager] displayNameAtPath:[folder path]];
    [_window setTitle:[NSString stringWithFormat:NSLocalizedString(@"Searching “%@”", nil), name]];
    [_window setReleasedWhenClosed:NO];
    [_window setMinSize:NSMakeSize(400, 240)];
    [_window setDelegate:self];
    [_window center];
    NSView *content = [_window contentView];

    _field = [[NSSearchField alloc] initWithFrame:NSMakeRect(12, NSHeight(frame) - 38, NSWidth(frame) - 24, 24)];
    [_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [[_field cell] setPlaceholderString:NSLocalizedString(@"Name contains", nil)];
    [_field setTarget:self];
    [_field setAction:@selector(search:)];
    [content addSubview:_field];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 22, NSWidth(frame), NSHeight(frame) - 70)];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSNoBorder];
    _table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *nameColumn = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [[nameColumn headerCell] setStringValue:NSLocalizedString(@"Name", nil)];
    [nameColumn setWidth:220];
    NSTableColumn *whereColumn = [[NSTableColumn alloc] initWithIdentifier:@"where"];
    [[whereColumn headerCell] setStringValue:NSLocalizedString(@"Folder", nil)];
    [whereColumn setWidth:380];
    [_table addTableColumn:nameColumn];
    [_table addTableColumn:whereColumn];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [_table setTarget:self];
    [_table setDoubleAction:@selector(revealRow:)];
    [scroll setDocumentView:_table];
    [content addSubview:scroll];

    _status = [[NSTextField alloc] initWithFrame:NSMakeRect(12, 3, NSWidth(frame) - 24, 16)];
    [_status setAutoresizingMask:NSViewWidthSizable];
    [_status setEditable:NO];
    [_status setBordered:NO];
    [_status setDrawsBackground:NO];
    [_status setFont:[NSFont systemFontOfSize:11]];
    [_status setTextColor:[NSColor secondaryLabelColor]];
    [content addSubview:_status];
    return self;
}

- (void)windowWillClose:(NSNotification *)note
{
    atomic_fetch_add(&_generation, 1);
    [openFinds removeObject:self];
}

/* The text typed (as it changes, the search field's action): the folder
   gone through again, results added in batches. */
- (void)search:(id)sender
{
    NSString *text = [[_field stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    unsigned generation = atomic_fetch_add(&_generation, 1) + 1;
    NSURL *folder = _folder;
    BOOL hidden = FilesShowsHiddenFiles();
    __weak FilesFind *weakSelf = self;

    [_results removeAllObjects];
    [_table reloadData];
    [_status setStringValue:@""];
    if ([text length] == 0)
        return;
    [_status setStringValue:NSLocalizedString(@"Searching…", nil)];
    os_log_info(OS_LOG_DEFAULT, "find \"%{public}s\" in %{public}s", [text UTF8String],
                [[folder path] fileSystemRepresentation]);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSDirectoryEnumerationOptions options = hidden ? 0 : NSDirectoryEnumerationSkipsHiddenFiles;
        NSDirectoryEnumerator *e = [[NSFileManager defaultManager] enumeratorAtURL:folder includingPropertiesForKeys:nil
                                                                           options:options errorHandler:^BOOL(NSURL *u, NSError *err) {
            (void)u; (void)err;
            return YES;
        }];
        NSMutableArray<NSURL *> *batch = [NSMutableArray array];
        NSUInteger found = 0;
        CFAbsoluteTime last = CFAbsoluteTimeGetCurrent();
        for (NSURL *url in e) {
            FilesFind *strong = weakSelf;
            if (strong == nil || atomic_load(&strong->_generation) != generation)
                return;
            /* An app is one item: its insides are not searched. */
            if ([[[url pathExtension] lowercaseString] isEqualToString:@"app"])
                [e skipDescendants];
            if ([[url lastPathComponent] rangeOfString:text options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch]
                    .location == NSNotFound)
                continue;
            [batch addObject:url];
            if (++found >= kMaxResults)
                break;
            if (CFAbsoluteTimeGetCurrent() - last > 0.2) {
                NSArray *some = [batch copy];
                [batch removeAllObjects];
                last = CFAbsoluteTimeGetCurrent();
                dispatch_async(dispatch_get_main_queue(), ^{ [weakSelf add:some generation:generation done:NO]; });
            }
        }
        NSArray *rest = [batch copy];
        dispatch_async(dispatch_get_main_queue(), ^{ [weakSelf add:rest generation:generation done:YES]; });
    });
}

- (void)add:(NSArray<NSURL *> *)urls generation:(unsigned)generation done:(BOOL)done
{
    if (atomic_load(&_generation) != generation)
        return;
    [_results addObjectsFromArray:urls];
    [_table reloadData];
    NSString *count = [_results count] == 1 ? NSLocalizedString(@"1 item", nil)
        : [NSString stringWithFormat:NSLocalizedString(@"%lu items", nil), (unsigned long)[_results count]];
    [_status setStringValue:done ? count : [NSString stringWithFormat:@"%@ — %@", count,
                                                                      NSLocalizedString(@"Searching…", nil)]];
    if (done)
        os_log_info(OS_LOG_DEFAULT, "found %lu: %{public}s", (unsigned long)[_results count],
                    [[[[_results valueForKey:@"lastPathComponent"] subarrayWithRange:
                        NSMakeRange(0, MIN([_results count], (NSUInteger)20))] componentsJoinedByString:@"|"] UTF8String]);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_results count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NSURL *url = _results[(NSUInteger)row];
    if ([[column identifier] isEqualToString:@"name"])
        return [url lastPathComponent];
    return [[url URLByDeletingLastPathComponent] path];
}

- (void)revealRow:(id)sender
{
    NSInteger row = [_table clickedRow] >= 0 ? [_table clickedRow] : [_table selectedRow];
    if (row >= 0 && (NSUInteger)row < [_results count] && _reveal != nil)
        _reveal(_results[(NSUInteger)row]);
}

@end
