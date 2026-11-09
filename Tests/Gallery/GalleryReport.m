/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "GalleryReport.h"
#include <dlfcn.h>
#include <objc/runtime.h>

/* AppKit's classes of API 19 and before that apps use for their
 * interface (the controls, views, panels and the classes around them). */
static NSString *const kClasses[] = {
    @"NSAlert", @"NSBox", @"NSBrowser", @"NSButton", @"NSCollectionView", @"NSColorPanel",
    @"NSColorWell", @"NSComboBox", @"NSDatePicker", @"NSDocument", @"NSDocumentController",
    @"NSFontManager", @"NSFontPanel", @"NSGridView", @"NSHelpManager", @"NSImageView",
    @"NSLevelIndicator", @"NSMatrix", @"NSMenu", @"NSOpenPanel", @"NSOutlineView",
    @"NSPageLayout", @"NSPathControl", @"NSPopover", @"NSPopUpButton", @"NSPredicateEditor",
    @"NSPrintOperation", @"NSPrintPanel", @"NSProgressIndicator", @"NSRuleEditor",
    @"NSSavePanel", @"NSScrollView", @"NSSearchField", @"NSSecureTextField",
    @"NSSegmentedControl", @"NSSharingServicePicker", @"NSSlider", @"NSSplitView",
    @"NSSplitViewController", @"NSStackView", @"NSStatusBar", @"NSStepper", @"NSSwitch",
    @"NSTableView", @"NSTabView", @"NSTabViewController", @"NSTextField", @"NSTextFinder",
    @"NSTextView", @"NSTokenField", @"NSToolbar", @"NSTouchBar", @"NSViewController",
    @"NSVisualEffectView", @"NSWindowController", @"NSWindowTab", @"NSWindowTabGroup",
};

/* Methods: class, then selector (instance methods; "+" for a class one). */
static NSString *const kMethods[][2] = {
    { @"NSApplication", @"orderFrontStandardAboutPanel:" },
    { @"NSApplication", @"orderFrontStandardAboutPanelWithOptions:" },
    { @"NSApplication", @"hide:" },
    { @"NSApplication", @"hideOtherApplications:" },
    { @"NSApplication", @"unhideAllApplications:" },
    { @"NSApplication", @"showHelp:" },
    { @"NSApplication", @"orderFrontCharacterPalette:" },
    { @"NSApplication", @"arrangeInFront:" },
    { @"NSApplication", @"registerServicesMenuSendTypes:returnTypes:" },
    { @"NSWindow", @"toggleFullScreen:" },
    { @"NSWindow", @"toggleToolbarShown:" },
    { @"NSWindow", @"runToolbarCustomizationPalette:" },
    { @"NSWindow", @"performZoom:" },
    { @"NSWindow", @"performMiniaturize:" },
    { @"NSWindow", @"addTabbedWindow:ordered:" },
    { @"NSWindow", @"setTabbingMode:" },
    { @"NSWindow", @"setTitlebarAppearsTransparent:" },
    { @"NSWindow", @"setAppearance:" },
    { @"NSView", @"setWantsLayer:" },
    { @"NSView", @"addConstraint:" },
    { @"NSView", @"safeAreaInsets" },
    { @"NSTextView", @"setUsesFindBar:" },
    { @"NSTextView", @"performFindPanelAction:" },
    { @"NSTextView", @"toggleRuler:" },
    { @"NSTextView", @"checkSpelling:" },
    { @"NSTextView", @"orderFrontSubstitutionsPanel:" },
    { @"NSTextView", @"startSpeaking:" },
    { @"NSTableView", @"makeViewWithIdentifier:owner:" },
    { @"NSTableView", @"setUsesAutomaticRowHeights:" },
    { @"NSOutlineView", @"setAutosaveExpandedItems:" },
    { @"NSButton", @"setContentTintColor:" },
    { @"NSButton", @"setBezelColor:" },
    { @"NSSegmentedControl", @"setSegmentDistribution:" },
    { @"NSDocument", @"autosaveWithImplicitCancellability:completionHandler:" },
    { @"NSDocument", @"browseDocumentVersions:" },
    { @"NSFontManager", @"orderFrontFontPanel:" },
    { @"NSColorPanel", @"+sharedColorPanel" },
    { @"NSPrintOperation", @"+printOperationWithView:" },
    { @"NSTextField", @"setPlaceholderString:" },
    { @"NSColor", @"+systemBlueColor" },
    { @"NSColor", @"+controlAccentColor" },
    { @"NSColor", @"+labelColor" },
    { @"NSLevelIndicator", @"setLevelIndicatorStyle:" },
    { @"NSAlert", @"beginSheetModalForWindow:completionHandler:" },
    { @"NSWindow", @"beginSheet:completionHandler:" },
    { @"NSSavePanel", @"beginWithCompletionHandler:" },
    { @"NSSavePanel", @"beginSheetModalForWindow:completionHandler:" },
};

/* Protocols, formal in the reference SDK. */
static NSString *const kProtocols[] = {
    @"NSApplicationDelegate", @"NSWindowDelegate", @"NSTableViewDataSource",
    @"NSTableViewDelegate", @"NSOutlineViewDataSource", @"NSOutlineViewDelegate",
    @"NSBrowserDelegate", @"NSToolbarDelegate", @"NSTextFieldDelegate", @"NSTextViewDelegate",
    @"NSMenuDelegate", @"NSComboBoxDataSource", @"NSTabViewDelegate", @"NSSplitViewDelegate",
    @"NSCollectionViewDataSource", @"NSCollectionViewDelegate", @"NSStackViewDelegate",
};

/* Constants and functions AppKit and Foundation export (their symbols). */
static const char *const kSymbols[] = {
    "NSImageNameApplicationIcon", "NSImageNameInfo", "NSImageNameAdvanced",
    "NSImageNameActionTemplate", "NSImageNameAddTemplate", "NSImageNameRemoveTemplate",
    "NSImageNameFolder", "NSImageNameCaution", "NSImageNameTrashEmpty",
    "NSToolbarFlexibleSpaceItemIdentifier", "NSToolbarSpaceItemIdentifier",
    "NSToolbarShowFontsItemIdentifier", "NSToolbarShowColorsItemIdentifier",
    "NSToolbarPrintItemIdentifier", "NSToolbarToggleSidebarItemIdentifier",
    "NSAppearanceNameAqua", "NSAppearanceNameDarkAqua",
    "NSFontWeightRegular", "NSFontWeightBold",
    /* Foundation's, which apps use with these. */
    "NSProtocolFromString", "NSStringFromProtocol",
};

@implementation GalleryReport {
    NSMutableSet<NSString *> *_lines;
}

+ (instancetype)sharedReport
{
    static GalleryReport *report;

    if (report == nil)
        report = [GalleryReport new];
    return report;
}

- (instancetype)init
{
    if ((self = [super init]) != nil)
        _lines = [NSMutableSet set];
    return self;
}

- (void)noteFailure:(NSString *)what reason:(NSString *)reason
{
    [_lines addObject:[NSString stringWithFormat:@"failed %@: %@", what, reason]];
}

- (void)checkAPI
{
    for (NSUInteger i = 0; i < sizeof(kProtocols) / sizeof(kProtocols[0]); i++)
        if (objc_getProtocol([kProtocols[i] UTF8String]) == NULL)
            [_lines addObject:[NSString stringWithFormat:@"missing protocol %@", kProtocols[i]]];
    for (NSUInteger i = 0; i < sizeof(kSymbols) / sizeof(kSymbols[0]); i++)
        if (dlsym(RTLD_DEFAULT, kSymbols[i]) == NULL)
            [_lines addObject:[NSString stringWithFormat:@"missing symbol %s", kSymbols[i]]];
    for (NSUInteger i = 0; i < sizeof(kClasses) / sizeof(kClasses[0]); i++)
        if (NSClassFromString(kClasses[i]) == Nil)
            [_lines addObject:[NSString stringWithFormat:@"missing class %@", kClasses[i]]];
    for (NSUInteger i = 0; i < sizeof(kMethods) / sizeof(kMethods[0]); i++) {
        Class c = NSClassFromString(kMethods[i][0]);
        NSString *name = kMethods[i][1];
        BOOL classMethod = [name hasPrefix:@"+"];
        SEL sel = NSSelectorFromString(classMethod ? [name substringFromIndex:1] : name);

        if (c == Nil)
            continue;	/* the class is reported already */
        if (classMethod ? ![c respondsToSelector:sel] : ![c instancesRespondToSelector:sel])
            [_lines addObject:[NSString stringWithFormat:@"missing method %@ %@%@",
                kMethods[i][0], classMethod ? @"+" : @"-", NSStringFromSelector(sel)]];
    }
}

- (void)checkMenu:(NSMenu *)menu path:(NSString *)path
{
    for (NSMenuItem *item in [menu itemArray]) {
        NSString *here = [path length] ? [NSString stringWithFormat:@"%@ > %@", path, [item title]] :
            [item title];

        if ([item hasSubmenu]) {
            [self checkMenu:[item submenu] path:here];
            continue;
        }
        if ([item action] == NULL || [item isSeparatorItem])
            continue;
        /* Whatever has the focus answers most of these (the text view, the
         * window, the document): missing only if no AppKit class that
         * takes them implements it. */
        static NSString *const responders[] = {
            @"NSApplication", @"NSWindow", @"NSView", @"NSText", @"NSTextView", @"NSTableView",
            @"NSDocument", @"NSDocumentController", @"NSFontManager", @"NSResponder",
        };
        BOOL implemented = [item target] != nil && [[item target] respondsToSelector:[item action]];
        for (NSUInteger i = 0; !implemented && i < sizeof(responders) / sizeof(responders[0]); i++)
            implemented = [NSClassFromString(responders[i]) instancesRespondToSelector:[item action]];
        if (!implemented)
            [_lines addObject:[NSString stringWithFormat:@"missing action %@ (%@)", here,
                NSStringFromSelector([item action])]];
    }
}

- (NSArray<NSString *> *)lines
{
    return [[_lines allObjects] sortedArrayUsingSelector:@selector(compare:)];
}

- (NSString *)text
{
    NSArray *lines = [self lines];

    if ([lines count] == 0)
        return @"Nothing missing.\n";
    return [[lines componentsJoinedByString:@"\n"] stringByAppendingString:@"\n"];
}

@end
