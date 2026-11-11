/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The proxies to reach a URL through (API 10.5): from the system's proxy
 * settings (SystemConfiguration's, as SCDynamicStoreCopyProxies gives
 * them), each proxy a dictionary with the keys below.
 */

#ifndef __CFPROXYSUPPORT__
#define __CFPROXYSUPPORT__

#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* The system's proxy settings (an empty dictionary without them). */
CF_EXPORT CFDictionaryRef CFNetworkCopySystemProxySettings(void);

/* The proxies for url under proxySettings, in the order to try them;
   kCFProxyTypeNone when the URL is reached directly. */
CF_EXPORT CFArrayRef CFNetworkCopyProxiesForURL(CFURLRef url, CFDictionaryRef proxySettings);

typedef void (*CFProxyAutoConfigurationResultCallback)(void *client, CFArrayRef proxyList, CFErrorRef error);

/* The proxies a proxy auto-configuration script gives for targetURL. Minkar
   does not run these scripts: the callback is told so with an error. */
CF_EXPORT CFArrayRef CFNetworkCopyProxiesForAutoConfigurationScript(CFStringRef proxyAutoConfigurationScript,
                                                                    CFURLRef targetURL, CFErrorRef *error);
CF_EXPORT CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationScript(CFStringRef proxyAutoConfigurationScript,
                                                                          CFURLRef targetURL,
                                                                          CFProxyAutoConfigurationResultCallback cb,
                                                                          CFStreamClientContext *clientContext);
CF_EXPORT CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationURL(CFURLRef proxyAutoConfigURL, CFURLRef targetURL,
                                                                       CFProxyAutoConfigurationResultCallback cb,
                                                                       CFStreamClientContext *clientContext);

/* A proxy's keys. */
CF_EXPORT const CFStringRef kCFProxyTypeKey;
CF_EXPORT const CFStringRef kCFProxyHostNameKey;
CF_EXPORT const CFStringRef kCFProxyPortNumberKey;
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationURLKey;
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationJavaScriptKey;
CF_EXPORT const CFStringRef kCFProxyUsernameKey;
CF_EXPORT const CFStringRef kCFProxyPasswordKey;

/* Its kCFProxyTypeKey. */
CF_EXPORT const CFStringRef kCFProxyTypeNone;
CF_EXPORT const CFStringRef kCFProxyTypeHTTP;
CF_EXPORT const CFStringRef kCFProxyTypeHTTPS;
CF_EXPORT const CFStringRef kCFProxyTypeSOCKS;
CF_EXPORT const CFStringRef kCFProxyTypeFTP;
CF_EXPORT const CFStringRef kCFProxyTypeAutoConfigurationURL;
CF_EXPORT const CFStringRef kCFProxyTypeAutoConfigurationJavaScript;

/* The settings' keys (as SystemConfiguration names them). */
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationHTTPResponseKey;
CF_EXPORT const CFStringRef kCFNetworkProxiesExceptionsList;
CF_EXPORT const CFStringRef kCFNetworkProxiesExcludeSimpleHostnames;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPPassive;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesGopherEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesGopherPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesGopherProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesRTSPEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesRTSPPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesRTSPProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigURLString;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigJavaScript;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoDiscoveryEnable;

#if defined(__cplusplus)
}
#endif

#endif /* __CFPROXYSUPPORT__ */
