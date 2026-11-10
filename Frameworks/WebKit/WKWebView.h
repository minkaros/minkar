// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Web views with the API's classes, on WPE WebKit: WebKit's own processes
// load and draw pages, JavaScript runs. Frames reach the view through
// shared memory (IOSurfaces later).

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class WKWebView, WKNavigation, WKUserScript, WKScriptMessage, WKUserContentController;

FOUNDATION_EXPORT NSString *const WKErrorDomain;

typedef NS_ENUM(NSInteger, WKErrorCode) {
    WKErrorUnknown = 1,
    WKErrorWebContentProcessTerminated,
    WKErrorWebViewInvalidated,
    WKErrorJavaScriptExceptionOccurred,
    WKErrorJavaScriptResultTypeIsUnsupported
};

typedef NS_ENUM(NSInteger, WKUserScriptInjectionTime) {
    WKUserScriptInjectionTimeAtDocumentStart,
    WKUserScriptInjectionTimeAtDocumentEnd
};

typedef NS_ENUM(NSInteger, WKNavigationActionPolicy) {
    WKNavigationActionPolicyCancel,
    WKNavigationActionPolicyAllow
};

typedef NS_ENUM(NSInteger, WKNavigationType) {
    WKNavigationTypeLinkActivated,
    WKNavigationTypeFormSubmitted,
    WKNavigationTypeBackForward,
    WKNavigationTypeReload,
    WKNavigationTypeFormResubmitted,
    WKNavigationTypeOther = -1
};

@interface WKPreferences : NSObject <NSSecureCoding>
@property CGFloat minimumFontSize;
@property BOOL javaScriptEnabled;
@property BOOL javaScriptCanOpenWindowsAutomatically;
@property BOOL javaEnabled;
@property BOOL plugInsEnabled;
@property BOOL tabFocusesLinks;
@end

@interface WKUserScript : NSObject <NSCopying>
@property (readonly, copy) NSString *source;
@property (readonly) WKUserScriptInjectionTime injectionTime;
@property (readonly, getter=isForMainFrameOnly) BOOL forMainFrameOnly;
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly;
@end

@interface WKScriptMessage : NSObject
@property (readonly, copy) id body;
@property (nullable, readonly, weak) WKWebView *webView;
@property (readonly, copy) NSString *name;
@end

@protocol WKScriptMessageHandler <NSObject>
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message;
@end

@interface WKUserContentController : NSObject <NSSecureCoding>
@property (readonly, copy) NSArray<WKUserScript *> *userScripts;
- (void)addUserScript:(WKUserScript *)userScript;
- (void)removeAllUserScripts;
- (void)addScriptMessageHandler:(id<WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name;
- (void)removeScriptMessageHandlerForName:(NSString *)name;
@end

@interface WKProcessPool : NSObject <NSSecureCoding>
@end

@interface WKWebsiteDataStore : NSObject <NSSecureCoding>
+ (WKWebsiteDataStore *)defaultDataStore;
+ (WKWebsiteDataStore *)nonPersistentDataStore;
@property (readonly, getter=isPersistent) BOOL persistent;
@end

@interface WKWebViewConfiguration : NSObject <NSCopying, NSSecureCoding>
@property (strong) WKProcessPool *processPool;
@property (strong) WKPreferences *preferences;
@property (strong) WKUserContentController *userContentController;
@property (strong) WKWebsiteDataStore *websiteDataStore;
@property (nullable, copy) NSString *applicationNameForUserAgent;
@property BOOL suppressesIncrementalRendering;
@end

@interface WKNavigation : NSObject
@end

@interface WKFrameInfo : NSObject <NSCopying>
@property (readonly, getter=isMainFrame) BOOL mainFrame;
@property (readonly, copy) NSURLRequest *request;
@end

@interface WKNavigationAction : NSObject
@property (nullable, readonly, copy) WKFrameInfo *sourceFrame;
@property (nullable, readonly, copy) WKFrameInfo *targetFrame;
@property (readonly) WKNavigationType navigationType;
@property (readonly, copy) NSURLRequest *request;
@property (readonly) NSEventModifierFlags modifierFlags;
@property (readonly) NSInteger buttonNumber;
@end

@protocol WKNavigationDelegate <NSObject>
@optional
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler;
- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(null_unspecified WKNavigation *)navigation;
- (void)webView:(WKWebView *)webView didCommitNavigation:(null_unspecified WKNavigation *)navigation;
- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation;
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error;
- (void)webView:(WKWebView *)webView didFailNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error;
@end

/* What a page asked of a window it opens; nil where it said nothing. */
@interface WKWindowFeatures : NSObject
@property (nullable, readonly) NSNumber *menuBarVisibility;
@property (nullable, readonly) NSNumber *statusBarVisibility;
@property (nullable, readonly) NSNumber *toolbarsVisibility;
@property (nullable, readonly) NSNumber *allowsResizing;
@property (nullable, readonly) NSNumber *x;
@property (nullable, readonly) NSNumber *y;
@property (nullable, readonly) NSNumber *width;
@property (nullable, readonly) NSNumber *height;
@end

@protocol WKUIDelegate <NSObject>
@optional
/* The web view must be made with the configuration given. */
- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration
            forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures;
- (void)webViewDidClose:(WKWebView *)webView;
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler;
- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler;
- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
    defaultText:(nullable NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame
    completionHandler:(void (^)(NSString *_Nullable result))completionHandler;
@end

@interface WKBackForwardListItem : NSObject
@property (readonly, copy) NSURL *URL;
@property (nullable, readonly, copy) NSString *title;
@property (readonly, copy) NSURL *initialURL;
@end

@interface WKBackForwardList : NSObject
@property (nullable, readonly, strong) WKBackForwardListItem *currentItem;
@property (nullable, readonly, strong) WKBackForwardListItem *backItem;
@property (nullable, readonly, strong) WKBackForwardListItem *forwardItem;
@property (readonly, copy) NSArray<WKBackForwardListItem *> *backList;
@property (readonly, copy) NSArray<WKBackForwardListItem *> *forwardList;
@end

@interface WKWebView : NSView
@property (readonly, copy) WKWebViewConfiguration *configuration;
@property (nullable, weak) id<WKNavigationDelegate> navigationDelegate;
@property (nullable, weak) id<WKUIDelegate> UIDelegate;
@property (readonly, strong) WKBackForwardList *backForwardList;
@property (nullable, readonly, copy) NSString *title;
@property (nullable, readonly, copy) NSURL *URL;
@property (readonly, getter=isLoading) BOOL loading;
@property (readonly) double estimatedProgress;
@property (readonly) BOOL canGoBack;
@property (readonly) BOOL canGoForward;
@property (nullable, copy) NSString *customUserAgent;
@property BOOL allowsBackForwardNavigationGestures;
@property BOOL allowsMagnification;
@property CGFloat magnification;

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration NS_DESIGNATED_INITIALIZER;
- (nullable WKNavigation *)loadRequest:(NSURLRequest *)request;
- (nullable WKNavigation *)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL;
- (nullable WKNavigation *)loadHTMLString:(NSString *)string baseURL:(nullable NSURL *)baseURL;
- (nullable WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName
                            baseURL:(NSURL *)baseURL;
- (nullable WKNavigation *)goBack;
- (nullable WKNavigation *)goForward;
- (nullable WKNavigation *)reload;
- (void)stopLoading;
- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^_Nullable)(_Nullable id, NSError *_Nullable error))completionHandler;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
- (IBAction)reload:(nullable id)sender;
- (IBAction)stopLoading:(nullable id)sender;
/* The page's text, waiting for it (for searches and tests). */
- (NSString *)_text;
/* The last frame drawn, nil before the first (for tests and snapshots). */
- (nullable NSBitmapImageRep *)_lastFrame;
@end

NS_ASSUME_NONNULL_END
