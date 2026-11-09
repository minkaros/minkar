/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Gallery, a development app (test images only): every AppKit control in
 * its variants, normal and disabled, on tabs; the reference platform's
 * standard menus with their usual actions; a toolbar; the standard panels;
 * and a report of what AppKit lacks (GalleryReport). Each tab is built
 * inside @try, so one control that fails is reported and the rest shown.
 *
 *	Gallery [-GalleryTab N]	opens on tab N (0 is the first); also
 *				'defaults write org.minkar.Gallery GalleryTab N'
 *				before 'open', as open passes no arguments
 *	Gallery -report		prints the report and exits
 */

#import <Cocoa/Cocoa.h>
#import "GalleryReport.h"
#import <objc/message.h>

static NSString *const kTabs[] = {
    @"Buttons", @"Text", @"Choices", @"Indicators", @"Containers", @"Lists", @"Text View",
    @"Panels", @"Report",
};
#define TAB_COUNT	(sizeof(kTabs) / sizeof(kTabs[0]))

/* ---- building blocks ------------------------------------------------------ */

static NSTextField *label(NSString *text, NSRect frame)
{
    NSTextField *f = [[NSTextField alloc] initWithFrame:frame];

    [f setStringValue:text];
    [f setEditable:NO];
    [f setBordered:NO];
    [f setDrawsBackground:NO];
    [f setSelectable:NO];
    return f;
}

/* A control, and a disabled copy of it to its right when twin. */
static void place(NSView *tab, NSControl *c, NSRect frame, NSControl *twin)
{
    [c setFrame:frame];
    [tab addSubview:c];
    if (twin != nil) {
        [twin setFrame:NSOffsetRect(frame, NSWidth(frame) + 20, 0)];
        [twin setEnabled:NO];
        [tab addSubview:twin];
    }
}

static NSButton *button(NSString *title, NSInteger bezel, NSInteger type)
{
    NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 120, 24)];

    [b setTitle:title];
    [b setButtonType:(NSButtonType)type];
    [b setBezelStyle:(NSBezelStyle)bezel];
    return b;
}

/* ---- the controller ------------------------------------------------------- */

/* The data source and delegate protocols are adopted informally: AppKit
 * may lack them as formal protocols (the report says so). */
@interface GalleryController : NSObject <NSApplicationDelegate>
@end

@implementation GalleryController {
    NSWindow *_window;
    NSTabView *_tabs;
    NSTextView *_reportView;
    NSInteger _firstTab;
    BOOL _reportOnly;
}

- (instancetype)initWithTab:(NSInteger)tab reportOnly:(BOOL)reportOnly
{
    if ((self = [super init]) != nil) {
        _firstTab = tab;
        _reportOnly = reportOnly;
    }
    return self;
}

/* ---- menus: the reference platform's standard ones ---- */

static NSMenuItem *add(NSMenu *menu, NSString *title, SEL action, NSString *key, NSUInteger mods)
{
    NSMenuItem *i = [menu addItemWithTitle:title action:action keyEquivalent:key ?: @""];

    if (key != nil)
        [i setKeyEquivalentModifierMask:mods];
    return i;
}

static NSMenu *submenu(NSMenu *parent, NSString *title)
{
    NSMenuItem *holder = [parent addItemWithTitle:title action:NULL keyEquivalent:@""];
    NSMenu *m = [[NSMenu alloc] initWithTitle:title];

    [holder setSubmenu:m];
    return m;
}

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;
    NSUInteger opt = NSCommandKeyMask | NSAlternateKeyMask;
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];
    [NSApp setMainMenu:main];

    NSMenu *app = submenu(main, @"Gallery");
    add(app, @"About Gallery", @selector(orderFrontStandardAboutPanel:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    [add(app, @"Preferences…", @selector(showPreferences:), @",", cmd) setTarget:self];
    [app addItem:[NSMenuItem separatorItem]];
    NSMenu *services = submenu(app, @"Services");
    if ([NSApp respondsToSelector:@selector(setServicesMenu:)])
        [NSApp setServicesMenu:services];
    [app addItem:[NSMenuItem separatorItem]];
    add(app, @"Hide Gallery", @selector(hide:), @"h", cmd);
    add(app, @"Hide Others", @selector(hideOtherApplications:), @"h", opt);
    add(app, @"Show All", @selector(unhideAllApplications:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    add(app, @"Quit Gallery", @selector(terminate:), @"q", cmd);

    NSMenu *file = submenu(main, @"File");
    add(file, @"New", @selector(newDocument:), @"n", cmd);
    add(file, @"Open…", @selector(openDocument:), @"o", cmd);
    NSMenu *recent = submenu(file, @"Open Recent");
    add(recent, @"Clear Menu", @selector(clearRecentDocuments:), nil, 0);
    [file addItem:[NSMenuItem separatorItem]];
    add(file, @"Close", @selector(performClose:), @"w", cmd);
    add(file, @"Save…", @selector(saveDocument:), @"s", cmd);
    add(file, @"Save As…", @selector(saveDocumentAs:), @"s", shift);
    add(file, @"Revert to Saved", @selector(revertDocumentToSaved:), nil, 0);
    [file addItem:[NSMenuItem separatorItem]];
    add(file, @"Page Setup…", @selector(runPageLayout:), @"p", shift);
    add(file, @"Print…", @selector(print:), @"p", cmd);

    NSMenu *edit = submenu(main, @"Edit");
    add(edit, @"Undo", @selector(undo:), @"z", cmd);
    add(edit, @"Redo", @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    add(edit, @"Cut", @selector(cut:), @"x", cmd);
    add(edit, @"Copy", @selector(copy:), @"c", cmd);
    add(edit, @"Paste", @selector(paste:), @"v", cmd);
    add(edit, @"Paste and Match Style", @selector(pasteAsPlainText:), @"v", opt | NSShiftKeyMask);
    add(edit, @"Delete", @selector(delete:), nil, 0);
    add(edit, @"Select All", @selector(selectAll:), @"a", cmd);
    [edit addItem:[NSMenuItem separatorItem]];
    NSMenu *find = submenu(edit, @"Find");
    [add(find, @"Find…", @selector(performFindPanelAction:), @"f", cmd) setTag:1];
    [add(find, @"Find and Replace…", @selector(performFindPanelAction:), @"f", opt) setTag:12];
    [add(find, @"Find Next", @selector(performFindPanelAction:), @"g", cmd) setTag:2];
    [add(find, @"Find Previous", @selector(performFindPanelAction:), @"g", shift) setTag:3];
    [add(find, @"Use Selection for Find", @selector(performFindPanelAction:), @"e", cmd) setTag:7];
    add(find, @"Jump to Selection", @selector(centerSelectionInVisibleArea:), @"j", cmd);
    NSMenu *spelling = submenu(edit, @"Spelling and Grammar");
    add(spelling, @"Show Spelling and Grammar", @selector(showGuessPanel:), @":", cmd);
    add(spelling, @"Check Document Now", @selector(checkSpelling:), @";", cmd);
    add(spelling, @"Check Spelling While Typing", @selector(toggleContinuousSpellChecking:), nil, 0);
    NSMenu *subst = submenu(edit, @"Substitutions");
    add(subst, @"Show Substitutions", @selector(orderFrontSubstitutionsPanel:), nil, 0);
    add(subst, @"Smart Quotes", @selector(toggleAutomaticQuoteSubstitution:), nil, 0);
    add(subst, @"Smart Dashes", @selector(toggleAutomaticDashSubstitution:), nil, 0);
    add(subst, @"Text Replacement", @selector(toggleAutomaticTextReplacement:), nil, 0);
    NSMenu *transform = submenu(edit, @"Transformations");
    add(transform, @"Make Upper Case", @selector(uppercaseWord:), nil, 0);
    add(transform, @"Make Lower Case", @selector(lowercaseWord:), nil, 0);
    add(transform, @"Capitalize", @selector(capitalizeWord:), nil, 0);
    NSMenu *speech = submenu(edit, @"Speech");
    add(speech, @"Start Speaking", @selector(startSpeaking:), nil, 0);
    add(speech, @"Stop Speaking", @selector(stopSpeaking:), nil, 0);
    [edit addItem:[NSMenuItem separatorItem]];
    add(edit, @"Emoji & Symbols", @selector(orderFrontCharacterPalette:), @" ", NSCommandKeyMask | NSControlKeyMask);

    NSMenu *format = submenu(main, @"Format");
    NSMenu *font = submenu(format, @"Font");
    add(font, @"Show Fonts", @selector(orderFrontFontPanel:), @"t", cmd);
    [add(font, @"Bold", @selector(addFontTrait:), @"b", cmd) setTag:NSBoldFontMask];
    [add(font, @"Italic", @selector(addFontTrait:), @"i", cmd) setTag:NSItalicFontMask];
    add(font, @"Underline", @selector(underline:), @"u", cmd);
    [add(font, @"Bigger", @selector(modifyFont:), @"+", cmd) setTag:3];
    [add(font, @"Smaller", @selector(modifyFont:), @"-", cmd) setTag:4];
    [font addItem:[NSMenuItem separatorItem]];
    add(font, @"Show Colors", @selector(orderFrontColorPanel:), @"C", cmd);
    NSMenu *text = submenu(format, @"Text");
    add(text, @"Align Left", @selector(alignLeft:), @"{", cmd);
    add(text, @"Center", @selector(alignCenter:), @"|", cmd);
    add(text, @"Justify", @selector(alignJustified:), nil, 0);
    add(text, @"Align Right", @selector(alignRight:), @"}", cmd);
    [text addItem:[NSMenuItem separatorItem]];
    add(text, @"Show Ruler", @selector(toggleRuler:), nil, 0);

    NSMenu *view = submenu(main, @"View");
    add(view, @"Show Toolbar", @selector(toggleToolbarShown:), @"t", opt);
    add(view, @"Customize Toolbar…", @selector(runToolbarCustomizationPalette:), nil, 0);
    [view addItem:[NSMenuItem separatorItem]];
    add(view, @"Enter Full Screen", @selector(toggleFullScreen:), @"f", NSCommandKeyMask | NSControlKeyMask);

    NSMenu *window = submenu(main, @"Window");
    add(window, @"Minimize", @selector(performMiniaturize:), @"m", cmd);
    add(window, @"Zoom", @selector(performZoom:), nil, 0);
    [window addItem:[NSMenuItem separatorItem]];
    add(window, @"Bring All to Front", @selector(arrangeInFront:), nil, 0);
    if ([NSApp respondsToSelector:@selector(setWindowsMenu:)])
        [NSApp setWindowsMenu:window];

    NSMenu *help = submenu(main, @"Help");
    add(help, @"Gallery Help", @selector(showHelp:), @"?", cmd);
}

- (void)showPreferences:(id)sender
{
    NSAlert *a = [NSAlert new];

    [a setMessageText:@"Preferences"];
    [a setInformativeText:@"The gallery has no preferences; the item is here as every app has it."];
    [a runModal];
}

/* ---- tabs ---- */

- (void)buildButtons:(NSView *)t
{
    struct { NSString *name; NSInteger bezel, type; } b[] = {
        { @"Push", 1, 7 }, { @"Square", 2, 7 }, { @"Shadowless", 6, 7 }, { @"Textured", 8, 7 },
        { @"Small square", 10, 7 }, { @"Textured rounded", 11, 7 }, { @"Round rect", 12, 7 },
        { @"Recessed", 13, 1 }, { @"Inline", 15, 7 },
    };
    CGFloat y = NSHeight([t bounds]) - 50;

    for (NSUInteger i = 0; i < sizeof(b) / sizeof(b[0]); i++, y -= 34) {
        [t addSubview:label(b[i].name, NSMakeRect(20, y, 140, 20))];
        place(t, button(b[i].name, b[i].bezel, b[i].type), NSMakeRect(170, y - 2, 150, 26),
            button(b[i].name, b[i].bezel, b[i].type));
    }
    y = NSHeight([t bounds]) - 50;
    NSButton *def = button(@"Default", 1, 7);
    [def setKeyEquivalent:@"\r"];
    place(t, def, NSMakeRect(520, y, 120, 26), nil);
    y -= 40;
    place(t, button(@"Check box", 0, 3), NSMakeRect(520, y, 150, 22), button(@"Check box", 0, 3));
    y -= 30;
    NSButton *mixed = button(@"Mixed", 0, 3);
    [mixed setAllowsMixedState:YES];
    [mixed setState:NSMixedState];
    place(t, mixed, NSMakeRect(520, y, 150, 22), nil);
    y -= 30;
    for (int i = 0; i < 3; i++, y -= 26) {
        NSButton *r = button([NSString stringWithFormat:@"Radio %d", i + 1], 0, 4);
        [r setState:i == 0 ? NSOnState : NSOffState];
        place(t, r, NSMakeRect(520, y, 150, 22), nil);
    }
    y -= 10;
    place(t, button(@"", 9, 7), NSMakeRect(520, y, 30, 26), nil);
    [t addSubview:label(@"Help", NSMakeRect(560, y + 3, 80, 20))];
    y -= 34;
    NSButton *disclosure = button(@"", 5, 1);
    place(t, disclosure, NSMakeRect(520, y, 20, 20), nil);
    [t addSubview:label(@"Disclosure", NSMakeRect(545, y, 100, 20))];
    y -= 34;
    NSButton *toggle = button(@"Toggle", 1, 6);
    place(t, toggle, NSMakeRect(520, y, 120, 26), nil);
    Class sw = NSClassFromString(@"NSSwitch");
    y -= 34;
    if (sw != Nil)
        place(t, [[sw alloc] initWithFrame:NSZeroRect], NSMakeRect(520, y, 40, 24), nil);
    else
        [t addSubview:label(@"NSSwitch: missing", NSMakeRect(520, y, 200, 20))];
}

- (void)buildText:(NSView *)t
{
    CGFloat y = NSHeight([t bounds]) - 50;
    NSTextField *field = [NSTextField new];
    [field setStringValue:@"Editable text"];
    NSTextField *off = [NSTextField new];
    [off setStringValue:@"Disabled text"];

    [t addSubview:label(@"Label (system font)", NSMakeRect(20, y, 300, 20))];
    y -= 34;
    place(t, field, NSMakeRect(20, y, 220, 24), off);
    y -= 34;
    NSTextField *placeholder = [NSTextField new];
    if ([placeholder respondsToSelector:NSSelectorFromString(@"setPlaceholderString:")])
        [placeholder performSelector:NSSelectorFromString(@"setPlaceholderString:") withObject:@"Placeholder"];
    place(t, placeholder, NSMakeRect(20, y, 220, 24), nil);
    y -= 34;
    NSSecureTextField *secure = [NSSecureTextField new];
    [secure setStringValue:@"secret"];
    place(t, secure, NSMakeRect(20, y, 220, 24), nil);
    y -= 34;
    NSSearchField *search = [NSSearchField new];
    place(t, search, NSMakeRect(20, y, 220, 24), [NSSearchField new]);
    y -= 34;
    NSComboBox *combo = [NSComboBox new];
    [combo addItemsWithObjectValues:@[ @"One", @"Two", @"Three" ]];
    [combo selectItemAtIndex:0];
    place(t, combo, NSMakeRect(20, y, 220, 26), nil);
    y -= 34;
    NSTokenField *tokens = [NSTokenField new];
    [tokens setObjectValue:@[ @"alpha", @"beta", @"gamma" ]];
    place(t, tokens, NSMakeRect(20, y, 300, 24), nil);
    y -= 34;
    NSForm *form = [[NSForm alloc] initWithFrame:NSMakeRect(20, y - 40, 300, 60)];
    [form addEntry:@"Name:"];
    [form addEntry:@"Mail:"];
    [t addSubview:form];
    y -= 80;
    NSTextField *wrapping = label(@"A label that wraps: the quick brown fox jumps over the lazy dog, "
        @"and then over it again, to fill a second line.", NSMakeRect(20, y - 20, 300, 40));
    [[wrapping cell] setWraps:YES];
    [t addSubview:wrapping];
}

- (void)buildChoices:(NSView *)t
{
    CGFloat y = NSHeight([t bounds]) - 50;
    NSPopUpButton *pop = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [pop addItemsWithTitles:@[ @"Pop-up one", @"Pop-up two", @"Pop-up three" ]];
    NSPopUpButton *pull = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES];
    [pull addItemsWithTitles:@[ @"Pull-down", @"Item one", @"Item two" ]];

    place(t, pop, NSMakeRect(20, y, 180, 26), nil);
    place(t, pull, NSMakeRect(220, y, 180, 26), nil);
    y -= 40;
    NSSegmentedControl *seg = [NSSegmentedControl new];
    [seg setSegmentCount:3];
    for (int i = 0; i < 3; i++) {
        [seg setLabel:[NSString stringWithFormat:@"Seg %d", i + 1] forSegment:i];
        [seg setWidth:70 forSegment:i];
    }
    [seg setSelectedSegment:0];
    place(t, seg, NSMakeRect(20, y, 220, 26), nil);
    y -= 40;
    NSSlider *slider = [NSSlider new];
    [slider setDoubleValue:0.4];
    place(t, slider, NSMakeRect(20, y, 220, 24), [NSSlider new]);
    NSSlider *ticks = [NSSlider new];
    [ticks setNumberOfTickMarks:5];
    [ticks setAllowsTickMarkValuesOnly:YES];
    place(t, ticks, NSMakeRect(500, y, 200, 30), nil);
    y -= 40;
    NSSlider *vertical = [[NSSlider alloc] initWithFrame:NSMakeRect(0, 0, 24, 120)];
    place(t, vertical, NSMakeRect(20, y - 100, 24, 120), nil);
    NSSlider *circular = [NSSlider new];
    [[circular cell] setSliderType:NSCircularSlider];
    place(t, circular, NSMakeRect(70, y - 30, 40, 40), nil);
    NSStepper *stepper = [NSStepper new];
    place(t, stepper, NSMakeRect(140, y - 30, 20, 28), [NSStepper new]);
    NSColorWell *well = [NSColorWell new];
    [well setColor:[NSColor blueColor]];
    place(t, well, NSMakeRect(240, y - 30, 50, 30), nil);
    y -= 140;
    NSDatePicker *date = [NSDatePicker new];
    [date setDateValue:[NSDate date]];
    place(t, date, NSMakeRect(20, y, 220, 26), nil);
    NSDatePicker *calendar = [NSDatePicker new];
    [calendar setDatePickerStyle:NSClockAndCalendarDatePickerStyle];
    [calendar setDateValue:[NSDate date]];
    place(t, calendar, NSMakeRect(260, y - 130, 280, 160), nil);
    y -= 40;
    NSPathControl *path = [NSPathControl new];
    [path setURL:[NSURL fileURLWithPath:@"/Applications/Developer"]];
    place(t, path, NSMakeRect(20, y, 220, 24), nil);
}

- (void)buildIndicators:(NSView *)t
{
    CGFloat y = NSHeight([t bounds]) - 50;
    NSProgressIndicator *bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, y, 300, 20)];

    [bar setIndeterminate:NO];
    [bar setDoubleValue:60];
    [t addSubview:bar];
    y -= 34;
    NSProgressIndicator *busy = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, y, 300, 20)];
    [busy setIndeterminate:YES];
    [busy startAnimation:nil];
    [t addSubview:busy];
    y -= 44;
    NSProgressIndicator *spin = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, y, 32, 32)];
    [spin setStyle:NSProgressIndicatorSpinningStyle];
    [spin startAnimation:nil];
    [t addSubview:spin];
    y -= 40;
    NSInteger styles[] = { 0, 1, 2, 3 };	/* relevancy, continuous, discrete, rating */
    NSString *names[] = { @"Relevancy", @"Continuous", @"Discrete", @"Rating" };
    for (int i = 0; i < 4; i++, y -= 30) {
        NSLevelIndicator *l = [[NSLevelIndicator alloc] initWithFrame:NSMakeRect(130, y, 200, 20)];
        SEL setStyle = NSSelectorFromString(@"setLevelIndicatorStyle:");
        id target = [l respondsToSelector:setStyle] ? l : [l cell];
        if ([target respondsToSelector:setStyle])
            ((void (*)(id, SEL, NSInteger))objc_msgSend)(target, setStyle, styles[i]);
        [l setMaxValue:5];
        [l setWarningValue:3];
        [l setCriticalValue:4];
        [l setDoubleValue:3.5];
        [t addSubview:label(names[i], NSMakeRect(20, y, 100, 20))];
        [t addSubview:l];
    }
    NSImageView *image = [[NSImageView alloc] initWithFrame:NSMakeRect(420, NSHeight([t bounds]) - 180, 128, 128)];
    [image setImage:[NSApp applicationIconImage] ?: [NSImage imageNamed:@"NSApplicationIcon"]];
    [image setImageFrameStyle:NSImageFrameGrayBezel];
    [t addSubview:image];
}

- (void)buildContainers:(NSView *)t
{
    NSRect b = [t bounds];
    NSBox *box = [[NSBox alloc] initWithFrame:NSMakeRect(20, NSHeight(b) - 160, 260, 130)];

    [box setTitle:@"Box with a title"];
    [[box contentView] addSubview:label(@"Inside the box", NSMakeRect(10, 40, 200, 20))];
    [t addSubview:box];
    NSSplitView *split = [[NSSplitView alloc] initWithFrame:NSMakeRect(300, NSHeight(b) - 160, 360, 130)];
    [split setVertical:YES];
    for (int i = 0; i < 2; i++) {
        NSTextField *pane = label(i ? @"Right pane" : @"Left pane", NSMakeRect(0, 0, 180, 130));
        [pane setDrawsBackground:YES];
        [pane setBackgroundColor:i ? [NSColor controlBackgroundColor] : [NSColor windowBackgroundColor]];
        [split addSubview:pane];
    }
    [t addSubview:split];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, NSHeight(b) - 330, 260, 150)];
    [scroll setHasVerticalScroller:YES];
    [scroll setHasHorizontalScroller:YES];
    NSTextView *long_ = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600)];
    NSMutableString *lines = [NSMutableString string];
    for (int i = 1; i <= 40; i++)
        [lines appendFormat:@"Line %d of a long text in a scroll view, wider than the view.\n", i];
    [long_ setString:lines];
    [scroll setDocumentView:long_];
    [t addSubview:scroll];
    Class stackClass = NSClassFromString(@"NSStackView");
    if (stackClass != Nil && [stackClass respondsToSelector:@selector(stackViewWithViews:)]) {
        NSStackView *stack = [stackClass stackViewWithViews:@[ button(@"One", 1, 7), button(@"Two", 1, 7),
            button(@"Three", 1, 7) ]];
        [stack setFrame:NSMakeRect(300, NSHeight(b) - 230, 360, 40)];
        [t addSubview:stack];
    }
    NSMatrix *matrix = [[NSMatrix alloc] initWithFrame:NSMakeRect(300, NSHeight(b) - 330, 200, 80)
        mode:NSRadioModeMatrix cellClass:[NSButtonCell class] numberOfRows:3 numberOfColumns:1];
    for (int i = 0; i < 3; i++) {
        NSButtonCell *c = [matrix cellAtRow:i column:0];
        [c setButtonType:NSRadioButton];
        [c setTitle:[NSString stringWithFormat:@"Matrix radio %d", i + 1]];
    }
    [t addSubview:matrix];
    NSTabView *inner = [[NSTabView alloc] initWithFrame:NSMakeRect(20, 20, 640, 120)];
    for (int i = 0; i < 3; i++) {
        NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:@(i)];
        [item setLabel:[NSString stringWithFormat:@"Inner tab %d", i + 1]];
        [inner addTabViewItem:item];
    }
    [t addSubview:inner];
}

- (void)buildLists:(NSView *)t
{
    NSRect b = [t bounds];
    NSScrollView *ts = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, NSHeight(b) - 230, 300, 200)];
    NSTableView *table = [[NSTableView alloc] initWithFrame:[[ts contentView] bounds]];
    for (NSString *name in @[ @"Name", @"Kind", @"Size" ]) {
        NSTableColumn *c = [[NSTableColumn alloc] initWithIdentifier:name];
        [[c headerCell] setStringValue:name];
        [c setWidth:90];
        [table addTableColumn:c];
    }
    [table setDataSource:self];
    [table setUsesAlternatingRowBackgroundColors:YES];
    [ts setDocumentView:table];
    [ts setHasVerticalScroller:YES];
    [t addSubview:ts];

    NSScrollView *os = [[NSScrollView alloc] initWithFrame:NSMakeRect(340, NSHeight(b) - 230, 300, 200)];
    NSOutlineView *outline = [[NSOutlineView alloc] initWithFrame:[[os contentView] bounds]];
    NSTableColumn *oc = [[NSTableColumn alloc] initWithIdentifier:@"Item"];
    [[oc headerCell] setStringValue:@"Outline"];
    [oc setWidth:280];
    [outline addTableColumn:oc];
    [outline setOutlineTableColumn:oc];
    [outline setDataSource:self];
    [os setDocumentView:outline];
    [os setHasVerticalScroller:YES];
    [t addSubview:os];
    [outline expandItem:@"Fruits"];

    NSBrowser *browser = [[NSBrowser alloc] initWithFrame:NSMakeRect(20, 20, 620, NSHeight(b) - 270)];
    [browser setDelegate:self];
    [t addSubview:browser];
    [browser loadColumnZero];
}

- (void)buildTextView:(NSView *)t
{
    NSScrollView *s = [[NSScrollView alloc] initWithFrame:NSInsetRect([t bounds], 20, 20)];
    NSTextView *tv = [[NSTextView alloc] initWithFrame:[[s contentView] bounds]];
    NSMutableAttributedString *a = [[NSMutableAttributedString alloc] init];
    NSFont *base = [NSFont systemFontOfSize:14];
    NSFontManager *fm = [NSFontManager sharedFontManager];
    void (^append)(NSString *, NSDictionary *) = ^(NSString *s_, NSDictionary *attrs) {
        [a appendAttributedString:[[NSAttributedString alloc] initWithString:s_ attributes:attrs]];
    };

    append(@"Rich text: ", @{ NSFontAttributeName : base });
    append(@"bold, ", @{ NSFontAttributeName : [fm convertFont:base toHaveTrait:NSBoldFontMask] });
    append(@"italic, ", @{ NSFontAttributeName : [fm convertFont:base toHaveTrait:NSItalicFontMask] });
    append(@"underlined, ", @{ NSFontAttributeName : base, NSUnderlineStyleAttributeName : @1 });
    append(@"red, ", @{ NSFontAttributeName : base, NSForegroundColorAttributeName : [NSColor redColor] });
    append(@"highlighted, ", @{ NSFontAttributeName : base, NSBackgroundColorAttributeName : [NSColor yellowColor] });
    append(@"large", @{ NSFontAttributeName : [NSFont systemFontOfSize:24] });
    append(@" and fixed pitch.\n\n", @{ NSFontAttributeName : [NSFont userFixedPitchFontOfSize:14] });
    append(@"Unicode: ñandú, Ελληνικά, Русский, 日本語, عربى, emoji 🙂.\n",
        @{ NSFontAttributeName : base });
    [[tv textStorage] setAttributedString:a];
    [tv setRichText:YES];
    if ([tv respondsToSelector:@selector(setUsesRuler:)])
        [tv setUsesRuler:YES];
    [s setDocumentView:tv];
    [s setHasVerticalScroller:YES];
    [t addSubview:s];
}

- (void)buildPanels:(NSView *)t
{
    struct { NSString *name; SEL action; } p[] = {
        { @"Open panel", @selector(showOpen:) }, { @"Save panel", @selector(showSave:) },
        { @"Alert", @selector(showAlert:) }, { @"Alert as a sheet", @selector(showSheet:) },
        { @"Font panel", @selector(showFonts:) }, { @"Color panel", @selector(showColors:) },
        { @"Page setup", @selector(showPageLayout:) }, { @"Print", @selector(showPrint:) },
        { @"About panel", @selector(orderFrontStandardAboutPanel:) },
        { @"Popover", @selector(showPopover:) },
    };
    CGFloat y = NSHeight([t bounds]) - 50;

    for (NSUInteger i = 0; i < sizeof(p) / sizeof(p[0]); i++, y -= 36) {
        NSButton *b = button(p[i].name, 1, 7);
        [b setTarget:p[i].action == @selector(orderFrontStandardAboutPanel:) ? NSApp : self];
        [b setAction:p[i].action];
        place(t, b, NSMakeRect(20, y, 200, 28), nil);
    }
}

- (void)run:(NSString *)what block:(void (^)(void))block
{
    /* On standard error, for a part that never returns. */
    fprintf(stderr, "Gallery: %s\n", [what UTF8String]);
    @try {
        block();
    } @catch (NSException *e) {
        [[GalleryReport sharedReport] noteFailure:what reason:[e reason] ?: [e name]];
        NSLog(@"Gallery: %@ failed: %@", what, e);
    }
}

- (void)showOpen:(id)sender { [self run:@"NSOpenPanel" block:^{ [[NSOpenPanel openPanel] runModal]; }]; }
- (void)showSave:(id)sender { [self run:@"NSSavePanel" block:^{ [[NSSavePanel savePanel] runModal]; }]; }
- (void)showFonts:(id)sender { [self run:@"NSFontPanel" block:^{ [[NSFontManager sharedFontManager] orderFrontFontPanel:nil]; }]; }
- (void)showColors:(id)sender { [self run:@"NSColorPanel" block:^{ [NSApp orderFrontColorPanel:nil]; }]; }
- (void)showPageLayout:(id)sender { [self run:@"NSPageLayout" block:^{ [[NSPageLayout pageLayout] runModal]; }]; }
- (void)showPrint:(id)sender
{
    [self run:@"NSPrintOperation" block:^{ [[NSPrintOperation printOperationWithView:[self->_window contentView]] runOperation]; }];
}

- (void)showAlert:(id)sender
{
    [self run:@"NSAlert" block:^{
        NSAlert *a = [NSAlert new];
        [a setMessageText:@"An alert"];
        [a setInformativeText:@"With informative text and three buttons."];
        [a addButtonWithTitle:@"OK"];
        [a addButtonWithTitle:@"Cancel"];
        [a addButtonWithTitle:@"Other"];
        [a runModal];
    }];
}

- (void)showSheet:(id)sender
{
    [self run:@"NSAlert sheet" block:^{
        NSAlert *a = [NSAlert new];
        SEL sheet = NSSelectorFromString(@"beginSheetModalForWindow:completionHandler:");
        [a setMessageText:@"An alert as a sheet"];
        if ([a respondsToSelector:sheet])
            ((void (*)(id, SEL, id, id))objc_msgSend)(a, sheet, self->_window, ^(NSInteger r) { (void)r; });
        else
            [a beginSheetModalForWindow:self->_window modalDelegate:nil didEndSelector:NULL contextInfo:NULL];
    }];
}

- (void)showPopover:(id)sender
{
    Class popover = NSClassFromString(@"NSPopover");

    if (popover == Nil) {
        [[GalleryReport sharedReport] noteFailure:@"NSPopover" reason:@"the class is missing"];
        [self refreshReport];
        return;
    }
    [self run:@"NSPopover" block:^{
        id p = [popover new];
        NSViewController *vc = [NSViewController new];
        [vc setView:label(@"A popover", NSMakeRect(0, 0, 160, 40))];
        ((void (*)(id, SEL, id))objc_msgSend)(p, NSSelectorFromString(@"setContentViewController:"), vc);
        ((void (*)(id, SEL, NSRect, id, NSUInteger))objc_msgSend)(p,
            NSSelectorFromString(@"showRelativeToRect:ofView:preferredEdge:"), [sender bounds], sender, NSMaxYEdge);
    }];
}

/* ---- data sources ---- */

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tv { return 20; }

- (id)tableView:(NSTableView *)tv objectValueForTableColumn:(NSTableColumn *)c row:(NSInteger)row
{
    NSString *col = [c identifier];

    if ([col isEqualToString:@"Name"])
        return [NSString stringWithFormat:@"File %ld", (long)row + 1];
    if ([col isEqualToString:@"Kind"])
        return row % 3 ? @"Document" : @"Folder";
    return [NSString stringWithFormat:@"%ld KB", (long)(row * 37 % 900 + 4)];
}

static NSDictionary *outlineData(void)
{
    static NSDictionary *d;

    if (d == nil)
        d = @{ @"Fruits" : @[ @"Apple", @"Orange", @"Pear" ], @"Vegetables" : @[ @"Carrot", @"Leek" ],
               @"Empty" : @[] };
    return d;
}

- (NSInteger)outlineView:(NSOutlineView *)ov numberOfChildrenOfItem:(id)item
{
    return item == nil ? [outlineData() count] : [outlineData()[item] count];
}

- (id)outlineView:(NSOutlineView *)ov child:(NSInteger)i ofItem:(id)item
{
    if (item == nil)
        return [[outlineData() allKeys] sortedArrayUsingSelector:@selector(compare:)][i];
    return outlineData()[item][i];
}

- (BOOL)outlineView:(NSOutlineView *)ov isItemExpandable:(id)item { return outlineData()[item] != nil; }
- (id)outlineView:(NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn *)c byItem:(id)item { return item; }

- (NSInteger)browser:(NSBrowser *)b numberOfRowsInColumn:(NSInteger)column { return column < 2 ? 5 : 3; }

- (void)browser:(NSBrowser *)b willDisplayCell:(id)cell atRow:(NSInteger)row column:(NSInteger)column
{
    [cell setStringValue:[NSString stringWithFormat:@"Column %ld, row %ld", (long)column + 1, (long)row + 1]];
    [cell setLeaf:column >= 2];
}

/* ---- toolbar ---- */

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)tb
{
    return @[ @"gallery.report", NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier,
              @"gallery.about" ];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)tb
{
    return @[ @"gallery.report", NSToolbarFlexibleSpaceItemIdentifier, @"gallery.about" ];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)tb itemForItemIdentifier:(NSString *)ident willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:ident];
    BOOL about = [ident isEqualToString:@"gallery.about"];

    [item setLabel:about ? @"About" : @"Report"];
    [item setImage:[NSImage imageNamed:about ? @"NSInfo" : @"NSAdvanced"]];
    [item setTarget:about ? NSApp : self];
    [item setAction:about ? @selector(orderFrontStandardAboutPanel:) : @selector(showReport:)];
    return item;
}

- (void)showReport:(id)sender { [_tabs selectTabViewItemAtIndex:TAB_COUNT - 1]; }

/* ---- the window and the report ---- */

/* The window's, its content's and a tab's sizes: in the title (to compare
 * screenshots of both systems), on the terminal and in the report. */
- (NSString *)sizes
{
    NSRect frame = [_window frame], content = [[_window contentView] frame], tab = [_tabs contentRect];

    return [NSString stringWithFormat:@"window %.0f×%.0f · content %.0f×%.0f · tab %.0f×%.0f",
        NSWidth(frame), NSHeight(frame), NSWidth(content), NSHeight(content), NSWidth(tab), NSHeight(tab)];
}

- (void)showSizes:(NSNotification *)note
{
    [_window setTitle:[NSString stringWithFormat:@"Gallery — %@", [self sizes]]];
    printf("Gallery: %s\n", [[self sizes] UTF8String]);
    fflush(stdout);
}

- (void)refreshReport
{
    NSString *text = [[GalleryReport sharedReport] text];

    if (_window != nil)
        text = [NSString stringWithFormat:@"Sizes: %@\n\n%@", [self sizes], text];
    [_reportView setString:text];
}

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
    GalleryReport *report = [GalleryReport sharedReport];

    [self buildMenus];
    [report checkAPI];
    [report checkMenu:[NSApp mainMenu] path:@""];
    if (_reportOnly) {
        fputs([[report text] UTF8String], stdout);
        exit(0);
    }

    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 80, 760, 620)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];
    [_window setTitle:@"Gallery"];
    /* The toolbar first: it takes its room from the content view (as on
     * the reference platform), so the tabs are laid out in what is left
     * and their top controls are not hidden under it. */
    [self run:@"NSToolbar" block:^{
        NSToolbar *tb = [[NSToolbar alloc] initWithIdentifier:@"gallery"];
        [tb setDelegate:self];
        [self->_window setToolbar:tb];
    }];
    _tabs = [[NSTabView alloc] initWithFrame:[[_window contentView] bounds]];
    [_tabs setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [[_window contentView] addSubview:_tabs];
    SEL builders[] = {
        @selector(buildButtons:), @selector(buildText:), @selector(buildChoices:),
        @selector(buildIndicators:), @selector(buildContainers:), @selector(buildLists:),
        @selector(buildTextView:), @selector(buildPanels:), NULL,
    };
    for (NSUInteger i = 0; i < TAB_COUNT; i++) {
        NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:kTabs[i]];
        [item setLabel:kTabs[i]];
        [_tabs addTabViewItem:item];
        /* A view of its own for each tab, the size of the tab's content
         * (its controls are placed from the top of it). */
        NSView *v = [[NSView alloc] initWithFrame:[_tabs contentRect]];
        [item setView:v];
        SEL builder = builders[i];
        if (builder != NULL) {
            [self run:[NSString stringWithFormat:@"tab %@", kTabs[i]] block:^{
                ((void (*)(id, SEL, NSView *))objc_msgSend)(self, builder, v);
            }];
        } else {
            NSScrollView *s = [[NSScrollView alloc] initWithFrame:NSInsetRect([v bounds], 10, 10)];
            _reportView = [[NSTextView alloc] initWithFrame:[[s contentView] bounds]];
            [_reportView setEditable:NO];
            [_reportView setFont:[NSFont userFixedPitchFontOfSize:12]];
            [s setDocumentView:_reportView];
            [s setHasVerticalScroller:YES];
            [v addSubview:s];
        }
    }
    [self refreshReport];
    [self showSizes:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showSizes:)
        name:NSWindowDidResizeNotification object:_window];
    if (_firstTab >= 0 && (NSUInteger)_firstTab < TAB_COUNT)
        [_tabs selectTabViewItemAtIndex:_firstTab];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES; }

@end

int
main(int argc, const char *argv[])
{
    @autoreleasepool {
        BOOL reportOnly = NO;

        for (int i = 1; i < argc; i++)
            if (strcmp(argv[i], "-report") == 0)
                reportOnly = YES;
        /* The argument domain (-GalleryTab N) or the app's defaults. */
        NSInteger tab = [[NSUserDefaults standardUserDefaults] integerForKey:@"GalleryTab"];
        [NSApplication sharedApplication];
        GalleryController *controller = [[GalleryController alloc] initWithTab:tab reportOnly:reportOnly];
        [NSApp setDelegate:controller];
        [NSApp run];
    }
    return 0;
}
