/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SecurityInterface (the maintainer's choice, 2026-11-23: authorization in
 * full, the panes' lock): SFAuthorization, an authorization as an object,
 * and SFAuthorizationView, the lock a window's settings sit behind. Closed,
 * a click obtains the view's rights through authd, asking the user (the
 * Authorization Agent's window); open, a click gives them up. Its delegate
 * hears both, and the view tells when the rights lapse (updateStatus:, or
 * every interval with setAutoupdate:interval:).
 */

#import <SecurityInterface/SFAuthorizationView.h>
#import <Security/AuthorizationTags.h>

@implementation SFAuthorization {
    AuthorizationRef _ref;
}

+ (instancetype)authorization
{
    return [[self alloc] initWithFlags:kAuthorizationFlagDefaults rights:NULL environment:NULL];
}

+ (instancetype)authorizationWithFlags:(AuthorizationFlags)flags rights:(const AuthorizationRights *)rights
                           environment:(const AuthorizationEnvironment *)environment
{
    return [[self alloc] initWithFlags:flags rights:rights environment:environment];
}

- (instancetype)init
{
    return [self initWithFlags:kAuthorizationFlagDefaults rights:NULL environment:NULL];
}

- (instancetype)initWithFlags:(AuthorizationFlags)flags rights:(const AuthorizationRights *)rights
                  environment:(const AuthorizationEnvironment *)environment
{
    if ((self = [super init]) == nil)
        return nil;
    if (AuthorizationCreate(rights, environment, flags, &_ref) != errAuthorizationSuccess)
        return nil;
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return [self init];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (void)dealloc
{
    if (_ref != NULL)
        AuthorizationFree(_ref, kAuthorizationFlagDefaults);
}

- (AuthorizationRef)authorizationRef
{
    return _ref;
}

static NSError *authorizationError(OSStatus status)
{
    return [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
}

- (BOOL)obtainWithRight:(AuthorizationString)rightName flags:(AuthorizationFlags)flags error:(NSError **)error
{
    AuthorizationItem item = { rightName, 0, NULL, 0 };
    AuthorizationRights rights = { 1, &item };
    return [self obtainWithRights:&rights flags:flags environment:NULL authorizedRights:NULL error:error];
}

- (BOOL)obtainWithRights:(const AuthorizationRights *)rights flags:(AuthorizationFlags)flags
             environment:(const AuthorizationEnvironment *)environment
        authorizedRights:(AuthorizationRights **)authorizedRights error:(NSError **)error
{
    OSStatus status = AuthorizationCopyRights(_ref, rights, environment, flags, authorizedRights);
    if (status != errAuthorizationSuccess && error != NULL)
        *error = authorizationError(status);
    return status == errAuthorizationSuccess;
}

/* The rights obtained are given up: the next use asks again. */
- (void)invalidateCredentials
{
    if (_ref != NULL)
        AuthorizationFree(_ref, kAuthorizationFlagDestroyRights);
    _ref = NULL;
    AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &_ref);
}

@end

@implementation SFAuthorizationView {
    NSButton *_lock;
    NSTextField *_label;
    SFAuthorization *_authorization;
    AuthorizationItem *_items;
    AuthorizationRights _rights;
    SFAuthorizationViewState _state;
    AuthorizationFlags _flags;
    __weak id _delegate;
    BOOL _enabled;
    NSTimer *_timer;
}

static NSString *text(NSString *english)
{
    return [[NSBundle bundleForClass:[SFAuthorizationView class]] localizedStringForKey:english value:english table:nil];
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _enabled = YES;
    _flags = kAuthorizationFlagDefaults;
    _lock = [[NSButton alloc] initWithFrame:NSMakeRect(0, floor((NSHeight(frame) - 32) / 2), 32, 32)];
    [_lock setBordered:NO];
    [_lock setImagePosition:NSImageOnly];
    [[_lock cell] setImageScaling:NSImageScaleProportionallyUpOrDown];
    [_lock setTarget:self];
    [_lock setAction:@selector(clicked:)];
    [_lock setAutoresizingMask:NSViewMaxXMargin];
    [self addSubview:_lock];
    _label = [[NSTextField alloc] initWithFrame:NSMakeRect(38, floor((NSHeight(frame) - 18) / 2), NSWidth(frame) - 38, 18)];
    [_label setEditable:NO];
    [_label setSelectable:NO];
    [_label setBordered:NO];
    [_label setDrawsBackground:NO];
    [_label setFont:[NSFont systemFontOfSize:12]];
    [_label setAutoresizingMask:NSViewWidthSizable];
    [self addSubview:_label];
    _authorization = [SFAuthorization authorization];
    [self setState:SFAuthorizationViewLockedState];
    return self;
}

- (void)dealloc
{
    free(_items);
    [_timer invalidate];
}

- (void)setState:(SFAuthorizationViewState)state
{
    _state = state;
    BOOL open = state == SFAuthorizationViewUnlockedState;
    [_lock setImage:[NSImage imageNamed:open ? NSImageNameLockUnlockedTemplate : NSImageNameLockLockedTemplate]];
    [_label setStringValue:open ? text(@"Click the lock to prevent further changes.")
                                : text(@"Click the lock to make changes.")];
}

/* The right to obtain, by name. */
- (void)setString:(AuthorizationString)authorizationString
{
    AuthorizationItem item = { authorizationString, 0, NULL, 0 };
    AuthorizationRights rights = { 1, &item };
    [self setAuthorizationRights:&rights];
}

- (void)setAuthorizationRights:(const AuthorizationRights *)authorizationRights
{
    free(_items);
    _items = NULL;
    _rights.count = 0;
    _rights.items = NULL;
    if (authorizationRights == NULL || authorizationRights->count == 0)
        return;
    _items = calloc(authorizationRights->count, sizeof(AuthorizationItem));
    for (UInt32 i = 0; i < authorizationRights->count; i++) {
        _items[i] = authorizationRights->items[i];
        _items[i].name = strdup(authorizationRights->items[i].name);
    }
    _rights.count = authorizationRights->count;
    _rights.items = _items;
    [self updateStatus:nil];
}

- (AuthorizationRights *)authorizationRights
{
    return _rights.count ? &_rights : NULL;
}

- (SFAuthorization *)authorization
{
    return _authorization;
}

/* Whether the rights are held still (without asking): the lock shows it. */
- (BOOL)updateStatus:(id)inSender
{
    if (_rights.count == 0 || _state == SFAuthorizationViewInProgressState)
        return _state == SFAuthorizationViewUnlockedState;
    BOOL held = [_authorization obtainWithRights:&_rights flags:kAuthorizationFlagExtendRights environment:NULL
                                authorizedRights:NULL error:NULL];
    SFAuthorizationViewState before = _state;
    [self setState:held ? SFAuthorizationViewUnlockedState : SFAuthorizationViewLockedState];
    if (before == SFAuthorizationViewUnlockedState && !held && [_delegate respondsToSelector:@selector(authorizationViewDidDeauthorize:)])
        [_delegate authorizationViewDidDeauthorize:self];
    return held;
}

- (void)setAutoupdate:(BOOL)autoupdate
{
    [self setAutoupdate:autoupdate interval:15];
}

- (void)setAutoupdate:(BOOL)autoupdate interval:(NSTimeInterval)interval
{
    [_timer invalidate];
    _timer = autoupdate ? [NSTimer scheduledTimerWithTimeInterval:interval target:self selector:@selector(updateStatus:)
                                                         userInfo:nil repeats:YES] : nil;
}

- (void)setDelegate:(id)delegate
{
    _delegate = delegate;
}

- (id)delegate
{
    return _delegate;
}

- (SFAuthorizationViewState)authorizationState
{
    return _state;
}

- (void)setEnabled:(BOOL)enabled
{
    _enabled = enabled;
    [_lock setEnabled:enabled];
    [_label setTextColor:enabled ? [NSColor labelColor] : [NSColor disabledControlTextColor]];
}

- (BOOL)isEnabled
{
    return _enabled;
}

- (void)setFlags:(AuthorizationFlags)flags
{
    _flags = flags;
}

- (void)clicked:(id)sender
{
    if (_state == SFAuthorizationViewUnlockedState)
        [self deauthorize:sender];
    else
        [self authorize:sender];
}

/* The rights, asking the user. */
- (BOOL)authorize:(id)inSender
{
    if (_rights.count == 0)
        return NO;
    [self setState:SFAuthorizationViewInProgressState];
    BOOL obtained = [_authorization obtainWithRights:&_rights
                                               flags:_flags | kAuthorizationFlagInteractionAllowed |
                                                     kAuthorizationFlagExtendRights
                                         environment:NULL authorizedRights:NULL error:NULL];
    [self setState:obtained ? SFAuthorizationViewUnlockedState : SFAuthorizationViewLockedState];
    if (obtained && [_delegate respondsToSelector:@selector(authorizationViewDidAuthorize:)])
        [_delegate authorizationViewDidAuthorize:self];
    return obtained;
}

- (BOOL)deauthorize:(id)inSender
{
    if ([_delegate respondsToSelector:@selector(authorizationViewShouldDeauthorize:)] &&
        ![_delegate authorizationViewShouldDeauthorize:self])
        return NO;
    [_authorization invalidateCredentials];
    [self setState:SFAuthorizationViewLockedState];
    if ([_delegate respondsToSelector:@selector(authorizationViewDidDeauthorize:)])
        [_delegate authorizationViewDidDeauthorize:self];
    return YES;
}

@end
