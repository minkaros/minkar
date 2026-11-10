// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The application's side of the older WebView's scripting bridge (the
// page's side is WebScriptingExtension.c): Objective-C objects shown to
// pages, under the WebScripting protocol's rules, and the window object
// cleared before a page's scripts run.

#import <WebKit/WebView.h>
#include <glib.h>

NS_ASSUME_NONNULL_BEGIN

/* Before the first web view: WebKit's web processes load the extension,
   which connects to this process. */
void _WebScriptBridgeStart(void);

/* The WebView whose engine shows the page with this identifier. */
void _WebScriptBridgeRegister(WebView *webView, guint64 pageID);

/* While the page of webView waits for this process (a request of its is
   being answered), script runs there at once: its value as JSON. NO when
   the page is not waiting. */
BOOL _WebScriptBridgeEvaluate(WebView *webView, NSString *script, NSString *_Nullable *_Nonnull json);

/* An object of the application as a JavaScript expression naming its
   proxy in the page. */
NSString *_WebScriptBridgeExpressionFor(WebView *webView, id object);

@interface WebView (WebScriptBridge)
/* The window object was cleared (the frame load delegate is told). */
- (void)_windowObjectCleared;
@end

NS_ASSUME_NONNULL_END
