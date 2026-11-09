// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import <WebKit/WKWebView.h>
#import "WebPage.h"

NSString *const WKErrorDomain = @"WKErrorDomain";

#define SECURE \
   + (BOOL)supportsSecureCoding { return YES; } \
   - (void)encodeWithCoder:(NSCoder *)coder {} \
   - (instancetype)initWithCoder:(NSCoder *)coder { return [self init]; }

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

@implementation WKScriptMessage
@end

/* Scripts are kept; with no JavaScript engine they do not run and no
   message comes from a page. */
@implementation WKUserContentController {
    NSMutableArray *_scripts;
    NSMutableDictionary *_handlers;
}
SECURE
- (instancetype)init {
    if ((self = [super init])) {
        _scripts = [NSMutableArray array];
        _handlers = [NSMutableDictionary dictionary];
    }
    return self;
}
- (NSArray *)userScripts { return [_scripts copy]; }
- (void)addUserScript:(WKUserScript *)userScript { [_scripts addObject:userScript]; }
- (void)removeAllUserScripts { [_scripts removeAllObjects]; }
- (void)addScriptMessageHandler:(id<WKScriptMessageHandler>)handler name:(NSString *)name {
    if (_handlers[name] != nil)
        [NSException raise:NSInvalidArgumentException format:@"a script message handler named %@ already exists", name];
    _handlers[name] = handler;
}
- (void)removeScriptMessageHandlerForName:(NSString *)name { [_handlers removeObjectForKey:name]; }
@end

@implementation WKProcessPool
SECURE
@end

@implementation WKWebsiteDataStore {
    BOOL _persistent;
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
    return copy;
}
@end

@implementation WKNavigation
@end

@interface WKFrameInfo ()
@property (readwrite, copy) NSURLRequest *request;
@end
@implementation WKFrameInfo
- (BOOL)isMainFrame { return YES; }
- (id)copyWithZone:(NSZone *)zone { return self; }
@end

@interface WKNavigationAction ()
@property (readwrite, copy) WKFrameInfo *sourceFrame;
@property (readwrite, copy) WKFrameInfo *targetFrame;
@property (readwrite) WKNavigationType navigationType;
@property (readwrite, copy) NSURLRequest *request;
@end
@implementation WKNavigationAction
- (NSEventModifierFlags)modifierFlags { return 0; }
- (NSInteger)buttonNumber { return 0; }
@end

@interface WKBackForwardListItem ()
@property (readwrite, copy) NSURL *URL;
@property (readwrite, copy) NSString *title;
@end
@implementation WKBackForwardListItem
- (NSURL *)initialURL { return self.URL; }
@end

@interface WKBackForwardList ()
@property (weak) _WebPage *page;
@end
@implementation WKBackForwardList
static WKBackForwardListItem *itemFor(NSDictionary *entry) {
    WKBackForwardListItem *item = [WKBackForwardListItem new];
    item.URL = entry[@"url"];
    item.title = [entry[@"title"] length] ? entry[@"title"] : nil;
    return item;
}
- (NSArray *)itemsInRange:(NSRange)range {
    NSMutableArray *items = [NSMutableArray array];
    NSArray *history = self.page.history;
    for (NSUInteger i = range.location; i < NSMaxRange(range) && i < history.count; i++)
        [items addObject:itemFor(history[i])];
    return items;
}
- (WKBackForwardListItem *)currentItem {
    NSInteger i = self.page.historyIndex;
    return i >= 0 ? itemFor(self.page.history[i]) : nil;
}
- (WKBackForwardListItem *)backItem {
    NSInteger i = self.page.historyIndex - 1;
    return i >= 0 ? itemFor(self.page.history[i]) : nil;
}
- (WKBackForwardListItem *)forwardItem {
    NSInteger i = self.page.historyIndex + 1;
    return i < (NSInteger)self.page.history.count ? itemFor(self.page.history[i]) : nil;
}
- (NSArray *)backList { return [self itemsInRange:NSMakeRange(0, MAX(self.page.historyIndex, 0))]; }
- (NSArray *)forwardList {
    NSInteger from = self.page.historyIndex + 1;
    return [self itemsInRange:NSMakeRange((NSUInteger)from, self.page.history.count - (NSUInteger)MIN(from, (NSInteger)self.page.history.count))];
}
@end

@interface WKWebView () <_WebPageClient>
@end

@implementation WKWebView {
    _WebPage *_page;
    WKWebViewConfiguration *_configuration;
    WKBackForwardList *_backForwardList;
    WKNavigation *_navigation;
}

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration {
    if ((self = [super initWithFrame:frame])) {
        _configuration = [configuration copy] ?: [WKWebViewConfiguration new];
        _page = [[_WebPage alloc] initWithFrame:self.bounds];
        _page.client = self;
        [self addSubview:_page.scrollView];
        _backForwardList = [WKBackForwardList new];
        _backForwardList.page = _page;
        _magnification = 1;
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frame {
    return [self initWithFrame:frame configuration:[WKWebViewConfiguration new]];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:[WKWebViewConfiguration new]];
}

- (WKWebViewConfiguration *)configuration { return [_configuration copy]; }
- (WKBackForwardList *)backForwardList { return _backForwardList; }
- (NSString *)title { return _page.title; }
- (NSURL *)URL { return _page.URL ?: _page.request.URL; }
- (BOOL)isLoading { return _page.isLoading; }
- (double)estimatedProgress { return _page.estimatedProgress; }
- (BOOL)canGoBack { return _page.historyIndex > 0; }
- (BOOL)canGoForward { return _page.historyIndex + 1 < (NSInteger)_page.history.count; }
- (NSString *)_text { return [_page text]; }

- (void)_setDrawsBackground:(BOOL)draws { _page.drawsBackground = draws; }
- (BOOL)_drawsBackground { return _page.drawsBackground; }

/* Asks the delegate, then loads. */
- (WKNavigation *)_navigate:(NSURLRequest *)request type:(WKNavigationType)type load:(void (^)(void))load {
    WKNavigation *navigation = [WKNavigation new];
    id<WKNavigationDelegate> delegate = self.navigationDelegate;
    if ([delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)]) {
        WKNavigationAction *action = [WKNavigationAction new];
        WKFrameInfo *frame = [WKFrameInfo new];
        frame.request = request;
        action.request = request;
        action.targetFrame = frame;
        action.navigationType = type;
        __block BOOL decided = NO, allowed = NO;
        [delegate webView:self decidePolicyForNavigationAction:action decisionHandler:^(WKNavigationActionPolicy policy) {
            decided = YES;
            allowed = (policy == WKNavigationActionPolicyAllow);
        }];
        if (decided && !allowed)
            return nil;
    }
    _navigation = navigation;
    load();
    return navigation;
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request {
    return [self _navigate:request type:WKNavigationTypeOther load:^{ [self->_page loadRequest:request recordHistory:YES]; }];
}

- (WKNavigation *)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL {
    return [self loadRequest:[NSURLRequest requestWithURL:URL]];
}

- (WKNavigation *)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL {
    return [self loadData:[string dataUsingEncoding:NSUTF8StringEncoding] MIMEType:@"text/html" characterEncodingName:@"utf-8" baseURL:baseURL];
}

- (WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL {
    NSURLRequest *request = [NSURLRequest requestWithURL:baseURL ?: [NSURL URLWithString:@"about:blank"]];
    return [self _navigate:request type:WKNavigationTypeOther load:^{
        [self->_page loadData:data MIMEType:MIMEType encodingName:characterEncodingName baseURL:baseURL];
    }];
}

- (WKNavigation *)goBack {
    if (!self.canGoBack) return nil;
    _navigation = [WKNavigation new];
    [_page goToHistoryIndex:_page.historyIndex - 1];
    return _navigation;
}

- (WKNavigation *)goForward {
    if (!self.canGoForward) return nil;
    _navigation = [WKNavigation new];
    [_page goToHistoryIndex:_page.historyIndex + 1];
    return _navigation;
}

- (WKNavigation *)reload {
    _navigation = [WKNavigation new];
    [_page reload];
    return _navigation;
}

- (void)stopLoading { [_page stopLoading]; }
- (IBAction)goBack:(id)sender { [self goBack]; }
- (IBAction)goForward:(id)sender { [self goForward]; }
- (IBAction)reload:(id)sender { [self reload]; }
- (IBAction)stopLoading:(id)sender { [self stopLoading]; }

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    NSError *error = [NSError errorWithDomain:WKErrorDomain code:WKErrorJavaScriptExceptionOccurred
        userInfo:@{ NSLocalizedDescriptionKey: @"JavaScript is not available." }];
    dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(nil, error); });
}

- (void)pageDidStartLoad:(_WebPage *)page {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if ([d respondsToSelector:@selector(webView:didStartProvisionalNavigation:)])
        [d webView:self didStartProvisionalNavigation:_navigation];
}

- (void)pageDidCommitLoad:(_WebPage *)page {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if ([d respondsToSelector:@selector(webView:didCommitNavigation:)])
        [d webView:self didCommitNavigation:_navigation];
}

- (void)pageDidFinishLoad:(_WebPage *)page {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if ([d respondsToSelector:@selector(webView:didFinishNavigation:)])
        [d webView:self didFinishNavigation:_navigation];
}

- (void)page:(_WebPage *)page didFailWithError:(NSError *)error {
    id<WKNavigationDelegate> d = self.navigationDelegate;
    if ([d respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)])
        [d webView:self didFailProvisionalNavigation:_navigation withError:error];
}

- (BOOL)page:(_WebPage *)page shouldFollowLink:(NSURL *)url {
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    return [self _navigate:request type:WKNavigationTypeLinkActivated load:^{}] != nil;
}

@end
