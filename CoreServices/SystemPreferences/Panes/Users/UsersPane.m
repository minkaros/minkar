/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Users & Groups pane (Atlas.md, M5), laid out as the reference
 * platform's: on the left the current user, the other users and Login
 * Options, with + and - below and the lock; on the right, for a user, its
 * name, Change Password... (the current user's own, with the old password)
 * or Reset Password..., and "Allow user to administer this computer"; for
 * Login Options, Automatic login, "Display login window as" (List of users
 * or Name and password: SHOWFULLNAME) and "Show the Sleep, Restart, and
 * Shut Down buttons" (PowerOffDisabled).
 *
 * Everything goes through OpenDirectory (directoryd): the lock asks for an
 * administrator's name and password, which the pane's node keeps
 * (ODNodeSetCredentials); with it the node creates and deletes users, adds
 * them to admin and takes them out, resets passwords and sets the login
 * window's options (directoryd's own custom calls). The view is made in
 * code.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <OpenDirectory/OpenDirectory.h>
#include <os/log.h>
#include <unistd.h>

@interface UsersPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate>
@end

static const CGFloat kWidth = 668, kHeight = 420;

/* directoryd's own ODNodeCustomCall codes (odapi.c). */
#define kCustomLoginOptions	0x4c4f
#define kCustomRemoveHome	0x524d

#define LOGIN_PREFS	@"/Library/Preferences/com.apple.loginwindow.plist"

static NSTextField *label(NSString *text, NSRect frame, CGFloat size, BOOL bold)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setFont:bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size]];
    return field;
}

static NSButton *button(NSString *title, NSRect frame, NSButtonType type, id target, SEL action)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:type];
    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    if (type == NSMomentaryPushInButton)
        [b setBezelStyle:NSRoundedBezelStyle];
    return b;
}

/* A user's picture: its initials in a circle. */
@interface UserPicture : NSView
@property (copy) NSString *fullName;
@end

@implementation UserPicture
- (void)drawRect:(NSRect)dirty
{
    NSRect r = NSInsetRect([self bounds], 1, 1);
    [[NSColor colorWithCalibratedRed:0.35 green:0.45 blue:0.52 alpha:1] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:r] fill];
    NSMutableString *initials = [NSMutableString string];
    for (NSString *word in [_fullName componentsSeparatedByString:@" "])
        if ([word length] > 0 && [initials length] < 2)
            [initials appendString:[[word substringToIndex:1] uppercaseString]];
    NSDictionary *a = @{ NSFontAttributeName: [NSFont systemFontOfSize:r.size.height * 0.38 weight:NSFontWeightMedium],
                         NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSSize s = [initials sizeWithAttributes:a];
    [initials drawAtPoint:NSMakePoint(NSMidX(r) - s.width / 2, NSMidY(r) - s.height / 2) withAttributes:a];
}
@end

@implementation UsersPane {
    ODNode *_node;
    BOOL _unlocked;
    NSString *_admin;                    /* who unlocked */
    NSMutableArray<NSDictionary *> *_rows; /* kind: header, user, options */
    NSTableView *_list;
    NSButton *_add, *_remove, *_lock;
    NSTextField *_lockLabel;
    NSView *_userView, *_optionsView;
    UserPicture *_picture;
    NSTextField *_name, *_kind;
    NSButton *_password, *_adminBox;
    NSPopUpButton *_autoLogin;
    NSButton *_list_, *_namePassword, *_powerButtons;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

/* ---- The directory ------------------------------------------------------ */

- (ODNode *)node
{
    if (_node == nil)
        _node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:NULL];
    return _node;
}

- (ODRecord *)user:(NSString *)name
{
    return [[self node] recordWithRecordType:kODRecordTypeUsers name:name attributes:nil error:NULL];
}

- (BOOL)isAdmin:(NSString *)name
{
    ODRecord *admin = [[self node] recordWithRecordType:kODRecordTypeGroups name:@"admin" attributes:nil error:NULL];
    NSArray *members = [admin valuesForAttribute:kODAttributeTypeGroupMembership error:NULL];
    NSString *gid = [[[self user:name] valuesForAttribute:kODAttributeTypePrimaryGroupID error:NULL] firstObject];
    return [members containsObject:name] || [gid isEqualToString:@"80"];
}

/* The users who can log in (UID from 500, not a service account), by name. */
- (NSArray<NSDictionary *> *)users
{
    ODQuery *q = [ODQuery queryWithNode:[self node] forRecordTypes:kODRecordTypeUsers attribute:nil
        matchType:kODMatchAny queryValues:nil returnAttributes:kODAttributeTypeStandardOnly maximumResults:0 error:NULL];
    NSMutableArray *users = [NSMutableArray array];
    for (ODRecord *r in [q resultsAllowingPartial:NO error:NULL]) {
        NSString *uid = [[r valuesForAttribute:kODAttributeTypeUniqueID error:NULL] firstObject];
        NSString *full = [[r valuesForAttribute:kODAttributeTypeFullName error:NULL] firstObject];
        NSString *shell = [[r valuesForAttribute:kODAttributeTypeUserShell error:NULL] firstObject];
        if ([uid intValue] < 500 || [[r recordName] hasPrefix:@"_"] || [shell hasSuffix:@"/false"])
            continue;
        [users addObject:@{ @"name": [r recordName], @"fullName": [full length] ? full : [r recordName] }];
    }
    [users sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
        return [a[@"fullName"] localizedCaseInsensitiveCompare:b[@"fullName"]];
    }];
    return users;
}

- (NSDictionary *)loginOptions
{
    return [NSDictionary dictionaryWithContentsOfFile:LOGIN_PREFS] ?: @{};
}

/* directoryd sets the login window's options (an administrator's node). */
- (BOOL)setLoginOption:(NSString *)key value:(id)value
{
    /* An empty string removes the key (a property list has no null). */
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:@{ key: value ?: @"" }
        format:NSPropertyListBinaryFormat_v1_0 options:0 error:NULL];
    NSError *error = nil;
    [[self node] customCall:kCustomLoginOptions sendData:data error:&error];
    os_log_info(OS_LOG_DEFAULT, "users pane: %{public}s set (%ld)", [key UTF8String], (long)[error code]);
    return error == nil;
}

/* ---- The view ----------------------------------------------------------- */

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 80, 200, 320)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _list = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"user"];
    [column setWidth:196];
    [column setEditable:NO];
    [_list addTableColumn:column];
    [_list setHeaderView:nil];
    [_list setRowHeight:36];
    [_list setDataSource:self];
    [_list setDelegate:self];
    [scroll setDocumentView:_list];
    [view addSubview:scroll];

    _add = button(@"+", NSMakeRect(20, 52, 28, 24), NSMomentaryPushInButton, self, @selector(addUser:));
    _remove = button(@"−", NSMakeRect(48, 52, 28, 24), NSMomentaryPushInButton, self, @selector(removeUser:));
    [_add setBezelStyle:NSSmallSquareBezelStyle];
    [_remove setBezelStyle:NSSmallSquareBezelStyle];
    [view addSubview:_add];
    [view addSubview:_remove];

    _lock = button(@"", NSMakeRect(20, 14, 28, 28), NSMomentaryPushInButton, self, @selector(toggleLock:));
    [_lock setBezelStyle:NSSmallSquareBezelStyle];
    [_lock setBordered:NO];
    _lockLabel = label(@"", NSMakeRect(54, 18, 400, 18), 12, NO);
    [view addSubview:_lock];
    [view addSubview:_lockLabel];

    /* A user. */
    _userView = [[NSView alloc] initWithFrame:NSMakeRect(240, 80, 408, 320)];
    _picture = [[UserPicture alloc] initWithFrame:NSMakeRect(0, 236, 72, 72)];
    _name = label(@"", NSMakeRect(88, 274, 320, 26), 18, YES);
    _kind = label(@"", NSMakeRect(88, 252, 320, 18), 12, NO);
    _password = button([self text:@"Change Password…"], NSMakeRect(84, 200, 180, 28), NSMomentaryPushInButton,
        self, @selector(changePassword:));
    _adminBox = button([self text:@"Allow user to administer this computer"], NSMakeRect(0, 150, 400, 20),
        NSSwitchButton, self, @selector(toggleAdmin:));
    for (NSView *v in @[ _picture, _name, _kind, _password, _adminBox ])
        [_userView addSubview:v];
    [view addSubview:_userView];

    /* Login Options. */
    _optionsView = [[NSView alloc] initWithFrame:NSMakeRect(240, 80, 408, 320)];
    NSTextField *autoLabel = label([self text:@"Automatic login:"], NSMakeRect(0, 280, 170, 20), 13, NO);
    [autoLabel setAlignment:NSRightTextAlignment];
    _autoLogin = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(176, 276, 220, 26) pullsDown:NO];
    [_autoLogin setTarget:self];
    [_autoLogin setAction:@selector(autoLoginChanged:)];
    NSTextField *asLabel = label([self text:@"Display login window as:"], NSMakeRect(0, 240, 170, 20), 13, NO);
    [asLabel setAlignment:NSRightTextAlignment];
    _list_ = button([self text:@"List of users"], NSMakeRect(176, 240, 220, 20), NSRadioButton, self, @selector(displayChanged:));
    _namePassword = button([self text:@"Name and password"], NSMakeRect(176, 216, 220, 20), NSRadioButton, self,
        @selector(displayChanged:));
    _powerButtons = button([self text:@"Show the Sleep, Restart, and Shut Down buttons"], NSMakeRect(176, 180, 230, 36),
        NSSwitchButton, self, @selector(powerChanged:));
    for (NSView *v in @[ autoLabel, _autoLogin, asLabel, _list_, _namePassword, _powerButtons ])
        [_optionsView addSubview:v];
    [view addSubview:_optionsView];

    [self setMainView:view];
    [self reload];
    [self updateLock];
    return view;
}

- (void)reload
{
    NSString *me = NSUserName();
    NSArray *users = [self users];
    NSInteger selected = [_list selectedRow];

    _rows = [NSMutableArray array];
    [_rows addObject:@{ @"kind": @"header", @"title": [self text:@"Current User"] }];
    for (NSDictionary *u in users)
        if ([u[@"name"] isEqualToString:me])
            [_rows addObject:[u mutableCopy]];
    if ([_rows count] == 1)
        [_rows addObject:[@{ @"name": me, @"fullName": NSFullUserName() ?: me } mutableCopy]];
    if ([users count] > 1 || ![[users valueForKey:@"name"] containsObject:me])
        [_rows addObject:@{ @"kind": @"header", @"title": [self text:@"Other Users"] }];
    for (NSDictionary *u in users)
        if (![u[@"name"] isEqualToString:me])
            [_rows addObject:[u mutableCopy]];
    [_rows addObject:@{ @"kind": @"options", @"title": [self text:@"Login Options"] }];
    for (NSMutableDictionary *row in _rows)
        if (row[@"name"] != nil)
            row[@"admin"] = @([self isAdmin:row[@"name"]]);
    [_list reloadData];
    if (selected < 1 || selected >= (NSInteger)[_rows count])
        selected = 1;
    [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)selected] byExtendingSelection:NO];
    [self showSelection];
    NSMutableArray *listed = [NSMutableArray array];
    for (NSDictionary *row in _rows)
        if (row[@"name"] != nil)
            [listed addObject:[NSString stringWithFormat:@"%@%@", row[@"name"], [row[@"admin"] boolValue] ? @" (admin)" : @""]];
    os_log_info(OS_LOG_DEFAULT, "users pane lists %{public}s", [[listed componentsJoinedByString:@", "] UTF8String]);
}

- (NSDictionary *)selectedRow
{
    NSInteger row = [_list selectedRow];
    return row >= 0 && row < (NSInteger)[_rows count] ? _rows[(NSUInteger)row] : nil;
}

- (void)showSelection
{
    NSDictionary *row = [self selectedRow];
    BOOL options = [row[@"kind"] isEqualToString:@"options"];
    BOOL me = [row[@"name"] isEqualToString:NSUserName()];

    [_optionsView setHidden:!options];
    [_userView setHidden:options || row[@"name"] == nil];
    if (row[@"name"] != nil) {
        [_picture setFullName:row[@"fullName"]];
        [_picture setNeedsDisplay:YES];
        [_name setStringValue:row[@"fullName"]];
        [_kind setStringValue:[row[@"admin"] boolValue] ? [self text:@"Admin"] : [self text:@"Standard"]];
        [_password setTitle:me ? [self text:@"Change Password…"] : [self text:@"Reset Password…"]];
        [_password setEnabled:me || _unlocked];
        [_adminBox setState:[row[@"admin"] boolValue] ? NSOnState : NSOffState];
        /* Not the administrator who unlocked: it would lock itself out. */
        [_adminBox setEnabled:_unlocked && ![row[@"name"] isEqualToString:_admin]];
    }
    if (options) {
        NSDictionary *prefs = [self loginOptions];
        [_autoLogin removeAllItems];
        [_autoLogin addItemWithTitle:[self text:@"Off"]];
        [[_autoLogin lastItem] setRepresentedObject:@""];
        for (NSDictionary *u in [self users]) {
            [_autoLogin addItemWithTitle:u[@"fullName"]];
            [[_autoLogin lastItem] setRepresentedObject:u[@"name"]];
        }
        NSInteger index = [_autoLogin indexOfItemWithRepresentedObject:prefs[@"autoLoginUser"] ?: @""];
        [_autoLogin selectItemAtIndex:index >= 0 ? index : 0];
        BOOL full = [prefs[@"SHOWFULLNAME"] boolValue];
        [_list_ setState:full ? NSOffState : NSOnState];
        [_namePassword setState:full ? NSOnState : NSOffState];
        [_powerButtons setState:[prefs[@"PowerOffDisabled"] boolValue] ? NSOffState : NSOnState];
        for (NSControl *c in @[ _autoLogin, _list_, _namePassword, _powerButtons ])
            [c setEnabled:_unlocked];
    }
    NSString *name = row[@"name"];
    [_remove setEnabled:_unlocked && name != nil && ![name isEqualToString:NSUserName()] &&
        ![name isEqualToString:_admin]];
    [_add setEnabled:_unlocked];
}

- (void)updateLock
{
    NSImage *image = [NSImage imageNamed:_unlocked ? NSImageNameLockUnlockedTemplate : NSImageNameLockLockedTemplate];
    [_lock setImage:image];
    if (image == nil)
        [_lock setTitle:_unlocked ? @"🔓" : @"🔒"];
    [_lockLabel setStringValue:_unlocked ? [self text:@"Click the lock to prevent further changes."] :
        [self text:@"Click the lock to make changes."]];
    [self showSelection];
}

/* ---- The list ----------------------------------------------------------- */

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_rows count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)i
{
    NSDictionary *row = _rows[(NSUInteger)i];
    if (row[@"name"] == nil) {
        BOOL header = [row[@"kind"] isEqualToString:@"header"];
        return [[NSAttributedString alloc] initWithString:row[@"title"] attributes:@{
            NSFontAttributeName: header ? [NSFont boldSystemFontOfSize:11] : [NSFont systemFontOfSize:13],
            NSForegroundColorAttributeName: header ? [NSColor grayColor] : [NSColor textColor] }];
    }
    NSMutableAttributedString *s = [[NSMutableAttributedString alloc] initWithString:row[@"fullName"]
        attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:13] }];
    [s appendAttributedString:[[NSAttributedString alloc] initWithString:[@"\n" stringByAppendingString:
        [row[@"admin"] boolValue] ? [self text:@"Admin"] : [self text:@"Standard"]]
        attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:11],
                      NSForegroundColorAttributeName: [NSColor grayColor] }]];
    return s;
}

- (BOOL)tableView:(NSTableView *)table shouldSelectRow:(NSInteger)i
{
    return ![_rows[(NSUInteger)i][@"kind"] isEqualToString:@"header"];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [self showSelection];
}

/* ---- Asking ------------------------------------------------------------- */

/* An alert with labeled fields: their values, or nil when cancelled. */
- (NSArray<NSString *> *)ask:(NSString *)message fields:(NSArray<NSArray *> *)fields button:(NSString *)title
{
    NSAlert *alert = [[NSAlert alloc] init];
    NSView *accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 30 * [fields count])];
    NSMutableArray<NSTextField *> *inputs = [NSMutableArray array];

    [alert setMessageText:message];
    [alert addButtonWithTitle:title];
    [alert addButtonWithTitle:[self text:@"Cancel"]];
    for (NSUInteger i = 0; i < [fields count]; i++) {
        CGFloat y = 30 * ([fields count] - 1 - i);
        NSTextField *l = label(fields[i][0], NSMakeRect(0, y + 3, 110, 18), 12, NO);
        [l setAlignment:NSRightTextAlignment];
        NSTextField *f = [fields[i][1] boolValue] ? [[NSSecureTextField alloc] initWithFrame:NSMakeRect(116, y, 200, 24)]
                                                  : [[NSTextField alloc] initWithFrame:NSMakeRect(116, y, 200, 24)];
        if ([fields[i] count] > 2)
            [f setStringValue:fields[i][2]];
        [accessory addSubview:l];
        [accessory addSubview:f];
        [inputs addObject:f];
    }
    [alert setAccessoryView:accessory];
    [[alert window] setInitialFirstResponder:inputs[0]];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return nil;
    return [inputs valueForKey:@"stringValue"];
}

- (void)tell:(NSString *)message error:(NSError *)error
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:message];
    if (error != nil)
        [alert setInformativeText:[error localizedDescription]];
    [alert runModal];
}

/* ---- Actions ------------------------------------------------------------ */

- (void)toggleLock:(id)sender
{
    if (_unlocked) {
        _unlocked = NO;
        _admin = nil;
        _node = nil;   /* a new node, without the credentials */
        [self updateLock];
        return;
    }
    NSArray *values = [self ask:[self text:@"System Preferences is trying to unlock Users & Groups preferences."]
        fields:@[ @[ [self text:@"User Name:"], @NO, NSUserName() ], @[ [self text:@"Password:"], @YES ] ]
        button:[self text:@"Unlock"]];
    if (values == nil)
        return;
    NSError *error = nil;
    ODNode *node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:NULL];
    if (![self isAdmin:values[0]] ||
        ![node setCredentialsWithRecordType:kODRecordTypeUsers recordName:values[0] password:values[1] error:&error]) {
        os_log_info(OS_LOG_DEFAULT, "users pane: not unlocked by %{public}s", [values[0] UTF8String]);
        [self tell:[self text:@"The name and password you entered are not an administrator's."] error:nil];
        return;
    }
    _node = node;
    _unlocked = YES;
    _admin = [values[0] copy];
    os_log_info(OS_LOG_DEFAULT, "users pane: unlocked by %{public}s", [_admin UTF8String]);
    [self updateLock];
}

- (void)addUser:(id)sender
{
    NSArray *v = [self ask:[self text:@"New Account"] fields:@[
        @[ [self text:@"Full Name:"], @NO ], @[ [self text:@"Account Name:"], @NO ],
        @[ [self text:@"Password:"], @YES ], @[ [self text:@"Verify:"], @YES ] ]
        button:[self text:@"Create User"]];
    if (v == nil)
        return;
    NSString *name = [v[1] length] ? v[1] : [[[v[0] lowercaseString] componentsSeparatedByCharactersInSet:
        [[NSCharacterSet alphanumericCharacterSet] invertedSet]] componentsJoinedByString:@""];
    if ([name length] == 0 || ![v[2] isEqualToString:v[3]] || [v[2] length] == 0) {
        [self tell:[self text:@"The account needs a name and a password, typed twice the same."] error:nil];
        return;
    }
    NSError *error = nil;
    ODRecord *r = [[self node] createRecordWithRecordType:kODRecordTypeUsers name:name
        attributes:@{ kODAttributeTypeFullName: @[ [v[0] length] ? v[0] : name ] } error:&error];
    if (r == nil || ![r changePassword:nil toPassword:v[2] error:&error]) {
        os_log_info(OS_LOG_DEFAULT, "users pane: %{public}s not created (%ld)", [name UTF8String], (long)[error code]);
        [self tell:[self text:@"The account could not be created."] error:error];
        [self reload];
        return;
    }
    os_log_info(OS_LOG_DEFAULT, "users pane: created %{public}s", [name UTF8String]);
    [self reload];
}

- (void)removeUser:(id)sender
{
    NSDictionary *row = [self selectedRow];
    NSString *name = row[@"name"];
    if (name == nil)
        return;
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:[self text:@"Are you sure you want to delete the user account “%@”?"],
        row[@"fullName"]]];
    [alert setInformativeText:[self text:@"To delete this user account, select what you want to do with the home folder."]];
    NSView *choices = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 48)];
    NSButton *keep = button([self text:@"Don’t change the home folder"], NSMakeRect(0, 26, 320, 20), NSRadioButton,
        self, @selector(radioChoice:));
    NSButton *drop = button([self text:@"Delete the home folder"], NSMakeRect(0, 2, 320, 20), NSRadioButton,
        self, @selector(radioChoice:));
    [keep setState:NSOnState];
    [choices addSubview:keep];
    [choices addSubview:drop];
    [alert setAccessoryView:choices];
    [alert addButtonWithTitle:[self text:@"Delete User"]];
    [alert addButtonWithTitle:[self text:@"Cancel"]];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;
    NSError *error = nil;
    ODRecord *r = [self user:name];
    if (r == nil || ![r deleteRecordAndReturnError:&error]) {
        [self tell:[self text:@"The account could not be deleted."] error:error];
        return;
    }
    if ([drop state] == NSOnState)
        [[self node] customCall:kCustomRemoveHome sendData:[name dataUsingEncoding:NSUTF8StringEncoding] error:&error];
    os_log_info(OS_LOG_DEFAULT, "users pane: deleted %{public}s%{public}s", [name UTF8String],
                [drop state] == NSOnState ? " and its home" : "");
    [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:1] byExtendingSelection:NO];
    [self reload];
}

/* One radio button of its siblings on. */
- (void)radioChoice:(NSButton *)sender
{
    for (NSView *v in [[sender superview] subviews])
        if ([v isKindOfClass:[NSButton class]] && v != sender)
            [(NSButton *)v setState:NSOffState];
    [sender setState:NSOnState];
}

- (void)changePassword:(id)sender
{
    NSString *name = [self selectedRow][@"name"];
    BOOL me = [name isEqualToString:NSUserName()];
    NSMutableArray *fields = [NSMutableArray array];
    if (me)
        [fields addObject:@[ [self text:@"Old Password:"], @YES ]];
    [fields addObject:@[ [self text:@"New Password:"], @YES ]];
    [fields addObject:@[ [self text:@"Verify:"], @YES ]];
    NSArray *v = [self ask:me ? [self text:@"Change Password"] :
        [NSString stringWithFormat:[self text:@"Reset password for “%@”"], [self selectedRow][@"fullName"]]
        fields:fields button:me ? [self text:@"Change Password"] : [self text:@"Reset Password"]];
    if (v == nil)
        return;
    NSString *old = me ? v[0] : nil, *new = v[me ? 1 : 0], *again = v[me ? 2 : 1];
    if (![new isEqualToString:again] || [new length] == 0) {
        [self tell:[self text:@"The new password, typed twice the same, is needed."] error:nil];
        return;
    }
    NSError *error = nil;
    /* The user's own, with the old password: a node without credentials
       is enough; another's, the administrator's node. */
    ODNode *node = me && !_unlocked ? [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:NULL]
                                    : [self node];
    ODRecord *r = [node recordWithRecordType:kODRecordTypeUsers name:name attributes:nil error:NULL];
    BOOL ok = [r changePassword:old toPassword:new error:&error];
    os_log_info(OS_LOG_DEFAULT, "users pane: password of %{public}s %{public}s (%ld)", [name UTF8String],
                ok ? "changed" : "not changed", (long)[error code]);
    if (!ok)
        [self tell:[self text:@"The password could not be changed."] error:error];
}

- (void)toggleAdmin:(NSButton *)sender
{
    NSString *name = [self selectedRow][@"name"];
    ODRecord *admin = [[self node] recordWithRecordType:kODRecordTypeGroups name:@"admin" attributes:nil error:NULL];
    NSError *error = nil;
    BOOL ok = [sender state] == NSOnState ? [admin addValue:name toAttribute:kODAttributeTypeGroupMembership error:&error]
                                          : [admin removeValue:name fromAttribute:kODAttributeTypeGroupMembership error:&error];
    os_log_info(OS_LOG_DEFAULT, "users pane: %{public}s %{public}s admin (%ld)", [name UTF8String],
                [sender state] == NSOnState ? "made" : "no longer", (long)[error code]);
    if (!ok)
        [self tell:[self text:@"The change could not be made."] error:error];
    [self reload];
}

- (void)autoLoginChanged:(id)sender
{
    NSString *name = [[_autoLogin selectedItem] representedObject];
    if ([name length] > 0) {
        /* As the reference does: the user's password, checked. */
        NSArray *v = [self ask:[NSString stringWithFormat:[self text:@"Enter the password for “%@”"], [_autoLogin titleOfSelectedItem]]
            fields:@[ @[ [self text:@"Password:"], @YES ] ] button:[self text:@"OK"]];
        if (v == nil || ![[self user:name] verifyPassword:v[0] error:NULL]) {
            [self showSelection];
            return;
        }
    }
    [self setLoginOption:@"autoLoginUser" value:[name length] ? name : nil];
    [self showSelection];
}

- (void)displayChanged:(NSButton *)sender
{
    BOOL full = sender == _namePassword;
    /* As the reference: 1 or 0, not removed. */
    [self setLoginOption:@"SHOWFULLNAME" value:@(full)];
    [self showSelection];
}

- (void)powerChanged:(NSButton *)sender
{
    [self setLoginOption:@"PowerOffDisabled" value:@([sender state] != NSOnState)];
    [self showSelection];
}

@end
