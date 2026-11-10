/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Gate W2: load a page in a WPE web view on the headless display, take a
 * snapshot of what it shows and write it as a PPM image (P6, RGB).
 *
 *   render URL OUT.ppm WIDTH HEIGHT
 *
 * Exits 0 with the image written, 1 on an error, 2 when the page does not
 * load and draw within the time allowed.
 */

#include <stdio.h>
#include <stdlib.h>

#include <wpe/webkit.h>
#include <wpe/headless/wpe-headless.h>

#define TIMEOUT_SECONDS 120

static GMainLoop *loop;
static const char *outPath;
static int status = 1;

static void fail(const char *what, GError *error)
{
    fprintf(stderr, "render: %s: %s\n", what, error ? error->message : "failed");
    g_clear_error(&error);
    g_main_loop_quit(loop);
}

static gboolean writePPM(WebKitImage *image)
{
    int width = webkit_image_get_width(image);
    int height = webkit_image_get_height(image);
    guint stride = webkit_image_get_stride(image);
    GBytes *bytes = webkit_image_as_bytes(image);   /* the image's own */
    gsize size;
    const guint8 *pixels = g_bytes_get_data(bytes, &size);

    if (width <= 0 || height <= 0 || size < (gsize)stride * (gsize)height)
        return FALSE;
    FILE *out = fopen(outPath, "wb");
    if (!out)
        return FALSE;
    fprintf(out, "P6\n%d %d\n255\n", width, height);
    /* BGRA, premultiplied; a page's pixels are opaque. */
    for (int y = 0; y < height; y++) {
        const guint8 *row = pixels + (gsize)y * stride;
        for (int x = 0; x < width; x++) {
            const guint8 *p = row + 4 * x;
            fputc(p[2], out);
            fputc(p[1], out);
            fputc(p[0], out);
        }
    }
    return fclose(out) == 0;
}

static void snapshotDone(GObject *object, GAsyncResult *result, gpointer data)
{
    (void)data;
    GError *error = NULL;
    WebKitImage *image = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(object), result, &error);

    if (!image) {
        fail("snapshot", error);
        return;
    }
    if (writePPM(image)) {
        printf("render: %dx%d written to %s\n", webkit_image_get_width(image),
               webkit_image_get_height(image), outPath);
        status = 0;
    } else {
        fprintf(stderr, "render: cannot write %s\n", outPath);
    }
    g_object_unref(image);
    g_main_loop_quit(loop);
}

/* The snapshot once the page has loaded, after its first frames. */
static gboolean takeSnapshot(gpointer data)
{
    webkit_web_view_get_snapshot(WEBKIT_WEB_VIEW(data), WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                 WEBKIT_SNAPSHOT_OPTIONS_NONE, NULL, snapshotDone, NULL);
    return G_SOURCE_REMOVE;
}

static void loadChanged(WebKitWebView *view, WebKitLoadEvent event, gpointer data)
{
    (void)data;
    if (event == WEBKIT_LOAD_FINISHED)
        g_timeout_add(500, takeSnapshot, view);
}

static gboolean loadFailed(WebKitWebView *view, WebKitLoadEvent event, char *uri,
                           GError *error, gpointer data)
{
    (void)view; (void)event; (void)data;
    fprintf(stderr, "render: %s: %s\n", uri, error->message);
    g_main_loop_quit(loop);
    return TRUE;
}

static void webProcessTerminated(WebKitWebView *view, WebKitWebProcessTerminationReason reason,
                                 gpointer data)
{
    (void)view; (void)data;
    fprintf(stderr, "render: the web process ended (reason %d)\n", (int)reason);
    g_main_loop_quit(loop);
}

static gboolean timedOut(gpointer data)
{
    (void)data;
    fprintf(stderr, "render: nothing drawn in %d seconds\n", TIMEOUT_SECONDS);
    status = 2;
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: render URL OUT.ppm WIDTH HEIGHT\n");
        return 1;
    }
    outPath = argv[2];
    int width = atoi(argv[3]), height = atoi(argv[4]);
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "render: bad size %sx%s\n", argv[3], argv[4]);
        return 1;
    }

    loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;
    WPEDisplay *display = wpe_display_headless_new();
    if (!wpe_display_connect(display, &error)) {
        fprintf(stderr, "render: headless display: %s\n", error->message);
        return 1;
    }

    WebKitWebView *view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "display", display, NULL));
    WPEToplevel *toplevel = wpe_view_get_toplevel(webkit_web_view_get_wpe_view(view));
    if (toplevel)
        wpe_toplevel_resize(toplevel, width, height);

    g_signal_connect(view, "load-changed", G_CALLBACK(loadChanged), NULL);
    g_signal_connect(view, "load-failed", G_CALLBACK(loadFailed), NULL);
    g_signal_connect(view, "web-process-terminated", G_CALLBACK(webProcessTerminated), NULL);
    g_timeout_add_seconds(TIMEOUT_SECONDS, timedOut, NULL);

    webkit_web_view_load_uri(view, argv[1]);
    g_main_loop_run(loop);

    g_object_unref(view);
    g_object_unref(display);
    g_main_loop_unref(loop);
    return status;
}
