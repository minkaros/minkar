// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import <WebKit/WebView.h>
#import "WebPage.h"
#import <objc/message.h>

NSString *const WebViewProgressStartedNotification = @"WebProgressStartedNotification";
NSString *const WebViewProgressEstimateChangedNotification = @"WebProgressEstimateChangedNotification";
NSString *const WebViewProgressFinishedNotification = @"WebProgressFinishedNotification";
NSString *const WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";

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

@implementation WebScriptObject
- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)arguments { return nil; }
- (id)evaluateWebScript:(NSString *)script { return nil; }
- (void)removeWebScriptKey:(NSString *)name {}
- (id)webScriptValueAtIndex:(unsigned)index { return [WebUndefined undefined]; }
- (void)setWebScriptValueAtIndex:(unsigned)index value:(id)value {}
- (void)setValue:(id)value forKey:(NSString *)key {}
- (id)valueForKey:(NSString *)key { return [WebUndefined undefined]; }
@end

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
@end

@interface WebDataSource ()
@property (readwrite, strong) NSURLRequest *initialRequest;
@property (readwrite, strong) NSURLRequest *request;
@property (readwrite, copy) NSString *pageTitle;
@property (readwrite) BOOL loading;
@property (readwrite, strong) WebFrame *webFrame;
@property (readwrite, copy) NSData *data;
@end
@implementation WebDataSource
- (NSURLResponse *)response { return nil; }
@end

@interface WebFrame ()
@property (weak) WebView *owner;
@property (readwrite, strong) WebDataSource *dataSource;
@property (readwrite, strong) WebDataSource *provisionalDataSource;
@property (strong) _WebPage *page;
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
    if (_windowObject == nil)
        _windowObject = [WebScriptObject new];
    return _windowObject;
}
- (void)loadRequest:(NSURLRequest *)request { [self.page loadRequest:request recordHistory:YES]; }
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL {
    [self.page loadData:data MIMEType:MIMEType encodingName:encodingName baseURL:URL];
}
- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL {
    [self loadData:[string dataUsingEncoding:NSUTF8StringEncoding] MIMEType:@"text/html" textEncodingName:@"utf-8" baseURL:URL];
}
- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL {
    [self loadHTMLString:string baseURL:baseURL];
}
- (void)stopLoading { [self.page stopLoading]; }
- (void)reload { [self.page reload]; }
- (WebFrame *)findFrameNamed:(NSString *)name {
    return (name.length == 0 || [name isEqualToString:@"_self"] || [name isEqualToString:@"_top"]) ? self : nil;
}
@end

@implementation WebFrameView
@end

@interface WebView () <_WebPageClient>
@end

@implementation WebView {
    _WebPage *_page;
    WebFrame *_mainFrame;
    WebPreferences *_preferences;
}

+ (BOOL)canShowMIMEType:(NSString *)MIMEType {
    NSString *type = MIMEType.lowercaseString;
    return [type hasPrefix:@"text/"] || [type hasPrefix:@"image/"] || [type isEqualToString:@"application/xhtml+xml"];
}

+ (void)registerURLSchemeAsLocal:(NSString *)scheme {
}

- (instancetype)initWithFrame:(NSRect)frame frameName:(NSString *)frameName groupName:(NSString *)groupName {
    if ((self = [super initWithFrame:frame])) {
        _page = [[_WebPage alloc] initWithFrame:self.bounds];
        _page.client = self;
        [self addSubview:_page.scrollView];
        _mainFrame = [WebFrame new];
        _mainFrame.owner = self;
        _mainFrame.page = _page;
        _preferences = [WebPreferences standardPreferences];
        _drawsBackground = YES;
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frame {
    return [self initWithFrame:frame frameName:nil groupName:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithFrame:NSMakeRect(0, 0, 100, 100) frameName:nil groupName:nil];
}

- (WebFrame *)mainFrame { return _mainFrame; }
- (WebFrame *)selectedFrame { return _mainFrame; }
- (WebPreferences *)preferences { return _preferences; }
- (void)setPreferences:(WebPreferences *)preferences { _preferences = preferences ?: [WebPreferences standardPreferences]; }
- (NSString *)preferencesIdentifier { return _preferences.identifier; }
- (void)setPreferencesIdentifier:(NSString *)identifier { _preferences = [[WebPreferences alloc] initWithIdentifier:identifier]; }
- (NSString *)mainFrameURL { return (_page.URL ?: _page.request.URL).absoluteString; }
- (void)setMainFrameURL:(NSString *)URLString {
    NSURL *URL = URLString != nil ? [NSURL URLWithString:URLString] : nil;
    if (URL != nil)
        [_mainFrame loadRequest:[NSURLRequest requestWithURL:URL]];
}
- (NSString *)mainFrameTitle { return _page.title ?: @""; }
- (double)estimatedProgress { return _page.isLoading ? _page.estimatedProgress : 0; }
- (BOOL)isLoading { return _page.isLoading; }
- (void)setDrawsBackground:(BOOL)draws { _drawsBackground = draws; _page.drawsBackground = draws; }
- (void)_setDrawsBackground:(BOOL)draws { self.drawsBackground = draws; }
- (DOMRange *)selectedDOMRange { return nil; }
- (WebScriptObject *)windowScriptObject { return _mainFrame.windowObject; }
- (BOOL)canGoBack { return _page.historyIndex > 0; }
- (BOOL)canGoForward { return _page.historyIndex + 1 < (NSInteger)_page.history.count; }
- (BOOL)goBack { return self.canGoBack && [_page goToHistoryIndex:_page.historyIndex - 1]; }
- (BOOL)goForward { return self.canGoForward && [_page goToHistoryIndex:_page.historyIndex + 1]; }
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script { return @""; }

/* The text after (or before) the selection, selected and shown. */
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag {
    NSTextView *text = _page.textView;
    NSString *all = text.string;
    NSRange selection = text.selectedRange;
    NSStringCompareOptions options = (caseFlag ? 0 : NSCaseInsensitiveSearch) | (forward ? 0 : NSBackwardsSearch);
    NSRange range = forward ? NSMakeRange(NSMaxRange(selection), all.length - NSMaxRange(selection)) : NSMakeRange(0, selection.location);
    NSRange found = string.length ? [all rangeOfString:string options:options range:range] : NSMakeRange(NSNotFound, 0);
    if (found.location == NSNotFound && wrapFlag && string.length)
        found = [all rangeOfString:string options:options];
    if (found.location == NSNotFound)
        return NO;
    text.selectedRange = found;
    [text scrollRangeToVisible:found];
    return YES;
}

- (IBAction)takeStringURLFrom:(id)sender { self.mainFrameURL = [sender stringValue]; }
- (IBAction)stopLoading:(id)sender { [_mainFrame stopLoading]; }
- (IBAction)reload:(id)sender { [_mainFrame reload]; }
- (IBAction)goBack:(id)sender { [self goBack]; }
- (IBAction)goForward:(id)sender { [self goForward]; }

- (WebDataSource *)_dataSourceLoading:(BOOL)loading {
    WebDataSource *source = [WebDataSource new];
    source.initialRequest = _page.request;
    source.request = _page.request;
    source.pageTitle = _page.title;
    source.loading = loading;
    source.webFrame = _mainFrame;
    return source;
}

- (void)pageDidStartLoad:(_WebPage *)page {
    _mainFrame.provisionalDataSource = [self _dataSourceLoading:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:self];
    id d = self.frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didStartProvisionalLoadForFrame:)])
        ((void (*)(id, SEL, id, id))objc_msgSend)(d, @selector(webView:didStartProvisionalLoadForFrame:), self, _mainFrame);
}

- (void)pageDidCommitLoad:(_WebPage *)page {
    _mainFrame.dataSource = _mainFrame.provisionalDataSource;
    _mainFrame.provisionalDataSource = nil;
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
    id d = self.frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didCommitLoadForFrame:)])
        ((void (*)(id, SEL, id, id))objc_msgSend)(d, @selector(webView:didCommitLoadForFrame:), self, _mainFrame);
}

- (void)pageDidFinishLoad:(_WebPage *)page {
    _mainFrame.dataSource = [self _dataSourceLoading:NO];
    id d = self.frameLoadDelegate;
    if (page.title.length && [d respondsToSelector:@selector(webView:didReceiveTitle:forFrame:)])
        ((void (*)(id, SEL, id, id, id))objc_msgSend)(d, @selector(webView:didReceiveTitle:forFrame:), self, page.title, _mainFrame);
    if ([d respondsToSelector:@selector(webView:didFinishLoadForFrame:)])
        ((void (*)(id, SEL, id, id))objc_msgSend)(d, @selector(webView:didFinishLoadForFrame:), self, _mainFrame);
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)page:(_WebPage *)page didFailWithError:(NSError *)error {
    id d = self.frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didFailProvisionalLoadWithError:forFrame:)])
        ((void (*)(id, SEL, id, id, id))objc_msgSend)(d, @selector(webView:didFailProvisionalLoadWithError:forFrame:), self, error, _mainFrame);
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

/* The policy delegate decides with a listener; without one, links are
   followed. */
- (BOOL)page:(_WebPage *)page shouldFollowLink:(NSURL *)url {
    return YES;
}

@end
