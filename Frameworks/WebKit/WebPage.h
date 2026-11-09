// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// What WKWebView and WebView share: a scrolling text view showing the
// page, its loading (file, data and HTTP URLs, and given data) and
// history.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class _WebPage;

@protocol _WebPageClient <NSObject>
- (void)pageDidStartLoad:(_WebPage *)page;
- (void)pageDidCommitLoad:(_WebPage *)page;
- (void)pageDidFinishLoad:(_WebPage *)page;
- (void)page:(_WebPage *)page didFailWithError:(NSError *)error;
/* A link was clicked: YES to follow it. */
- (BOOL)page:(_WebPage *)page shouldFollowLink:(NSURL *)url;
@end

@interface _WebPage : NSObject <NSTextViewDelegate>
@property (nullable, weak) id<_WebPageClient> client;
@property (readonly) NSScrollView *scrollView;
@property (readonly) NSTextView *textView;
@property (nullable, readonly, copy) NSURL *URL;
@property (nullable, readonly, copy) NSString *title;
@property (nullable, readonly, copy) NSURLRequest *request;
@property (readonly, getter=isLoading) BOOL loading;
@property (readonly) double estimatedProgress;
@property (readonly) NSArray<NSDictionary *> *history;      /* {url, title} */
@property (readonly) NSInteger historyIndex;
@property BOOL drawsBackground;

- (instancetype)initWithFrame:(NSRect)frame;
- (void)loadRequest:(NSURLRequest *)request recordHistory:(BOOL)record;
- (void)loadData:(NSData *)data MIMEType:(nullable NSString *)MIMEType encodingName:(nullable NSString *)encoding baseURL:(nullable NSURL *)baseURL;
- (void)stopLoading;
- (void)reload;
- (BOOL)goToHistoryIndex:(NSInteger)index;
- (NSString *)text;
@end

NS_ASSUME_NONNULL_END
