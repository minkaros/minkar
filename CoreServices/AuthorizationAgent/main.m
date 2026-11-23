/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Authorization Agent: the user's side of authorization (the maintainer's
 * choice, 2026-11-23: authorization in full). authd (APSL) evaluates a
 * right's mechanisms by asking an agent over XPC, in its protocol
 * (OSX/authd/agent.c): it connects to one of three services, creates a
 * mechanism (plugin and name), invokes it with hints and a context, and
 * takes back a result and the context it filled. This process, started by
 * launchd when authd connects, answers all three:
 *
 *   com.apple.security.agent        the user's mechanisms: builtin:authenticate
 *   com.apple.security.agent.login  (the same, for the login session)
 *                                   shows Atlas's window that asks for an
 *                                   administrator's name and password, and
 *                                   puts them in the context;
 *   com.apple.security.authhost     the privileged mechanisms:
 *                                   builtin:authenticate,privileged checks
 *                                   them in the local directory and gives
 *                                   authd the user's uid.
 *
 * Any other mechanism (another plugin's, which Minkar does not have) allows
 * without doing anything, so a rule that lists one still works.
 */

#import <AppKit/AppKit.h>
#import <OpenDirectory/OpenDirectory.h>
#include <grp.h>
#include <pwd.h>
#include <xpc/xpc.h>
#include <os/log.h>

/* The protocol's keys and values (authd_private.h, authitems.h,
   AuthorizationPlugin.h). */
#define kRequestKey		"_agent_request_key"
#define kRequestCreate		"_agent_request_create"
#define kRequestInvoke		"_agent_request_invoke"
#define kRequestDeactivate	"_agent_request_deactivate"
#define kRequestDestroy		"_agent_request_destroy"
#define kRequestInterrupt	"_agent_request_interrupt"
#define kReplyKey		"_agent_reply_key"
#define kReplyResult		"_agent_reply_result"
#define kReplyCreate		"_agent_reply_create"
#define kReplyDeactivate	"_agent_reply_deactivate"
#define kPluginName		"_agent_plugin"
#define kMechanismName		"_agent_mechanism"
#define kHints			"_agent_hints"
#define kContext		"_agent_context"
#define kImmutableHints		"_agent_immutable_hints"
#define kResultValue		"_agent_reply_result_value"
#define kItemName		"_item_name"
#define kItemFlags		"_item_flags"
#define kItemValue		"_item_value"
#define kItemType		"_item_type"
#define kItemSensitiveLength	"_item_sensitive_value_length"

enum { kItemString = 2, kItemUInt = 4 };
enum { kItemFlagVolatile = 1 << 1 };
enum { kResultAllow = 0, kResultDeny = 1, kResultUndefined = 2, kResultUserCanceled = 3 };

static os_log_t agentLog;

/* A mechanism a connection created: its plugin and name, and whether its
   service is the privileged one. */
@interface AgentMechanism : NSObject
@property (nonatomic, copy) NSString *plugin;
@property (nonatomic, copy) NSString *name;
@property (nonatomic) BOOL privileged;
@property (nonatomic, strong) NSPanel *panel;
@end
@implementation AgentMechanism
@end

static NSString *text(NSString *english)
{
    return NSLocalizedString(english, nil);
}

#pragma mark - Items

/* An item array (hints, context) as name -> NSData of its value. */
static NSDictionary *itemsFromXPC(xpc_object_t array)
{
    NSMutableDictionary *items = [NSMutableDictionary dictionary];

    if (array == NULL || xpc_get_type(array) != XPC_TYPE_ARRAY)
        return items;
    xpc_array_apply(array, ^bool(size_t index __unused, xpc_object_t item) {
        const char *name = xpc_get_type(item) == XPC_TYPE_DICTIONARY ? xpc_dictionary_get_string(item, kItemName) : NULL;
        size_t length = 0;
        const void *bytes = name != NULL ? xpc_dictionary_get_data(item, kItemValue, &length) : NULL;

        if (name == NULL)
            return true;
        if (xpc_dictionary_get_value(item, kItemSensitiveLength) != NULL) {
            size_t sensitive = (size_t)xpc_dictionary_get_uint64(item, kItemSensitiveLength);
            if (sensitive <= length)
                length = sensitive;
        }
        items[@(name)] = bytes != NULL ? [NSData dataWithBytes:bytes length:length] : [NSData data];
        return true;
    });
    return items;
}

static NSString *itemString(NSDictionary *items, const char *name)
{
    NSData *data = items[@(name)];
    if (data == nil)
        return nil;
    NSString *s = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    /* A C string's value may carry its NUL. */
    return [s stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithRange:NSMakeRange(0, 1)]];
}

static uint32_t itemUInt(NSDictionary *items, const char *name)
{
    NSData *data = items[@(name)];
    uint32_t value = 0;
    if ([data length] >= sizeof(value))
        memcpy(&value, [data bytes], sizeof(value));
    return value;
}

static xpc_object_t stringItem(const char *name, NSString *value)
{
    xpc_object_t item = xpc_dictionary_create(NULL, NULL, 0);
    const char *utf8 = [value UTF8String];
    xpc_dictionary_set_string(item, kItemName, name);
    xpc_dictionary_set_data(item, kItemValue, utf8, strlen(utf8));
    xpc_dictionary_set_uint64(item, kItemFlags, 0);
    xpc_dictionary_set_uint64(item, kItemType, kItemString);
    return item;
}

/* A number in the context, volatile: authd keeps only extractable and
   volatile items when the mechanisms are done (a uid is not given out). */
static xpc_object_t uintItem(const char *name, uint32_t value)
{
    xpc_object_t item = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(item, kItemName, name);
    xpc_dictionary_set_data(item, kItemValue, &value, sizeof(value));
    xpc_dictionary_set_uint64(item, kItemFlags, kItemFlagVolatile);
    xpc_dictionary_set_uint64(item, kItemType, kItemUInt);
    return item;
}

static void reply(xpc_connection_t peer, xpc_object_t request, uint64_t result, xpc_object_t context)
{
    xpc_object_t answer = xpc_dictionary_create_reply(request);
    if (answer == NULL)
        return;
    xpc_dictionary_set_string(answer, kReplyKey, kReplyResult);
    xpc_dictionary_set_uint64(answer, kResultValue, result);
    xpc_dictionary_set_value(answer, kContext, context != NULL ? context : xpc_array_create(NULL, 0));
    xpc_dictionary_set_value(answer, kHints, xpc_array_create(NULL, 0));
    xpc_connection_send_message(peer, answer);
}

#pragma mark - Checking a password (privileged)

/* The name and password in the local directory, and the user in the group
   the rule asks for (the reference's rules ask for admin). */
static BOOL verify(NSString *name, NSString *password, NSString *group, uid_t *uid)
{
    NSError *error = nil;
    ODNode *node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:&error];
    ODRecord *record = [name length] ? [node recordWithRecordType:kODRecordTypeUsers name:name attributes:nil error:&error] : nil;
    struct passwd *pw;

    if (record == nil || ![record verifyPassword:password ?: @"" error:&error])
        return NO;
    if ((pw = getpwnam([[record recordName] UTF8String])) == NULL)
        return NO;
    *uid = pw->pw_uid;
    if ([group length] > 0 && pw->pw_uid != 0) {
        int groups[64], count = 64;
        struct group *wanted = getgrnam([group UTF8String]);
        BOOL member = NO;
        if (wanted == NULL)
            return NO;
        getgrouplist(pw->pw_name, (int)pw->pw_gid, groups, &count);
        for (int i = 0; i < count; i++)
            if ((gid_t)groups[i] == wanted->gr_gid)
                member = YES;
        if (!member)
            return NO;
    }
    return YES;
}

#pragma mark - The window (the user's side)

@interface AgentPrompt : NSObject <NSWindowDelegate>
@property (nonatomic, strong) NSPanel *panel;
@property (nonatomic, strong) NSTextField *nameField, *passwordField, *message;
@property (nonatomic) NSModalResponse response;
@end

@implementation AgentPrompt

- (void)ok:(id)sender { [NSApp stopModalWithCode:NSModalResponseOK]; }
- (void)cancel:(id)sender { [NSApp stopModalWithCode:NSModalResponseCancel]; }

static NSTextField *label(NSString *string, NSRect frame, NSFont *font)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:string];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setFont:font];
    [[field cell] setWraps:YES];
    return field;
}

/* What the user is asked: who wants to make changes (the client's name),
   the right's prompt or the standard sentence, the name and password. */
- (instancetype)initWithClient:(NSString *)client prompt:(NSString *)prompt user:(NSString *)user
                         admin:(BOOL)admin retry:(BOOL)retry
{
    if ((self = [super init]) == nil)
        return nil;
    NSRect frame = NSMakeRect(0, 0, 440, 216);
    _panel = [[NSPanel alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled
                                          backing:NSBackingStoreBuffered defer:NO];
    [_panel setLevel:NSModalPanelWindowLevel];
    [_panel setTitle:@""];
    [_panel setDelegate:self];
    NSView *content = [_panel contentView];

    NSImageView *lock = [[NSImageView alloc] initWithFrame:NSMakeRect(20, 136, 56, 56)];
    NSImage *icon = [NSImage imageNamed:NSImageNameLockLockedTemplate] ?: [NSImage imageNamed:NSImageNameCaution];
    [lock setImage:icon];
    [lock setImageScaling:NSImageScaleProportionallyUpOrDown];
    [content addSubview:lock];

    NSString *title = [NSString stringWithFormat:text(@"%@ wants to make changes."), client];
    [content addSubview:label(title, NSMakeRect(92, 170, 330, 20), [NSFont boldSystemFontOfSize:13])];
    NSString *detail = [prompt length] ? prompt :
        admin ? text(@"Enter an administrator’s name and password to allow this.")
              : text(@"Enter your password to allow this.");
    _message = label(retry ? [NSString stringWithFormat:@"%@\n%@", text(@"The name or password is incorrect."), detail] : detail,
                     NSMakeRect(92, 128, 330, 36), [NSFont systemFontOfSize:11]);
    if (retry)
        [_message setTextColor:[NSColor systemRedColor]];
    [content addSubview:_message];

    NSTextField *nameLabel = label(text(@"User Name:"), NSMakeRect(60, 94, 120, 18), [NSFont systemFontOfSize:12]);
    [nameLabel setAlignment:NSTextAlignmentRight];
    [content addSubview:nameLabel];
    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(184, 92, 236, 22)];
    [_nameField setStringValue:user ?: @""];
    [content addSubview:_nameField];
    NSTextField *passwordLabel = label(text(@"Password:"), NSMakeRect(60, 64, 120, 18), [NSFont systemFontOfSize:12]);
    [passwordLabel setAlignment:NSTextAlignmentRight];
    [content addSubview:passwordLabel];
    _passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(184, 62, 236, 22)];
    [content addSubview:_passwordField];

    NSButton *cancel = [NSButton buttonWithTitle:text(@"Cancel") target:self action:@selector(cancel:)];
    [cancel setFrame:NSMakeRect(232, 16, 92, 28)];
    [cancel setKeyEquivalent:@"\033"];
    [content addSubview:cancel];
    NSButton *ok = [NSButton buttonWithTitle:text(@"OK") target:self action:@selector(ok:)];
    [ok setFrame:NSMakeRect(328, 16, 92, 28)];
    [ok setKeyEquivalent:@"\r"];
    [content addSubview:ok];

    [_nameField setNextKeyView:_passwordField];
    [_passwordField setNextKeyView:_nameField];
    [_panel setInitialFirstResponder:[user length] ? _passwordField : _nameField];
    [_panel center];
    return self;
}

- (NSModalResponse)run
{
    [NSApp activateIgnoringOtherApps:YES];
    [_panel makeKeyAndOrderFront:nil];
    [_panel makeFirstResponder:[[_nameField stringValue] length] ? _passwordField : _nameField];
    _response = [NSApp runModalForWindow:_panel];
    [_panel orderOut:nil];
    return _response;
}

@end

/* The client's name as the user knows it: its app's, else its file's. */
static NSString *clientName(NSDictionary *hints)
{
    NSString *path = itemString(hints, "client-path");
    if ([path length] == 0)
        return text(@"An application");
    NSString *bundlePath = path;
    while ([bundlePath length] > 1 && ![[bundlePath pathExtension] isEqualToString:@"app"])
        bundlePath = [bundlePath stringByDeletingLastPathComponent];
    if ([[bundlePath pathExtension] isEqualToString:@"app"]) {
        NSBundle *bundle = [NSBundle bundleWithPath:bundlePath];
        NSString *name = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"] ?:
            [bundle objectForInfoDictionaryKey:@"CFBundleName"];
        if ([name length])
            return name;
        return [[bundlePath lastPathComponent] stringByDeletingPathExtension];
    }
    return [path lastPathComponent];
}

/* builtin:authenticate: the window, on the main thread; the name and
   password go to authd in the context. */
static void authenticate(xpc_connection_t peer, xpc_object_t request, NSDictionary *hints, AgentMechanism *mechanism)
{
    /* The asking user's name first, as on the reference; authd's
       suggestion (the console's owner) only for root's clients. */
    NSString *user = nil;
    uint32_t uid = itemUInt(hints, "client-uid");
    struct passwd *pw = uid != 0 ? getpwuid(uid) : NULL;
    if (pw != NULL)
        user = @(pw->pw_name);
    if ([user length] == 0)
        user = itemString(hints, "suggested-user");
    NSString *group = itemString(hints, "require-user-in-group");
    BOOL retry = itemUInt(hints, "reason") != 0;
    NSString *prompt = itemString(hints, "prompt");
    /* Asked again: the name the user typed stays. */
    static NSString *lastUser;
    if (retry && [lastUser length])
        user = lastUser;

    /* The request is answered later, from the main thread: kept till then. */
    xpc_retain(peer);
    xpc_retain(request);
    dispatch_async(dispatch_get_main_queue(), ^{
        AgentPrompt *window = [[AgentPrompt alloc] initWithClient:clientName(hints) prompt:prompt user:user
                                                  admin:[group isEqualToString:@"admin"] retry:retry];
        mechanism.panel = window.panel;
        NSModalResponse response = [window run];
        mechanism.panel = nil;
        if (response != NSModalResponseOK) {
            os_log_info(agentLog, "authentication cancelled");
            reply(peer, request, kResultUserCanceled, NULL);
        } else {
            xpc_object_t context = xpc_array_create(NULL, 0);
            lastUser = [window.nameField stringValue];
            xpc_array_append_value(context, stringItem("username", [window.nameField stringValue]));
            xpc_array_append_value(context, stringItem("password", [window.passwordField stringValue]));
            reply(peer, request, kResultAllow, context);
        }
        xpc_release(request);
        xpc_release(peer);
    });
}

/* builtin:authenticate,privileged: the name and password the window put in
   the context, checked; the user's uid back to authd. */
static void authenticatePrivileged(xpc_connection_t peer, xpc_object_t request, NSDictionary *hints, NSDictionary *context)
{
    NSString *name = itemString(context, "username");
    NSString *password = itemString(context, "password");
    NSString *group = itemString(hints, "require-user-in-group");
    uid_t uid = 0;

    if (!verify(name, password, group, &uid)) {
        os_log(agentLog, "authentication failed for %{public}s", [name UTF8String]);
        reply(peer, request, kResultDeny, NULL);
        return;
    }
    os_log(agentLog, "authenticated %{public}s", [name UTF8String]);
    xpc_object_t out = xpc_array_create(NULL, 0);
    xpc_array_append_value(out, uintItem("uid", uid));
    /* The administrator's name, extractable: AuthorizationCopyInfo gives it
       (the Users & Groups pane, directoryd). */
    xpc_object_t user = stringItem("username", name);
    xpc_dictionary_set_uint64(user, kItemFlags, 1);
    xpc_array_append_value(out, user);
    xpc_release(user);
    reply(peer, request, kResultAllow, out);
}

#pragma mark - The services

static void handle(xpc_connection_t peer, xpc_object_t event, AgentMechanism *mechanism)
{
    const char *method = xpc_dictionary_get_string(event, kRequestKey);

    if (method == NULL)
        return;
    if (strcmp(method, kRequestCreate) == 0) {
        const char *plugin = xpc_dictionary_get_string(event, kPluginName);
        const char *name = xpc_dictionary_get_string(event, kMechanismName);
        mechanism.plugin = plugin ? @(plugin) : @"";
        mechanism.name = name ? @(name) : @"";
        os_log_info(agentLog, "mechanism %{public}s:%{public}s%{public}s", [mechanism.plugin UTF8String],
                    [mechanism.name UTF8String], mechanism.privileged ? ",privileged" : "");
        xpc_object_t answer = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_string(answer, kReplyKey, kReplyCreate);
        xpc_dictionary_set_uint64(answer, kResultValue, kResultAllow);
        xpc_connection_send_message(peer, answer);
    } else if (strcmp(method, kRequestInvoke) == 0) {
        NSDictionary *hints = itemsFromXPC(xpc_dictionary_get_value(event, kHints));
        NSMutableDictionary *allHints = [hints mutableCopy];
        [allHints addEntriesFromDictionary:itemsFromXPC(xpc_dictionary_get_value(event, kImmutableHints))];
        NSDictionary *context = itemsFromXPC(xpc_dictionary_get_value(event, kContext));
        BOOL builtin = [mechanism.plugin isEqualToString:@"builtin"];

        if (builtin && [mechanism.name isEqualToString:@"authenticate"] && !mechanism.privileged)
            authenticate(peer, event, allHints, mechanism);
        else if (builtin && [mechanism.name isEqualToString:@"authenticate"] && mechanism.privileged)
            authenticatePrivileged(peer, event, allHints, context);
        else
            /* Another plugin's mechanism, or one with nothing to do here
               (reset-password): allowed. */
            reply(peer, event, kResultAllow, NULL);
    } else if (strcmp(method, kRequestDeactivate) == 0) {
        xpc_object_t answer = xpc_dictionary_create_reply(event);
        if (answer != NULL) {
            xpc_dictionary_set_string(answer, kReplyKey, kReplyDeactivate);
            xpc_connection_send_message(peer, answer);
        }
    } else if (strcmp(method, kRequestInterrupt) == 0 || strcmp(method, kRequestDestroy) == 0) {
        /* The evaluation goes no further: a window still open is closed. */
        dispatch_async(dispatch_get_main_queue(), ^{
            if (mechanism.panel != nil)
                [NSApp stopModalWithCode:NSModalResponseCancel];
        });
        xpc_object_t answer = xpc_dictionary_create_reply(event);
        if (answer != NULL) {
            xpc_dictionary_set_string(answer, kReplyKey, kReplyResult);
            xpc_connection_send_message(peer, answer);
        }
    }
}

static void serve(const char *service, BOOL privileged)
{
    xpc_connection_t listener = xpc_connection_create_mach_service(service, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (listener == NULL) {
        /* Not started by launchd with the service. */
        os_log_error(agentLog, "cannot listen on %{public}s", service);
        return;
    }
    xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
        if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
            return;
        AgentMechanism *mechanism = [AgentMechanism new];
        mechanism.privileged = privileged;
        xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
            if (xpc_get_type(event) == XPC_TYPE_DICTIONARY)
                handle(peer, event, mechanism);
        });
        xpc_connection_resume(peer);
    });
    xpc_connection_resume(listener);
}

int main(void)
{
    @autoreleasepool {
        agentLog = os_log_create("org.minkar.AuthorizationAgent", "agent");
        /* No Dock tile, no menu bar of its own: LSUIElement (Info.plist). */
        [NSApplication sharedApplication];
        serve("com.apple.security.agent", NO);
        serve("com.apple.security.agent.login", NO);
        serve("com.apple.security.authhost", YES);
        os_log_info(agentLog, "listening");
        [NSApp run];
    }
    return 0;
}
