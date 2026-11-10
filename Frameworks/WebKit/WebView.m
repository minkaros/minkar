// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.
//
// The older web view API on WPE WebKit: a WebView shows a WKWebView of its
// own (filling it) and is its navigation and UI delegate, turning their
// calls into the older delegates'. Scripts cross through JavaScript:
// objects the page hands out are kept in a table of the page's
// (window.__webScriptObjects) and named by their index there.

#import <WebKit/WebView.h>
#import <objc/message.h>
#import "WKWebViewInternal.h"
#import "WebGLibRunLoop.h"

NSString *const WebViewProgressStartedNotification = @"WebProgressStartedNotification";
NSString *const WebViewProgressEstimateChangedNotification = @"WebProgressEstimateChangedNotification";
NSString *const WebViewProgressFinishedNotification = @"WebProgressFinishedNotification";
NSString *const WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";

NSString *const WebActionNavigationTypeKey = @"WebActionNavigationTypeKey";
NSString *const WebActionElementKey = @"WebActionElementKey";
NSString *const WebActionButtonKey = @"WebActionButtonKey";
NSString *const WebActionModifierFlagsKey = @"WebActionModifierFlagsKey";
NSString *const WebActionOriginalURLKey = @"WebActionOriginalURLKey";

/* How long a synchronous call waits for WebKit's processes. */
static const NSTimeInterval SyncTimeout = 15;

@interface WebView ()
- (WKWebView *)_engine;
- (WebFrame *)_frame;
- (void)_prepareLoad;
@end

#pragma mark - Scripts

@implementation WebUndefined
+ (WebUndefined *)undefined {
    static WebUndefined *undefined;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ undefined = [WebUndefined new]; });
    return undefined;
}
- (instancetype)initWithCoder:(NSCoder *)coder { return [WebUndefined undefined]; }
- (void)encodeWithCoder:(NSCoder *)coder {}
- (id)copyWithZone:(NSZone *)zone { return self; }
@end

@interface WebScriptObject ()
@property (weak) WebView *webView;
/* The JavaScript expression naming the object (window, or its entry in
   the page's table). */
@property (copy) NSString *expression;
@end

static NSString *literal(NSString *string)
{
    NSData *json = [NSJSONSerialization dataWithJSONObject:@[ string ?: @"" ] options:0 error:nil];
    NSString *array = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
    return [array substringWithRange:NSMakeRange(1, array.length - 2)];
}

/* A Foundation value as a JavaScript expression (nil if it has none). */
static NSString *scriptValue(id value)
{
    if (value == nil || value == [NSNull null])
        return @"null";
    if (value == [WebUndefined undefined])
        return @"undefined";
    if ([value isKindOfClass:[WebScriptObject class]])
        return [value expression];
    if ([value isKindOfClass:[NSString class]])
        return literal(value);
    if ([value isKindOfClass:[NSNumber class]]) {
        const char *type = [value objCType];
        if (!strcmp(type, @encode(BOOL)) || !strcmp(type, "c"))
            return [value boolValue] ? @"true" : @"false";
        return [value stringValue];
    }
    if ([NSJSONSerialization isValidJSONObject:value]) {
        NSData *json = [NSJSONSerialization dataWithJSONObject:value options:0 error:nil];
        return [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
    }
    return nil;
}

@implementation WebScriptObject

/* Runs body (statements, ending in "return r;" for a value) and gives the
   value back as WebScriptObject's rules say; nil on an exception. */
- (id)_run:(NSString *)body {
    WebView *webView = self.webView;
    if (webView == nil)
        return nil;
    NSString *script = [NSString stringWithFormat:
        @"(function () { var r; try { r = (function () { %@ })(); } catch (e) { return { t: 'x' }; }"
         " if (r === undefined) return { t: 'u' }; if (r === null) return { t: 'n' };"
         " var k = typeof r; if (k === 'string' || k === 'number' || k === 'boolean') return { t: k, v: r };"
         " var table = window.__webScriptObjects || (window.__webScriptObjects = []);"
         " table.push(r); return { t: 'o', i: table.length - 1 }; })()", body];
    __block NSDictionary *result = nil;
    __block BOOL done = NO;
    [webView._engine evaluateJavaScript:script completionHandler:^(id value, NSError *error) {
        (void)error;
        result = [value isKindOfClass:[NSDictionary class]] ? value : nil;
        done = YES;
    }];
    _WebGLibRunLoopWait(^{ return done; }, SyncTimeout);
    NSString *type = result[@"t"];
    if (type == nil || [type isEqualToString:@"x"])
        return nil;
    if ([type isEqualToString:@"u"])
        return [WebUndefined undefined];
    if ([type isEqualToString:@"n"])
        return [NSNull null];
    if ([type isEqualToString:@"o"]) {
        WebScriptObject *object = [WebScriptObject new];
        object.webView = webView;
        object.expression = [NSString stringWithFormat:@"window.__webScriptObjects[%@]", result[@"i"]];
        return object;
    }
    return result[@"v"];
}

- (id)evaluateWebScript:(NSString *)script {
    /* The script runs as the page's own (indirect eval), with this object
       as "this" when it is not the window. */
    return [self _run:[NSString stringWithFormat:@"return (0, eval)(%@);", literal(script)]];
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)arguments {
    NSMutableArray *values = [NSMutableArray array];
    for (id argument in arguments) {
        NSString *value = scriptValue(argument);
        if (value == nil)
            return nil;
        [values addObject:value];
    }
    return [self _run:[NSString stringWithFormat:@"var o = %@; return o[%@](%@);", self.expression, literal(name),
                       [values componentsJoinedByString:@", "]]];
}

- (id)valueForKey:(NSString *)key {
    return [self _run:[NSString stringWithFormat:@"return (%@)[%@];", self.expression, literal(key)]];
}

- (void)setValue:(id)value forKey:(NSString *)key {
    NSString *script = scriptValue(value);
    if (script != nil)
        [self _run:[NSString stringWithFormat:@"(%@)[%@] = %@;", self.expression, literal(key), script]];
}

- (void)removeWebScriptKey:(NSString *)name {
    [self _run:[NSString stringWithFormat:@"delete (%@)[%@];", self.expression, literal(name)]];
}

- (id)webScriptValueAtIndex:(unsigned)index {
    return [self _run:[NSString stringWithFormat:@"return (%@)[%u];", self.expression, index]];
}

- (void)setWebScriptValueAtIndex:(unsigned)index value:(id)value {
    NSString *script = scriptValue(value);
    if (script != nil)
        [self _run:[NSString stringWithFormat:@"(%@)[%u] = %@;", self.expression, index, script]];
}

@end

#pragma mark - Preferences

@implementation WebPreferences
+ (WebPreferences *)standardPreferences {
    static WebPreferences *standard;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ standard = [[WebPreferences alloc] initWithIdentifier:nil]; });
    return standard;
}
- (instancetype)init { return [self initWithIdentifier:nil]; }
- (instancetype)initWithIdentifier:(NSString *)identifier {
    if ((self = [super init])) {
        _identifier = [identifier copy];
        _standardFontFamily = @"Inter";
        _fixedFontFamily = @"Agave";
        _serifFontFamily = @"Inter";
        _sansSerifFontFamily = @"Inter";
        _defaultFontSize = 16;
        _defaultFixedFontSize = 13;
        _defaultTextEncodingName = @"utf-8";
        _javaScriptEnabled = YES;
        _loadsImagesAutomatically = YES;
        _allowsAnimatedImages = YES;
        _allowsAnimatedImageLooping = YES;
        _usesPageCache = YES;
    }
    return self;
}
- (instancetype)initWithCoder:(NSCoder *)coder { return [self initWithIdentifier:[coder decodeObjectForKey:@"Identifier"]]; }
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:_identifier forKey:@"Identifier"]; }

/* The preferences as WebKit's settings. */
- (void)_applyTo:(WebKitWebView *)webView {
    WebKitSettings *settings = webkit_web_view_get_settings(webView);
    webkit_settings_set_default_font_family(settings, self.standardFontFamily.UTF8String);
    webkit_settings_set_monospace_font_family(settings, self.fixedFontFamily.UTF8String);
    webkit_settings_set_serif_font_family(settings, self.serifFontFamily.UTF8String);
    webkit_settings_set_sans_serif_font_family(settings, self.sansSerifFontFamily.UTF8String);
    webkit_settings_set_default_font_size(settings, (guint32)MAX(self.defaultFontSize, 1));
    webkit_settings_set_default_monospace_font_size(settings, (guint32)MAX(self.defaultFixedFontSize, 1));
    webkit_settings_set_minimum_font_size(settings, (guint32)MAX(self.minimumFontSize, 0));
    webkit_settings_set_default_charset(settings, self.defaultTextEncodingName.UTF8String ?: "utf-8");
    webkit_settings_set_enable_javascript(settings, self.javaScriptEnabled);
    webkit_settings_set_javascript_can_open_windows_automatically(settings, self.javaScriptCanOpenWindowsAutomatically);
    webkit_settings_set_auto_load_images(settings, self.loadsImagesAutomatically);
    webkit_settings_set_enable_page_cache(settings, self.usesPageCache);
    webkit_settings_set_print_backgrounds(settings, self.shouldPrintBackgrounds);
    webkit_settings_set_enable_tabs_to_links(settings, self.tabsToLinks);

    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(webView);
    webkit_user_content_manager_remove_all_style_sheets(manager);
    NSString *css = self.userStyleSheetEnabled && self.userStyleSheetLocation ?
        [NSString stringWithContentsOfURL:self.userStyleSheetLocation encoding:NSUTF8StringEncoding error:nil] : nil;
    if (css.length) {
        WebKitUserStyleSheet *sheet = webkit_user_style_sheet_new(css.UTF8String, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                                                  WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL);
        webkit_user_content_manager_add_style_sheet(manager, sheet);
        webkit_user_style_sheet_unref(sheet);
    }
}
@end

#pragma mark - Frames and data sources

@interface WebDataSource ()
@property (readwrite, strong) NSURLRequest *initialRequest;
@property (readwrite, strong) NSURLRequest *request;
@property (readwrite) BOOL loading;
@property (readwrite, strong) WebFrame *webFrame;
@end

static void gotData(GObject *object, GAsyncResult *result, gpointer block);

@implementation WebDataSource
- (NSURLResponse *)response {
    WebKitWebResource *resource = webkit_web_view_get_main_resource(((WebView *)self.webFrame.webView)._engine._webKitView);
    WebKitURIResponse *response = resource ? webkit_web_resource_get_response(resource) : NULL;
    if (response == NULL)
        return nil;
    NSURL *url = [NSURL URLWithString:@(webkit_uri_response_get_uri(response) ?: "about:blank")];
    const char *mime = webkit_uri_response_get_mime_type(response);
    return [[NSURLResponse alloc] initWithURL:url MIMEType:mime ? @(mime) : nil
                        expectedContentLength:(NSInteger)webkit_uri_response_get_content_length(response)
                             textEncodingName:nil];
}

- (NSString *)pageTitle {
    NSString *title = ((WebView *)self.webFrame.webView)._engine.title;
    return title.length ? title : nil;
}

/* The main resource's bytes, waiting for them. */
- (NSData *)data {
    WebKitWebResource *resource = webkit_web_view_get_main_resource(((WebView *)self.webFrame.webView)._engine._webKitView);
    if (resource == NULL)
        return [NSData data];
    __block NSData *data = nil;
    void (^done)(NSData *) = ^(NSData *d) { data = d ?: [NSData data]; };
    webkit_web_resource_get_data(resource, NULL, gotData, (__bridge_retained void *)[done copy]);
    _WebGLibRunLoopWait(^{ return (BOOL)(data != nil); }, SyncTimeout);
    return data ?: [NSData data];
}

static void gotData(GObject *object, GAsyncResult *result, gpointer block)
{
    void (^done)(NSData *) = (__bridge_transfer void (^)(NSData *))block;
    gsize length = 0;
    guchar *bytes = webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(object), result, &length, NULL);
    done(bytes ? [NSData dataWithBytes:bytes length:length] : nil);
    g_free(bytes);
}
@end

@interface WebFrame ()
@property (weak) WebView *owner;
@property (readwrite, strong) WebDataSource *dataSource;
@property (readwrite, strong) WebDataSource *provisionalDataSource;
@end

@implementation WebFrame {
    WebScriptObject *_windowObject;
}
- (NSString *)name { return @""; }
- (id)webView { return self.owner; }
- (WebFrame *)parentFrame { return nil; }
- (NSArray *)childFrames { return @[]; }
- (DOMDocument *)DOMDocument { return nil; }
- (WebFrameView *)frameView { return nil; }
- (WebScriptObject *)windowObject {
    if (_windowObject == nil) {
        _windowObject = [WebScriptObject new];
        _windowObject.webView = self.owner;
        _windowObject.expression = @"window";
    }
    return _windowObject;
}
- (void)loadRequest:(NSURLRequest *)request {
    [self.owner _prepareLoad];
    [self.owner._engine loadRequest:request];
}
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL {
    [self.owner _prepareLoad];
    [self.owner._engine loadData:data MIMEType:MIMEType ?: @"text/html" characterEncodingName:encodingName ?: @"utf-8"
                         baseURL:URL ?: [NSURL URLWithString:@"about:blank"]];
}
- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL {
    [self.owner _prepareLoad];
    [self.owner._engine loadHTMLString:string baseURL:URL];
}
- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL {
    [self.owner _prepareLoad];
    webkit_web_view_load_alternate_html(self.owner._engine._webKitView, string.UTF8String ?: "",
                                        unreachableURL.absoluteString.UTF8String ?: "about:blank",
                                        baseURL.absoluteString.UTF8String);
}
- (void)stopLoading { [self.owner._engine stopLoading]; }
- (void)reload { [self.owner._engine reload]; }
- (WebFrame *)findFrameNamed:(NSString *)name {
    return (name.length == 0 || [name isEqualToString:@"_self"] || [name isEqualToString:@"_top"]) ? self : nil;
}
@end

@implementation WebFrameView
@end

#pragma mark - Policy decisions

@interface _WebPolicyListener : NSObject <WebPolicyDecisionListener>
@property (copy) void (^decide)(WKNavigationActionPolicy);
@end

@implementation _WebPolicyListener
- (void)_decide:(WKNavigationActionPolicy)policy {
    void (^decide)(WKNavigationActionPolicy) = self.decide;
    self.decide = nil;
    if (decide)
        decide(policy);
}
- (void)use { [self _decide:WKNavigationActionPolicyAllow]; }
- (void)ignore { [self _decide:WKNavigationActionPolicyCancel]; }
/* There are no downloads yet: the navigation does not happen. */
- (void)download { [self _decide:WKNavigationActionPolicyCancel]; }
- (void)dealloc {
    /* A listener let go of undecided: the navigation is ignored. */
    if (_decide)
        _decide(WKNavigationActionPolicyCancel);
}
@end

static WebNavigationType legacyNavigationType(WKNavigationType type)
{
    switch (type) {
    case WKNavigationTypeLinkActivated: return WebNavigationTypeLinkClicked;
    case WKNavigationTypeFormSubmitted: return WebNavigationTypeFormSubmitted;
    case WKNavigationTypeBackForward: return WebNavigationTypeBackForward;
    case WKNavigationTypeReload: return WebNavigationTypeReload;
    case WKNavigationTypeFormResubmitted: return WebNavigationTypeFormResubmitted;
    default: return WebNavigationTypeOther;
    }
}

#pragma mark - The web view

@interface WebView () <WKNavigationDelegate, WKUIDelegate>
@end

@implementation WebView {
    WKWebView *_engine;
    WebFrame *_mainFrame;
    WebPreferences *_preferences;
    NSString *_applicationNameForUserAgent;
    NSString *_customUserAgent;
    BOOL _drawsBackground;
}

+ (BOOL)canShowMIMEType:(NSString *)MIMEType {
    NSString *type = MIMEType.lowercaseString;
    return [type hasPrefix:@"text/"] || [type hasPrefix:@"image/"] || [type isEqualToString:@"application/xhtml+xml"] ||
           [type isEqualToString:@"application/xml"] || [type isEqualToString:@"image/svg+xml"];
}

+ (void)registerURLSchemeAsLocal:(NSString *)scheme {
}

- (instancetype)initWithFrame:(NSRect)frame frameName:(NSString *)frameName groupName:(NSString *)groupName {
    if ((self = [super initWithFrame:frame])) {
        _mainFrame = [WebFrame new];
        _mainFrame.owner = self;
        _preferences = [WebPreferences standardPreferences];
        _drawsBackground = YES;
        [self _makeEngineWithConfiguration:[WKWebViewConfiguration new]];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frame {
    return [self initWithFrame:frame frameName:nil groupName:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithFrame:NSMakeRect(0, 0, 100, 100) frameName:nil groupName:nil];
}

- (void)dealloc {
    [_engine removeObserver:self forKeyPath:@"title"];
    [_engine removeObserver:self forKeyPath:@"estimatedProgress"];
}

/* The WKWebView showing the pages (made again for a page another one
   opens: it must be related to the opener's). */
- (void)_makeEngineWithConfiguration:(WKWebViewConfiguration *)configuration {
    if (_engine) {
        [_engine removeObserver:self forKeyPath:@"title"];
        [_engine removeObserver:self forKeyPath:@"estimatedProgress"];
        [_engine removeFromSuperview];
    }
    configuration.preferences.javaScriptEnabled = _preferences.javaScriptEnabled;
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = _preferences.javaScriptCanOpenWindowsAutomatically;
    _engine = [[WKWebView alloc] initWithFrame:self.bounds configuration:configuration];
    _engine.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _engine.navigationDelegate = self;
    _engine.UIDelegate = self;
    [_engine addObserver:self forKeyPath:@"title" options:0 context:NULL];
    [_engine addObserver:self forKeyPath:@"estimatedProgress" options:0 context:NULL];
    [self addSubview:_engine];
    [_preferences _applyTo:_engine._webKitView];
    [_engine _setDrawsBackground:_drawsBackground];
    if (_customUserAgent.length)
        _engine.customUserAgent = _customUserAgent;
}

- (WKWebView *)_engine { return _engine; }
- (WebFrame *)_frame { return _mainFrame; }

/* Before a load: the preferences as they are now (they may have been
   changed since). */
- (void)_prepareLoad {
    [_preferences _applyTo:_engine._webKitView];
}

#pragma mark Properties

- (WebFrame *)mainFrame { return _mainFrame; }
- (WebFrame *)selectedFrame { return _mainFrame; }
- (WebPreferences *)preferences { return _preferences; }
- (void)setPreferences:(WebPreferences *)preferences {
    _preferences = preferences ?: [WebPreferences standardPreferences];
    [_preferences _applyTo:_engine._webKitView];
}
- (NSString *)preferencesIdentifier { return _preferences.identifier; }
- (void)setPreferencesIdentifier:(NSString *)identifier {
    self.preferences = [[WebPreferences alloc] initWithIdentifier:identifier];
}

- (NSString *)customUserAgent { return _customUserAgent; }
- (void)setCustomUserAgent:(NSString *)agent {
    _customUserAgent = [agent copy];
    _engine.customUserAgent = agent;
}

- (NSString *)applicationNameForUserAgent { return _applicationNameForUserAgent; }
- (void)setApplicationNameForUserAgent:(NSString *)name {
    _applicationNameForUserAgent = [name copy];
    if (_customUserAgent.length == 0)
        webkit_settings_set_user_agent_with_application_details(webkit_web_view_get_settings(_engine._webKitView),
                                                                name.UTF8String, NULL);
}

- (NSString *)mainFrameURL { return _engine.URL.absoluteString; }
- (void)setMainFrameURL:(NSString *)URLString {
    NSURL *url = URLString.length ? [NSURL URLWithString:URLString] : nil;
    if (url != nil)
        [_mainFrame loadRequest:[NSURLRequest requestWithURL:url]];
}
- (NSString *)mainFrameTitle { return _engine.title ?: @""; }
- (double)estimatedProgress { return _engine.isLoading ? _engine.estimatedProgress : 0; }
- (BOOL)isLoading { return _engine.isLoading; }
- (BOOL)drawsBackground { return _drawsBackground; }
- (void)setDrawsBackground:(BOOL)draws {
    _drawsBackground = draws;
    [_engine _setDrawsBackground:draws];
}
- (void)_setDrawsBackground:(BOOL)draws { self.drawsBackground = draws; }
- (DOMRange *)selectedDOMRange { return nil; }
- (WebScriptObject *)windowScriptObject { return _mainFrame.windowObject; }
- (BOOL)canGoBack { return _engine.canGoBack; }
- (BOOL)canGoForward { return _engine.canGoForward; }
- (BOOL)goBack { return [_engine goBack] != nil; }
- (BOOL)goForward { return [_engine goForward] != nil; }

/* The result as a string: strings, numbers and booleans are; anything
   else is empty. */
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script {
    NSString *wrapped = [NSString stringWithFormat:
        @"(function () { var r = (0, eval)(%@); var k = typeof r;"
         " return (k === 'string' || k === 'number' || k === 'boolean') ? String(r) : ''; })()", literal(script)];
    __block NSString *result = nil;
    __block BOOL done = NO;
    [_engine evaluateJavaScript:wrapped completionHandler:^(id value, NSError *error) {
        result = [value isKindOfClass:[NSString class]] ? value : (error ? nil : @"");
        done = YES;
    }];
    _WebGLibRunLoopWait(^{ return done; }, SyncTimeout);
    return result;
}

static void foundText(WebKitFindController *controller, guint count, gpointer data)
{
    (void)controller;
    *(int *)data = count > 0 ? 1 : 2;
}

static void failedToFindText(WebKitFindController *controller, gpointer data)
{
    (void)controller;
    *(int *)data = 2;
}

/* The next match (or the previous one), highlighted and scrolled to. */
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag {
    if (string.length == 0)
        return NO;
    WebKitFindController *finder = webkit_web_view_get_find_controller(_engine._webKitView);
    __block int outcome = 0;
    gulong found = g_signal_connect(finder, "found-text", G_CALLBACK(foundText), &outcome);
    gulong failed = g_signal_connect(finder, "failed-to-find-text", G_CALLBACK(failedToFindText), &outcome);
    guint32 options = (caseFlag ? 0 : WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE) |
                      (forward ? 0 : WEBKIT_FIND_OPTIONS_BACKWARDS) | (wrapFlag ? WEBKIT_FIND_OPTIONS_WRAP_AROUND : 0);
    webkit_find_controller_search(finder, string.UTF8String, options, 1);
    _WebGLibRunLoopWait(^{ return (BOOL)(outcome != 0); }, SyncTimeout);
    g_signal_handler_disconnect(finder, found);
    g_signal_handler_disconnect(finder, failed);
    return outcome == 1;
}

- (IBAction)takeStringURLFrom:(id)sender { self.mainFrameURL = [sender stringValue]; }
- (IBAction)stopLoading:(id)sender { [_mainFrame stopLoading]; }
- (IBAction)reload:(id)sender { [_mainFrame reload]; }
- (IBAction)goBack:(id)sender { [self goBack]; }
- (IBAction)goForward:(id)sender { [self goForward]; }

#pragma mark The engine's navigation

- (WebDataSource *)_dataSourceFor:(NSURLRequest *)request loading:(BOOL)loading {
    WebDataSource *source = [WebDataSource new];
    source.initialRequest = request;
    source.request = request;
    source.loading = loading;
    source.webFrame = _mainFrame;
    return source;
}

- (NSURLRequest *)_currentRequest {
    return [NSURLRequest requestWithURL:_engine.URL ?: [NSURL URLWithString:@"about:blank"]];
}

- (void)_tellFrameLoadDelegate:(SEL)selector with:(id)argument {
    id d = self.frameLoadDelegate;
    if (![d respondsToSelector:selector])
        return;
    if (argument)
        ((void (*)(id, SEL, id, id, id))objc_msgSend)(d, selector, self, argument, _mainFrame);
    else
        ((void (*)(id, SEL, id, id))objc_msgSend)(d, selector, self, _mainFrame);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    _mainFrame.provisionalDataSource = [self _dataSourceFor:[self _currentRequest] loading:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:self];
    [self _tellFrameLoadDelegate:@selector(webView:didStartProvisionalLoadForFrame:) with:nil];
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation {
    _mainFrame.dataSource = [self _dataSourceFor:[self _currentRequest] loading:YES];
    _mainFrame.provisionalDataSource = nil;
    [self _tellFrameLoadDelegate:@selector(webView:didCommitLoadForFrame:) with:nil];
    /* A new page, a new window object: scripts may set things on it. */
    [self _tellFrameLoadDelegate:@selector(webView:didClearWindowObject:forFrame:) with:_mainFrame.windowObject];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    _mainFrame.dataSource = [self _dataSourceFor:[self _currentRequest] loading:NO];
    [self _tellFrameLoadDelegate:@selector(webView:didFinishLoadForFrame:) with:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    _mainFrame.provisionalDataSource = nil;
    [self _tellFrameLoadDelegate:@selector(webView:didFailProvisionalLoadWithError:forFrame:) with:error];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    [self _tellFrameLoadDelegate:@selector(webView:didFailLoadWithError:forFrame:) with:error];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if ([keyPath isEqualToString:@"title"]) {
        NSString *title = _engine.title;
        if (title.length)
            [self _tellFrameLoadDelegate:@selector(webView:didReceiveTitle:forFrame:) with:title];
    } else if ([keyPath isEqualToString:@"estimatedProgress"]) {
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
    }
}

/* The policy delegate decides through a listener, now or later; without
   one, navigations go ahead. */
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)action
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    id<WebPolicyDelegate> d = self.policyDelegate;
    BOOL newWindow = action.targetFrame == nil;
    SEL navigationSelector = @selector(webView:decidePolicyForNavigationAction:request:frame:decisionListener:);
    SEL windowSelector = @selector(webView:decidePolicyForNewWindowAction:request:newFrameName:decisionListener:);
    if (![d respondsToSelector:newWindow ? windowSelector : navigationSelector]) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }
    NSMutableDictionary *information = [NSMutableDictionary dictionary];
    information[WebActionNavigationTypeKey] = @(legacyNavigationType(action.navigationType));
    information[WebActionButtonKey] = @(action.buttonNumber);
    information[WebActionModifierFlagsKey] = @(action.modifierFlags);
    if (action.request.URL)
        information[WebActionOriginalURLKey] = action.request.URL;
    _WebPolicyListener *listener = [_WebPolicyListener new];
    listener.decide = decisionHandler;
    if (newWindow)
        [d webView:self decidePolicyForNewWindowAction:information request:action.request newFrameName:@"" decisionListener:listener];
    else
        [d webView:self decidePolicyForNavigationAction:information request:action.request frame:_mainFrame decisionListener:listener];
}

#pragma mark The engine's UI

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler {
    id d = self.UIDelegate;
    if ([d respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:)])
        [d webView:self runJavaScriptAlertPanelWithMessage:message initiatedByFrame:_mainFrame];
    else if ([d respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:)])
        ((void (*)(id, SEL, id, id))objc_msgSend)(d, @selector(webView:runJavaScriptAlertPanelWithMessage:), self, message);
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler {
    id d = self.UIDelegate;
    BOOL result = NO;
    if ([d respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:)])
        result = [d webView:self runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:_mainFrame];
    else if ([d respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:)])
        result = ((BOOL (*)(id, SEL, id, id))objc_msgSend)(d, @selector(webView:runJavaScriptConfirmPanelWithMessage:), self, message);
    completionHandler(result);
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
    defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *))completionHandler {
    id d = self.UIDelegate;
    NSString *result = nil;
    if ([d respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:)])
        result = [d webView:self runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:_mainFrame];
    else if ([d respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:)])
        result = ((id (*)(id, SEL, id, id, id))objc_msgSend)(d, @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:),
                                                             self, prompt, defaultText);
    completionHandler(result);
}

/* A page opens a window: the delegate's new WebView, shown once WebKit
   has it (webViewShow:). */
- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration
   forNavigationAction:(WKNavigationAction *)action windowFeatures:(WKWindowFeatures *)features {
    id<WebUIDelegate> d = self.UIDelegate;
    if (![d respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        return nil;
    WebView *created = [d webView:self createWebViewWithRequest:nil];
    if (created == nil)
        return nil;
    [created _makeEngineWithConfiguration:configuration];
    dispatch_async(dispatch_get_main_queue(), ^{
        id<WebUIDelegate> createdDelegate = created.UIDelegate;
        if ([createdDelegate respondsToSelector:@selector(webViewShow:)])
            [createdDelegate webViewShow:created];
    });
    return created->_engine;
}

- (void)webViewDidClose:(WKWebView *)webView {
    id<WebUIDelegate> d = self.UIDelegate;
    if ([d respondsToSelector:@selector(webViewClose:)])
        [d webViewClose:self];
}

@end
