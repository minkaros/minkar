/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "LGWindowController.h"

#define MAX_MESSAGES 50000
#define SIDEBAR 190

static NSArray *sourceNames(void)
{
    return @[ NSLocalizedString(@"Messages", nil), NSLocalizedString(@"Crash Reports", nil),
        NSLocalizedString(@"Log Files", nil), NSLocalizedString(@"My Logs", nil) ];
}

static NSArray *sourceFolders(LGSource source)
{
    NSString *home = NSHomeDirectory();

    switch (source) {
    case LGSourceCrashReports:
        return @[ @"/Library/Logs/DiagnosticReports", [home stringByAppendingPathComponent:@"Library/Logs/DiagnosticReports"] ];
    case LGSourceSystemLogs:
        return @[ @"/var/log", @"/Library/Logs" ];
    case LGSourceUserLogs:
        return @[ [home stringByAppendingPathComponent:@"Library/Logs"] ];
    default:
        return @[];
    }
}

static NSTextField *field(NSString *placeholder, NSRect frame)
{
    NSTextField *f = [[NSTextField alloc] initWithFrame:frame];

    [[f cell] setPlaceholderString:placeholder];
    return f;
}

static NSTextView *textView(NSScrollView **scrollOut, NSRect frame)
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
    NSTextView *text = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];

    [text setEditable:NO];
    [text setSelectable:YES];
    [text setFont:[NSFont userFixedPitchFontOfSize:12]];
    [text setAutoresizingMask:NSViewWidthSizable];
    [scroll setDocumentView:text];
    [scroll setHasVerticalScroller:YES];
    *scrollOut = scroll;
    return text;
}

@implementation LGWindowController {
    NSTableView *_sidebar, *_table, *_fileTable;
    NSView *_messagesView, *_filesView;
    NSPopUpButton *_levelPopUp;
    NSButton *_pauseButton;
    NSTextField *_processField, *_subsystemField, *_countLabel;
    NSSearchField *_searchField;
    NSTextView *_detail, *_fileText;
    NSMutableArray<LGMessage *> *_all;
    NSArray<LGMessage *> *_visible;
    NSArray<NSString *> *_files;
    BOOL _paused;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 980, 620)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];

    if ((self = [super initWithWindow:window]) == nil)
        return nil;
    _all = [NSMutableArray array];
    _visible = @[];
    _files = @[];
    _processFilter = _subsystemFilter = _searchString = @"";
    _reader = [LGLogReader new];
    __weak LGWindowController *weakSelf = self;
    _reader.handler = ^(NSArray *messages) {
        [weakSelf addMessages:messages];
    };
    [self makeViews];
    [self setSource:LGSourceMessages];
    [_reader startAtLevel:LGLevelDefault];
    return self;
}

- (NSTableColumn *)column:(NSString *)identifier title:(NSString *)title width:(CGFloat)width
{
    NSTableColumn *c = [[NSTableColumn alloc] initWithIdentifier:identifier];

    [[c headerCell] setStringValue:title];
    [c setWidth:width];
    [c setEditable:NO];
    return c;
}

- (NSTableView *)tableIn:(NSView *)parent frame:(NSRect)frame autoresizing:(NSUInteger)mask
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
    NSTableView *table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];

    [table setDataSource:self];
    [table setDelegate:self];
    [scroll setDocumentView:table];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:mask];
    [parent addSubview:scroll];
    return table;
}

- (void)makeViews
{
    NSWindow *window = [self window];
    NSView *content = [window contentView];
    NSRect bounds = [content bounds];
    NSRect main = NSMakeRect(SIDEBAR, 0, NSWidth(bounds) - SIDEBAR, NSHeight(bounds));
    NSScrollView *scroll;

    [window setTitle:NSLocalizedString(@"Logs", nil)];
    [window setMinSize:NSMakeSize(720, 420)];
    [window setFrameAutosaveName:@"Logs"];

    _sidebar = [self tableIn:content frame:NSMakeRect(0, 0, SIDEBAR, NSHeight(bounds)) autoresizing:NSViewHeightSizable];
    [_sidebar addTableColumn:[self column:@"source" title:@"" width:SIDEBAR - 4]];
    [_sidebar setHeaderView:nil];
    [_sidebar setBackgroundColor:[NSColor windowBackgroundColor]];

    /* The messages: the filters' bar, the table, the selected message. */
    _messagesView = [[NSView alloc] initWithFrame:main];
    [_messagesView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [content addSubview:_messagesView];
    CGFloat top = NSHeight(main) - 38, w = NSWidth(main);

    _pauseButton = [[NSButton alloc] initWithFrame:NSMakeRect(10, top, 90, 26)];
    [_pauseButton setBezelStyle:NSRoundedBezelStyle];
    [_pauseButton setTitle:NSLocalizedString(@"Pause", nil)];
    [_pauseButton setTarget:self];
    [_pauseButton setAction:@selector(togglePause:)];
    [_pauseButton setAutoresizingMask:NSViewMinYMargin];
    [_messagesView addSubview:_pauseButton];

    _levelPopUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(106, top, 150, 26) pullsDown:NO];
    [_levelPopUp addItemsWithTitles:@[ NSLocalizedString(@"Default", nil), NSLocalizedString(@"Info", nil),
        NSLocalizedString(@"Debug", nil), NSLocalizedString(@"Errors and Faults", nil) ]];
    [_levelPopUp setTarget:self];
    [_levelPopUp setAction:@selector(levelChanged:)];
    [_levelPopUp setAutoresizingMask:NSViewMinYMargin];
    [_messagesView addSubview:_levelPopUp];

    _processField = field(NSLocalizedString(@"Process", nil), NSMakeRect(264, top + 2, 130, 22));
    _subsystemField = field(NSLocalizedString(@"Subsystem", nil), NSMakeRect(400, top + 2, 160, 22));
    for (NSTextField *f in @[ _processField, _subsystemField ]) {
        [f setTarget:self];
        [f setAction:@selector(filtersChanged:)];
        [f setAutoresizingMask:NSViewMinYMargin];
        [_messagesView addSubview:f];
    }
    _searchField = [[NSSearchField alloc] initWithFrame:NSMakeRect(w - 196, top + 2, 186, 22)];
    [[_searchField cell] setPlaceholderString:NSLocalizedString(@"Search Messages", nil)];
    [_searchField setTarget:self];
    [_searchField setAction:@selector(filtersChanged:)];
    [_searchField setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [_messagesView addSubview:_searchField];

    _table = [self tableIn:_messagesView frame:NSMakeRect(0, 150, w, top - 156)
        autoresizing:NSViewWidthSizable | NSViewHeightSizable];
    [_table addTableColumn:[self column:@"timestamp" title:NSLocalizedString(@"Time", nil) width:190]];
    [_table addTableColumn:[self column:@"type" title:NSLocalizedString(@"Type", nil) width:60]];
    [_table addTableColumn:[self column:@"process" title:NSLocalizedString(@"Process", nil) width:110]];
    [_table addTableColumn:[self column:@"pid" title:NSLocalizedString(@"PID", nil) width:50]];
    [_table addTableColumn:[self column:@"subsystem" title:NSLocalizedString(@"Subsystem", nil) width:150]];
    [_table addTableColumn:[self column:@"category" title:NSLocalizedString(@"Category", nil) width:90]];
    [_table addTableColumn:[self column:@"message" title:NSLocalizedString(@"Message", nil) width:500]];
    [_table setUsesAlternatingRowBackgroundColors:YES];

    _detail = textView(&scroll, NSMakeRect(0, 24, w, 120));
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
    [_messagesView addSubview:scroll];
    _countLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 2, w - 20, 18)];
    [_countLabel setBezeled:NO];
    [_countLabel setDrawsBackground:NO];
    [_countLabel setEditable:NO];
    [_countLabel setFont:[NSFont systemFontOfSize:11]];
    [_countLabel setTextColor:[NSColor secondaryLabelColor]];
    [_countLabel setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
    [_messagesView addSubview:_countLabel];

    /* A source's files: the list, the selected one's text. */
    _filesView = [[NSView alloc] initWithFrame:main];
    [_filesView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [content addSubview:_filesView];
    _fileTable = [self tableIn:_filesView frame:NSMakeRect(0, NSHeight(main) * 0.6, w, NSHeight(main) * 0.4)
        autoresizing:NSViewWidthSizable | NSViewMinYMargin];
    [_fileTable addTableColumn:[self column:@"name" title:NSLocalizedString(@"Name", nil) width:320]];
    [_fileTable addTableColumn:[self column:@"date" title:NSLocalizedString(@"Date Modified", nil) width:180]];
    [_fileTable addTableColumn:[self column:@"size" title:NSLocalizedString(@"Size", nil) width:90]];
    [_fileTable addTableColumn:[self column:@"folder" title:NSLocalizedString(@"Folder", nil) width:220]];
    _fileText = textView(&scroll, NSMakeRect(0, 0, w, NSHeight(main) * 0.6 - 4));
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_filesView addSubview:scroll];
}

/* ---- The messages ---- */

- (void)addMessages:(NSArray *)messages
{
    [_all addObjectsFromArray:messages];
    if ([_all count] > MAX_MESSAGES)
        [_all removeObjectsInRange:NSMakeRange(0, [_all count] - MAX_MESSAGES)];
    [self applyFilters];
}

- (NSArray *)messages
{
    return [_all copy];
}

- (NSArray *)visibleMessages
{
    return _visible;
}

static BOOL has(NSString *text, NSString *part)
{
    return [part length] == 0 || [text rangeOfString:part options:NSCaseInsensitiveSearch].location != NSNotFound;
}

- (void)applyFilters
{
    BOOL errorsOnly = (_level == LGChoiceErrors);
    BOOL atEnd = [_table numberOfRows] == 0 || [_table selectedRow] < 0;
    NSMutableArray *visible = [NSMutableArray array];

    for (LGMessage *m in _all) {
        if (errorsOnly && ![m.type isEqual:@"Error"] && ![m.type isEqual:@"Fault"])
            continue;
        if (has(m.process, _processFilter) && has(m.subsystem, _subsystemFilter) && has(m.message, _searchString))
            [visible addObject:m];
    }
    _visible = visible;
    [_table reloadData];
    if (atEnd && [_visible count])
        [_table scrollRowToVisible:[_visible count] - 1];
    [_countLabel setStringValue:[NSString stringWithFormat:NSLocalizedString(@"%lu of %lu messages%@", nil),
        (unsigned long)[_visible count], (unsigned long)[_all count],
        _paused ? NSLocalizedString(@" — paused", nil) : @""]];
}

- (void)setProcessFilter:(NSString *)value
{
    _processFilter = [value copy] ?: @"";
    [self applyFilters];
}

- (void)setSubsystemFilter:(NSString *)value
{
    _subsystemFilter = [value copy] ?: @"";
    [self applyFilters];
}

- (void)setSearchString:(NSString *)value
{
    _searchString = [value copy] ?: @"";
    [self applyFilters];
}

- (void)filtersChanged:(id)sender
{
    _processFilter = [_processField stringValue] ?: @"";
    _subsystemFilter = [_subsystemField stringValue] ?: @"";
    _searchString = [_searchField stringValue] ?: @"";
    [self applyFilters];
}

/* Info and debug are read again from logd at that level; errors and
 * faults are the default level's, filtered here. */
- (void)setLevel:(LGLevelChoice)level
{
    LGLevel readerLevel = (level == LGChoiceInfo) ? LGLevelInfo : (level == LGChoiceDebug) ? LGLevelDebug : LGLevelDefault;

    _level = level;
    [_levelPopUp selectItemAtIndex:level];
    if (readerLevel != _reader.level) {
        [_all removeAllObjects];
        [_reader startAtLevel:readerLevel];
    }
    [self applyFilters];
}

- (void)levelChanged:(id)sender
{
    [self setLevel:[_levelPopUp indexOfSelectedItem]];
}

- (IBAction)togglePause:(id)sender
{
    _paused = !_paused;
    if (_paused)
        [_reader pause];
    else
        [_reader resume];
    [_pauseButton setTitle:_paused ? NSLocalizedString(@"Resume", nil) : NSLocalizedString(@"Pause", nil)];
    [self applyFilters];
}

- (IBAction)clear:(id)sender
{
    [_all removeAllObjects];
    [_detail setString:@""];
    [self applyFilters];
}

/* ---- The files ---- */

- (void)setSource:(LGSource)source
{
    _source = source;
    [_sidebar selectRowIndexes:[NSIndexSet indexSetWithIndex:source] byExtendingSelection:NO];
    [_messagesView setHidden:source != LGSourceMessages];
    [_filesView setHidden:source == LGSourceMessages];
    if (source != LGSourceMessages)
        [self loadFiles];
}

- (void)loadFiles
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableArray *files = [NSMutableArray array];

    for (NSString *folder in sourceFolders(_source))
        for (NSString *name in [fm contentsOfDirectoryAtPath:folder error:NULL]) {
            NSString *path = [folder stringByAppendingPathComponent:name];
            BOOL dir = NO;
            if ([name hasPrefix:@"."] || ![fm fileExistsAtPath:path isDirectory:&dir] || dir)
                continue;
            [files addObject:path];
        }
    [files sortUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        NSDate *da = [[fm attributesOfItemAtPath:a error:NULL] fileModificationDate];
        NSDate *db = [[fm attributesOfItemAtPath:b error:NULL] fileModificationDate];
        return [db compare:da];
    }];
    _files = files;
    [_fileTable reloadData];
    [_fileText setString:([files count] ? @"" : NSLocalizedString(@"There are no files here.", nil))];
}

- (NSArray *)files
{
    return _files;
}

- (void)showFile:(NSString *)path
{
    NSData *data = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:NULL];
    NSString *text = nil;

    /* Large logs: their last megabyte. */
    if ([data length] > (1 << 20))
        data = [data subdataWithRange:NSMakeRange([data length] - (1 << 20), 1 << 20)];
    if (data != nil)
        text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?:
            [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    [_fileText setString:text ?: NSLocalizedString(@"This file could not be read.", nil)];
    [_fileText scrollRangeToVisible:NSMakeRange([[_fileText string] length], 0)];
}

- (NSString *)shownText
{
    return _source == LGSourceMessages ? [_detail string] : [_fileText string];
}

- (IBAction)reveal:(id)sender
{
    NSInteger row = [_fileTable selectedRow];

    if (_source != LGSourceMessages && row >= 0 && row < (NSInteger)[_files count])
        [[NSWorkspace sharedWorkspace] selectFile:_files[row] inFileViewerRootedAtPath:@""];
}

/* ---- The tables ---- */

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    if (table == _sidebar)
        return [sourceNames() count];
    if (table == _table)
        return [_visible count];
    return [_files count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NSString *key = [column identifier];

    if (table == _sidebar)
        return sourceNames()[row];
    if (table == _table)
        return [_visible[row] valueForKey:key];

    NSString *path = _files[row];
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
    if ([key isEqual:@"name"])
        return [path lastPathComponent];
    if ([key isEqual:@"folder"])
        return [path stringByDeletingLastPathComponent];
    if ([key isEqual:@"size"])
        return [NSByteCountFormatter stringFromByteCount:(long long)[attributes fileSize]
            countStyle:NSByteCountFormatterCountStyleFile];
    NSDateFormatter *when = [NSDateFormatter new];
    [when setDateStyle:NSDateFormatterShortStyle];
    [when setTimeStyle:NSDateFormatterMediumStyle];
    return [when stringFromDate:[attributes fileModificationDate]];
}

/* Errors in red, faults in purple, as the reference platform marks them. */
- (void)tableView:(NSTableView *)table willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if (table != _table || ![cell respondsToSelector:@selector(setTextColor:)])
        return;
    NSString *type = _visible[row].type;
    [cell setTextColor:[type isEqual:@"Error"] ? [NSColor systemRedColor] :
        [type isEqual:@"Fault"] ? [NSColor systemPurpleColor] : [NSColor controlTextColor]];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    NSTableView *table = [note object];
    NSInteger row = [table selectedRow];

    if (table == _sidebar) {
        if (row >= 0 && row != _source)
            [self setSource:row];
    } else if (table == _table) {
        if (row >= 0 && row < (NSInteger)[_visible count]) {
            LGMessage *m = _visible[row];
            [_detail setString:[NSString stringWithFormat:@"%@  %@  %@[%d]  %@%@%@\n\n%@", m.timestamp, m.type,
                m.process, m.pid, m.subsystem, [m.category length] ? @":" : @"", m.category, m.message]];
        }
    } else if (table == _fileTable) {
        if (row >= 0 && row < (NSInteger)[_files count])
            [self showFile:_files[row]];
    }
}

@end
