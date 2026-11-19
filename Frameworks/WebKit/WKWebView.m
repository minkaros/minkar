// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.
//
// WKWebView on WPE WebKit: a WebKitWebView on Minkar's WPE platform, its
// frames drawn in this view, its signals turned into the delegates' calls.
// GLib's main context runs on the main run loop (WebGLibRunLoop.m).

#import <WebKit/WKWebView.h>
#import "WKWebViewInternal.h"
#import <objc/message.h>

#include <wpe/webkit.h>
#include "WebGLibRunLoop.h"
#import "WebScriptBridge.h"
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

/* A compiled rule list: its WebKit filter. */
@interface WKContentRuleList ()
@property (readwrite, copy) NSString *identifier;
@property (assign) WebKitUserContentFilter *filter;
@end

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
    } else if (error->domain == G_IO_ERROR) {
        /* Errors of the connection itself, as Foundation's. */
        switch (error->code) {
        case G_IO_ERROR_CONNECTION_REFUSED: code = NSURLErrorCannotConnectToHost; break;
        case G_IO_ERROR_HOST_NOT_FOUND: code = NSURLErrorCannotFindHost; break;
        case G_IO_ERROR_HOST_UNREACHABLE:
        case G_IO_ERROR_NETWORK_UNREACHABLE: code = NSURLErrorNotConnectedToInternet; break;
        case G_IO_ERROR_TIMED_OUT: code = NSURLErrorTimedOut; break;
        case G_IO_ERROR_CANCELLED: code = NSURLErrorCancelled; break;
        case G_IO_ERROR_CONNECTION_CLOSED: code = NSURLErrorNetworkConnectionLost; break;   /* also a broken pipe */
        default: code = NSURLErrorUnknown; break;
        }
    } else if (error->domain == G_RESOLVER_ERROR) {
        code = NSURLErrorCannotFindHost;
    } else if (error->domain == G_TLS_ERROR) {
        code = NSURLErrorSecureConnectionFailed;
    } else if (error->domain == WEBKIT_POLICY_ERROR) {
        domain = @"WebKitErrorDomain";
        code = error->code;              /* 100 on, as WebKit's: 102 frame load interrupted */
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


@interface WKUserContentController ()
- (BOOL)_fileInputTakesDirectories;
@end

static void fileInputClicked(WebKitUserContentManager *manager, JSCValue *value, gpointer data);

/* The world of WebKit.framework's own scripts: pages do not see it. */
static const char *const kWKInternalWorld = "org.minkar.WebKit";

/* What WebKit's file chooser request does not say: whether the file input
   that asked takes directories. Each click on one (a script's click() and
   the keyboard's included) is told from the internal world, as it
   happens: the message comes before the request, on the same
   connection, so the delegate is asked at once, as on the reference. */
static const char *const kWKFileInputHandler = "org.minkar.WebKit.fileInput";
static const char *const kWKFileInputScript =
    "document.addEventListener('click', function (event) {"
    "  var input = event.target;"
    "  if (input instanceof HTMLInputElement && input.type === 'file')"
    "    window.webkit.messageHandlers['org.minkar.WebKit.fileInput'].postMessage(input.hasAttribute('webkitdirectory'));"
    "}, true);";

/* The scripts and handlers of every web view using it: a WebKit user
   content manager, made with the controller. */
@implementation WKUserContentController {
    NSMutableArray<WKUserScript *> *_scripts;
    NSMutableDictionary<NSString *, id<WKScriptMessageHandler>> *_handlers;
    WebKitUserContentManager *_manager;
    NSHashTable<WKWebView *> *_webViews;
    BOOL _fileInputTakesDirectories;     /* the last file input clicked */
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
        webkit_user_content_manager_register_script_message_handler(_manager, kWKFileInputHandler, kWKInternalWorld);
        NSString *signal = [@"script-message-received::" stringByAppendingString:@(kWKFileInputHandler)];
        g_signal_connect(_manager, signal.UTF8String, G_CALLBACK(fileInputClicked), (__bridge void *)self);
        [self _addInternalScripts];
        for (WKUserScript *script in _scripts)
            [self _addToManager:script];
        for (NSString *name in _handlers)
            [self _registerHandlerNamed:name];
    }
    return _manager;
}
- (void)_addWebView:(WKWebView *)webView { [_webViews addObject:webView]; }
static void fileInputClicked(WebKitUserContentManager *manager, JSCValue *value, gpointer data)
{
    (void)manager;
    WKUserContentController *controller = (__bridge WKUserContentController *)data;
    controller->_fileInputTakesDirectories = jsc_value_is_boolean(value) && jsc_value_to_boolean(value);
}
- (BOOL)_fileInputTakesDirectories { return _fileInputTakesDirectories; }
- (void)_addInternalScripts {
    WebKitUserScript *script = webkit_user_script_new_for_world(kWKFileInputScript, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, kWKInternalWorld, NULL, NULL);
    webkit_user_content_manager_add_script(_manager, script);
    webkit_user_script_unref(script);
}
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
    if (_manager) {
        webkit_user_content_manager_remove_all_scripts(_manager);
        [self _addInternalScripts];      /* the app's go, not WebKit.framework's */
    }
}
- (void)addScriptMessageHandler:(id<WKScriptMessageHandler>)handler name:(NSString *)name {
    if (_handlers[name] != nil)
        [NSException raise:NSInvalidArgumentException format:@"a script message handler named %@ already exists", name];
    _handlers[name] = handler;
    if (_manager)
        [self _registerHandlerNamed:name];
}
- (void)addContentRuleList:(WKContentRuleList *)contentRuleList {
    if (contentRuleList.filter)
        webkit_user_content_manager_add_filter([self _manager], contentRuleList.filter);
}

- (void)removeContentRuleList:(WKContentRuleList *)contentRuleList {
    if (contentRuleList.filter)
        webkit_user_content_manager_remove_filter([self _manager], contentRuleList.filter);
}

- (void)removeAllContentRuleLists {
    webkit_user_content_manager_remove_all_filters([self _manager]);
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

#pragma mark - Content rules

@implementation WKContentRuleList
- (void)dealloc {
    if (_filter)
        webkit_user_content_filter_unref(_filter);
}
@end

static WKContentRuleList *ruleListFrom(WebKitUserContentFilter *filter)
{
    WKContentRuleList *list = [WKContentRuleList new];
    list.identifier = stringOrNil(webkit_user_content_filter_get_identifier(filter)) ?: @"";
    list.filter = filter;           /* the reference the caller had */
    return list;
}

static NSError *ruleListError(WKErrorCode code, GError *error)
{
    return [NSError errorWithDomain:WKErrorDomain code:code
        userInfo:@{ NSLocalizedDescriptionKey: (error ? stringOrNil(error->message) : nil) ?: @"The content rule list store failed." }];
}

/* A WebKit filter store's asynchronous calls and the block each one ends
   with: the block, held until the call ends. */
static void ruleListSaved(GObject *object, GAsyncResult *result, gpointer data)
{
    void (^done)(WKContentRuleList *, NSError *) = (__bridge_transfer void (^)(WKContentRuleList *, NSError *))data;
    GError *error = NULL;
    WebKitUserContentFilter *filter = webkit_user_content_filter_store_save_finish(WEBKIT_USER_CONTENT_FILTER_STORE(object), result, &error);
    if (filter)
        done(ruleListFrom(filter), nil);
    else
        done(nil, ruleListError(WKErrorContentRuleListStoreCompileFailed, error));
    g_clear_error(&error);
}

static void ruleListLoaded(GObject *object, GAsyncResult *result, gpointer data)
{
    void (^done)(WKContentRuleList *, NSError *) = (__bridge_transfer void (^)(WKContentRuleList *, NSError *))data;
    GError *error = NULL;
    WebKitUserContentFilter *filter = webkit_user_content_filter_store_load_finish(WEBKIT_USER_CONTENT_FILTER_STORE(object), result, &error);
    if (filter)
        done(ruleListFrom(filter), nil);
    else
        done(nil, ruleListError(WKErrorContentRuleListStoreLookUpFailed, error));
    g_clear_error(&error);
}

static void ruleListRemoved(GObject *object, GAsyncResult *result, gpointer data)
{
    void (^done)(NSError *) = (__bridge_transfer void (^)(NSError *))data;
    GError *error = NULL;
    BOOL removed = webkit_user_content_filter_store_remove_finish(WEBKIT_USER_CONTENT_FILTER_STORE(object), result, &error);
    done(removed ? nil : ruleListError(WKErrorContentRuleListStoreRemoveFailed, error));
    g_clear_error(&error);
}

static void ruleListIdentifiers(GObject *object, GAsyncResult *result, gpointer data)
{
    void (^done)(NSArray *) = (__bridge_transfer void (^)(NSArray *))data;
    gchar **identifiers = webkit_user_content_filter_store_fetch_identifiers_finish(WEBKIT_USER_CONTENT_FILTER_STORE(object), result);
    NSMutableArray *names = [NSMutableArray array];
    for (gchar **i = identifiers; i && *i; i++)
        [names addObject:@(*i)];
    g_strfreev(identifiers);
    done(names);
}

@implementation WKContentRuleListStore {
    WebKitUserContentFilterStore *_store;
}

+ (instancetype)defaultStore {
    static WKContentRuleListStore *store;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *identifier = NSBundle.mainBundle.bundleIdentifier ?: NSProcessInfo.processInfo.processName;
        NSString *path = [[[NSHomeDirectory() stringByAppendingPathComponent:@"Library/WebKit"]
            stringByAppendingPathComponent:identifier] stringByAppendingPathComponent:@"ContentRuleLists"];
        store = [self storeWithURL:[NSURL fileURLWithPath:path isDirectory:YES]];
    });
    return store;
}

+ (instancetype)storeWithURL:(NSURL *)url {
    WKContentRuleListStore *store = [self new];
    _WebGLibRunLoopStart();
    store->_store = webkit_user_content_filter_store_new(url.fileSystemRepresentation);
    return store;
}

- (void)dealloc {
    if (_store)
        g_object_unref(_store);
}

/* Each completion handler on the main queue, as the reference platform
   calls them. */
- (void)compileContentRuleListForIdentifier:(NSString *)identifier encodedContentRuleList:(NSString *)encoded
                          completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler {
    void (^handler)(WKContentRuleList *, NSError *) = [completionHandler copy];
    void (^done)(WKContentRuleList *, NSError *) = ^(WKContentRuleList *list, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{ if (handler) handler(list, error); });
    };
    NSData *json = [encoded dataUsingEncoding:NSUTF8StringEncoding] ?: [NSData data];
    GBytes *bytes = g_bytes_new(json.bytes, json.length);
    webkit_user_content_filter_store_save(_store, identifier.UTF8String ?: "", bytes, NULL, ruleListSaved,
                                          (__bridge_retained void *)[done copy]);
    g_bytes_unref(bytes);
}

- (void)lookUpContentRuleListForIdentifier:(NSString *)identifier
                         completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler {
    void (^handler)(WKContentRuleList *, NSError *) = [completionHandler copy];
    void (^done)(WKContentRuleList *, NSError *) = ^(WKContentRuleList *list, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{ if (handler) handler(list, error); });
    };
    webkit_user_content_filter_store_load(_store, identifier.UTF8String ?: "", NULL, ruleListLoaded,
                                          (__bridge_retained void *)[done copy]);
}

- (void)removeContentRuleListForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler {
    void (^handler)(NSError *) = [completionHandler copy];
    void (^done)(NSError *) = ^(NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{ if (handler) handler(error); });
    };
    webkit_user_content_filter_store_remove(_store, identifier.UTF8String ?: "", NULL, ruleListRemoved,
                                            (__bridge_retained void *)[done copy]);
}

- (void)getAvailableContentRuleListIdentifiers:(void (^)(NSArray<NSString *> *))completionHandler {
    void (^handler)(NSArray *) = [completionHandler copy];
    void (^done)(NSArray *) = ^(NSArray *names) {
        dispatch_async(dispatch_get_main_queue(), ^{ if (handler) handler(names); });
    };
    webkit_user_content_filter_store_fetch_identifiers(_store, NULL, ruleListIdentifiers, (__bridge_retained void *)[done copy]);
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

/* _relatedWebView (WKWebViewInternal.h): the web view whose page opens
   the one made with this configuration. */
@implementation WKWebViewConfiguration {
    __weak WKWebView *_related;
}
SECURE
- (WKWebView *)_relatedWebView { return _related; }
- (void)set_relatedWebView:(WKWebView *)webView { _related = webView; }
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
    copy.mediaTypesRequiringUserActionForPlayback = self.mediaTypesRequiringUserActionForPlayback;
    copy.allowsAirPlayForMediaPlayback = self.allowsAirPlayForMediaPlayback;
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

@interface WKNavigationResponse ()
@property (readwrite, getter=isForMainFrame) BOOL forMainFrame;
@property (readwrite, copy) NSURLResponse *response;
@property (readwrite) BOOL canShowMIMEType;
@end

@implementation WKNavigationResponse
@end

@interface WKOpenPanelParameters ()
@property (readwrite) BOOL allowsMultipleSelection;
@property (readwrite) BOOL allowsDirectories;
@end

@implementation WKOpenPanelParameters
@end

/* A response as WebKit has it, as the Foundation response a delegate
   reads: an HTTP one with its status and headers. */
static NSURLResponse *responseFrom(WebKitURIResponse *response)
{
    NSURL *url = [NSURL URLWithString:@(webkit_uri_response_get_uri(response) ?: "about:blank")];
    guint status = webkit_uri_response_get_status_code(response);
    SoupMessageHeaders *headers = webkit_uri_response_get_http_headers(response);
    if (status != 0 && headers != NULL) {
        NSMutableDictionary *fields = [NSMutableDictionary dictionary];
        SoupMessageHeadersIter iter;
        const char *name, *value;
        soup_message_headers_iter_init(&iter, headers);
        while (soup_message_headers_iter_next(&iter, &name, &value))
            fields[@(name)] = @(value);
        if (fields[@"Content-Type"] == nil && webkit_uri_response_get_mime_type(response))
            fields[@"Content-Type"] = @(webkit_uri_response_get_mime_type(response));
        return [[NSHTTPURLResponse alloc] initWithURL:url statusCode:status HTTPVersion:@"HTTP/1.1" headerFields:fields];
    }
    const char *mime = webkit_uri_response_get_mime_type(response);
    return [[NSURLResponse alloc] initWithURL:url MIMEType:mime ? @(mime) : nil
                        expectedContentLength:(NSInteger)webkit_uri_response_get_content_length(response)
                             textEncodingName:nil];
}

/* The sender of a challenge the delegate answers by its completion
   handler; the sender's own methods are not used for a web view's. */
@interface _WKAuthenticationSender : NSObject <NSURLAuthenticationChallengeSender>
@end

@implementation _WKAuthenticationSender
- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {}
- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {}
- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {}
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
    /* What key-value observers saw last: each changes, with its old
       value, only when WebKit's does. */
    NSString *_kvoTitle;
    NSURL *_kvoURL;
    double _kvoProgress;
    BOOL _kvoLoading, _kvoCanGoBack, _kvoCanGoForward, _kvoSecure;
    /* The page loaded something over an insecure connection. */
    BOOL _insecureContent;
    /* A load's end held until the page's title has come (see
       _syncEndOfLoad), and the finished navigation waiting for it. */
    BOOL _endSyncing, _finishWaiting;
    NSUInteger _syncGeneration;
    WKNavigation *_finishedNavigation;
    NSString *_pageTitle;                 /* the page's own answer */
}

static void evaluated(GObject *object, GAsyncResult *result, gpointer data);

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

static void backForwardChanged(WebKitBackForwardList *list, WebKitBackForwardListItem *added, gpointer removed, gpointer data)
{
    (void)list; (void)added; (void)removed;
    /* Forward first, as on the reference. */
    [(__bridge WKWebView *)data _refreshKey:@"canGoForward"];
    [(__bridge WKWebView *)data _refreshKey:@"canGoBack"];
}

static void insecureContentDetected(WebKitWebView *webView, WebKitInsecureContentEvent event, gpointer data)
{
    (void)webView; (void)event;
    [(__bridge WKWebView *)data _insecureContentDetected];
}

static gboolean runFileChooser(WebKitWebView *webView, WebKitFileChooserRequest *request, gpointer data)
{
    (void)webView;
    return [(__bridge WKWebView *)data _runFileChooser:request];
}

static gboolean authenticate(WebKitWebView *webView, WebKitAuthenticationRequest *request, gpointer data)
{
    (void)webView;
    return [(__bridge WKWebView *)data _authenticate:request];
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
        _WebScriptBridgeStart();
        _configuration = [configuration copy] ?: [WKWebViewConfiguration new];
        _magnification = 1;

        WKPreferences *preferences = _configuration.preferences;
        WebKitSettings *settings = webkit_settings_new();
        /* As on the reference platform, the page's own scripts: user scripts
           and evaluateJavaScript: run either way. */
        webkit_settings_set_enable_javascript_markup(settings, preferences.javaScriptEnabled);
        webkit_settings_set_javascript_can_open_windows_automatically(settings,
            preferences.javaScriptCanOpenWindowsAutomatically);
        webkit_settings_set_minimum_font_size(settings, (guint32)preferences.minimumFontSize);
        /* WebKit's one switch: a gesture for any media, when any is asked. */
        webkit_settings_set_media_playback_requires_user_gesture(settings,
            _configuration.mediaTypesRequiringUserActionForPlayback != WKAudiovisualMediaTypeNone);
        if (_configuration.applicationNameForUserAgent.length)
            webkit_settings_set_user_agent_with_application_details(settings,
                _configuration.applicationNameForUserAgent.UTF8String, NULL);

        WKUserContentController *controller = _configuration.userContentController;
        [controller _addWebView:self];
        WKWebView *related = _configuration._relatedWebView;
        _configuration._relatedWebView = nil;
        /* Autoplay as the configuration says: with sound when no media
           needs the user's action (WPE's own default plays muted only),
           muted when audio does, not at all when video does. */
        WKAudiovisualMediaTypes needAction = _configuration.mediaTypesRequiringUserActionForPlayback;
        WebKitWebsitePolicies *policies = webkit_website_policies_new_with_policies("autoplay",
            needAction == WKAudiovisualMediaTypeNone ? WEBKIT_AUTOPLAY_ALLOW :
            (needAction & WKAudiovisualMediaTypeVideo) ? WEBKIT_AUTOPLAY_DENY : WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND, NULL);
        if (related != nil) {
            /* Opened by related's page: in its web process, session and
               display. */
            _webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "related-view", related->_webView,
                "settings", settings,
                "user-content-manager", [controller _manager],
                "website-policies", policies,
                NULL));
            _openedByPage = YES;
        } else {
            _webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "display", _WebWPEDisplayGet(),
                "settings", settings,
                "user-content-manager", [controller _manager],
                "network-session", [_configuration.websiteDataStore _session],
                "website-policies", policies,
                NULL));
        }
        g_object_unref(settings);
        g_object_unref(policies);
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
        g_signal_connect(webkit_web_view_get_back_forward_list(_webView), "changed", G_CALLBACK(backForwardChanged), me);
        g_signal_connect(_webView, "insecure-content-detected", G_CALLBACK(insecureContentDetected), me);
        g_signal_connect(_webView, "authenticate", G_CALLBACK(authenticate), me);
        g_signal_connect(_webView, "run-file-chooser", G_CALLBACK(runFileChooser), me);
        _kvoTitle = @"";

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

/* Without a background, what is behind the view shows where the page
   draws none. */
- (void)_setDrawsBackground:(BOOL)draws {
    WebKitColor color = { 1, 1, 1, draws ? 1 : 0 };
    webkit_web_view_set_background_color(_webView, &color);
}

- (BOOL)_drawsBackground {
    WebKitColor color;
    webkit_web_view_get_background_color(_webView, &color);
    return color.alpha > 0;
}
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
/* As key-value observers last saw them (they follow WebKit's own). */
- (NSString *)title { return _kvoTitle; }
- (NSURL *)URL { return _kvoURL; }
- (BOOL)isLoading { return _kvoLoading; }
- (double)estimatedProgress { return _kvoProgress; }
- (BOOL)canGoBack { return _kvoCanGoBack; }
- (BOOL)canGoForward { return _kvoCanGoForward; }
- (BOOL)hasOnlySecureContent { return _kvoSecure; }

/* Secure: an https page whose certificate WebKit took without errors,
   and nothing it loaded since came over an insecure connection. */
- (BOOL)_liveSecure {
    const char *uri = webkit_web_view_get_uri(_webView);
    GTlsCertificate *certificate = NULL;
    GTlsCertificateFlags errors = 0;
    if (uri == NULL || strncmp(uri, "https:", 6) != 0 || _insecureContent)
        return NO;
    return webkit_web_view_get_tls_info(_webView, &certificate, &errors) && errors == 0;
}

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

/* Key-value observing sees these change as WebKit's properties do: once,
   with the old value, as on the reference (the title empty, not nil, while
   a page has none). */
- (void)_refreshKey:(NSString *)key {
    if ([key isEqualToString:@"title"]) {
        NSString *now = stringOrNil(webkit_web_view_get_title(_webView)) ?: @"";
        if ([now isEqualToString:_kvoTitle])
            return;
        [self willChangeValueForKey:key];
        _kvoTitle = [now copy];
        [self didChangeValueForKey:key];
    } else if ([key isEqualToString:@"URL"]) {
        NSURL *now = URLOrNil(webkit_web_view_get_uri(_webView));
        if (now == _kvoURL || [now isEqual:_kvoURL])
            return;
        [self willChangeValueForKey:key];
        _kvoURL = [now copy];
        [self didChangeValueForKey:key];
    } else if ([key isEqualToString:@"estimatedProgress"]) {
        double now = webkit_web_view_get_estimated_load_progress(_webView);
        if (now == _kvoProgress)
            return;
        [self willChangeValueForKey:key];
        _kvoProgress = now;
        [self didChangeValueForKey:key];
    } else {
        BOOL *slot;
        BOOL now;
        if ([key isEqualToString:@"loading"]) {
            slot = &_kvoLoading;
            now = webkit_web_view_is_loading(_webView);
        } else if ([key isEqualToString:@"canGoBack"]) {
            slot = &_kvoCanGoBack;
            now = webkit_web_view_can_go_back(_webView);
        } else if ([key isEqualToString:@"canGoForward"]) {
            slot = &_kvoCanGoForward;
            now = webkit_web_view_can_go_forward(_webView);
        } else if ([key isEqualToString:@"hasOnlySecureContent"]) {
            slot = &_kvoSecure;
            now = [self _liveSecure];
        } else
            return;
        if (now == *slot)
            return;
        [self willChangeValueForKey:key];
        *slot = now;
        [self didChangeValueForKey:key];
    }
}

/* WebKit tells its properties in an order of its own; observers see them
   as on the reference: as a load starts, the progress, the URL, then
   loading; as it ends, the title, the progress and the URL (back where it
   was, after a failure), then loading. */
- (void)_propertyChanged:(const char *)property {
    if (!strcmp(property, "title")) {
        [self _refreshKey:@"title"];
        return;
    }
    BOOL liveLoading = webkit_web_view_is_loading(_webView);
    /* A load that completes: its end waits for the title. */
    if (_kvoLoading && webkit_web_view_get_estimated_load_progress(_webView) >= 1.0 &&
        (!strcmp(property, "estimated-load-progress") || (!strcmp(property, "is-loading") && !liveLoading))) {
        [self _syncEndOfLoad];
        return;
    }
    if (_endSyncing) {
        if (!(!strcmp(property, "is-loading") && liveLoading))
            return;                       /* told when the end is */
        [self _flushEndOfLoad];           /* a new load: the last one ends first */
    }
    if (!strcmp(property, "uri")) [self _refreshKey:@"URL"];
    else if (!strcmp(property, "estimated-load-progress")) [self _refreshKey:@"estimatedProgress"];
    else if (!strcmp(property, "is-loading")) {
        [self _refreshKey:@"title"];
        [self _refreshKey:@"estimatedProgress"];
        [self _refreshKey:@"URL"];
        [self _refreshKey:@"loading"];
    }
}

/* WebKit tells a page's title a moment after its load has finished; on
   the reference the title is there as the load ends, before the delegate
   hears of it (an app reads it then, a browser for its history). The
   page is asked for its title, and the end is told with it: title,
   progress, URL, loading, then the delegate's didFinishNavigation:
   (WebKit's own notice of the same title, later, changes nothing). A
   second at most; without an answer (scripts off), WebKit's title. */
- (void)_syncEndOfLoad {
    if (_endSyncing)
        return;
    _endSyncing = YES;
    NSUInteger generation = ++_syncGeneration;
    __weak WKWebView *weakSelf = self;
    void (^done)(void) = ^{
        WKWebView *me = weakSelf;
        if (me != nil && me->_syncGeneration == generation)
            [me _flushEndOfLoad];
    };
    webkit_web_view_evaluate_javascript(_webView, "document.title", -1, NULL, NULL, NULL, evaluated,
                                        (__bridge_retained void *)[^(id result, NSError *error) {
                                            WKWebView *me = weakSelf;
                                            if (me != nil && me->_syncGeneration == generation &&
                                                error == nil && [result isKindOfClass:[NSString class]])
                                                me->_pageTitle = [result copy];
                                            done();
                                        } copy]);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), done);
}

- (void)_flushEndOfLoad {
    if (!_endSyncing)
        return;
    _endSyncing = NO;
    _syncGeneration++;
    NSString *pageTitle = _pageTitle;
    _pageTitle = nil;
    if (pageTitle != nil && ![pageTitle isEqualToString:_kvoTitle]) {
        [self willChangeValueForKey:@"title"];
        _kvoTitle = pageTitle;
        [self didChangeValueForKey:@"title"];
    } else
        [self _refreshKey:@"title"];
    [self _refreshKey:@"estimatedProgress"];
    [self _refreshKey:@"URL"];
    [self _refreshKey:@"loading"];
    if (_finishWaiting) {
        _finishWaiting = NO;
        WKNavigation *navigation = _finishedNavigation;
        _finishedNavigation = nil;
        id<WKNavigationDelegate> d = self.navigationDelegate;
        if ([d respondsToSelector:@selector(webView:didFinishNavigation:)])
            [d webView:self didFinishNavigation:navigation];
    }
}

- (void)_insecureContentDetected {
    _insecureContent = YES;
    [self _refreshKey:@"hasOnlySecureContent"];
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    NSSet *ours = [NSSet setWithObjects:@"title", @"URL", @"estimatedProgress", @"loading", @"canGoBack", @"canGoForward",
                   @"hasOnlySecureContent", nil];
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
        _insecureContent = NO;
        if ([d respondsToSelector:@selector(webView:didStartProvisionalNavigation:)])
            [d webView:self didStartProvisionalNavigation:_navigation];
        break;
    case WEBKIT_LOAD_REDIRECTED:
        [self _refreshKey:@"URL"];
        if ([d respondsToSelector:@selector(webView:didReceiveServerRedirectForProvisionalNavigation:)])
            [d webView:self didReceiveServerRedirectForProvisionalNavigation:_navigation];
        break;
    case WEBKIT_LOAD_COMMITTED:
        [self _refreshKey:@"hasOnlySecureContent"];
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
        if (_endSyncing) {                /* told with the title */
            _finishWaiting = YES;
            _finishedNavigation = navigation;
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
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
        return [self _decideResponse:WEBKIT_RESPONSE_POLICY_DECISION(decision)];
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

/* A response: the delegate's decision, now or later; without one,
   WebKit's own (show what it can). */
- (gboolean)_decideResponse:(WebKitResponsePolicyDecision *)decision {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    SEL selector = @selector(webView:decidePolicyForNavigationResponse:decisionHandler:);
    if (![d respondsToSelector:selector])
        return FALSE;
    WKNavigationResponse *response = [WKNavigationResponse new];
    response.response = responseFrom(webkit_response_policy_decision_get_response(decision));
    response.forMainFrame = webkit_response_policy_decision_is_main_frame_main_resource(decision);
    response.canShowMIMEType = webkit_response_policy_decision_is_mime_type_supported(decision);

    g_object_ref(decision);
    __block BOOL decided = NO;
    [d webView:self decidePolicyForNavigationResponse:response decisionHandler:^(WKNavigationResponsePolicy policy) {
        if (decided)
            return;
        decided = YES;
        if (policy == WKNavigationResponsePolicyAllow)
            webkit_policy_decision_use(WEBKIT_POLICY_DECISION(decision));
        else
            webkit_policy_decision_ignore(WEBKIT_POLICY_DECISION(decision));
        g_object_unref(decision);
    }];
    return TRUE;
}

/* A file input asks for files: the UI delegate's open panel, answered
   now or later with URLs (nil: cancelled, the input keeps what it had).
   Without one, nothing is chosen. Whether the input takes directories was
   told as it was clicked (see kWKFileInputScript). */
- (gboolean)_runFileChooser:(WebKitFileChooserRequest *)request {
    id<WKUIDelegate> d = self.UIDelegate;
    SEL selector = @selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:);
    if (![d respondsToSelector:selector]) {
        webkit_file_chooser_request_cancel(request);
        return TRUE;
    }
    g_object_ref(request);
    BOOL multiple = webkit_file_chooser_request_get_select_multiple(request);
    __weak WKWebView *weakSelf = self;
    void (^ask)(BOOL) = ^(BOOL directories) {
        WKWebView *me = weakSelf;
        id<WKUIDelegate> delegate = me.UIDelegate;
        if (me == nil || ![delegate respondsToSelector:selector]) {
            webkit_file_chooser_request_cancel(request);
            g_object_unref(request);
            return;
        }
        WKOpenPanelParameters *parameters = [WKOpenPanelParameters new];
        parameters.allowsMultipleSelection = multiple;
        parameters.allowsDirectories = directories;
        WKFrameInfo *frame = [WKFrameInfo new];
        frame.mainFrame = YES;
        frame.request = [NSURLRequest requestWithURL:me.URL ?: [NSURL URLWithString:@"about:blank"]];
        __block BOOL answered = NO;
        [delegate webView:me runOpenPanelWithParameters:parameters initiatedByFrame:frame
            completionHandler:^(NSArray<NSURL *> *URLs) {
            if (answered)
                return;
            answered = YES;
            NSMutableArray *paths = [NSMutableArray array];
            for (NSURL *url in URLs)
                if (url.isFileURL && url.path != nil)
                    [paths addObject:url.path];
            if (URLs == nil || paths.count == 0)
                webkit_file_chooser_request_cancel(request);
            else {
                const char **files = calloc(paths.count + 1, sizeof(char *));
                for (NSUInteger i = 0; i < paths.count; i++)
                    files[i] = [paths[i] fileSystemRepresentation];
                webkit_file_chooser_request_select_files(request, files);
                free(files);
            }
            g_object_unref(request);
        }];
    };
    ask([_configuration.userContentController _fileInputTakesDirectories]);
    return TRUE;
}

/* A server asks who the user is: the delegate answers, now or later,
   with a credential, the default handling (none: the request goes on
   without one) or a cancellation. Without a delegate, as the default. */
- (gboolean)_authenticate:(WebKitAuthenticationRequest *)request {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    SEL selector = @selector(webView:didReceiveAuthenticationChallenge:completionHandler:);
    if (![d respondsToSelector:selector])
        return FALSE;
    NSString *method;
    switch (webkit_authentication_request_get_scheme(request)) {
    case WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC: method = NSURLAuthenticationMethodHTTPBasic; break;
    case WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST: method = NSURLAuthenticationMethodHTTPDigest; break;
    case WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM: method = NSURLAuthenticationMethodHTMLForm; break;
    case WEBKIT_AUTHENTICATION_SCHEME_NTLM: method = NSURLAuthenticationMethodNTLM; break;
    case WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE: method = NSURLAuthenticationMethodNegotiate; break;
    case WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED: method = NSURLAuthenticationMethodClientCertificate; break;
    case WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED: method = NSURLAuthenticationMethodServerTrust; break;
    default: method = NSURLAuthenticationMethodDefault; break;
    }
    const char *uri = webkit_web_view_get_uri(_webView);
    NSString *protocol = (uri != NULL && strncmp(uri, "https:", 6) == 0) ? @"https" : @"http";
    NSURLProtectionSpace *space = [[NSURLProtectionSpace alloc]
        initWithHost:@(webkit_authentication_request_get_host(request) ?: "")
                port:webkit_authentication_request_get_port(request)
            protocol:protocol
               realm:stringOrNil(webkit_authentication_request_get_realm(request))
authenticationMethod:method];
    NSURLAuthenticationChallenge *challenge = [[NSURLAuthenticationChallenge alloc]
        initWithProtectionSpace:space proposedCredential:nil
           previousFailureCount:webkit_authentication_request_is_retry(request) ? 1 : 0
                failureResponse:nil error:nil sender:[_WKAuthenticationSender new]];

    g_object_ref(request);
    __block BOOL answered = NO;
    [d webView:self didReceiveAuthenticationChallenge:challenge
        completionHandler:^(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
        if (answered)
            return;
        answered = YES;
        if (disposition == NSURLSessionAuthChallengeUseCredential && credential.user != nil) {
            WebKitCredentialPersistence persistence =
                credential.persistence == NSURLCredentialPersistencePermanent ? WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT
                : credential.persistence == NSURLCredentialPersistenceForSession ? WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION
                : WEBKIT_CREDENTIAL_PERSISTENCE_NONE;
            WebKitCredential *answer = webkit_credential_new(credential.user.UTF8String,
                                                             (credential.password ?: @"").UTF8String, persistence);
            webkit_authentication_request_authenticate(request, answer);
            webkit_credential_free(answer);
        } else if (disposition == NSURLSessionAuthChallengePerformDefaultHandling)
            webkit_authentication_request_authenticate(request, NULL);   /* go on without one */
        else
            webkit_authentication_request_cancel(request);
        g_object_unref(request);
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
    if (jsc_value_is_undefined(value))
        handler(nil, nil);               /* no result, no error: as on the reference */
    else if (foundationValue(value, &foundation))
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
    _WebGLibRunLoopWait(^{ return done; }, 10);
    return text ?: @"";
}

@end
