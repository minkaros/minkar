/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * WebKit.framework's web process extension: the page's side of the older
 * WebView's scripting bridge (Objective-C objects in JavaScript, and the
 * window object cleared before the page's scripts run).
 *
 * When the main frame's window object is cleared, it defines a native
 * function, __minkarWebScript (a request as JSON in, the reply as JSON
 * out), and the code that shows the application's objects to the page as
 * proxies whose properties and methods go through it; then it tells the
 * application (which calls its frame load delegate's
 * webView:didClearWindowObject:forFrame: then, before any of the page's
 * scripts). Requests go over a Unix socket to the application (its path
 * is the extension's initialization data), synchronously: while waiting
 * for a reply, the extension runs scripts the application sends it on
 * that connection (a delegate reading or changing the page).
 *
 * Messages are a 4-byte length (host order) and UTF-8 JSON:
 *   to the application:   {"op": ..., "page": ..., ...}  a request
 *                          {"evalResult": "..."}          a script's value
 *   from the application: {"reply": "..."}                a request's reply
 *                          {"eval": "..."}                 a script to run
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wpe/webkit-web-process-extension.h>

static char *socketPath;
static int connection = -1;

static gboolean writeAll(int fd, const void *data, size_t length)
{
    const char *p = data;
    while (length > 0) {
        ssize_t n = write(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return FALSE;
        p += n;
        length -= (size_t)n;
    }
    return TRUE;
}

static gboolean readAll(int fd, void *data, size_t length)
{
    char *p = data;
    while (length > 0) {
        ssize_t n = read(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return FALSE;
        p += n;
        length -= (size_t)n;
    }
    return TRUE;
}

static gboolean sendMessage(const char *json)
{
    uint32_t length = (uint32_t)strlen(json);
    return writeAll(connection, &length, sizeof(length)) && writeAll(connection, json, length);
}

static char *receiveMessage(void)
{
    uint32_t length;
    if (!readAll(connection, &length, sizeof(length)) || length > 64 * 1024 * 1024)
        return NULL;
    char *json = g_malloc(length + 1);
    if (!readAll(connection, json, length)) {
        g_free(json);
        return NULL;
    }
    json[length] = 0;
    return json;
}

static gboolean connectToApplication(void)
{
    if (connection >= 0)
        return TRUE;
    if (socketPath == NULL)
        return FALSE;
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    if (strlen(socketPath) >= sizeof(address.sun_path))
        return FALSE;
    strcpy(address.sun_path, socketPath);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return FALSE;
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return FALSE;
    }
    connection = fd;
    return TRUE;
}

static void dropConnection(void)
{
    if (connection >= 0)
        close(connection);
    connection = -1;
}

/* A string member of a JSON object (JSC parses it). */
static char *member(JSCContext *context, const char *json, const char *name)
{
    JSCValue *object = jsc_value_new_from_json(context, json);
    char *result = NULL;
    if (object && jsc_value_is_object(object) && jsc_value_object_has_property(object, name)) {
        JSCValue *value = jsc_value_object_get_property(object, name);
        if (jsc_value_is_string(value))
            result = jsc_value_to_string(value);
        g_object_unref(value);
    }
    if (object)
        g_object_unref(object);
    return result;
}

/* A script the application sends, run in the page: its value as JSON. */
static char *runScript(JSCContext *context, const char *script)
{
    JSCValue *value = jsc_context_evaluate(context, script, -1);
    JSCException *exception = jsc_context_get_exception(context);
    char *json = NULL;
    if (exception) {
        jsc_context_clear_exception(context);
    } else if (value) {
        json = jsc_value_to_json(value, 0);
    }
    if (value)
        g_object_unref(value);
    return json ? json : g_strdup("null");
}

/* __minkarWebScript(request): the application's reply, running what it
   sends to run meanwhile. */
static char *request(const char *json, gpointer data)
{
    (void)data;
    JSCContext *context = jsc_context_get_current();
    if (!connectToApplication() || !sendMessage(json)) {
        dropConnection();
        return g_strdup("{\"t\":\"x\",\"v\":\"The application does not answer.\"}");
    }
    for (;;) {
        char *message = receiveMessage();
        if (message == NULL) {
            dropConnection();
            return g_strdup("{\"t\":\"x\",\"v\":\"The application does not answer.\"}");
        }
        char *script = member(context, message, "eval");
        if (script) {
            char *value = runScript(context, script);
            JSCValue *wrapped = jsc_value_new_object(context, NULL, NULL);
            JSCValue *string = jsc_value_new_string(context, value);
            jsc_value_object_set_property(wrapped, "evalResult", string);
            char *reply = jsc_value_to_json(wrapped, 0);
            gboolean sent = sendMessage(reply);
            g_free(reply);
            g_object_unref(string);
            g_object_unref(wrapped);
            g_free(value);
            g_free(script);
            g_free(message);
            if (!sent) {
                dropConnection();
                return g_strdup("{\"t\":\"x\",\"v\":\"The application does not answer.\"}");
            }
            continue;
        }
        char *reply = member(context, message, "reply");
        g_free(message);
        return reply ? reply : g_strdup("{\"t\":\"u\"}");
    }
}

/* The page's side, run when the window object is cleared: PAGE is the
   page's identifier. Objects of the application are proxies; a value
   crosses as {t: type, v: value}, a page object as its index in
   window.__webScriptObjects (with its kind), an application object as
   its identifier. */
static const char *pageScript =
    "(function (native, page) {\n"
    "  var proxies = {};\n"
    "  function table() { return window.__webScriptObjects || (window.__webScriptObjects = []); }\n"
    "  function kind(v) {\n"
    "    if (typeof Node !== 'undefined' && v instanceof Node) return 'n' + v.nodeType;\n"
    "    if (typeof Range !== 'undefined' && v instanceof Range) return 'r';\n"
    "    if (typeof NodeIterator !== 'undefined' && v instanceof NodeIterator) return 'i';\n"
    "    return 'o';\n"
    "  }\n"
    "  function encode(v) {\n"
    "    if (v === undefined) return { t: 'u' };\n"
    "    if (v === null) return { t: 'n' };\n"
    "    var k = typeof v;\n"
    "    if (k === 'string' || k === 'number' || k === 'boolean') return { t: k, v: v };\n"
    "    if (v.__minkarNativeId__ !== undefined) return { t: 'native', i: v.__minkarNativeId__ };\n"
    "    var t = table(), at = t.indexOf(v);\n"
    "    if (at < 0) { t.push(v); at = t.length - 1; }\n"
    "    return { t: 'o', i: at, c: kind(v) };\n"
    "  }\n"
    "  function decode(r) {\n"
    "    switch (r && r.t) {\n"
    "    case 'n': return null;\n"
    "    case 'string': case 'number': case 'boolean': case 'json': return r.v;\n"
    "    case 'o': return table()[r.i];\n"
    "    case 'expr': return (0, eval)(r.v);\n"
    "    case 'native': return proxy(r.i);\n"
    "    case 'method': return r;\n"
    "    case 'x': throw new Error(r.v || 'An Objective-C exception occurred.');\n"
    "    }\n"
    "    return undefined;\n"
    "  }\n"
    "  function send(op, id, name, args) {\n"
    "    return decode(JSON.parse(native(JSON.stringify({ op: op, page: page, object: id, name: name,\n"
    "                                                      args: (args || []).map(encode) }))));\n"
    "  }\n"
    "  function proxy(id) {\n"
    "    if (proxies[id]) return proxies[id];\n"
    "    var methods = {};\n"
    "    var p = new Proxy({}, {\n"
    "      get: function (target, name) {\n"
    "        if (name === '__minkarNativeId__') return id;\n"
    "        if (typeof name !== 'string' || name === 'then' || name === 'toJSON') return undefined;\n"
    "        if (methods[name]) return methods[name];\n"
    "        var r = send('get', id, name);\n"
    "        if (r && r.t === 'method') {\n"
    "          methods[name] = function () { return send('call', id, name, Array.prototype.slice.call(arguments)); };\n"
    "          return methods[name];\n"
    "        }\n"
    "        return r;\n"
    "      },\n"
    "      set: function (target, name, value) {\n"
    "        if (typeof name === 'string') send('set', id, name, [value]);\n"
    "        return true;\n"
    "      }\n"
    "    });\n"
    "    proxies[id] = p;\n"
    "    return p;\n"
    "  }\n"
    "  Object.defineProperty(window, '__minkarNative', { value: proxy, configurable: true });\n"
    "  send('cleared', 0, '', []);\n"
    "})(window.__minkarWebScript, %" G_GUINT64_FORMAT ");\n";

static void windowObjectCleared(WebKitScriptWorld *world, WebKitWebPage *page, WebKitFrame *frame, gpointer data)
{
    (void)data;
    if (!webkit_frame_is_main_frame(frame))
        return;
    JSCContext *context = webkit_frame_get_js_context_for_script_world(frame, world);
    JSCValue *function = jsc_value_new_function(context, "__minkarWebScript", G_CALLBACK(request), NULL, NULL,
                                                G_TYPE_STRING, 1, G_TYPE_STRING);
    JSCValue *global = jsc_context_get_global_object(context);
    jsc_value_object_define_property_data(global, "__minkarWebScript", JSC_VALUE_PROPERTY_CONFIGURABLE, function);
    char *script = g_strdup_printf(pageScript, webkit_web_page_get_id(page));
    JSCValue *result = jsc_context_evaluate(context, script, -1);
    if (jsc_context_get_exception(context))
        jsc_context_clear_exception(context);
    g_free(script);
    if (result)
        g_object_unref(result);
    g_object_unref(global);
    g_object_unref(function);
    g_object_unref(context);
}

G_MODULE_EXPORT void webkit_web_process_extension_initialize_with_user_data(WebKitWebProcessExtension *extension,
                                                                            GVariant *data)
{
    (void)extension;
    if (data && g_variant_is_of_type(data, G_VARIANT_TYPE_STRING))
        socketPath = g_variant_dup_string(data, NULL);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectCleared), NULL);
}
