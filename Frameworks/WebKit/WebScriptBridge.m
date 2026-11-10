// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.
//
// The scripting bridge's application side (see WebScriptBridge.h and
// WebScriptingExtension.c for the messages). A Unix socket in the
// temporary directory takes the web processes' connections; they are
// served from GLib's main context, so a request is answered even while the
// main thread waits in one of the API's synchronous calls. Answering one
// may run script in its page: the script goes back on the same connection
// and the reply is read there, answering any request the script makes
// meanwhile.
//
// Objects of the application are kept by identifier for as long as their
// page shows (until its window object is cleared again); their methods
// and keys are the ones the WebScripting protocol allows: a class that
// does not say otherwise shows none.

#import "WebScriptBridge.h"
#import "WebScriptObjectInternal.h"
#import "WKWebViewInternal.h"
#import <WebKit/DOM.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

@interface WebView (WebScriptBridgeEngine)
- (WKWebView *)_engine;
@end

static NSString *socketPath;
static NSMapTable<NSNumber *, WebView *> *webViews;          /* page → view, weak */
static NSMutableDictionary<NSNumber *, id> *objects;           /* identifier → object */
static NSMutableDictionary<NSNumber *, NSMutableArray *> *pageObjects; /* page → identifiers */
static NSMutableDictionary<NSNumber *, NSMutableArray *> *waiting;     /* page → connections waiting */
static long nextIdentifier = 1;

#pragma mark - Messages

static BOOL writeAll(int fd, const void *data, size_t length)
{
    const char *p = data;
    while (length > 0) {
        ssize_t n = write(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return NO;
        p += n;
        length -= (size_t)n;
    }
    return YES;
}

static BOOL readAll(int fd, void *data, size_t length)
{
    char *p = data;
    while (length > 0) {
        ssize_t n = read(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return NO;
        p += n;
        length -= (size_t)n;
    }
    return YES;
}

static BOOL sendMessage(int fd, NSDictionary *message)
{
    NSData *json = [NSJSONSerialization dataWithJSONObject:message options:0 error:nil];
    uint32_t length = (uint32_t)json.length;
    return json && writeAll(fd, &length, sizeof(length)) && writeAll(fd, json.bytes, json.length);
}

static NSDictionary *receiveMessage(int fd)
{
    uint32_t length;
    if (!readAll(fd, &length, sizeof(length)) || length > 64 * 1024 * 1024)
        return nil;
    NSMutableData *json = [NSMutableData dataWithLength:length];
    if (!readAll(fd, json.mutableBytes, length))
        return nil;
    id message = [NSJSONSerialization JSONObjectWithData:json options:0 error:nil];
    return [message isKindOfClass:[NSDictionary class]] ? message : nil;
}

static NSString *jsonString(id object)
{
    NSData *json = [NSJSONSerialization dataWithJSONObject:object options:NSJSONWritingFragmentsAllowed error:nil];
    return json ? [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] : @"null";
}

#pragma mark - Objects and values

static NSNumber *keep(id object, NSNumber *page)
{
    for (NSNumber *identifier in pageObjects[page])
        if (objects[identifier] == object)
            return identifier;
    NSNumber *identifier = @(nextIdentifier++);
    objects[identifier] = object;
    if (pageObjects[page] == nil)
        pageObjects[page] = [NSMutableArray array];
    [pageObjects[page] addObject:identifier];
    return identifier;
}

static NSNumber *pageOf(WebView *webView)
{
    for (NSNumber *page in webViews)
        if ([webViews objectForKey:page] == webView)
            return page;
    return nil;
}

/* The objects of a page let go (their finalizeForWebScript called). */
static void forget(NSNumber *page)
{
    NSArray *identifiers = pageObjects[page];
    [pageObjects removeObjectForKey:page];
    for (NSNumber *identifier in identifiers) {
        id object = objects[identifier];
        [objects removeObjectForKey:identifier];
        if ([object respondsToSelector:@selector(finalizeForWebScript)])
            [object performSelector:@selector(finalizeForWebScript)];
    }
}

NSString *_WebScriptBridgeExpressionFor(WebView *webView, id object)
{
    NSNumber *page = pageOf(webView);
    if (page == nil)
        return @"undefined";
    return [NSString stringWithFormat:@"window.__minkarNative(%@)", keep(object, page)];
}

/* A page's value (as the extension encodes it) as an Objective-C one. */
static id decodeValue(NSDictionary *value, WebView *webView)
{
    NSString *type = value[@"t"];
    if ([type isEqualToString:@"string"] || [type isEqualToString:@"number"] || [type isEqualToString:@"boolean"])
        return value[@"v"];
    if ([type isEqualToString:@"u"])
        return [WebUndefined undefined];
    if ([type isEqualToString:@"native"])
        return objects[value[@"i"]];
    if ([type isEqualToString:@"o"]) {
        NSString *kind = value[@"c"];
        Class class = [WebScriptObject class];
        if ([kind isEqualToString:@"n9"]) class = [DOMDocument class];
        else if ([kind isEqualToString:@"n11"]) class = [DOMDocumentFragment class];
        else if ([kind isEqualToString:@"n1"]) class = [DOMElement class];
        else if ([kind isEqualToString:@"n3"]) class = [DOMText class];
        else if ([kind hasPrefix:@"n"]) class = [DOMNode class];
        else if ([kind isEqualToString:@"r"]) class = [DOMRange class];
        else if ([kind isEqualToString:@"i"]) class = [DOMNodeIterator class];
        WebScriptObject *object = [class new];
        object.webView = webView;
        object.expression = [NSString stringWithFormat:@"window.__webScriptObjects[%@]", value[@"i"]];
        return object;
    }
    return nil;                      /* null */
}

/* An Objective-C value for the page. */
static NSDictionary *encodeValue(id value, WebView *webView, NSNumber *page)
{
    (void)webView;
    if (value == nil || value == [NSNull null])
        return @{ @"t": @"n" };
    if (value == [WebUndefined undefined])
        return @{ @"t": @"u" };
    if ([value isKindOfClass:[NSString class]])
        return @{ @"t": @"string", @"v": value };
    if ([value isKindOfClass:[NSNumber class]]) {
        const char *type = [value objCType];
        BOOL boolean = !strcmp(type, @encode(BOOL)) || !strcmp(type, "c") || value == (id)kCFBooleanTrue || value == (id)kCFBooleanFalse;
        /* A boolean as one (KVC wraps a BOOL as a char number, which JSON
           writes as 1). */
        return boolean ? @{ @"t": @"boolean", @"v": @([value boolValue]) } : @{ @"t": @"number", @"v": value };
    }
    if ([value isKindOfClass:[WebScriptObject class]])
        return @{ @"t": @"expr", @"v": [value expression] ?: @"undefined" };
    if (([value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSDictionary class]]) &&
        [NSJSONSerialization isValidJSONObject:value])
        return @{ @"t": @"json", @"v": value };
    return @{ @"t": @"native", @"i": keep(value, page) };
}

#pragma mark - The WebScripting rules

static NSString *defaultScriptName(SEL selector)
{
    return [NSStringFromSelector(selector) stringByReplacingOccurrencesOfString:@":" withString:@"_"];
}

/* The selector a script name stands for on object's class, if its class
   shows it. */
static SEL selectorFor(id object, NSString *name)
{
    Class class = object_getClass(object);
    Class meta = object_getClass((id)class);
    BOOL hasExclusion = class_respondsToSelector(meta, @selector(isSelectorExcludedFromWebScript:));
    BOOL hasNames = class_respondsToSelector(meta, @selector(webScriptNameForSelector:));
    if (!hasExclusion)
        return NULL;
    for (Class c = class; c != Nil && c != [NSObject class]; c = class_getSuperclass(c)) {
        unsigned count = 0;
        Method *methods = class_copyMethodList(c, &count);
        SEL found = NULL;
        for (unsigned i = 0; i < count && found == NULL; i++) {
            SEL selector = method_getName(methods[i]);
            if (((BOOL (*)(id, SEL, SEL))objc_msgSend)((id)class, @selector(isSelectorExcludedFromWebScript:), selector))
                continue;
            NSString *scriptName = hasNames ?
                ((NSString *(*)(id, SEL, SEL))objc_msgSend)((id)class, @selector(webScriptNameForSelector:), selector) : nil;
            if ([scriptName ?: defaultScriptName(selector) isEqualToString:name])
                found = selector;
        }
        free(methods);
        if (found)
            return found;
    }
    return NULL;
}

/* The key a script name stands for, if the class shows it. */
static NSString *keyFor(id object, NSString *name)
{
    Class class = object_getClass(object);
    Class meta = object_getClass((id)class);
    if (!class_respondsToSelector(meta, @selector(isKeyExcludedFromWebScript:)))
        return nil;
    NSString *key = name;
    if (class_respondsToSelector(meta, @selector(webScriptNameForKey:))) {
        /* The key whose script name this is, among the class's instance
           variables and properties. */
        key = nil;
        for (Class c = class; c != Nil && key == nil && c != [NSObject class]; c = class_getSuperclass(c)) {
            unsigned count = 0;
            Ivar *ivars = class_copyIvarList(c, &count);
            for (unsigned i = 0; i < count && key == nil; i++) {
                const char *ivar = ivar_getName(ivars[i]);
                NSString *scriptName = ((NSString *(*)(id, SEL, const char *))objc_msgSend)((id)class, @selector(webScriptNameForKey:), ivar);
                if ([scriptName isEqualToString:name])
                    key = @(ivar);
            }
            free(ivars);
            objc_property_t *properties = class_copyPropertyList(c, &count);
            for (unsigned i = 0; i < count && key == nil; i++) {
                const char *property = property_getName(properties[i]);
                NSString *scriptName = ((NSString *(*)(id, SEL, const char *))objc_msgSend)((id)class, @selector(webScriptNameForKey:), property);
                if ([scriptName isEqualToString:name])
                    key = @(property);
            }
            free(properties);
        }
        if (key == nil)
            key = name;
    }
    if (((BOOL (*)(id, SEL, const char *))objc_msgSend)((id)class, @selector(isKeyExcludedFromWebScript:), key.UTF8String))
        return nil;
    return key;
}

/* object's method called with the page's arguments, by its signature. */
static NSDictionary *invoke(id object, SEL selector, NSArray *arguments, WebView *webView, NSNumber *page)
{
    NSMethodSignature *signature = [object methodSignatureForSelector:selector];
    if (signature == nil)
        return @{ @"t": @"x", @"v": @"No such method." };
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    invocation.target = object;
    invocation.selector = selector;
    NSMutableArray *held = [NSMutableArray array];
    for (NSUInteger i = 2; i < signature.numberOfArguments; i++) {
        id value = (i - 2 < arguments.count) ? decodeValue(arguments[i - 2], webView) : nil;
        const char *type = [signature getArgumentTypeAtIndex:i];
        while (strchr("rnNoORV", *type) && *type)
            type++;
        NSNumber *number = [value isKindOfClass:[NSNumber class]] ? value :
                           [value isKindOfClass:[NSString class]] ? @([value doubleValue]) : @0;
        switch (*type) {
        case '@': if (value) [held addObject:value]; [invocation setArgument:&value atIndex:(NSInteger)i]; break;
        case 'c': { char v = number.charValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'B': { bool v = number.boolValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'C': { unsigned char v = number.unsignedCharValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 's': { short v = number.shortValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'S': { unsigned short v = number.unsignedShortValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'i': { int v = number.intValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'I': { unsigned v = number.unsignedIntValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'l': { long v = number.longValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'L': { unsigned long v = number.unsignedLongValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'q': { long long v = number.longLongValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'Q': { unsigned long long v = number.unsignedLongLongValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'f': { float v = number.floatValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        case 'd': { double v = number.doubleValue; [invocation setArgument:&v atIndex:(NSInteger)i]; break; }
        default: return @{ @"t": @"x", @"v": @"A method argument has a type scripts cannot give." };
        }
    }
    @try {
        [invocation invoke];
    } @catch (id exception) {
        return @{ @"t": @"x", @"v": [exception respondsToSelector:@selector(reason)] ? [exception reason] ?: @"" : @"" };
    }
    const char *type = signature.methodReturnType;
    while (strchr("rnNoORV", *type) && *type)
        type++;
    switch (*type) {
    case 'v': return @{ @"t": @"u" };
    case '@': { __unsafe_unretained id result = nil; [invocation getReturnValue:&result]; return encodeValue(result, webView, page); }
    case 'c': { char v; [invocation getReturnValue:&v]; return @{ @"t": @"boolean", @"v": @(v != 0) }; }
    case 'B': { bool v; [invocation getReturnValue:&v]; return @{ @"t": @"boolean", @"v": @(v) }; }
    case 'C': { unsigned char v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 's': { short v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'S': { unsigned short v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'i': { int v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'I': { unsigned v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'l': { long v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'L': { unsigned long v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'q': { long long v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'Q': { unsigned long long v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'f': { float v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    case 'd': { double v; [invocation getReturnValue:&v]; return @{ @"t": @"number", @"v": @(v) }; }
    default: return @{ @"t": @"u" };
    }
}

/* The key's getter (key, or is<Key>) returns a BOOL. */
static BOOL isBooleanKey(id object, NSString *key)
{
    NSString *capitalized = key.length ? [[key substringToIndex:1].uppercaseString stringByAppendingString:[key substringFromIndex:1]] : key;
    for (NSString *name in @[ key, [@"is" stringByAppendingString:capitalized] ]) {
        SEL getter = NSSelectorFromString(name);
        if (![object respondsToSelector:getter])
            continue;
        const char *type = [object methodSignatureForSelector:getter].methodReturnType;
        return type && (*type == 'c' || *type == 'B');
    }
    return NO;
}

#pragma mark - Requests

/* A page's request answered (the reply's JSON). */
static NSString *answer(NSDictionary *request)
{
    NSNumber *page = request[@"page"];
    WebView *webView = page ? [webViews objectForKey:page] : nil;
    NSString *op = request[@"op"];
    if (webView == nil)
        return jsonString(@{ @"t": @"u" });

    if ([op isEqualToString:@"cleared"]) {
        forget(page);
        [webView _windowObjectCleared];
        return jsonString(@{ @"t": @"u" });
    }

    id object = objects[request[@"object"]];
    NSString *name = request[@"name"];
    NSArray *arguments = request[@"args"];
    if (object == nil || ![name isKindOfClass:[NSString class]])
        return jsonString(@{ @"t": @"u" });

    if ([op isEqualToString:@"get"]) {
        if (selectorFor(object, name) != NULL)
            return jsonString(@{ @"t": @"method" });
        NSString *key = keyFor(object, name);
        if (key == nil)
            return jsonString(@{ @"t": @"u" });
        @try {
            NSDictionary *value = encodeValue([object valueForKey:key], webView, page);
            /* A BOOL getter's value is a boolean (KVC gives a number). */
            if ([value[@"t"] isEqualToString:@"number"] && isBooleanKey(object, key))
                value = @{ @"t": @"boolean", @"v": @([value[@"v"] boolValue]) };
            return jsonString(value);
        } @catch (id exception) {
            return jsonString(@{ @"t": @"u" });
        }
    }
    if ([op isEqualToString:@"set"]) {
        NSString *key = keyFor(object, name);
        if (key != nil && arguments.count > 0) {
            @try {
                id value = decodeValue(arguments[0], webView);
                [object setValue:value == [WebUndefined undefined] ? nil : value forKey:key];
            } @catch (id exception) {
            }
        }
        return jsonString(@{ @"t": @"u" });
    }
    if ([op isEqualToString:@"call"]) {
        SEL selector = selectorFor(object, name);
        if (selector == NULL)
            return jsonString(@{ @"t": @"x", @"v": [NSString stringWithFormat:@"%@ is not a function.", name] });
        return jsonString(invoke(object, selector, arguments, webView, page));
    }
    return jsonString(@{ @"t": @"u" });
}

/* One request from fd answered. NO when the connection is gone. */
static BOOL serve(int fd, NSDictionary *request)
{
    NSNumber *page = request[@"page"];
    NSString *reply;
    if (page) {
        if (waiting[page] == nil)
            waiting[page] = [NSMutableArray array];
        [waiting[page] addObject:@(fd)];
    }
    @autoreleasepool {
        reply = answer(request);
    }
    if (page)
        [waiting[page] removeLastObject];
    return sendMessage(fd, @{ @"reply": reply });
}

BOOL _WebScriptBridgeEvaluate(WebView *webView, NSString *script, NSString **json)
{
    NSNumber *page = pageOf(webView);
    NSNumber *connection = page ? [waiting[page] lastObject] : nil;
    if (connection == nil)
        return NO;
    int fd = connection.intValue;
    *json = nil;
    if (!sendMessage(fd, @{ @"eval": script }))
        return YES;
    for (;;) {
        NSDictionary *message = receiveMessage(fd);
        if (message == nil)
            return YES;
        if (message[@"evalResult"]) {
            *json = message[@"evalResult"];
            return YES;
        }
        /* The script called the application meanwhile. */
        if (!serve(fd, message))
            return YES;
    }
}

static gboolean connectionReadable(gint fd, GIOCondition condition, gpointer data)
{
    (void)data;
    if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        close(fd);
        return G_SOURCE_REMOVE;
    }
    NSDictionary *request = receiveMessage(fd);
    if (request == nil || !serve(fd, request)) {
        close(fd);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean listenerReadable(gint fd, GIOCondition condition, gpointer data)
{
    (void)condition; (void)data;
    int client = accept(fd, NULL, NULL);
    if (client >= 0) {
        fcntl(client, F_SETFD, FD_CLOEXEC);
        g_unix_fd_add(client, G_IO_IN | G_IO_HUP | G_IO_ERR, connectionReadable, NULL);
    }
    return G_SOURCE_CONTINUE;
}

static void removeSocket(void)
{
    if (socketPath)
        unlink(socketPath.fileSystemRepresentation);
}

void _WebScriptBridgeStart(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        webViews = [NSMapTable strongToWeakObjectsMapTable];
        objects = [NSMutableDictionary dictionary];
        pageObjects = [NSMutableDictionary dictionary];
        waiting = [NSMutableDictionary dictionary];

        socketPath = [NSTemporaryDirectory() stringByAppendingPathComponent:
                      [NSString stringWithFormat:@"minkar-webscript-%d", getpid()]];
        struct sockaddr_un address = { .sun_family = AF_UNIX };
        if (strlen(socketPath.fileSystemRepresentation) >= sizeof(address.sun_path))
            socketPath = [NSString stringWithFormat:@"/tmp/minkar-webscript-%d", getpid()];
        strncpy(address.sun_path, socketPath.fileSystemRepresentation, sizeof(address.sun_path) - 1);
        unlink(address.sun_path);
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0 || bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(fd, 8) != 0) {
            NSLog(@"WebKit: the scripting bridge's socket cannot be made at %@", socketPath);
            if (fd >= 0)
                close(fd);
            return;
        }
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        atexit(removeSocket);
        g_unix_fd_add(fd, G_IO_IN, listenerReadable, NULL);

        /* The extension, beside WebKit.framework's executable, told where
           to connect. */
        NSString *extensions = [[NSBundle bundleForClass:[WebView class]].executablePath.stringByDeletingLastPathComponent
            stringByAppendingPathComponent:@"WebProcessExtensions"];
        WebKitWebContext *context = webkit_web_context_get_default();
        webkit_web_context_set_web_process_extensions_directory(context, extensions.fileSystemRepresentation);
        webkit_web_context_set_web_process_extensions_initialization_user_data(context,
            g_variant_new_string(socketPath.fileSystemRepresentation));
    });
}

void _WebScriptBridgeRegister(WebView *webView, guint64 pageID)
{
    [webViews setObject:webView forKey:@(pageID)];
}
