// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// What the DOM classes (DOM.m) use of WebScriptObject: the object's
// JavaScript expression, and running a body of statements on it.

#import <WebKit/WebView.h>

NS_ASSUME_NONNULL_BEGIN

@interface WebScriptObject (WebKitInternal)
@property (nullable, weak) WebView *webView;
/* The JavaScript expression naming the object. */
@property (copy) NSString *expression;
/* Runs body (statements, ending in "return r;" for a value) and gives the
   value back as WebScriptObject's rules say (DOM objects as their DOM
   class); nil on an exception. */
- (nullable id)_run:(NSString *)body;
@end

/* A string as a JavaScript string literal. */
NSString *_WebScriptLiteral(NSString *_Nullable string);

NS_ASSUME_NONNULL_END
