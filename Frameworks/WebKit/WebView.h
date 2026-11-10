// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The older web view API, on the same engine as WKWebView (WPE WebKit): a
// WebView shows a WKWebView of its own and turns the older delegates'
// calls into its. Its synchronous calls (stringByEvaluatingJavaScript...,
// WebScriptObject, searchFor:) wait for WebKit's processes. One frame (the
// main one) is modeled; there is no DOM API.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class WebFrame, WebDataSource, WebPreferences, WebScriptObject, WebFrameView, DOMDocument, DOMRange;

FOUNDATION_EXPORT NSString *const WebViewProgressStartedNotification;
FOUNDATION_EXPORT NSString *const WebViewProgressEstimateChangedNotification;
FOUNDATION_EXPORT NSString *const WebViewProgressFinishedNotification;
FOUNDATION_EXPORT NSString *const WebPreferencesChangedNotification;

typedef NS_ENUM(NSInteger, WebNavigationType) {
    WebNavigationTypeLinkClicked,
    WebNavigationTypeFormSubmitted,
    WebNavigationTypeBackForward,
    WebNavigationTypeReload,
    WebNavigationTypeFormResubmitted,
    WebNavigationTypeOther
};

/* The keys of a policy decision's action information. */
FOUNDATION_EXPORT NSString *const WebActionNavigationTypeKey;
FOUNDATION_EXPORT NSString *const WebActionElementKey;
FOUNDATION_EXPORT NSString *const WebActionButtonKey;
FOUNDATION_EXPORT NSString *const WebActionModifierFlagsKey;
FOUNDATION_EXPORT NSString *const WebActionOriginalURLKey;

typedef NS_ENUM(NSUInteger, WebCacheModel) {
    WebCacheModelDocumentViewer = 0,
    WebCacheModelDocumentBrowser = 1,
    WebCacheModelPrimaryWebBrowser = 2
};

@interface WebUndefined : NSObject <NSCoding, NSCopying>
+ (WebUndefined *)undefined;
@end

/* A JavaScript object of the page. Values cross as NSString, NSNumber,
   NSNull, WebUndefined and WebScriptObject (other objects); arrays and
   dictionaries given to scripts cross as JavaScript arrays and objects.
   An exception gives nil. */
@interface WebScriptObject : NSObject
- (nullable id)callWebScriptMethod:(NSString *)name withArguments:(nullable NSArray *)arguments;
- (nullable id)evaluateWebScript:(NSString *)script;
- (void)removeWebScriptKey:(NSString *)name;
- (nullable id)webScriptValueAtIndex:(unsigned)index;
- (void)setWebScriptValueAtIndex:(unsigned)index value:(nullable id)value;
@end

@interface WebPreferences : NSObject <NSCoding>
+ (WebPreferences *)standardPreferences;
- (instancetype)initWithIdentifier:(nullable NSString *)identifier;
@property (nullable, readonly, copy) NSString *identifier;
@property (copy) NSString *standardFontFamily;
@property (copy) NSString *fixedFontFamily;
@property (copy) NSString *serifFontFamily;
@property (copy) NSString *sansSerifFontFamily;
@property int defaultFontSize;
@property int defaultFixedFontSize;
@property int minimumFontSize;
@property (copy) NSString *defaultTextEncodingName;
@property BOOL userStyleSheetEnabled;
@property (nullable, strong) NSURL *userStyleSheetLocation;
@property (getter=isJavaEnabled) BOOL javaEnabled;
@property (getter=isJavaScriptEnabled) BOOL javaScriptEnabled;
@property BOOL javaScriptCanOpenWindowsAutomatically;
@property BOOL plugInsEnabled;
@property BOOL allowsAnimatedImages;
@property BOOL allowsAnimatedImageLooping;
@property BOOL loadsImagesAutomatically;
@property BOOL autosaves;
@property BOOL shouldPrintBackgrounds;
@property BOOL privateBrowsingEnabled;
@property BOOL tabsToLinks;
@property BOOL usesPageCache;
@property WebCacheModel cacheModel;
@end

@interface WebDataSource : NSObject
@property (readonly, strong) NSURLRequest *initialRequest;
@property (nullable, readonly, strong) NSURLRequest *request;
@property (nullable, readonly, strong) NSURLResponse *response;
@property (nullable, readonly, copy) NSString *pageTitle;
@property (readonly, getter=isLoading) BOOL loading;
@property (readonly, strong) WebFrame *webFrame;
@property (readonly, copy) NSData *data;
@end

@interface WebFrame : NSObject
@property (readonly, copy) NSString *name;
@property (readonly, weak) id webView;
@property (nullable, readonly, strong) WebFrame *parentFrame;
@property (readonly, copy) NSArray<WebFrame *> *childFrames;
@property (nullable, readonly, strong) WebDataSource *dataSource;
@property (nullable, readonly, strong) WebDataSource *provisionalDataSource;
@property (nullable, readonly, strong) DOMDocument *DOMDocument;
@property (readonly, strong) WebScriptObject *windowObject;
@property (nullable, readonly, strong) WebFrameView *frameView;
- (void)loadRequest:(NSURLRequest *)request;
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(nullable NSString *)encodingName baseURL:(nullable NSURL *)URL;
- (void)loadHTMLString:(NSString *)string baseURL:(nullable NSURL *)URL;
- (void)loadAlternateHTMLString:(NSString *)string baseURL:(nullable NSURL *)baseURL forUnreachableURL:(nullable NSURL *)unreachableURL;
- (void)stopLoading;
- (void)reload;
- (nullable WebFrame *)findFrameNamed:(NSString *)name;
@end

@interface WebFrameView : NSView
@property (readonly, weak) WebFrame *webFrame;
@property BOOL allowsScrolling;
@end

@protocol WebPolicyDecisionListener <NSObject>
- (void)use;
- (void)download;
- (void)ignore;
@end

@class WebView;

@protocol WebFrameLoadDelegate <NSObject>
@optional
- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didClearWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame;
@end

@protocol WebPolicyDelegate <NSObject>
@optional
- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation
        request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener;
- (void)webView:(WebView *)webView decidePolicyForNewWindowAction:(NSDictionary *)actionInformation
        request:(NSURLRequest *)request newFrameName:(NSString *)frameName decisionListener:(id<WebPolicyDecisionListener>)listener;
@end

@protocol WebUIDelegate <NSObject>
@optional
- (nullable WebView *)webView:(WebView *)sender createWebViewWithRequest:(nullable NSURLRequest *)request;
- (void)webViewShow:(WebView *)sender;
- (void)webViewClose:(WebView *)sender;
- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame;
- (BOOL)webView:(WebView *)sender runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame;
- (nullable NSString *)webView:(WebView *)sender runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
                   defaultText:(nullable NSString *)defaultText initiatedByFrame:(WebFrame *)frame;
@end

@interface WebView : NSView
+ (BOOL)canShowMIMEType:(NSString *)MIMEType;
+ (void)registerURLSchemeAsLocal:(nullable NSString *)scheme;
- (instancetype)initWithFrame:(NSRect)frame frameName:(nullable NSString *)frameName groupName:(nullable NSString *)groupName;
@property (readonly, strong) WebFrame *mainFrame;
@property (nullable, readonly, strong) WebFrame *selectedFrame;
@property (nullable, weak) id<WebFrameLoadDelegate> frameLoadDelegate;
@property (nullable, weak) id<WebPolicyDelegate> policyDelegate;
@property (nullable, weak) id<WebUIDelegate> UIDelegate;
@property (nullable, weak) id resourceLoadDelegate;
@property (nullable, weak) id downloadDelegate;
@property (nullable, weak) id editingDelegate;
@property (strong) WebPreferences *preferences;
@property (nullable, copy) NSString *preferencesIdentifier;
@property (nullable, copy) NSString *customUserAgent;
@property (nullable, copy) NSString *applicationNameForUserAgent;
@property (nullable, copy) NSString *mainFrameURL;
@property (nullable, readonly, copy) NSString *mainFrameTitle;
@property (readonly) double estimatedProgress;
@property (readonly, getter=isLoading) BOOL loading;
@property BOOL drawsBackground;
@property BOOL shouldCloseWithWindow;
@property (nullable, readonly, strong) DOMRange *selectedDOMRange;
@property (readonly, strong) WebScriptObject *windowScriptObject;
@property (readonly) BOOL canGoBack;
@property (readonly) BOOL canGoForward;
- (BOOL)goBack;
- (BOOL)goForward;
- (nullable NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script;
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
- (IBAction)takeStringURLFrom:(nullable id)sender;
- (IBAction)stopLoading:(nullable id)sender;
- (IBAction)reload:(nullable id)sender;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
@end

NS_ASSUME_NONNULL_END
