/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's commands beyond K2's operations (FilesCommands.h).
 */

#import "FilesCommands.h"
#import "FilesOperations.h"
#import "FilesPreferences.h"
#include <os/log.h>

static NSString *const kRecentFolders = @"RecentFolders";
static const NSUInteger kRecentCount = 10;

BOOL FilesTrashIsEmpty(void)
{
    NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[[FilesOperations trashURL] path]
                                                                         error:NULL];
    for (NSString *name in names)
        if (![name isEqualToString:@".DS_Store"])
            return NO;
    return YES;
}

static BOOL confirm(NSString *message, NSString *button)
{
    NSAlert *alert = [[NSAlert alloc] init];

    [alert setMessageText:message];
    [alert setInformativeText:NSLocalizedString(@"You can't undo this action.", nil)];
    [alert addButtonWithTitle:button];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    return [alert runModal] == NSAlertFirstButtonReturn;
}

void FilesEmptyTrash(BOOL ask)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *path = [[FilesOperations trashURL] path];

    if (FilesTrashIsEmpty())
        return;
    if (ask && FilesWarnsOnEmptyTrash() && !confirm(NSLocalizedString(@"Are you sure you want to permanently erase the items in the Trash?", nil),
                        NSLocalizedString(@"Empty Trash", nil)))
        return;
    for (NSString *name in [fm contentsOfDirectoryAtPath:path error:NULL])
        [fm removeItemAtPath:[path stringByAppendingPathComponent:name] error:NULL];
    os_log_info(OS_LOG_DEFAULT, "emptied the Trash");
}

BOOL FilesDeleteImmediately(NSArray<NSURL *> *items)
{
    if ([items count] == 0)
        return NO;
    NSString *message = [items count] == 1
        ? [NSString stringWithFormat:NSLocalizedString(@"Are you sure you want to delete “%@” immediately?", nil),
                                     [[NSFileManager defaultManager] displayNameAtPath:[items[0] path]]]
        : [NSString stringWithFormat:NSLocalizedString(@"Are you sure you want to delete these %lu items immediately?", nil),
                                     (unsigned long)[items count]];
    if (!confirm(message, NSLocalizedString(@"Delete", nil)))
        return NO;
    for (NSURL *url in items) {
        NSError *error = nil;
        if (![[NSFileManager defaultManager] removeItemAtURL:url error:&error]) {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"“%@” could not be deleted.", nil),
                                                             [url lastPathComponent]]];
            [alert setInformativeText:[error localizedDescription] ?: @""];
            [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
            [alert runModal];
            return NO;
        }
        os_log_info(OS_LOG_DEFAULT, "deleted %{public}s", [[url path] fileSystemRepresentation]);
    }
    return YES;
}

void FilesCompress(NSArray<NSURL *> *items, void (^done)(NSURL *archive))
{
    if ([items count] == 0) {
        done(nil);
        return;
    }
    NSURL *folder = [items[0] URLByDeletingLastPathComponent];
    NSString *base = [items count] == 1 ? [[items[0] lastPathComponent] stringByAppendingPathExtension:@"zip"]
                                        : @"Archive.zip";
    NSURL *archive = [folder URLByAppendingPathComponent:[FilesOperations freeNameFor:base inFolder:folder copy:NO]];
    NSTask *task = [[NSTask alloc] init];

    /* One item: ditto keeps its folder in the archive, as the reference's
       archives have it; several: zip, each by its name in the folder. */
    if ([items count] == 1) {
        [task setLaunchPath:@"/usr/bin/ditto"];
        [task setArguments:@[ @"-c", @"-k", @"--sequesterRsrc", @"--keepParent", [items[0] path], [archive path] ]];
    } else {
        NSMutableArray *args = [NSMutableArray arrayWithObjects:@"-q", @"-r", @"-y", [archive path], nil];
        for (NSURL *url in items)
            [args addObject:[url lastPathComponent]];
        [task setLaunchPath:@"/usr/bin/zip"];
        [task setCurrentDirectoryPath:[folder path]];
        [task setArguments:args];
    }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        int status = -1;
        @try {
            [task launch];
            [task waitUntilExit];
            status = [task terminationStatus];
        } @catch (NSException *e) {
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            if (status != 0) {
                [[NSFileManager defaultManager] removeItemAtURL:archive error:NULL];
                NSAlert *alert = [[NSAlert alloc] init];
                [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"“%@” could not be made.", nil),
                                                                 [archive lastPathComponent]]];
                [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
                [alert runModal];
                done(nil);
                return;
            }
            os_log_info(OS_LOG_DEFAULT, "compressed into %{public}s", [[archive path] fileSystemRepresentation]);
            done(archive);
        });
    });
}

NSArray<NSURL *> *FilesMakeAliases(NSArray<NSURL *> *items)
{
    NSMutableArray<NSURL *> *made = [NSMutableArray array];

    for (NSURL *url in items) {
        NSURL *folder = [url URLByDeletingLastPathComponent];
        NSString *name = [NSString stringWithFormat:NSLocalizedString(@"%@ alias", nil), [url lastPathComponent]];
        name = [FilesOperations freeNameFor:name inFolder:folder copy:NO];
        NSURL *alias = [folder URLByAppendingPathComponent:name];
        if ([[NSFileManager defaultManager] createSymbolicLinkAtPath:[alias path] withDestinationPath:[url path]
                                                               error:NULL]) {
            [made addObject:alias];
            os_log_info(OS_LOG_DEFAULT, "alias %{public}s", [[alias path] fileSystemRepresentation]);
        } else
            NSBeep();
    }
    return made;
}

NSURL *FilesOriginalOf(NSURL *url)
{
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[url path] error:NULL];

    if (![[attributes fileType] isEqualToString:NSFileTypeSymbolicLink])
        return nil;
    NSString *target = [[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:[url path] error:NULL];
    if (target == nil)
        return nil;
    if (![target isAbsolutePath])
        target = [[[url path] stringByDeletingLastPathComponent] stringByAppendingPathComponent:target];
    target = [target stringByStandardizingPath];
    return [[NSFileManager defaultManager] fileExistsAtPath:target] ? [NSURL fileURLWithPath:target] : nil;
}

NSURL *FilesNewFolderWithItems(NSArray<NSURL *> *items, NSURL *folder)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *name = [FilesOperations freeNameFor:NSLocalizedString(@"New Folder With Items", nil)
                                         inFolder:folder copy:NO];
    NSURL *made = [folder URLByAppendingPathComponent:name isDirectory:YES];

    if ([items count] == 0 || ![fm createDirectoryAtURL:made withIntermediateDirectories:NO attributes:nil error:NULL]) {
        NSBeep();
        return nil;
    }
    for (NSURL *url in items)
        if (![fm moveItemAtURL:url toURL:[made URLByAppendingPathComponent:[url lastPathComponent]] error:NULL])
            NSBeep();
    os_log_info(OS_LOG_DEFAULT, "new folder with items %{public}s", [[made path] fileSystemRepresentation]);
    return made;
}

void FilesCopyPathnames(NSArray<NSURL *> *items)
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];

    if ([items count] == 0)
        return;
    [pb clearContents];
    [pb setString:[[items valueForKey:@"path"] componentsJoinedByString:@"\n"] forType:NSPasteboardTypeString];
}

void FilesNoteRecentFolder(NSURL *folder)
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableArray<NSString *> *paths = [[defaults stringArrayForKey:kRecentFolders] ?: @[] mutableCopy];
    NSString *path = [[folder path] stringByStandardizingPath];

    if (path == nil || [[paths firstObject] isEqualToString:path])
        return;
    [paths removeObject:path];
    [paths insertObject:path atIndex:0];
    while ([paths count] > kRecentCount)
        [paths removeLastObject];
    [defaults setObject:paths forKey:kRecentFolders];
}

NSArray<NSURL *> *FilesRecentFolders(void)
{
    NSMutableArray<NSURL *> *urls = [NSMutableArray array];
    BOOL dir;

    for (NSString *path in [[NSUserDefaults standardUserDefaults] stringArrayForKey:kRecentFolders] ?: @[])
        if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir] && dir)
            [urls addObject:[NSURL fileURLWithPath:path isDirectory:YES]];
    return urls;
}

void FilesClearRecentFolders(void)
{
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:kRecentFolders];
}

NSURL *FilesAskFolder(NSWindow *window)
{
    static NSString *last;
    (void)window;   /* asked as a modal window, not a sheet */
    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 320, 22)];

    [field setStringValue:last ?: @""];
    for (;;) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"Go to the folder:", nil)];
        [alert setAccessoryView:field];
        [alert addButtonWithTitle:NSLocalizedString(@"Go", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
        [[alert window] setInitialFirstResponder:field];
        if ([alert runModal] != NSAlertFirstButtonReturn)
            return nil;
        NSString *typed = [[field stringValue] stringByTrimmingCharactersInSet:
                                                   [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        last = typed;
        NSString *path = [[typed stringByExpandingTildeInPath] stringByStandardizingPath];
        BOOL dir = NO;
        if ([path isAbsolutePath] && [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir])
            return [NSURL fileURLWithPath:path isDirectory:dir];
        NSAlert *missing = [[NSAlert alloc] init];
        [missing setMessageText:NSLocalizedString(@"The folder can't be found.", nil)];
        [missing addButtonWithTitle:NSLocalizedString(@"OK", nil)];
        [missing runModal];
    }
}

void FilesAddToDock(NSArray<NSURL *> *items)
{
    for (NSURL *url in items)
        if ([[[url pathExtension] lowercaseString] isEqualToString:@"app"])
            [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"org.minkar.dock.keep"
                                                                           object:[url path]
                                                                         userInfo:nil
                                                               deliverImmediately:YES];
}
