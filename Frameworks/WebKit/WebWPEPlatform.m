// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Minkar's WPE platform (see WebWPEPlatform.h). Frames are paced at 60 per
// second, as a display would: a buffer is reported rendered at the next
// tick after it came.

#import <AppKit/AppKit.h>

#include "WebWPEPlatform.h"

#define FRAME_INTERVAL_USEC (G_USEC_PER_SEC / 60)

/* The display. */

G_DECLARE_FINAL_TYPE(MinkarWPEDisplay, minkar_wpe_display, MINKAR, WPE_DISPLAY, WPEDisplay)
struct _MinkarWPEDisplay {
    WPEDisplay parent;
};

/* A toplevel: one view each, its window's size and state. */

G_DECLARE_FINAL_TYPE(MinkarWPEToplevel, minkar_wpe_toplevel, MINKAR, WPE_TOPLEVEL, WPEToplevel)
struct _MinkarWPEToplevel {
    WPEToplevel parent;
};

/* A view: where its frames go, and the buffer to report rendered. */

G_DECLARE_FINAL_TYPE(MinkarWPEView, minkar_wpe_view, MINKAR, WPE_VIEW, WPEView)
struct _MinkarWPEView {
    WPEView parent;
    _WebWPEFrameFunction frame;
    void *frameContext;
    WPEBuffer *pending;
    GSource *frameSource;
    gint64 lastFrameTime;
};

G_DEFINE_FINAL_TYPE(MinkarWPEToplevel, minkar_wpe_toplevel, WPE_TYPE_TOPLEVEL)

static gboolean resizeView(WPEToplevel *toplevel, WPEView *view, gpointer data)
{
    (void)data;
    int width, height;
    wpe_toplevel_get_size(toplevel, &width, &height);
    wpe_view_resized(view, width, height);
    return FALSE;
}

static gboolean toplevelResizeViews(WPEToplevel *toplevel, int width, int height)
{
    wpe_toplevel_resized(toplevel, width, height);
    wpe_toplevel_foreach_view(toplevel, resizeView, NULL);
    return TRUE;
}

static gboolean toplevelSetState(WPEToplevel *toplevel, WPEToplevelState flag, gboolean on)
{
    WPEToplevelState state = wpe_toplevel_get_state(toplevel);
    state = on ? (state | flag) : (state & ~flag);
    wpe_toplevel_state_changed(toplevel, state);
    return TRUE;
}

static gboolean toplevelSetFullscreen(WPEToplevel *toplevel, gboolean fullscreen)
{
    return toplevelSetState(toplevel, WPE_TOPLEVEL_STATE_FULLSCREEN, fullscreen);
}

static gboolean toplevelSetMaximized(WPEToplevel *toplevel, gboolean maximized)
{
    return toplevelSetState(toplevel, WPE_TOPLEVEL_STATE_MAXIMIZED, maximized);
}

static void minkar_wpe_toplevel_class_init(MinkarWPEToplevelClass *klass)
{
    WPEToplevelClass *toplevelClass = WPE_TOPLEVEL_CLASS(klass);
    toplevelClass->resize = toplevelResizeViews;
    toplevelClass->set_fullscreen = toplevelSetFullscreen;
    toplevelClass->set_maximized = toplevelSetMaximized;
}

static void minkar_wpe_toplevel_init(MinkarWPEToplevel *toplevel)
{
    (void)toplevel;
}

G_DEFINE_FINAL_TYPE(MinkarWPEView, minkar_wpe_view, WPE_TYPE_VIEW)

/* At the frame tick: the buffer that came is reported rendered. */
static gboolean viewFrameTick(gpointer data)
{
    MinkarWPEView *self = data;
    g_source_set_ready_time(self->frameSource, -1);
    WPEBuffer *buffer = self->pending;
    self->pending = NULL;
    if (buffer) {
        self->lastFrameTime = g_get_monotonic_time();
        wpe_view_buffer_rendered(WPE_VIEW(self), buffer);
        g_object_unref(buffer);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean frameSourceDispatch(GSource *source, GSourceFunc callback, gpointer data)
{
    if (g_source_get_ready_time(source) == -1)
        return G_SOURCE_CONTINUE;
    return callback(data);
}

static GSourceFuncs frameSourceFuncs = { .dispatch = frameSourceDispatch };

static gboolean viewRenderBuffer(WPEView *view, WPEBuffer *buffer, const WPERectangle *damage,
                                 guint damageCount, GError **error)
{
    MinkarWPEView *self = MINKAR_WPE_VIEW(view);
    if (!WPE_IS_BUFFER_SHM(buffer)) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED,
                            "Minkar's web views take shared memory buffers only");
        return FALSE;
    }

    GBytes *bytes = wpe_buffer_shm_get_data(WPE_BUFFER_SHM(buffer));
    gsize size = 0;
    const guint8 *pixels = g_bytes_get_data(bytes, &size);
    int width = wpe_buffer_get_width(buffer), height = wpe_buffer_get_height(buffer);
    guint stride = wpe_buffer_shm_get_stride(WPE_BUFFER_SHM(buffer));
    if (self->frame && pixels && size >= (gsize)stride * (gsize)height)
        self->frame(self->frameContext, pixels, width, height, stride, damage, damageCount);

    /* Copied: the buffer goes back now, and is reported rendered at the
       next tick. */
    wpe_view_buffer_released(view, buffer);
    if (self->pending) {
        wpe_view_buffer_rendered(view, self->pending);
        g_object_unref(self->pending);
    }
    self->pending = g_object_ref(buffer);
    gint64 next = self->lastFrameTime + FRAME_INTERVAL_USEC, now = g_get_monotonic_time();
    g_source_set_ready_time(self->frameSource, next <= now ? 0 : next);
    return TRUE;
}

static void viewNotifyToplevel(WPEView *view, GParamSpec *pspec, gpointer data)
{
    (void)pspec; (void)data;
    WPEToplevel *toplevel = wpe_view_get_toplevel(view);
    if (!toplevel) {
        wpe_view_unmap(view);
        return;
    }
    int width = 0, height = 0;
    wpe_toplevel_get_size(toplevel, &width, &height);
    if (width && height)
        wpe_view_resized(view, width, height);
}

static void viewConstructed(GObject *object)
{
    G_OBJECT_CLASS(minkar_wpe_view_parent_class)->constructed(object);
    MinkarWPEView *self = MINKAR_WPE_VIEW(object);
    g_signal_connect(object, "notify::toplevel", G_CALLBACK(viewNotifyToplevel), NULL);
    self->frameSource = g_source_new(&frameSourceFuncs, sizeof(GSource));
    g_source_set_name(self->frameSource, "Minkar WPE frame tick");
    g_source_set_callback(self->frameSource, viewFrameTick, self, NULL);
    g_source_set_ready_time(self->frameSource, -1);
    g_source_attach(self->frameSource, g_main_context_get_thread_default());
}

static void viewDispose(GObject *object)
{
    MinkarWPEView *self = MINKAR_WPE_VIEW(object);
    if (self->frameSource) {
        g_source_destroy(self->frameSource);
        g_source_unref(self->frameSource);
        self->frameSource = NULL;
    }
    g_clear_object(&self->pending);
    self->frame = NULL;
    G_OBJECT_CLASS(minkar_wpe_view_parent_class)->dispose(object);
}

/* The cursor a page asks for (CSS's names), from AppKit's. */
static void viewSetCursorFromName(WPEView *view, const char *name)
{
    (void)view;
    NSCursor *cursor = [NSCursor arrowCursor];
    if (!strcmp(name, "pointer"))
        cursor = [NSCursor pointingHandCursor];
    else if (!strcmp(name, "text") || !strcmp(name, "vertical-text"))
        cursor = [NSCursor IBeamCursor];
    else if (!strcmp(name, "crosshair"))
        cursor = [NSCursor crosshairCursor];
    else if (!strcmp(name, "grab"))
        cursor = [NSCursor openHandCursor];
    else if (!strcmp(name, "grabbing") || !strcmp(name, "move"))
        cursor = [NSCursor closedHandCursor];
    else if (!strcmp(name, "not-allowed") || !strcmp(name, "no-drop"))
        cursor = [NSCursor operationNotAllowedCursor];
    else if (!strcmp(name, "ew-resize") || !strcmp(name, "col-resize") || !strcmp(name, "e-resize") || !strcmp(name, "w-resize"))
        cursor = [NSCursor resizeLeftRightCursor];
    else if (!strcmp(name, "ns-resize") || !strcmp(name, "row-resize") || !strcmp(name, "n-resize") || !strcmp(name, "s-resize"))
        cursor = [NSCursor resizeUpDownCursor];
    [cursor set];
}

static void minkar_wpe_view_class_init(MinkarWPEViewClass *klass)
{
    WPE_VIEW_CLASS(klass)->set_cursor_from_name = viewSetCursorFromName;
    GObjectClass *objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = viewConstructed;
    objectClass->dispose = viewDispose;
    WPE_VIEW_CLASS(klass)->render_buffer = viewRenderBuffer;
}

static void minkar_wpe_view_init(MinkarWPEView *view)
{
    (void)view;
}

/* The keymap: the modifiers held. Keys reach WebKit with their key
   symbols already (from AppKit's characters), so nothing is translated. */

G_DECLARE_FINAL_TYPE(MinkarWPEKeymap, minkar_wpe_keymap, MINKAR, WPE_KEYMAP, WPEKeymap)
struct _MinkarWPEKeymap {
    WPEKeymap parent;
};
G_DEFINE_FINAL_TYPE(MinkarWPEKeymap, minkar_wpe_keymap, WPE_TYPE_KEYMAP)

static WPEModifiers heldModifiers;

static WPEModifiers keymapGetModifiers(WPEKeymap *keymap)
{
    (void)keymap;
    return heldModifiers;
}

static gboolean keymapEntriesForKeyval(WPEKeymap *keymap, guint keyval, WPEKeymapEntry **entries, guint *count)
{
    (void)keymap; (void)keyval;
    *entries = NULL;
    *count = 0;
    return FALSE;
}

static gboolean keymapTranslate(WPEKeymap *keymap, guint keycode, WPEModifiers modifiers, int group, guint *keyval,
                                int *effectiveGroup, int *level, WPEModifiers *consumed)
{
    (void)keymap; (void)keycode; (void)modifiers; (void)group;
    if (keyval)
        *keyval = 0;
    if (effectiveGroup)
        *effectiveGroup = 0;
    if (level)
        *level = 0;
    if (consumed)
        *consumed = 0;
    return FALSE;
}

static void minkar_wpe_keymap_class_init(MinkarWPEKeymapClass *klass)
{
    WPEKeymapClass *keymapClass = WPE_KEYMAP_CLASS(klass);
    keymapClass->get_modifiers = keymapGetModifiers;
    keymapClass->get_entries_for_keyval = keymapEntriesForKeyval;
    keymapClass->translate_keyboard_state = keymapTranslate;
}

static void minkar_wpe_keymap_init(MinkarWPEKeymap *keymap)
{
    (void)keymap;
}

void _WebWPESetModifiers(WPEModifiers modifiers)
{
    heldModifiers = modifiers;
}

/* The clipboard: what WebKit copies goes to the general pasteboard, and
   what another program put there is offered to WebKit, in MIME types. */

G_DECLARE_FINAL_TYPE(MinkarWPEClipboard, minkar_wpe_clipboard, MINKAR, WPE_CLIPBOARD, WPEClipboard)
struct _MinkarWPEClipboard {
    WPEClipboard parent;
    NSInteger seenChangeCount;
};
G_DEFINE_FINAL_TYPE(MinkarWPEClipboard, minkar_wpe_clipboard, WPE_TYPE_CLIPBOARD)

static WPEClipboard *sharedClipboard;

static NSPasteboardType pasteboardTypeFor(const char *format)
{
    if (!strcmp(format, "text/plain") || !strcmp(format, "text/plain;charset=utf-8"))
        return NSPasteboardTypeString;
    if (!strcmp(format, "text/html"))
        return NSPasteboardTypeHTML;
    if (!strcmp(format, "image/png"))
        return NSPasteboardTypePNG;
    if (!strcmp(format, "text/uri-list"))
        return NSPasteboardTypeURL;
    return nil;
}

static void clipboardChanged(WPEClipboard *clipboard, GPtrArray *formats, gboolean isLocal, WPEClipboardContent *content)
{
    WPE_CLIPBOARD_CLASS(minkar_wpe_clipboard_parent_class)->changed(clipboard, formats, isLocal, content);
    if (!isLocal)
        return;

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    for (guint i = 0; formats && i < formats->len; i++) {
        const char *format = g_ptr_array_index(formats, i);
        NSPasteboardType type = format ? pasteboardTypeFor(format) : nil;
        if (type == nil)
            continue;
        GBytes *bytes = wpe_clipboard_read_bytes(clipboard, format);
        if (bytes == NULL)
            continue;
        gsize size = 0;
        const void *data = g_bytes_get_data(bytes, &size);
        if (type == NSPasteboardTypeString)
            [pasteboard setString:[[NSString alloc] initWithBytes:data length:size encoding:NSUTF8StringEncoding] ?: @""
                          forType:type];
        else
            [pasteboard setData:[NSData dataWithBytes:data length:size] forType:type];
        g_bytes_unref(bytes);
    }
    MINKAR_WPE_CLIPBOARD(clipboard)->seenChangeCount = pasteboard.changeCount;
}

static GBytes *clipboardRead(WPEClipboard *clipboard, const char *format)
{
    (void)clipboard;
    NSPasteboardType type = pasteboardTypeFor(format);
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    if (type == nil)
        return NULL;
    if (type == NSPasteboardTypeString) {
        NSString *string = [pasteboard stringForType:type];
        const char *utf8 = string.UTF8String;
        return utf8 ? g_bytes_new(utf8, strlen(utf8)) : NULL;
    }
    NSData *data = [pasteboard dataForType:type];
    return data ? g_bytes_new(data.bytes, data.length) : NULL;
}

static void minkar_wpe_clipboard_class_init(MinkarWPEClipboardClass *klass)
{
    WPEClipboardClass *clipboardClass = WPE_CLIPBOARD_CLASS(klass);
    clipboardClass->changed = clipboardChanged;
    clipboardClass->read = clipboardRead;
}

static void minkar_wpe_clipboard_init(MinkarWPEClipboard *clipboard)
{
    clipboard->seenChangeCount = -1;
}

/* An interned format into a formats array (GPtrArray holds gpointers). */
static void addFormat(GPtrArray *formats, const char *format)
{
    union { const char *constant; gpointer pointer; } entry = { .constant = g_intern_static_string(format) };
    g_ptr_array_add(formats, entry.pointer);
}

void _WebWPEClipboardSync(void)
{
    if (sharedClipboard == NULL)
        return;
    MinkarWPEClipboard *self = MINKAR_WPE_CLIPBOARD(sharedClipboard);
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    if (pasteboard.changeCount == self->seenChangeCount)
        return;
    self->seenChangeCount = pasteboard.changeCount;

    GPtrArray *formats = g_ptr_array_new_null_terminated(4, NULL, TRUE);
    NSArray<NSPasteboardType> *types = pasteboard.types;
    if ([types containsObject:NSPasteboardTypeString]) {
        addFormat(formats, "text/plain");
        addFormat(formats, "text/plain;charset=utf-8");
    }
    if ([types containsObject:NSPasteboardTypeHTML])
        addFormat(formats, "text/html");
    if ([types containsObject:NSPasteboardTypePNG])
        addFormat(formats, "image/png");
    if ([types containsObject:NSPasteboardTypeURL])
        addFormat(formats, "text/uri-list");
    WPE_CLIPBOARD_GET_CLASS(sharedClipboard)->changed(sharedClipboard, formats, FALSE, NULL);
    g_ptr_array_unref(formats);
}

G_DEFINE_FINAL_TYPE(MinkarWPEDisplay, minkar_wpe_display, WPE_TYPE_DISPLAY)

static gboolean displayConnect(WPEDisplay *display, GError **error)
{
    (void)display; (void)error;
    return TRUE;
}

static WPEView *displayCreateView(WPEDisplay *display)
{
    return WPE_VIEW(g_object_new(minkar_wpe_view_get_type(), "display", display, NULL));
}

static WPEToplevel *displayCreateToplevel(WPEDisplay *display, guint maxViews)
{
    (void)maxViews;
    return WPE_TOPLEVEL(g_object_new(minkar_wpe_toplevel_get_type(), "display", display, "max-views", 1, NULL));
}

static WPEKeymap *displayGetKeymap(WPEDisplay *display)
{
    (void)display;
    static WPEKeymap *keymap;
    if (keymap == NULL)
        keymap = WPE_KEYMAP(g_object_new(minkar_wpe_keymap_get_type(), NULL));
    return keymap;
}

static WPEClipboard *displayGetClipboard(WPEDisplay *display)
{
    if (sharedClipboard == NULL)
        sharedClipboard = WPE_CLIPBOARD(g_object_new(minkar_wpe_clipboard_get_type(), "display", display, NULL));
    return sharedClipboard;
}

static void minkar_wpe_display_class_init(MinkarWPEDisplayClass *klass)
{
    WPEDisplayClass *displayClass = WPE_DISPLAY_CLASS(klass);
    displayClass->get_keymap = displayGetKeymap;
    displayClass->get_clipboard = displayGetClipboard;
    displayClass->connect = displayConnect;
    displayClass->create_view = displayCreateView;
    displayClass->create_toplevel = displayCreateToplevel;
}

static void minkar_wpe_display_init(MinkarWPEDisplay *display)
{
    (void)display;
}

WPEDisplay *_WebWPEDisplayGet(void)
{
    static WPEDisplay *display;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        display = WPE_DISPLAY(g_object_new(minkar_wpe_display_get_type(), NULL));
        GError *error = NULL;
        if (!wpe_display_connect(display, &error)) {
            NSLog(@"WebKit: the WPE display does not connect: %s", error->message);
            g_error_free(error);
        }
        wpe_display_set_primary(display);
    });
    return display;
}

void _WebWPEViewSetFrameFunction(WPEView *view, _WebWPEFrameFunction function, void *context)
{
    if (!MINKAR_IS_WPE_VIEW(view))
        return;
    MinkarWPEView *self = MINKAR_WPE_VIEW(view);
    self->frame = function;
    self->frameContext = context;
}

void _WebWPEViewResize(WPEView *view, int width, int height)
{
    WPEToplevel *toplevel = wpe_view_get_toplevel(view);
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    if (toplevel)
        wpe_toplevel_resize(toplevel, width, height);
    else
        wpe_view_resized(view, width, height);
}

void _WebWPEViewSetMapped(WPEView *view, gboolean mapped)
{
    if (mapped && !wpe_view_get_mapped(view))
        wpe_view_map(view);
    else if (!mapped && wpe_view_get_mapped(view))
        wpe_view_unmap(view);
}

void _WebWPEViewSetActive(WPEView *view, gboolean active)
{
    WPEToplevel *toplevel = wpe_view_get_toplevel(view);
    if (toplevel)
        toplevelSetState(toplevel, WPE_TOPLEVEL_STATE_ACTIVE, active);
}
