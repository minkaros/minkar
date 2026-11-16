/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's file operations (Atlas.md, K2). Copies are made by
 * hand, a block at a time, so the progress window can count bytes and
 * Stop can end them between blocks; a move within a volume is a rename.
 */

#import "FilesOperations.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <unistd.h>

/* Where an item in the Trash was (NSFileManager's trashItemAtURL: sets it). */
static const char *const kPutBackAttribute = "org.minkar.trash.put-back";
/* A progress window only for what takes longer than this. */
static const NSTimeInterval kProgressDelay = 0.5;
static const size_t kBlock = 1 << 20;

/* ---- The progress window ---- */

@interface FilesProgress : NSObject
@property (atomic) BOOL cancelled;
@property (atomic) unsigned long long done, total;
@property (nonatomic, copy) NSString *text;
- (void)start;
- (void)finish;
@end

@implementation FilesProgress {
    NSPanel *_panel;
    NSTextField *_label;
    NSProgressIndicator *_bar;
    NSTimer *_timer;
    CFAbsoluteTime _started;
}

- (void)start
{
    _started = CFAbsoluteTimeGetCurrent();
    _timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(tick:)
                                            userInfo:nil repeats:YES];
}

- (void)show
{
    _panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 420, 96)
                                        styleMask:NSWindowStyleMaskTitled
                                          backing:NSBackingStoreBuffered defer:NO];
    [_panel setTitle:NSLocalizedString(@"Files", nil)];
    [_panel setReleasedWhenClosed:NO];
    NSView *content = [_panel contentView];
    _label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 62, 380, 18)];
    [_label setEditable:NO];
    [_label setBordered:NO];
    [_label setDrawsBackground:NO];
    [_label setStringValue:self.text ?: @""];
    _bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 40, 300, 16)];
    [_bar setIndeterminate:NO];
    [_bar setMinValue:0];
    [_bar setMaxValue:1];
    NSButton *stop = [[NSButton alloc] initWithFrame:NSMakeRect(326, 32, 80, 30)];
    [stop setTitle:NSLocalizedString(@"Stop", nil)];
    [stop setBezelStyle:NSRoundedBezelStyle];
    [stop setTarget:self];
    [stop setAction:@selector(stop:)];
    for (NSView *v in @[ _label, _bar, stop ])
        [content addSubview:v];
    [_panel center];
    [_panel orderFront:nil];
}

- (void)tick:(NSTimer *)timer
{
    (void)timer;
    if (_panel == nil && CFAbsoluteTimeGetCurrent() - _started >= kProgressDelay)
        [self show];
    unsigned long long total = self.total;
    [_bar setDoubleValue:total > 0 ? (double)self.done / (double)total : 0];
}

- (void)stop:(id)sender
{
    (void)sender;
    self.cancelled = YES;
}

- (void)finish
{
    [_timer invalidate];
    _timer = nil;
    [_panel orderOut:nil];
    _panel = nil;
}
@end

/* ---- Copying, a block at a time ---- */

static unsigned long long treeSize(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0)
        return 0;
    if (!S_ISDIR(st.st_mode))
        return S_ISREG(st.st_mode) ? (unsigned long long)st.st_size : 0;
    unsigned long long sum = 0;
    for (NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:@(path) error:NULL])
        sum += treeSize([[@(path) stringByAppendingPathComponent:name] fileSystemRepresentation]);
    return sum;
}

static void copyAttributes(const char *from, const char *to)
{
    char names[4096];
    ssize_t n = listxattr(from, names, sizeof(names), XATTR_NOFOLLOW);
    for (ssize_t i = 0; i < n; i += (ssize_t)strlen(names + i) + 1) {
        char value[65536];
        ssize_t len = getxattr(from, names + i, value, sizeof(value), 0, XATTR_NOFOLLOW);
        if (len >= 0 && strcmp(names + i, kPutBackAttribute) != 0)
            setxattr(to, names + i, value, (size_t)len, 0, XATTR_NOFOLLOW);
    }
}

/* 0, or an errno; ECANCELED when stopped. */
static int copyTree(const char *from, const char *to, FilesProgress *progress)
{
    struct stat st;

    if (progress.cancelled)
        return ECANCELED;
    if (lstat(from, &st) != 0)
        return errno;
    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t len = readlink(from, target, sizeof(target) - 1);
        if (len < 0)
            return errno;
        target[len] = '\0';
        return symlink(target, to) == 0 ? 0 : errno;
    }
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(to, (st.st_mode & 07777) | S_IRWXU) != 0)
            return errno;
        for (NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:@(from) error:NULL]) {
            int e = copyTree([[@(from) stringByAppendingPathComponent:name] fileSystemRepresentation],
                             [[@(to) stringByAppendingPathComponent:name] fileSystemRepresentation], progress);
            if (e != 0)
                return e;
        }
        chmod(to, st.st_mode & 07777);
    } else if (S_ISREG(st.st_mode)) {
        int in = open(from, O_RDONLY), out = in < 0 ? -1 : open(to, O_WRONLY | O_CREAT | O_EXCL, st.st_mode & 07777);
        int e = in < 0 || out < 0 ? errno : 0;
        char *buffer = e == 0 ? malloc(kBlock) : NULL;
        while (e == 0) {
            ssize_t n = read(in, buffer, kBlock);
            if (n == 0)
                break;
            if (n < 0 || write(out, buffer, (size_t)n) != n)
                e = errno ? errno : EIO;
            else
                progress.done += (unsigned long long)n;
            if (e == 0 && progress.cancelled)
                e = ECANCELED;
        }
        free(buffer);
        if (in >= 0)
            close(in);
        if (out >= 0)
            close(out);
        if (e != 0) {
            unlink(to);
            return e;
        }
    } else
        return ENOTSUP;     /* devices, sockets, pipes: not copied */
    copyAttributes(from, to);
    struct timeval times[2] = { { st.st_atimespec.tv_sec, (int)(st.st_atimespec.tv_nsec / 1000) },
                                { st.st_mtimespec.tv_sec, (int)(st.st_mtimespec.tv_nsec / 1000) } };
    lutimes(to, times);
    return 0;
}

static void removeTree(NSString *path)
{
    [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
}

/* ---- The operations ---- */

@implementation FilesOperations {
    dispatch_queue_t _queue;
}

+ (instancetype)shared
{
    static FilesOperations *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ shared = [[FilesOperations alloc] init]; });
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _undoManager = [[NSUndoManager alloc] init];
        [_undoManager setGroupsByEvent:NO];
        _queue = dispatch_queue_create("org.minkar.Files.operations", NULL);
    }
    return self;
}

+ (NSURL *)trashURL
{
    return [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@".Trash"] isDirectory:YES];
}

+ (BOOL)isInTrash:(NSURL *)url
{
    NSString *trash = [[[self trashURL] path] stringByStandardizingPath];
    NSString *path = [[url path] stringByStandardizingPath];
    return [path hasPrefix:[trash stringByAppendingString:@"/"]];
}

+ (NSString *)freeNameFor:(NSString *)name inFolder:(NSURL *)folder copy:(BOOL)copy
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *extension = [name pathExtension];
    NSString *stem = [extension length] ? [name stringByDeletingPathExtension] : name;
    NSString *base = copy ? [stem stringByAppendingFormat:@" %@", NSLocalizedString(@"copy", nil)] : stem;

    for (int n = 1;; n++) {
        NSString *candidate = n == 1 ? (copy ? base : stem) : [NSString stringWithFormat:@"%@ %d", base, n];
        if ([extension length])
            candidate = [candidate stringByAppendingPathExtension:extension];
        if (![fm fileExistsAtPath:[[folder path] stringByAppendingPathComponent:candidate]])
            return candidate;
    }
}

- (void)tell:(NSString *)message about:(NSURL *)url error:(int)e
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:message, [url lastPathComponent]]];
    [alert setInformativeText:@(strerror(e))];
    [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [alert runModal];
}

/* Copies (or, across volumes, moves by copying) items into folder under
   the names given, off the main thread; done gets what was made. */
- (void)transfer:(NSArray<NSURL *> *)items to:(NSArray<NSURL *> *)targets moving:(BOOL)moving
           label:(NSString *)label done:(void (^)(NSArray<NSURL *> *))done
{
    FilesProgress *progress = [[FilesProgress alloc] init];
    progress.text = label;
    [progress start];
    dispatch_async(_queue, ^{
        unsigned long long total = 0;
        for (NSURL *u in items)
            total += treeSize([[u path] fileSystemRepresentation]);
        progress.total = total;
        NSMutableArray<NSURL *> *made = [NSMutableArray array];
        int e = 0;
        NSURL *failed = nil;
        for (NSUInteger i = 0; i < [items count] && e == 0; i++) {
            const char *from = [[items[i] path] fileSystemRepresentation];
            const char *to = [[targets[i] path] fileSystemRepresentation];
            if (moving && rename(from, to) == 0) {
                [made addObject:targets[i]];
                continue;
            }
            if (moving && errno != EXDEV) {
                e = errno;
                failed = items[i];
                break;
            }
            e = copyTree(from, to, progress);
            if (e != 0) {
                removeTree([targets[i] path]);
                failed = items[i];
                break;
            }
            if (moving)
                removeTree([items[i] path]);
            [made addObject:targets[i]];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [progress finish];
            if (e != 0 && e != ECANCELED)
                [self tell:moving ? NSLocalizedString(@"“%@” could not be moved.", nil)
                                  : NSLocalizedString(@"“%@” could not be copied.", nil)
                     about:failed error:e];
            done(made);
        });
    });
}

static NSArray<NSURL *> *targetsIn(NSURL *folder, NSArray<NSURL *> *items, BOOL copyNames)
{
    NSMutableArray *targets = [NSMutableArray array];
    NSMutableSet *taken = [NSMutableSet set];
    for (NSURL *u in items) {
        NSString *name = [u lastPathComponent];
        BOOL sameFolder = [[[u URLByDeletingLastPathComponent] path] isEqual:[folder path]];
        NSString *free = [FilesOperations freeNameFor:name inFolder:folder copy:copyNames || sameFolder];
        /* Two of the same name in one operation. */
        for (int n = 2; [taken containsObject:free]; n++)
            free = [NSString stringWithFormat:@"%@ %d", [name stringByDeletingPathExtension], n];
        [taken addObject:free];
        [targets addObject:[folder URLByAppendingPathComponent:free]];
    }
    return targets;
}

static NSString *labelFor(NSString *one, NSString *many, NSArray<NSURL *> *items, NSURL *folder)
{
    return [items count] == 1 ? [NSString stringWithFormat:one, [items[0] lastPathComponent], [folder lastPathComponent]]
        : [NSString stringWithFormat:many, (unsigned long)[items count], [folder lastPathComponent]];
}

- (void)copyItems:(NSArray<NSURL *> *)items toFolder:(NSURL *)folder done:(void (^)(NSArray<NSURL *> *))done
{
    NSArray *targets = targetsIn(folder, items, NO);
    NSString *label = labelFor(NSLocalizedString(@"Copying “%@” to “%@”", nil),
                               NSLocalizedString(@"Copying %lu items to “%@”", nil), items, folder);
    [self transfer:items to:targets moving:NO label:label done:^(NSArray<NSURL *> *made) {
        if ([made count] > 0)
            [self registerUndoMaking:made redo:^{ [self copyItems:items toFolder:folder done:nil]; }
                                name:NSLocalizedString(@"Copy", nil)];
        if (done)
            done(made);
    }];
}

- (void)duplicateItems:(NSArray<NSURL *> *)items done:(void (^)(NSArray<NSURL *> *))done
{
    if ([items count] == 0)
        return;
    NSURL *folder = [items[0] URLByDeletingLastPathComponent];
    NSArray *targets = targetsIn(folder, items, YES);
    NSString *label = labelFor(NSLocalizedString(@"Duplicating “%@”", nil),
                               NSLocalizedString(@"Duplicating %lu items", nil), items, folder);
    [self transfer:items to:targets moving:NO label:label done:^(NSArray<NSURL *> *made) {
        if ([made count] > 0)
            [self registerUndoMaking:made redo:^{ [self duplicateItems:items done:nil]; }
                                name:NSLocalizedString(@"Duplicate", nil)];
        if (done)
            done(made);
    }];
}

- (void)moveItems:(NSArray<NSURL *> *)items toFolder:(NSURL *)folder done:(void (^)(NSArray<NSURL *> *))done
{
    NSMutableArray *moving = [NSMutableArray array];
    for (NSURL *u in items)     /* already there, or into itself: left */
        if (![[[u URLByDeletingLastPathComponent] path] isEqual:[folder path]] &&
            ![[folder path] hasPrefix:[[u path] stringByAppendingString:@"/"]] && ![[folder path] isEqual:[u path]])
            [moving addObject:u];
    if ([moving count] == 0) {
        if (done)
            done(@[]);
        return;
    }
    NSArray *targets = targetsIn(folder, moving, NO);
    NSString *label = labelFor(NSLocalizedString(@"Moving “%@” to “%@”", nil),
                               NSLocalizedString(@"Moving %lu items to “%@”", nil), moving, folder);
    [self transfer:moving to:targets moving:YES label:label done:^(NSArray<NSURL *> *made) {
        if ([made count] > 0)
            [self registerUndoMoving:made backTo:[moving subarrayWithRange:NSMakeRange(0, [made count])]
                                name:NSLocalizedString(@"Move", nil)];
        if (done)
            done(made);
    }];
}

- (NSURL *)renameItem:(NSURL *)item to:(NSString *)name
{
    if ([name length] == 0 || [name containsString:@"/"] || [name isEqualToString:[item lastPathComponent]])
        return nil;
    NSURL *target = [[item URLByDeletingLastPathComponent] URLByAppendingPathComponent:name];
    if ([[NSFileManager defaultManager] fileExistsAtPath:[target path]]) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:
            NSLocalizedString(@"The name “%@” is already taken. Please choose a different name.", nil), name]];
        [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
        [alert runModal];
        return nil;
    }
    if (rename([[item path] fileSystemRepresentation], [[target path] fileSystemRepresentation]) != 0) {
        [self tell:NSLocalizedString(@"“%@” could not be renamed.", nil) about:item error:errno];
        return nil;
    }
    [self registerUndoMoving:@[ target ] backTo:@[ item ] name:NSLocalizedString(@"Rename", nil)];
    return target;
}

- (NSURL *)newFolderIn:(NSURL *)folder
{
    NSString *name = [FilesOperations freeNameFor:NSLocalizedString(@"untitled folder", nil) inFolder:folder copy:NO];
    NSURL *url = [folder URLByAppendingPathComponent:name];
    if (mkdir([[url path] fileSystemRepresentation], 0755) != 0) {
        [self tell:NSLocalizedString(@"The folder “%@” could not be made.", nil) about:url error:errno];
        return nil;
    }
    [self registerUndoMaking:@[ url ] redo:^{ [self newFolderIn:folder]; } name:NSLocalizedString(@"New Folder", nil)];
    return url;
}

- (void)trashItems:(NSArray<NSURL *> *)items
{
    NSMutableArray *trashed = [NSMutableArray array], *from = [NSMutableArray array];
    for (NSURL *u in items) {
        NSURL *result = nil;
        NSError *error = nil;
        if ([[NSFileManager defaultManager] trashItemAtURL:u resultingItemURL:&result error:&error]) {
            [trashed addObject:result];
            [from addObject:u];
        } else
            [self tell:NSLocalizedString(@"“%@” could not be moved to the Trash.", nil) about:u error:EPERM];
    }
    if ([trashed count] > 0)
        [self registerUndoMoving:trashed backTo:from name:NSLocalizedString(@"Move to Trash", nil)];
}

+ (NSURL *)putBackLocationOf:(NSURL *)trashed
{
    char path[PATH_MAX];
    ssize_t len = getxattr([[trashed path] fileSystemRepresentation], kPutBackAttribute, path, sizeof(path) - 1, 0,
                           XATTR_NOFOLLOW);
    if (len <= 0)
        return nil;
    path[len] = '\0';
    return [NSURL fileURLWithPath:@(path)];
}

- (void)putBackItems:(NSArray<NSURL *> *)items
{
    NSMutableArray *back = [NSMutableArray array], *from = [NSMutableArray array];
    for (NSURL *u in items) {
        NSURL *origin = [FilesOperations putBackLocationOf:u];
        if (origin == nil) {
            NSBeep();
            continue;
        }
        NSURL *folder = [origin URLByDeletingLastPathComponent];
        [[NSFileManager defaultManager] createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:NULL];
        NSURL *target = [folder URLByAppendingPathComponent:
            [FilesOperations freeNameFor:[origin lastPathComponent] inFolder:folder copy:NO]];
        if (rename([[u path] fileSystemRepresentation], [[target path] fileSystemRepresentation]) != 0 &&
            ![[NSFileManager defaultManager] moveItemAtURL:u toURL:target error:NULL]) {
            [self tell:NSLocalizedString(@"“%@” could not be put back.", nil) about:u error:errno];
            continue;
        }
        removexattr([[target path] fileSystemRepresentation], kPutBackAttribute, XATTR_NOFOLLOW);
        [back addObject:target];
        [from addObject:u];
    }
    if ([back count] > 0)
        [self registerUndoMoving:back backTo:from name:NSLocalizedString(@"Put Back", nil)];
}

/* -- Undo -- */

/* Undone, what an operation made goes to the Trash; redone, made again. */
- (void)registerUndoMaking:(NSArray<NSURL *> *)made redo:(void (^)(void))redo name:(NSString *)name
{
    [_undoManager beginUndoGrouping];
    [_undoManager registerUndoWithTarget:self handler:^(FilesOperations *ops) {
        for (NSURL *u in made) {
            NSURL *result = nil;
            [[NSFileManager defaultManager] trashItemAtURL:u resultingItemURL:&result error:NULL];
            if (result != nil)  /* not to be put back where nothing was */
                removexattr([[result path] fileSystemRepresentation], kPutBackAttribute, XATTR_NOFOLLOW);
        }
        [ops->_undoManager registerUndoWithTarget:ops handler:^(FilesOperations *again) {
            (void)again;
            redo();
        }];
    }];
    [_undoManager setActionName:name];
    [_undoManager endUndoGrouping];
}

/* Undone, items go back to where they were (renamed back, or moved), and
   the redo moves them forward again. */
- (void)registerUndoMoving:(NSArray<NSURL *> *)now backTo:(NSArray<NSURL *> *)before name:(NSString *)name
{
    [_undoManager beginUndoGrouping];
    [_undoManager registerUndoWithTarget:self handler:^(FilesOperations *ops) {
        NSMutableArray *moved = [NSMutableArray array], *was = [NSMutableArray array];
        for (NSUInteger i = 0; i < [now count]; i++) {
            const char *from = [[now[i] path] fileSystemRepresentation], *to = [[before[i] path] fileSystemRepresentation];
            if (access(to, F_OK) != 0 && (rename(from, to) == 0 ||
                [[NSFileManager defaultManager] moveItemAtURL:now[i] toURL:before[i] error:NULL])) {
                [moved addObject:before[i]];
                [was addObject:now[i]];
            }
        }
        [ops registerUndoMoving:moved backTo:was name:name];
    }];
    [_undoManager setActionName:name];
    [_undoManager endUndoGrouping];
}

@end
