// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.
//
// WKWebView on WPE WebKit: a WebKitWebView on Minkar's WPE platform, its
// frames drawn in this view, its signals turned into the delegates' calls.
// GLib's main context runs on the main run loop (WebGLibRunLoop.m).

#import <WebKit/WKWebView.h>
#import <objc/message.h>

#include <wpe/webkit.h>
#include "WebGLibRunLoop.h"
#include "WebWPEPlatform.h"
#import "WebWPEEvents.h"

NSString *const WKErrorDomain = @"WKErrorDomain";

#define SECURE \
   + (BOOL)supportsSecureCoding { return YES; } \
   - (void)encodeWithCoder:(NSCoder *)coder {} \
   - (instancetype)initWithCoder:(NSCoder *)coder { return [self init]; }

static NSString *stringOrNil(const char *s)
{
    return s ? [NSString stringWithUTF8String:s] : nil;
}

static NSURL *URLOrNil(const char *s)
{
    return s && *s ? [NSURL URLWithString:[NSString stringWithUTF8String:s]] : nil;
}

#pragma mark - JavaScript values

/* A JavaScript value as Foundation's: strings, numbers, booleans, dates,
   null (NSNull), arrays and plain objects (through JSON). NO when it has
   no Foundation counterpart (undefined, functions). */
static BOOL foundationValue(JSCValue *value, id *result)
{
    if (jsc_value_is_undefined(value) || jsc_value_is_function(value))
        return NO;
    if (jsc_value_is_null(value)) {
        *result = [NSNull null];
        return YES;
    }
    if (jsc_value_is_boolean(value)) {
        *result = @(jsc_value_to_boolean(value) ? YES : NO);
        return YES;
    }
    if (jsc_value_is_number(value)) {
        *result = @(jsc_value_to_double(value));
        return YES;
    }
    if (jsc_value_is_string(value)) {
        char *s = jsc_value_to_string(value);
        *result = [NSString stringWithUTF8String:s ?: ""];
        g_free(s);
        return YES;
    }
    JSCContext *context = jsc_value_get_context(value);
    JSCValue *dateClass = jsc_context_get_value(context, "Date");
    gboolean isDate = jsc_value_is_constructor(dateClass) &&
        jsc_value_is_object(value) && jsc_value_object_is_instance_of(value, "Date");
    g_object_unref(dateClass);
    if (isDate) {
        JSCValue *time = jsc_value_object_invoke_method(value, "getTime", G_TYPE_NONE);
        *result = [NSDate dateWithTimeIntervalSince1970:jsc_value_to_double(time) / 1000.0];
        g_object_unref(time);
        return YES;
    }
    char *json = jsc_value_to_json(value, 0);
    if (!json)
        return NO;
    NSData *data = [NSData dataWithBytes:json length:strlen(json)];
    g_free(json);
    id object = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nil];
    if (object == nil)
        return NO;
    *result = object;
    return YES;
}

static NSError *javaScriptError(GError *error)
{
    NSInteger code = (error->domain == WEBKIT_JAVASCRIPT_ERROR &&
                      error->code == WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ?
        WKErrorJavaScriptExceptionOccurred : WKErrorUnknown;
    NSString *message = stringOrNil(error->message) ?: @"A JavaScript exception occurred";
    return [NSError errorWithDomain:WKErrorDomain code:code
                           userInfo:@{ NSLocalizedDescriptionKey: @"A JavaScript exception occurred",
                                       @"WKJavaScriptExceptionMessage": message }];
}

/* A load's error as the reference platform's: NSURLErrorDomain's codes
   where they say the same, WebKitErrorDomain's for policy errors. */
static NSError *loadError(GError *error, const char *uri)
{
    NSString *domain = NSURLErrorDomain;
    NSInteger code = NSURLErrorUnknown;
    if (error->domain == WEBKIT_NETWORK_ERROR) {
        switch (error->code) {
        case WEBKIT_NETWORK_ERROR_CANCELLED: code = NSURLErrorCancelled; break;
        case WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST: code = NSURLErrorFileDoesNotExist; break;
        case WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL: code = NSURLErrorUnsupportedURL; break;
        case WEBKIT_NETWORK_ERROR_TRANSPORT: code = NSURLErrorCannotConnectToHost; break;
        default: code = NSURLErrorUnknown; break;
        }
    } else if (error->domain == WEBKIT_POLICY_ERROR) {
        domain = @"WebKitErrorDomain";
        code = 100 + error->code;        /* 102: frame load interrupted */
    }
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    info[NSLocalizedDescriptionKey] = stringOrNil(error->message) ?: @"The page did not load.";
    NSURL *url = URLOrNil(uri);
    if (url != nil) {
        info[NSURLErrorFailingURLErrorKey] = url;
        info[NSURLErrorFailingURLStringErrorKey] = url.absoluteString;
    }
    return [NSError errorWithDomain:domain code:code userInfo:info];
}

#pragma mark - Configuration

@implementation WKPreferences
SECURE
- (instancetype)init {
    if ((self = [super init]))
        _javaScriptEnabled = YES;
    return self;
}
@end

@implementation WKUserScript
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly {
    if ((self = [super init])) {
        _source = [source copy];
        _injectionTime = injectionTime;
        _forMainFrameOnly = forMainFrameOnly;
    }
    return self;
}
- (id)copyWithZone:(NSZone *)zone { return self; }
@end

@interface WKScriptMessage ()
@property (readwrite, copy) id body;
@property (readwrite, weak) WKWebView *webView;
@property (readwrite, copy) NSString *name;
@end
@implementation WKScriptMessage
@end

@interface WKWebView ()
- (WebKitWebView *)_webKitView;
@end

/* The scripts and handlers of every web view using it: a WebKit user
   content manager, made with the controller. */
@implementation WKUserContentController {
    NSMutableArray<WKUserScript *> *_scripts;
    NSMutableDictionary<NSString *, id<WKScriptMessageHandler>> *_handlers;
    WebKitUserContentManager *_manager;
    NSHashTable<WKWebView *> *_webViews;
}
SECURE
- (instancetype)init {
    if ((self = [super init])) {
        _scripts = [NSMutableArray array];
        _handlers = [NSMutableDictionary dictionary];
        _webViews = [NSHashTable weakObjectsHashTable];
    }
    return self;
}
- (void)dealloc {
    if (_manager) {
        g_signal_handlers_disconnect_by_data(_manager, (__bridge void *)self);
        g_object_unref(_manager);
    }
}
- (WebKitUserContentManager *)_manager {
    if (_manager == NULL) {
        _WebGLibRunLoopStart();
        _manager = webkit_user_content_manager_new();
        for (WKUserScript *script in _scripts)
            [self _addToManager:script];
        for (NSString *name in _handlers)
            [self _registerHandlerNamed:name];
    }
    return _manager;
}
- (void)_addWebView:(WKWebView *)webView { [_webViews addObject:webView]; }
- (void)_addToManager:(WKUserScript *)script {
    WebKitUserScript *userScript = webkit_user_script_new(script.source.UTF8String,
        script.forMainFrameOnly ? WEBKIT_USER_CONTENT_INJECT_TOP_FRAME : WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        script.injectionTime == WKUserScriptInjectionTimeAtDocumentStart ?
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START : WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
        NULL, NULL);
    webkit_user_content_manager_add_script(_manager, userScript);
    webkit_user_script_unref(userScript);
}

static void scriptMessageReceived(WebKitUserContentManager *manager, JSCValue *value, gpointer data)
{
    (void)manager;
    WKUserContentController *controller = (__bridge WKUserContentController *)data;
    [controller _received:value];
}

- (void)_registerHandlerNamed:(NSString *)name {
    webkit_user_content_manager_register_script_message_handler(_manager, name.UTF8String, NULL);
    NSString *signal = [@"script-message-received::" stringByAppendingString:name];
    g_signal_connect(_manager, signal.UTF8String, G_CALLBACK(scriptMessageReceived), (__bridge void *)self);
}

/* A page posted a message: its handler (by the detail of the signal that
   brought it, found again through the value's handler name). */
- (void)_received:(JSCValue *)value {
    /* The signal's detail is not given to the handler: each name has its
       own connection, and the one that runs is the current emission's. */
    GSignalInvocationHint *hint = g_signal_get_invocation_hint(_manager);
    NSString *name = hint ? stringOrNil(g_quark_to_string(hint->detail)) : nil;
    id<WKScriptMessageHandler> handler = name ? _handlers[name] : nil;
    if (handler == nil)
        return;
    id body = nil;
    if (!foundationValue(value, &body))
        body = [NSNull null];
    WKScriptMessage *message = [WKScriptMessage new];
    message.body = body;
    message.name = name;
    message.webView = _webViews.anyObject;
    [handler userContentController:self didReceiveScriptMessage:message];
}

- (NSArray *)userScripts { return [_scripts copy]; }
- (void)addUserScript:(WKUserScript *)userScript {
    [_scripts addObject:userScript];
    if (_manager)
        [self _addToManager:userScript];
}
- (void)removeAllUserScripts {
    [_scripts removeAllObjects];
    if (_manager)
        webkit_user_content_manager_remove_all_scripts(_manager);
}
- (void)addScriptMessageHandler:(id<WKScriptMessageHandler>)handler name:(NSString *)name {
    if (_handlers[name] != nil)
        [NSException raise:NSInvalidArgumentException format:@"a script message handler named %@ already exists", name];
    _handlers[name] = handler;
    if (_manager)
        [self _registerHandlerNamed:name];
}
- (void)removeScriptMessageHandlerForName:(NSString *)name {
    if (_handlers[name] == nil)
        return;
    [_handlers removeObjectForKey:name];
    if (_manager) {
        webkit_user_content_manager_unregister_script_message_handler(_manager, name.UTF8String, NULL);
    }
}
@end

@implementation WKProcessPool
SECURE
@end

/* Website data in ~/Library/WebKit/<the app's identifier>, its cache in
   ~/Library/Caches/<identifier>/WebKit, or kept in memory only. */
@implementation WKWebsiteDataStore {
    BOOL _persistent;
    WebKitNetworkSession *_session;
}
SECURE
+ (WKWebsiteDataStore *)defaultDataStore {
    static WKWebsiteDataStore *store;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ store = [self new]; store->_persistent = YES; });
    return store;
}
+ (WKWebsiteDataStore *)nonPersistentDataStore { return [self new]; }
- (BOOL)isPersistent { return _persistent; }
- (WebKitNetworkSession *)_session {
    if (_session == NULL) {
        _WebGLibRunLoopStart();
        if (_persistent) {
            NSString *identifier = NSBundle.mainBundle.bundleIdentifier ?: NSProcessInfo.processInfo.processName;
            NSString *library = [NSHomeDirectory() stringByAppendingPathComponent:@"Library"];
            NSString *data = [[library stringByAppendingPathComponent:@"WebKit"] stringByAppendingPathComponent:identifier];
            NSString *cache = [[[library stringByAppendingPathComponent:@"Caches"]
                stringByAppendingPathComponent:identifier] stringByAppendingPathComponent:@"WebKit"];
            _session = webkit_network_session_new(data.fileSystemRepresentation, cache.fileSystemRepresentation);
        } else {
            _session = webkit_network_session_new_ephemeral();
        }
    }
    return _session;
}
@end

@interface WKWebViewConfiguration ()
/* The web view whose page opens the one made with this configuration. */
@property (weak) WKWebView *_relatedWebView;
@end

@implementation WKWebViewConfiguration
SECURE
- (instancetype)init {
    if ((self = [super init])) {
        _processPool = [WKProcessPool new];
        _preferences = [WKPreferences new];
        _userContentController = [WKUserContentController new];
        _websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    }
    return self;
}
- (id)copyWithZone:(NSZone *)zone {
    WKWebViewConfiguration *copy = [WKWebViewConfiguration new];
    copy.processPool = self.processPool;
    copy.preferences = self.preferences;
    copy.userContentController = self.userContentController;
    copy.websiteDataStore = self.websiteDataStore;
    copy.applicationNameForUserAgent = self.applicationNameForUserAgent;
    copy.suppressesIncrementalRendering = self.suppressesIncrementalRendering;
    copy._relatedWebView = self._relatedWebView;
    return copy;
}
@end

@implementation WKNavigation
@end

#pragma mark - Navigation actions and history

@interface WKFrameInfo ()
@property (readwrite, getter=isMainFrame) BOOL mainFrame;
@property (readwrite, copy) NSURLRequest *request;
@end
@implementation WKFrameInfo
- (id)copyWithZone:(NSZone *)zone { return self; }
@end

@interface WKNavigationAction ()
@property (readwrite, copy) WKFrameInfo *sourceFrame;
@property (readwrite, copy) WKFrameInfo *targetFrame;
@property (readwrite) WKNavigationType navigationType;
@property (readwrite, copy) NSURLRequest *request;
@property (readwrite) NSEventModifierFlags modifierFlags;
@property (readwrite) NSInteger buttonNumber;
@end
@implementation WKNavigationAction
@end

@interface WKWindowFeatures ()
@property (readwrite) NSNumber *menuBarVisibility, *statusBarVisibility, *toolbarsVisibility, *allowsResizing;
@property (readwrite) NSNumber *x, *y, *width, *height;
@end
@implementation WKWindowFeatures
@end

static NSURLRequest *requestFrom(WebKitURIRequest *request)
{
    NSURL *url = URLOrNil(webkit_uri_request_get_uri(request)) ?: [NSURL URLWithString:@"about:blank"];
    NSMutableURLRequest *result = [NSMutableURLRequest requestWithURL:url];
    const char *method = webkit_uri_request_get_http_method(request);
    if (method)
        result.HTTPMethod = [NSString stringWithUTF8String:method];
    return result;
}

static WKNavigationType navigationTypeFrom(WebKitNavigationType type)
{
    switch (type) {
    case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: return WKNavigationTypeLinkActivated;
    case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: return WKNavigationTypeFormSubmitted;
    case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: return WKNavigationTypeBackForward;
    case WEBKIT_NAVIGATION_TYPE_RELOAD: return WKNavigationTypeReload;
    case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED: return WKNavigationTypeFormResubmitted;
    default: return WKNavigationTypeOther;
    }
}

static WKNavigationAction *actionFrom(WebKitNavigationAction *webAction, BOOL newWindow)
{
    WKNavigationAction *action = [WKNavigationAction new];
    action.request = requestFrom(webkit_navigation_action_get_request(webAction));
    action.navigationType = navigationTypeFrom(webkit_navigation_action_get_navigation_type(webAction));
    action.buttonNumber = webkit_navigation_action_get_mouse_button(webAction);
    WKFrameInfo *target = [WKFrameInfo new];
    target.mainFrame = YES;
    target.request = action.request;
    action.targetFrame = newWindow ? nil : target;
    return action;
}

@interface WKBackForwardListItem ()
@property (readwrite, copy) NSURL *URL;
@property (readwrite, copy) NSString *title;
@property (readwrite, copy) NSURL *initialURL;
@end
@implementation WKBackForwardListItem
@end

static WKBackForwardListItem *itemFrom(WebKitBackForwardListItem *item)
{
    if (item == NULL)
        return nil;
    WKBackForwardListItem *result = [WKBackForwardListItem new];
    result.URL = URLOrNil(webkit_back_forward_list_item_get_uri(item));
    const char *title = webkit_back_forward_list_item_get_title(item);
    result.title = title && *title ? [NSString stringWithUTF8String:title] : nil;
    result.initialURL = URLOrNil(webkit_back_forward_list_item_get_original_uri(item)) ?: result.URL;
    return result;
}

static NSArray *itemsFrom(GList *list)
{
    NSMutableArray *items = [NSMutableArray array];
    for (GList *l = list; l; l = l->next)
        [items addObject:itemFrom(l->data)];
    g_list_free(list);
    return items;
}

@interface WKBackForwardList ()
@property (weak) WKWebView *webView;
@end
@implementation WKBackForwardList
- (WebKitBackForwardList *)_list { return webkit_web_view_get_back_forward_list(self.webView._webKitView); }
- (WKBackForwardListItem *)currentItem { return itemFrom(webkit_back_forward_list_get_current_item(self._list)); }
- (WKBackForwardListItem *)backItem { return itemFrom(webkit_back_forward_list_get_back_item(self._list)); }
- (WKBackForwardListItem *)forwardItem { return itemFrom(webkit_back_forward_list_get_forward_item(self._list)); }
/* Oldest first, as the reference platform gives them. */
- (NSArray *)backList { return itemsFrom(g_list_reverse(webkit_back_forward_list_get_back_list(self._list))); }
- (NSArray *)forwardList { return itemsFrom(webkit_back_forward_list_get_forward_list(self._list)); }
@end

#pragma mark - The web view

@implementation WKWebView {
    WKWebViewConfiguration *_configuration;
    WebKitWebView *_webView;
    WPEView *_wpeView;
    WKBackForwardList *_backForwardList;
    WKNavigation *_navigation;
    NSString *_customUserAgent;
    CGFloat _magnification;
    /* The last frame, its pixels copied. */
    CGImageRef _frame;
    NSTrackingArea *_trackingArea;
    BOOL _openedByPage;
    /* The load failed: WebKit's "finished" after it is not passed on. */
    BOOL _loadFailed;
}

static void frameArrived(void *context, const guint8 *pixels, int width, int height, guint stride,
                         const WPERectangle *damage, guint damageCount)
{
    WKWebView *self = (__bridge WKWebView *)context;
    [self _frame:pixels width:width height:height stride:stride damage:damage count:damageCount];
}

static void loadChanged(WebKitWebView *webView, WebKitLoadEvent event, gpointer data)
{
    (void)webView;
    [(__bridge WKWebView *)data _loadChanged:event];
}

static gboolean loadFailed(WebKitWebView *webView, WebKitLoadEvent event, char *uri, GError *error, gpointer data)
{
    (void)webView;
    [(__bridge WKWebView *)data _loadFailed:event uri:uri error:error];
    return TRUE;
}

static gboolean decidePolicy(WebKitWebView *webView, WebKitPolicyDecision *decision,
                             WebKitPolicyDecisionType type, gpointer data)
{
    (void)webView;
    return [(__bridge WKWebView *)data _decidePolicy:decision type:type];
}

static void propertyChanged(GObject *object, GParamSpec *pspec, gpointer data)
{
    (void)object;
    [(__bridge WKWebView *)data _propertyChanged:pspec->name];
}

static WebKitWebView *createWebView(WebKitWebView *webView, WebKitNavigationAction *action, gpointer data)
{
    (void)webView;
    return [(__bridge WKWebView *)data _createWebView:action];
}

static void closeWebView(WebKitWebView *webView, gpointer data)
{
    (void)webView;
    WKWebView *self = (__bridge WKWebView *)data;
    id<WKUIDelegate> d = self.UIDelegate;
    if ([d respondsToSelector:@selector(webViewDidClose:)])
        [d webViewDidClose:self];
}

static gboolean scriptDialog(WebKitWebView *webView, WebKitScriptDialog *dialog, gpointer data)
{
    (void)webView;
    [(__bridge WKWebView *)data _scriptDialog:dialog];
    return TRUE;
}

static void webProcessTerminated(WebKitWebView *webView, WebKitWebProcessTerminationReason reason, gpointer data)
{
    (void)webView; (void)reason;
    [(__bridge WKWebView *)data _webProcessTerminated];
}

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration {
    if ((self = [super initWithFrame:frame])) {
        _WebGLibRunLoopStart();
        _configuration = [configuration copy] ?: [WKWebViewConfiguration new];
        _magnification = 1;

        WKPreferences *preferences = _configuration.preferences;
        WebKitSettings *settings = webkit_settings_new();
        webkit_settings_set_enable_javascript(settings, preferences.javaScriptEnabled);
        webkit_settings_set_javascript_can_open_windows_automatically(settings,
            preferences.javaScriptCanOpenWindowsAutomatically);
        webkit_settings_set_minimum_font_size(settings, (guint32)preferences.minimumFontSize);
        if (_configuration.applicationNameForUserAgent.length)
            webkit_settings_set_user_agent_with_application_details(settings,
                _configuration.applicationNameForUserAgent.UTF8String, NULL);

        WKUserContentController *controller = _configuration.userContentController;
        [controller _addWebView:self];
        WKWebView *related = _configuration._relatedWebView;
        _configuration._relatedWebView = nil;
        if (related != nil) {
            /* Opened by related's page: in its web process, session and
               display. */
            _webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "related-view", related->_webView,
                "settings", settings,
                "user-content-manager", [controller _manager],
                NULL));
            _openedByPage = YES;
        } else {
            _webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "display", _WebWPEDisplayGet(),
                "settings", settings,
                "user-content-manager", [controller _manager],
                "network-session", [_configuration.websiteDataStore _session],
                NULL));
        }
        g_object_unref(settings);
        g_object_ref_sink(_webView);

        void *me = (__bridge void *)self;
        g_signal_connect(_webView, "load-changed", G_CALLBACK(loadChanged), me);
        g_signal_connect(_webView, "load-failed", G_CALLBACK(loadFailed), me);
        g_signal_connect(_webView, "decide-policy", G_CALLBACK(decidePolicy), me);
        g_signal_connect(_webView, "web-process-terminated", G_CALLBACK(webProcessTerminated), me);
        g_signal_connect(_webView, "create", G_CALLBACK(createWebView), me);
        g_signal_connect(_webView, "close", G_CALLBACK(closeWebView), me);
        g_signal_connect(_webView, "script-dialog", G_CALLBACK(scriptDialog), me);
        const char *notifications[] = { "notify::title", "notify::uri", "notify::estimated-load-progress",
                                        "notify::is-loading" };
        for (size_t i = 0; i < sizeof(notifications) / sizeof(*notifications); i++)
            g_signal_connect(_webView, notifications[i], G_CALLBACK(propertyChanged), me);

        _wpeView = webkit_web_view_get_wpe_view(_webView);
        _WebWPEViewSetFrameFunction(_wpeView, frameArrived, me);
        _WebWPEViewResize(_wpeView, (int)NSWidth(frame), (int)NSHeight(frame));

        _backForwardList = [WKBackForwardList new];
        _backForwardList.webView = self;
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frame {
    return [self initWithFrame:frame configuration:[WKWebViewConfiguration new]];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:[WKWebViewConfiguration new]];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (_wpeView)
        _WebWPEViewSetFrameFunction(_wpeView, NULL, NULL);
    if (_webView) {
        g_signal_handlers_disconnect_by_data(_webView, (__bridge void *)self);
        g_object_unref(_webView);
    }
    if (_frame)
        CGImageRelease(_frame);
}

- (WebKitWebView *)_webKitView { return _webView; }
- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return NO; }

#pragma mark Drawing

- (void)_frame:(const guint8 *)pixels width:(int)width height:(int)height stride:(guint)stride
        damage:(const WPERectangle *)damage count:(guint)count {
    size_t rowBytes = (size_t)width * 4;
    NSMutableData *copy = [NSMutableData dataWithLength:rowBytes * (size_t)height];
    for (int y = 0; y < height; y++)
        memcpy((guint8 *)copy.mutableBytes + rowBytes * (size_t)y, pixels + (size_t)stride * (size_t)y, rowBytes);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)copy);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate((size_t)width, (size_t)height, 8, 32, rowBytes, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, provider, NULL, false,
        kCGRenderingIntentDefault);
    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);
    if (_frame)
        CGImageRelease(_frame);
    _frame = image;

    if (count == 0) {
        [self setNeedsDisplay:YES];
        return;
    }
    for (guint i = 0; i < count; i++)
        [self setNeedsDisplayInRect:NSMakeRect(damage[i].x, damage[i].y, damage[i].width, damage[i].height)];
}

- (void)drawRect:(NSRect)dirtyRect {
    if (_frame == NULL)
        return;
    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    CGFloat width = (CGFloat)CGImageGetWidth(_frame), height = (CGFloat)CGImageGetHeight(_frame);
    CGContextSaveGState(context);
    /* The view is flipped: the image is drawn upright. */
    CGContextTranslateCTM(context, 0, height);
    CGContextScaleCTM(context, 1, -1);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), _frame);
    CGContextRestoreGState(context);
}

/* The last frame drawn (for tests and snapshots). */
- (NSBitmapImageRep *)_lastFrame {
    return _frame ? [[NSBitmapImageRep alloc] initWithCGImage:_frame] : nil;
}

#pragma mark Size, window and focus

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    _WebWPEViewResize(_wpeView, (int)size.width, (int)size.height);
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [center removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    NSWindow *window = self.window;
    _WebWPEViewSetMapped(_wpeView, window != nil && !self.isHiddenOrHasHiddenAncestor);
    if (window != nil) {
        [center addObserver:self selector:@selector(_windowKeyChanged:) name:NSWindowDidBecomeKeyNotification object:window];
        [center addObserver:self selector:@selector(_windowKeyChanged:) name:NSWindowDidResignKeyNotification object:window];
    }
    _WebWPEViewSetActive(_wpeView, window.isKeyWindow);
}

- (void)viewDidHide { [super viewDidHide]; _WebWPEViewSetMapped(_wpeView, NO); }
- (void)viewDidUnhide { [super viewDidUnhide]; _WebWPEViewSetMapped(_wpeView, self.window != nil); }

- (void)_windowKeyChanged:(NSNotification *)notification {
    _WebWPEViewSetActive(_wpeView, self.window.isKeyWindow);
}

#pragma mark Input

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (BOOL)becomeFirstResponder {
    wpe_view_focus_in(_wpeView);
    return YES;
}

- (BOOL)resignFirstResponder {
    wpe_view_focus_out(_wpeView);
    return YES;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea)
        [self removeTrackingArea:_trackingArea];
    _trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
        options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
        owner:self userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

/* An event to WebKit, at its place in the view. */
- (void)_send:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    WPEEvent *wpeEvent = _WebWPEEventCreate(_wpeView, event, point);
    if (wpeEvent == NULL)
        return;
    wpe_view_event(_wpeView, wpeEvent);
    wpe_event_unref(wpeEvent);
}

- (void)mouseDown:(NSEvent *)event {
    if (self.window.firstResponder != self)
        [self.window makeFirstResponder:self];
    [self _send:event];
}
- (void)mouseUp:(NSEvent *)event { [self _send:event]; }
- (void)mouseDragged:(NSEvent *)event { [self _send:event]; }
- (void)mouseMoved:(NSEvent *)event { [self _send:event]; }
- (void)mouseEntered:(NSEvent *)event { [self _send:event]; }
- (void)mouseExited:(NSEvent *)event { [self _send:event]; }
- (void)rightMouseDown:(NSEvent *)event {
    if (self.window.firstResponder != self)
        [self.window makeFirstResponder:self];
    [self _send:event];
}
- (void)rightMouseUp:(NSEvent *)event { [self _send:event]; }
- (void)rightMouseDragged:(NSEvent *)event { [self _send:event]; }
- (void)otherMouseDown:(NSEvent *)event { [self _send:event]; }
- (void)otherMouseUp:(NSEvent *)event { [self _send:event]; }
- (void)otherMouseDragged:(NSEvent *)event { [self _send:event]; }
- (void)scrollWheel:(NSEvent *)event { [self _send:event]; }
- (void)keyDown:(NSEvent *)event { [self _send:event]; }
- (void)keyUp:(NSEvent *)event { [self _send:event]; }

- (void)flagsChanged:(NSEvent *)event {
    WPEEvent *wpeEvent = _WebWPEModifierEventCreate(_wpeView, event);
    if (wpeEvent == NULL)
        return;
    wpe_view_event(_wpeView, wpeEvent);
    wpe_event_unref(wpeEvent);
}

#pragma mark Editing

- (void)_editingCommand:(const char *)command {
    webkit_web_view_execute_editing_command(_webView, command);
}

- (IBAction)copy:(id)sender { [self _editingCommand:WEBKIT_EDITING_COMMAND_COPY]; }
- (IBAction)cut:(id)sender { [self _editingCommand:WEBKIT_EDITING_COMMAND_CUT]; }
- (IBAction)paste:(id)sender {
    _WebWPEClipboardSync();
    [self _editingCommand:WEBKIT_EDITING_COMMAND_PASTE];
}
- (IBAction)selectAll:(id)sender { [self _editingCommand:WEBKIT_EDITING_COMMAND_SELECT_ALL]; }
- (IBAction)undo:(id)sender { [self _editingCommand:WEBKIT_EDITING_COMMAND_UNDO]; }
- (IBAction)redo:(id)sender { [self _editingCommand:WEBKIT_EDITING_COMMAND_REDO]; }

/* Command's editing shortcuts, when no menu took them: the actions above
   (WebKit's own shortcuts are Control's, as on other systems). */
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if (self.window.firstResponder != self || event.type != NSEventTypeKeyDown)
        return [super performKeyEquivalent:event];
    NSEventModifierFlags flags = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
    if ((flags & ~(NSEventModifierFlagShift | NSEventModifierFlagCapsLock)) != NSEventModifierFlagCommand)
        return [super performKeyEquivalent:event];
    NSString *key = event.charactersIgnoringModifiers.lowercaseString;
    BOOL shift = (flags & NSEventModifierFlagShift) != 0;
    if ([key isEqualToString:@"c"]) [self copy:nil];
    else if ([key isEqualToString:@"x"]) [self cut:nil];
    else if ([key isEqualToString:@"v"]) [self paste:nil];
    else if ([key isEqualToString:@"a"]) [self selectAll:nil];
    else if ([key isEqualToString:@"z"]) shift ? [self redo:nil] : [self undo:nil];
    else return [super performKeyEquivalent:event];
    return YES;
}

#pragma mark Properties

- (WKWebViewConfiguration *)configuration { return [_configuration copy]; }
- (WKBackForwardList *)backForwardList { return _backForwardList; }
- (NSString *)title { return stringOrNil(webkit_web_view_get_title(_webView)); }
- (NSURL *)URL { return URLOrNil(webkit_web_view_get_uri(_webView)); }
- (BOOL)isLoading { return webkit_web_view_is_loading(_webView); }
- (double)estimatedProgress { return webkit_web_view_get_estimated_load_progress(_webView); }
- (BOOL)canGoBack { return webkit_web_view_can_go_back(_webView); }
- (BOOL)canGoForward { return webkit_web_view_can_go_forward(_webView); }

- (NSString *)customUserAgent { return _customUserAgent; }

- (void)setCustomUserAgent:(NSString *)agent {
    _customUserAgent = [agent copy];
    WebKitSettings *settings = webkit_web_view_get_settings(_webView);
    webkit_settings_set_user_agent(settings, agent.length ? agent.UTF8String : NULL);
}

- (CGFloat)magnification { return _magnification; }

- (void)setMagnification:(CGFloat)magnification {
    _magnification = magnification;
    webkit_web_view_set_zoom_level(_webView, magnification);
}

/* Key-value observing sees these change as WebKit's properties do. */
- (void)_propertyChanged:(const char *)property {
    NSString *key = nil;
    if (!strcmp(property, "title")) key = @"title";
    else if (!strcmp(property, "uri")) key = @"URL";
    else if (!strcmp(property, "estimated-load-progress")) key = @"estimatedProgress";
    else if (!strcmp(property, "is-loading")) key = @"loading";
    if (key == nil)
        return;
    [self willChangeValueForKey:key];
    [self didChangeValueForKey:key];
    if ([key isEqualToString:@"URL"]) {
        [self willChangeValueForKey:@"canGoBack"];
        [self didChangeValueForKey:@"canGoBack"];
        [self willChangeValueForKey:@"canGoForward"];
        [self didChangeValueForKey:@"canGoForward"];
    }
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    NSSet *ours = [NSSet setWithObjects:@"title", @"URL", @"estimatedProgress", @"loading", @"canGoBack", @"canGoForward", nil];
    return [ours containsObject:key] ? NO : [super automaticallyNotifiesObserversForKey:key];
}

#pragma mark Loading

- (WKNavigation *)_start {
    _navigation = [WKNavigation new];
    return _navigation;
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request {
    WKNavigation *navigation = [self _start];
    WebKitURIRequest *uriRequest = webkit_uri_request_new(request.URL.absoluteString.UTF8String ?: "about:blank");
    SoupMessageHeaders *headers = webkit_uri_request_get_http_headers(uriRequest);
    if (headers) {
        [request.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSString *value, BOOL *stop) {
            (void)stop;
            soup_message_headers_replace(headers, name.UTF8String, value.UTF8String);
        }];
    }
    webkit_web_view_load_request(_webView, uriRequest);
    g_object_unref(uriRequest);
    return navigation;
}

- (WKNavigation *)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL {
    return [self loadRequest:[NSURLRequest requestWithURL:URL]];
}

- (WKNavigation *)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL {
    WKNavigation *navigation = [self _start];
    webkit_web_view_load_html(_webView, string.UTF8String ?: "", baseURL.absoluteString.UTF8String);
    return navigation;
}

- (WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL {
    WKNavigation *navigation = [self _start];
    GBytes *bytes = g_bytes_new(data.bytes, data.length);
    webkit_web_view_load_bytes(_webView, bytes, MIMEType.UTF8String, characterEncodingName.UTF8String,
                               baseURL.absoluteString.UTF8String);
    g_bytes_unref(bytes);
    return navigation;
}

- (WKNavigation *)goBack {
    if (!self.canGoBack)
        return nil;
    WKNavigation *navigation = [self _start];
    webkit_web_view_go_back(_webView);
    return navigation;
}

- (WKNavigation *)goForward {
    if (!self.canGoForward)
        return nil;
    WKNavigation *navigation = [self _start];
    webkit_web_view_go_forward(_webView);
    return navigation;
}

- (WKNavigation *)reload {
    WKNavigation *navigation = [self _start];
    webkit_web_view_reload(_webView);
    return navigation;
}

- (void)stopLoading { webkit_web_view_stop_loading(_webView); }
- (IBAction)goBack:(id)sender { [self goBack]; }
- (IBAction)goForward:(id)sender { [self goForward]; }
- (IBAction)reload:(id)sender { [self reload]; }
- (IBAction)stopLoading:(id)sender { [self stopLoading]; }

#pragma mark Delegates

- (void)_loadChanged:(WebKitLoadEvent)event {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if (_navigation == nil)
        _navigation = [WKNavigation new];   /* one the page started */
    switch (event) {
    case WEBKIT_LOAD_STARTED:
        _loadFailed = NO;
        if ([d respondsToSelector:@selector(webView:didStartProvisionalNavigation:)])
            [d webView:self didStartProvisionalNavigation:_navigation];
        break;
    case WEBKIT_LOAD_COMMITTED:
        if ([d respondsToSelector:@selector(webView:didCommitNavigation:)])
            [d webView:self didCommitNavigation:_navigation];
        break;
    case WEBKIT_LOAD_FINISHED: {
        WKNavigation *navigation = _navigation;
        _navigation = nil;
        if (_loadFailed) {
            _loadFailed = NO;
            break;
        }
        if ([d respondsToSelector:@selector(webView:didFinishNavigation:)])
            [d webView:self didFinishNavigation:navigation];
        break;
    }
    default:
        break;
    }
}

- (void)_loadFailed:(WebKitLoadEvent)event uri:(const char *)uri error:(GError *)error {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    NSError *nsError = loadError(error, uri);
    _loadFailed = YES;
    if (event == WEBKIT_LOAD_COMMITTED) {
        if ([d respondsToSelector:@selector(webView:didFailNavigation:withError:)])
            [d webView:self didFailNavigation:_navigation withError:nsError];
    } else if ([d respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)]) {
        [d webView:self didFailProvisionalNavigation:_navigation withError:nsError];
    }
}

/* A navigation the delegate decides on, now or later: WebKit waits. */
- (gboolean)_decidePolicy:(WebKitPolicyDecision *)decision type:(WebKitPolicyDecisionType)type {
    if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
        return FALSE;                    /* WebKit's own choice */
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if (![d respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)])
        return FALSE;

    WebKitNavigationAction *webAction = webkit_navigation_policy_decision_get_navigation_action(
        WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WKNavigationAction *action = actionFrom(webAction, type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION);

    g_object_ref(decision);
    __block BOOL decided = NO;
    [d webView:self decidePolicyForNavigationAction:action decisionHandler:^(WKNavigationActionPolicy policy) {
        if (decided)
            return;
        decided = YES;
        if (policy == WKNavigationActionPolicyAllow)
            webkit_policy_decision_use(decision);
        else
            webkit_policy_decision_ignore(decision);
        g_object_unref(decision);
    }];
    return TRUE;
}

/* A page opens a window: the delegate makes its web view, with the
   configuration given (which relates it to this one). */
- (WebKitWebView *)_createWebView:(WebKitNavigationAction *)webAction {
    id<WKUIDelegate> d = self.UIDelegate;
    SEL create = @selector(webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:);
    if (![d respondsToSelector:create])
        return NULL;
    WKWebViewConfiguration *configuration = [_configuration copy];
    configuration._relatedWebView = self;
    WKWebView *created = [d webView:self createWebViewWithConfiguration:configuration
                forNavigationAction:actionFrom(webAction, YES) windowFeatures:[WKWindowFeatures new]];
    if (created == nil)
        return NULL;
    if (!created->_openedByPage)
        [NSException raise:NSInternalInconsistencyException
                    format:@"The web view returned by webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures: was not made with the configuration given."];
    return WEBKIT_WEB_VIEW(g_object_ref(created->_webView));
}

/* alert(), confirm() and prompt(): the delegate's panels, answered now or
   later; without them, as if dismissed. An answer given during the call
   is only recorded: WebKit closes the dialog itself when the signal
   returns. A later one closes it. */
- (void)_scriptDialog:(WebKitScriptDialog *)dialog {
    id<WKUIDelegate> d = self.UIDelegate;
    NSString *message = stringOrNil(webkit_script_dialog_get_message(dialog)) ?: @"";
    WKFrameInfo *frame = [WKFrameInfo new];
    frame.mainFrame = YES;
    frame.request = [NSURLRequest requestWithURL:self.URL ?: [NSURL URLWithString:@"about:blank"]];

    __block BOOL inCall = YES, answered = NO;
    void (^finish)(void) = ^{
        if (!inCall)
            webkit_script_dialog_close(dialog);
        webkit_script_dialog_unref(dialog);
    };

    switch (webkit_script_dialog_get_dialog_type(dialog)) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        if ([d respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:)]) {
            webkit_script_dialog_ref(dialog);
            [d webView:self runJavaScriptAlertPanelWithMessage:message initiatedByFrame:frame completionHandler:^{
                if (answered)
                    return;
                answered = YES;
                finish();
            }];
        }
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
        if ([d respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:)]) {
            webkit_script_dialog_ref(dialog);
            [d webView:self runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:frame completionHandler:^(BOOL result) {
                if (answered)
                    return;
                answered = YES;
                webkit_script_dialog_confirm_set_confirmed(dialog, result);
                finish();
            }];
        } else {
            webkit_script_dialog_confirm_set_confirmed(dialog, FALSE);
        }
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        if ([d respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:)]) {
            webkit_script_dialog_ref(dialog);
            [d webView:self runJavaScriptTextInputPanelWithPrompt:message
                defaultText:stringOrNil(webkit_script_dialog_prompt_get_default_text(dialog))
                initiatedByFrame:frame completionHandler:^(NSString *result) {
                if (answered)
                    return;
                answered = YES;
                if (result != nil)
                    webkit_script_dialog_prompt_set_text(dialog, result.UTF8String);
                finish();
            }];
        }
        break;
    default:
        /* Leaving a page with a beforeunload handler: allowed. */
        webkit_script_dialog_confirm_set_confirmed(dialog, TRUE);
        break;
    }
    inCall = NO;
}

- (void)_webProcessTerminated {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    SEL selector = NSSelectorFromString(@"webViewWebContentProcessDidTerminate:");
    if ([d respondsToSelector:selector])
        ((void (*)(id, SEL, WKWebView *))objc_msgSend)(d, selector, self);
}

#pragma mark JavaScript

static void evaluated(GObject *object, GAsyncResult *result, gpointer data)
{
    void (^handler)(id, NSError *) = (__bridge_transfer void (^)(id, NSError *))data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);
    if (value == NULL) {
        handler(nil, javaScriptError(error));
        g_error_free(error);
        return;
    }
    id foundation = nil;
    if (foundationValue(value, &foundation))
        handler(foundation, nil);
    else
        handler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorJavaScriptResultTypeIsUnsupported
            userInfo:@{ NSLocalizedDescriptionKey: @"JavaScript execution returned a result of an unsupported type" }]);
    g_object_unref(value);
}

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler {
    void (^handler)(id, NSError *) = completionHandler ? (void (^)(id, NSError *))[completionHandler copy] :
        ^(id result, NSError *error) { (void)result; (void)error; };
    webkit_web_view_evaluate_javascript(_webView, javaScriptString.UTF8String ?: "", -1, NULL, NULL, NULL,
                                        evaluated, (__bridge_retained void *)handler);
}

/* The page's text, waiting for it (for searches and tests). */
- (NSString *)_text {
    __block NSString *text = nil;
    __block BOOL done = NO;
    [self evaluateJavaScript:@"document.body ? document.body.innerText : ''" completionHandler:^(id result, NSError *error) {
        (void)error;
        text = [result isKindOfClass:[NSString class]] ? result : @"";
        done = YES;
    }];
    NSDate *limit = [NSDate dateWithTimeIntervalSinceNow:10];
    while (!done && limit.timeIntervalSinceNow > 0)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    return text ?: @"";
}

@end
