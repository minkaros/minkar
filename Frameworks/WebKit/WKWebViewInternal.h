// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// What WebKit.framework's own classes (the older WebView) use of
// WKWebView: its WPE WebKit view, and configurations that relate a new
// view to the one whose page opened it.

#import <WebKit/WKWebView.h>
#include <wpe/webkit.h>

NS_ASSUME_NONNULL_BEGIN

@interface WKWebView (WebKitInternal)
- (WebKitWebView *)_webKitView;
- (void)_setDrawsBackground:(BOOL)draws;
- (BOOL)_drawsBackground;
@end

@interface WKWebViewConfiguration (WebKitInternal)
@property (nullable, weak) WKWebView *_relatedWebView;
@end

NS_ASSUME_NONNULL_END
