/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A Menu Extra in the menu bar or not, for the panes' "Show ... in menu
 * bar" boxes: SystemUIServer's menuExtras list (its domain), as on the
 * reference platform, and org.minkar.systemuiserver.extras posted so it
 * shows or hides the extra at once. Header-only: each pane is a bundle of
 * its own.
 */

#ifndef MINKAR_MENU_EXTRA_LIST_H
#define MINKAR_MENU_EXTRA_LIST_H

#import <Foundation/Foundation.h>
#include <notify.h>

#define kMenuExtrasDirectory	@"/System/Library/CoreServices/Menu Extras"
#define kUIServerDomain		CFSTR("org.minkar.systemuiserver")

/* The path of a Menu Extra by its name ("Clock", "Volume"...). */
static inline NSString *MenuExtraPath(NSString *name)
{
    return [kMenuExtrasDirectory stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"menu"]];
}

/* SystemUIServer's list, or its default one when the user has none. */
static inline NSMutableArray *MenuExtraList(void)
{
    CFPreferencesAppSynchronize(kUIServerDomain);
    CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("menuExtras"), kUIServerDomain);
    NSMutableArray *list;

    if (value != NULL && CFGetTypeID(value) == CFArrayGetTypeID())
        list = [(__bridge NSArray *)value mutableCopy];
    else {
        list = [NSMutableArray array];
        for (NSString *name in @[ @"Clock", @"Displays", @"Network", @"Volume" ])
            [list addObject:MenuExtraPath(name)];
    }
    if (value != NULL)
        CFRelease(value);
    return list;
}

static inline BOOL MenuExtraIsShown(NSString *name)
{
    return [MenuExtraList() containsObject:MenuExtraPath(name)];
}

static inline void MenuExtraSetShown(NSString *name, BOOL shown)
{
    NSMutableArray *list = MenuExtraList();
    NSString *path = MenuExtraPath(name);

    [list removeObject:path];
    /* The clock goes back to its place, the right end (the list's first). */
    if (shown && [name isEqualToString:@"Clock"])
        [list insertObject:path atIndex:0];
    else if (shown)
        [list addObject:path];
    CFPreferencesSetAppValue(CFSTR("menuExtras"), (__bridge CFArrayRef)list, kUIServerDomain);
    CFPreferencesAppSynchronize(kUIServerDomain);
    notify_post("org.minkar.systemuiserver.extras");
}

#endif
