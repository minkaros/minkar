/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFProxySupport.h: the proxies for a URL from the system's settings.
 * The settings are SystemConfiguration's (looked up at run time, as
 * CFNetwork does not link it).
 */

#include "CFProxySupport.h"
#include <dlfcn.h>
#include <stdlib.h>

#define KEY(name, text) const CFStringRef name = CFSTR(text);
KEY(kCFProxyTypeKey, "kCFProxyTypeKey")
KEY(kCFProxyHostNameKey, "kCFProxyHostNameKey")
KEY(kCFProxyPortNumberKey, "kCFProxyPortNumberKey")
KEY(kCFProxyAutoConfigurationURLKey, "kCFProxyAutoConfigurationURLKey")
KEY(kCFProxyAutoConfigurationJavaScriptKey, "kCFProxyAutoConfigurationJavaScriptKey")
KEY(kCFProxyUsernameKey, "kCFProxyUsernameKey")
KEY(kCFProxyPasswordKey, "kCFProxyPasswordKey")
KEY(kCFProxyTypeNone, "kCFProxyTypeNone")
KEY(kCFProxyTypeHTTP, "kCFProxyTypeHTTP")
KEY(kCFProxyTypeHTTPS, "kCFProxyTypeHTTPS")
KEY(kCFProxyTypeSOCKS, "kCFProxyTypeSOCKS")
KEY(kCFProxyTypeFTP, "kCFProxyTypeFTP")
KEY(kCFProxyTypeAutoConfigurationURL, "kCFProxyTypeAutoConfigurationURL")
KEY(kCFProxyTypeAutoConfigurationJavaScript, "kCFProxyTypeAutoConfigurationJavaScript")
KEY(kCFProxyAutoConfigurationHTTPResponseKey, "kCFProxyAutoConfigurationHTTPResponseKey")
/* SystemConfiguration's names for the settings. */
KEY(kCFNetworkProxiesExceptionsList, "ExceptionsList")
KEY(kCFNetworkProxiesExcludeSimpleHostnames, "ExcludeSimpleHostnames")
KEY(kCFNetworkProxiesFTPEnable, "FTPEnable")
KEY(kCFNetworkProxiesFTPPassive, "FTPPassive")
KEY(kCFNetworkProxiesFTPPort, "FTPPort")
KEY(kCFNetworkProxiesFTPProxy, "FTPProxy")
KEY(kCFNetworkProxiesGopherEnable, "GopherEnable")
KEY(kCFNetworkProxiesGopherPort, "GopherPort")
KEY(kCFNetworkProxiesGopherProxy, "GopherProxy")
KEY(kCFNetworkProxiesHTTPEnable, "HTTPEnable")
KEY(kCFNetworkProxiesHTTPPort, "HTTPPort")
KEY(kCFNetworkProxiesHTTPProxy, "HTTPProxy")
KEY(kCFNetworkProxiesHTTPSEnable, "HTTPSEnable")
KEY(kCFNetworkProxiesHTTPSPort, "HTTPSPort")
KEY(kCFNetworkProxiesHTTPSProxy, "HTTPSProxy")
KEY(kCFNetworkProxiesRTSPEnable, "RTSPEnable")
KEY(kCFNetworkProxiesRTSPPort, "RTSPPort")
KEY(kCFNetworkProxiesRTSPProxy, "RTSPProxy")
KEY(kCFNetworkProxiesSOCKSEnable, "SOCKSEnable")
KEY(kCFNetworkProxiesSOCKSPort, "SOCKSPort")
KEY(kCFNetworkProxiesSOCKSProxy, "SOCKSProxy")
KEY(kCFNetworkProxiesProxyAutoConfigEnable, "ProxyAutoConfigEnable")
KEY(kCFNetworkProxiesProxyAutoConfigURLString, "ProxyAutoConfigURLString")
KEY(kCFNetworkProxiesProxyAutoConfigJavaScript, "ProxyAutoConfigJavaScript")
KEY(kCFNetworkProxiesProxyAutoDiscoveryEnable, "ProxyAutoDiscoveryEnable")

CFDictionaryRef CFNetworkCopySystemProxySettings(void) {
    static CFDictionaryRef (*copyProxies)(void *store);
    static int looked;
    if (!looked) {
        void *sc = dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", RTLD_LAZY);
        if (sc)
            copyProxies = (CFDictionaryRef (*)(void *))dlsym(sc, "SCDynamicStoreCopyProxies");
        looked = 1;
    }
    CFDictionaryRef settings = copyProxies ? copyProxies(NULL) : NULL;
    return settings ? settings
                    : CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
}

static Boolean enabled(CFDictionaryRef settings, CFStringRef key) {
    CFNumberRef n = settings ? CFDictionaryGetValue(settings, key) : NULL;
    int v = 0;
    return n && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberIntType, &v) && v != 0;
}

/* A proxy of type at the host and port settings name, when enabled. */
static void addProxy(CFMutableArrayRef list, CFDictionaryRef settings, CFStringRef enable, CFStringRef host,
                     CFStringRef port, CFStringRef type) {
    CFStringRef h = CFDictionaryGetValue(settings, host);
    if (!enabled(settings, enable) || !h || CFGetTypeID(h) != CFStringGetTypeID() || CFStringGetLength(h) == 0)
        return;
    CFMutableDictionaryRef proxy = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(proxy, kCFProxyTypeKey, type);
    CFDictionarySetValue(proxy, kCFProxyHostNameKey, h);
    CFNumberRef p = CFDictionaryGetValue(settings, port);
    if (p && CFGetTypeID(p) == CFNumberGetTypeID())
        CFDictionarySetValue(proxy, kCFProxyPortNumberKey, p);
    CFArrayAppendValue(list, proxy);
    CFRelease(proxy);
}

static CFDictionaryRef proxyOfType(CFStringRef type) {
    const void *k = kCFProxyTypeKey, *v = type;
    return CFDictionaryCreate(kCFAllocatorDefault, &k, &v, 1, &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
}

/* Whether host is reached directly: the local host, a simple host name
   (no dot) when they are excluded, or a match of an exception ("name",
   "*.domain" or ".domain", compared without case). */
static Boolean bypassed(CFStringRef host, CFDictionaryRef settings) {
    CFMutableStringRef h = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, host);
    CFStringLowercase(h, NULL);
    Boolean result = CFEqual(h, CFSTR("localhost")) || CFEqual(h, CFSTR("127.0.0.1")) || CFEqual(h, CFSTR("::1"));
    if (!result && enabled(settings, kCFNetworkProxiesExcludeSimpleHostnames))
        result = CFStringFind(h, CFSTR("."), 0).location == kCFNotFound;
    CFArrayRef exceptions = CFDictionaryGetValue(settings, kCFNetworkProxiesExceptionsList);
    if (!result && exceptions && CFGetTypeID(exceptions) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; !result && i < CFArrayGetCount(exceptions); i++) {
            CFStringRef e = CFArrayGetValueAtIndex(exceptions, i);
            if (!e || CFGetTypeID(e) != CFStringGetTypeID() || CFStringGetLength(e) == 0)
                continue;
            CFMutableStringRef p = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, e);
            CFStringLowercase(p, NULL);
            if (CFStringHasPrefix(p, CFSTR("*.")))
                CFStringDelete(p, CFRangeMake(0, 1));
            if (CFStringHasPrefix(p, CFSTR("."))) {
                /* ".domain": the domain itself and its hosts. */
                CFStringRef domain = CFStringCreateWithSubstring(kCFAllocatorDefault, p,
                                                                 CFRangeMake(1, CFStringGetLength(p) - 1));
                result = CFStringHasSuffix(h, p) || CFEqual(h, domain);
                CFRelease(domain);
            }
            else
                result = CFEqual(h, p);
            CFRelease(p);
        }
    }
    CFRelease(h);
    return result;
}

CFArrayRef CFNetworkCopyProxiesForURL(CFURLRef url, CFDictionaryRef settings) {
    CFMutableArrayRef list = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFStringRef scheme = url ? CFURLCopyScheme(url) : NULL;
    CFStringRef host = url ? CFURLCopyHostName(url) : NULL;
    Boolean direct = TRUE;

    if (settings && host)
        direct = bypassed(host, settings);
    if (settings && !direct) {
        if (enabled(settings, kCFNetworkProxiesProxyAutoConfigEnable)) {
            CFStringRef pac = CFDictionaryGetValue(settings, kCFNetworkProxiesProxyAutoConfigURLString);
            CFURLRef pacURL = pac && CFGetTypeID(pac) == CFStringGetTypeID()
                                  ? CFURLCreateWithString(kCFAllocatorDefault, pac, NULL) : NULL;
            if (pacURL) {
                CFMutableDictionaryRef proxy = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                         &kCFTypeDictionaryKeyCallBacks,
                                                                         &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(proxy, kCFProxyTypeKey, kCFProxyTypeAutoConfigurationURL);
                CFDictionarySetValue(proxy, kCFProxyAutoConfigurationURLKey, pacURL);
                CFArrayAppendValue(list, proxy);
                CFRelease(proxy);
                CFRelease(pacURL);
            }
        }
        if (scheme && CFStringCompare(scheme, CFSTR("https"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            addProxy(list, settings, kCFNetworkProxiesHTTPSEnable, kCFNetworkProxiesHTTPSProxy,
                     kCFNetworkProxiesHTTPSPort, kCFProxyTypeHTTPS);
        else if (scheme && CFStringCompare(scheme, CFSTR("http"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            addProxy(list, settings, kCFNetworkProxiesHTTPEnable, kCFNetworkProxiesHTTPProxy,
                     kCFNetworkProxiesHTTPPort, kCFProxyTypeHTTP);
        else if (scheme && CFStringCompare(scheme, CFSTR("ftp"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            addProxy(list, settings, kCFNetworkProxiesFTPEnable, kCFNetworkProxiesFTPProxy,
                     kCFNetworkProxiesFTPPort, kCFProxyTypeFTP);
        addProxy(list, settings, kCFNetworkProxiesSOCKSEnable, kCFNetworkProxiesSOCKSProxy,
                 kCFNetworkProxiesSOCKSPort, kCFProxyTypeSOCKS);
    }
    CFDictionaryRef none = proxyOfType(kCFProxyTypeNone);
    CFArrayAppendValue(list, none);
    CFRelease(none);
    if (scheme)
        CFRelease(scheme);
    if (host)
        CFRelease(host);
    return list;
}

/* Proxy auto-configuration scripts are not run. */
static CFErrorRef unsupportedScript(void) {
    const void *k = kCFErrorDescriptionKey, *v = CFSTR("Proxy auto-configuration scripts are not supported");
    CFDictionaryRef info = CFDictionaryCreate(kCFAllocatorDefault, &k, &v, 1, &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    CFErrorRef error = CFErrorCreate(kCFAllocatorDefault, CFSTR("kCFErrorDomainCFNetwork"), 1, info);
    CFRelease(info);
    return error;
}

CFArrayRef CFNetworkCopyProxiesForAutoConfigurationScript(CFStringRef script, CFURLRef targetURL, CFErrorRef *error) {
    (void)script;
    (void)targetURL;
    if (error)
        *error = unsupportedScript();
    return NULL;
}

struct PACRequest {
    CFProxyAutoConfigurationResultCallback cb;
    CFStreamClientContext context;
    CFRunLoopSourceRef source;
};

static void pacPerform(void *info) {
    struct PACRequest *r = info;
    CFErrorRef error = unsupportedScript();
    r->cb(r->context.info, NULL, error);
    CFRelease(error);
    CFRunLoopSourceInvalidate(r->source);
}

static void pacRelease(const void *info) {
    struct PACRequest *r = (struct PACRequest *)info;
    if (r->context.release && r->context.info)
        r->context.release(r->context.info);
    free(r);
}

/* A source that, once in a run loop, tells cb the script was not run. */
static CFRunLoopSourceRef pacSource(CFProxyAutoConfigurationResultCallback cb, CFStreamClientContext *clientContext) {
    struct PACRequest *r = calloc(1, sizeof(*r));
    r->cb = cb;
    if (clientContext) {
        r->context = *clientContext;
        if (r->context.retain && r->context.info)
            r->context.info = (void *)r->context.retain(r->context.info);
    }
    CFRunLoopSourceContext context = { 0, r, NULL, pacRelease, NULL, NULL, NULL, NULL, NULL, pacPerform };
    r->source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopSourceSignal(r->source);
    return r->source;
}

CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationScript(CFStringRef script, CFURLRef targetURL,
                                                                 CFProxyAutoConfigurationResultCallback cb,
                                                                 CFStreamClientContext *clientContext) {
    (void)script;
    (void)targetURL;
    return pacSource(cb, clientContext);
}

CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationURL(CFURLRef pacURL, CFURLRef targetURL,
                                                              CFProxyAutoConfigurationResultCallback cb,
                                                              CFStreamClientContext *clientContext) {
    (void)pacURL;
    (void)targetURL;
    return pacSource(cb, clientContext);
}
