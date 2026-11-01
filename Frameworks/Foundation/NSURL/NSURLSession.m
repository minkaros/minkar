/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * NSURLSession on libcurl.
 *
 * libcurl (/usr/lib/libcurl.4.dylib) is loaded the first time a session
 * starts a task, so a program that never uses one does not load it. One
 * engine thread runs a curl multi handle for every session: the tasks hand
 * it work as blocks (enginePerform) and it calls the tasks back from curl's
 * callbacks. The delegate's methods and the completion handlers run on the
 * session's delegate queue, a serial NSOperationQueue.
 *
 * HTTP redirects and authentication challenges are handled here, not by
 * curl, so that the delegate can take part: the body of such a response is
 * held until it is known whether it is the final response.
 */

#import <Foundation/NSURLSession.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSHTTPCookieStorage.h>
#import <Foundation/NSHTTPURLResponse.h>
#import <Foundation/NSMutableURLRequest.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSOperationQueue.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSStream.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLCredential.h>
#import <Foundation/NSURLError.h>
#import <Foundation/NSURLProtectionSpace.h>
#import <Foundation/NSURLResponse.h>
#include <curl/curl.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

const int64_t NSURLSessionTransferSizeUnknown = -1LL;
const float NSURLSessionTaskPriorityDefault = 0.5f;
const float NSURLSessionTaskPriorityLow = 0.25f;
const float NSURLSessionTaskPriorityHigh = 0.75f;
NSString *const NSURLSessionDownloadTaskResumeData = @"NSURLSessionDownloadTaskResumeData";

#pragma mark - libcurl

static struct {
    CURLcode (*global_init)(long);
    CURL *(*easy_init)(void);
    void (*easy_cleanup)(CURL *);
    CURLcode (*easy_setopt)(CURL *, CURLoption, ...);
    CURLcode (*easy_getinfo)(CURL *, CURLINFO, ...);
    CURLcode (*easy_pause)(CURL *, int);
    CURLM *(*multi_init)(void);
    CURLMcode (*multi_setopt)(CURLM *, CURLMoption, ...);
    CURLMcode (*multi_add_handle)(CURLM *, CURL *);
    CURLMcode (*multi_remove_handle)(CURLM *, CURL *);
    CURLMcode (*multi_perform)(CURLM *, int *);
    CURLMcode (*multi_poll)(CURLM *, struct curl_waitfd[], unsigned int, int, int *);
    CURLMcode (*multi_wakeup)(CURLM *);
    CURLMcode (*multi_timeout)(CURLM *, long *);
    CURLMsg *(*multi_info_read)(CURLM *, int *);
    struct curl_slist *(*slist_append)(struct curl_slist *, const char *);
    void (*slist_free_all)(struct curl_slist *);
} curl;

static BOOL loadCurl(void) {
    static dispatch_once_t once;
    static BOOL loaded;
    dispatch_once(&once, ^{
        void *handle = dlopen("/usr/lib/libcurl.4.dylib", RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL)
            return;
#define SYMBOL(field, name) \
        if ((*(void **)&curl.field = dlsym(handle, name)) == NULL) return;
        SYMBOL(global_init, "curl_global_init")
        SYMBOL(easy_init, "curl_easy_init")
        SYMBOL(easy_cleanup, "curl_easy_cleanup")
        SYMBOL(easy_setopt, "curl_easy_setopt")
        SYMBOL(easy_getinfo, "curl_easy_getinfo")
        SYMBOL(easy_pause, "curl_easy_pause")
        SYMBOL(multi_init, "curl_multi_init")
        SYMBOL(multi_setopt, "curl_multi_setopt")
        SYMBOL(multi_add_handle, "curl_multi_add_handle")
        SYMBOL(multi_remove_handle, "curl_multi_remove_handle")
        SYMBOL(multi_perform, "curl_multi_perform")
        SYMBOL(multi_poll, "curl_multi_poll")
        SYMBOL(multi_wakeup, "curl_multi_wakeup")
        SYMBOL(multi_timeout, "curl_multi_timeout")
        SYMBOL(multi_info_read, "curl_multi_info_read")
        SYMBOL(slist_append, "curl_slist_append")
        SYMBOL(slist_free_all, "curl_slist_free_all")
#undef SYMBOL
        loaded = curl.global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    });
    return loaded;
}

#pragma mark - Engine

@interface NSURLSessionTask ()
- (void)_transferDidComplete:(CURLcode)code;
@end

static CURLM *engineMulti;
static pthread_mutex_t engineLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableArray *engineCommands;

static void *engineMain(void *unused __attribute__((unused))) {
    pthread_setname_np("org.minkar.NSURLSession");
    for (;;) {
        @autoreleasepool {
            pthread_mutex_lock(&engineLock);
            NSArray *commands = engineCommands;
            engineCommands = [NSMutableArray new];
            pthread_mutex_unlock(&engineLock);
            for (void (^command)(void) in commands)
                command();

            int running = 0;
            curl.multi_perform(engineMulti, &running);
            CURLMsg *message;
            int left;
            while ((message = curl.multi_info_read(engineMulti, &left)) != NULL) {
                if (message->msg != CURLMSG_DONE)
                    continue;
                CURLcode result = message->data.result;
                char *owner = NULL;
                curl.easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &owner);
                [(__bridge NSURLSessionTask *)(void *)owner _transferDidComplete:result];
            }

            long timeout = -1;
            curl.multi_timeout(engineMulti, &timeout);
            if (timeout < 0 || timeout > 1000)
                timeout = 1000;
            curl.multi_poll(engineMulti, NULL, 0, (int)timeout, NULL);
        }
    }
    return NULL;
}

static BOOL engineStart(void) {
    static dispatch_once_t once;
    static BOOL started;
    dispatch_once(&once, ^{
        if (!loadCurl())
            return;
        engineMulti = curl.multi_init();
        if (engineMulti == NULL)
            return;
        curl.multi_setopt(engineMulti, CURLMOPT_MAX_HOST_CONNECTIONS, 6L);
        engineCommands = [NSMutableArray new];
        pthread_t thread;
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        started = pthread_create(&thread, &attributes, engineMain, NULL) == 0;
        pthread_attr_destroy(&attributes);
    });
    return started;
}

// Runs the block on the engine thread.
static void enginePerform(void (^block)(void)) {
    pthread_mutex_lock(&engineLock);
    [engineCommands addObject:[block copy]];
    pthread_mutex_unlock(&engineLock);
    curl.multi_wakeup(engineMulti);
}

#pragma mark - Errors

static NSString *errorDescription(NSInteger code) {
    switch (code) {
    case NSURLErrorCancelled:                 return @"cancelled";
    case NSURLErrorBadURL:                    return @"bad URL";
    case NSURLErrorTimedOut:                  return @"The request timed out.";
    case NSURLErrorUnsupportedURL:            return @"unsupported URL";
    case NSURLErrorCannotFindHost:            return @"A server with the specified hostname could not be found.";
    case NSURLErrorCannotConnectToHost:       return @"Could not connect to the server.";
    case NSURLErrorNetworkConnectionLost:     return @"The network connection was lost.";
    case NSURLErrorHTTPTooManyRedirects:      return @"too many HTTP redirects";
    case NSURLErrorBadServerResponse:         return @"The server returned a bad response.";
    case NSURLErrorCannotDecodeContentData:   return @"The data could not be decoded.";
    case NSURLErrorFileDoesNotExist:          return @"The requested file does not exist.";
    case NSURLErrorFileIsDirectory:           return @"The requested file is a directory.";
    case NSURLErrorNoPermissionsToReadFile:   return @"The file could not be read.";
    case NSURLErrorRequestBodyStreamExhausted: return @"The request body could not be sent.";
    case NSURLErrorSecureConnectionFailed:    return @"An SSL error has occurred and a secure connection to the server cannot be made.";
    case NSURLErrorServerCertificateUntrusted: return @"The certificate for this server is invalid.";
    case NSURLErrorClientCertificateRejected: return @"The client certificate was rejected.";
    case NSURLErrorCannotCreateFile:          return @"The file could not be created.";
    case NSURLErrorCannotWriteToFile:         return @"The file could not be written.";
    default:                                  return @"An unknown error occurred.";
    }
}

static NSError *URLError(NSInteger code, NSURL *url) {
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    info[NSLocalizedDescriptionKey] = errorDescription(code);
    if (url != nil) {
        info[NSURLErrorFailingURLErrorKey] = url;
        info[NSURLErrorFailingURLStringErrorKey] = [url absoluteString];
    }
    return [NSError errorWithDomain:NSURLErrorDomain code:code userInfo:info];
}

static NSInteger URLErrorCodeForCurl(CURLcode code) {
    switch (code) {
    case CURLE_UNSUPPORTED_PROTOCOL:     return NSURLErrorUnsupportedURL;
    case CURLE_URL_MALFORMAT:            return NSURLErrorBadURL;
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:     return NSURLErrorCannotFindHost;
    case CURLE_COULDNT_CONNECT:          return NSURLErrorCannotConnectToHost;
    case CURLE_OPERATION_TIMEDOUT:       return NSURLErrorTimedOut;
    case CURLE_WEIRD_SERVER_REPLY:
    case CURLE_RANGE_ERROR:              return NSURLErrorBadServerResponse;
    case CURLE_PARTIAL_FILE:
    case CURLE_GOT_NOTHING:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:               return NSURLErrorNetworkConnectionLost;
    case CURLE_TOO_MANY_REDIRECTS:       return NSURLErrorHTTPTooManyRedirects;
    case CURLE_FILE_COULDNT_READ_FILE:   return NSURLErrorFileDoesNotExist;
    case CURLE_READ_ERROR:               return NSURLErrorRequestBodyStreamExhausted;
    case CURLE_WRITE_ERROR:              return NSURLErrorCannotWriteToFile;
    case CURLE_BAD_CONTENT_ENCODING:     return NSURLErrorCannotDecodeContentData;
    case CURLE_ABORTED_BY_CALLBACK:      return NSURLErrorCancelled;
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CACERT_BADFILE:       return NSURLErrorServerCertificateUntrusted;
    case CURLE_SSL_CERTPROBLEM:          return NSURLErrorClientCertificateRejected;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_CIPHER:
    case CURLE_USE_SSL_FAILED:
    case CURLE_SSL_SHUTDOWN_FAILED:      return NSURLErrorSecureConnectionFailed;
    default:                             return NSURLErrorUnknown;
    }
}

@interface NSHTTPCookieStorage (NSURLSession)
- (void)_setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL
             policy:(NSHTTPCookieAcceptPolicy)policy;
@end

#pragma mark - Configuration

@implementation NSURLSessionConfiguration {
    NSString *_identifier;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    _requestCachePolicy = NSURLRequestUseProtocolCachePolicy;
    _timeoutIntervalForRequest = 60;
    _timeoutIntervalForResource = 7 * 24 * 60 * 60;
    _networkServiceType = NSURLNetworkServiceTypeDefault;
    _allowsCellularAccess = YES;
    _allowsExpensiveNetworkAccess = YES;
    _allowsConstrainedNetworkAccess = YES;
    _TLSMinimumSupportedProtocolVersion = tls_protocol_version_TLSv10;
    _TLSMaximumSupportedProtocolVersion = tls_protocol_version_TLSv13;
    _TLSMinimumSupportedProtocol = 0;
    _TLSMaximumSupportedProtocol = 0;
    _HTTPShouldSetCookies = YES;
    _HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    _HTTPMaximumConnectionsPerHost = 6;
    _protocolClasses = @[];
    return self;
}

+ (NSURLSessionConfiguration *)defaultSessionConfiguration {
    NSURLSessionConfiguration *configuration = [[self alloc] init];
    configuration.HTTPCookieStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    return configuration;
}

// Nothing is kept on disk: the session's cookies stay in memory.
+ (NSURLSessionConfiguration *)ephemeralSessionConfiguration {
    NSURLSessionConfiguration *configuration = [[self alloc] init];
    configuration.HTTPCookieStorage = [[NSHTTPCookieStorage alloc] init];
    return configuration;
}

// Minkar: a background session's transfers run in the process, like a
// default session's; they do not outlive it.
+ (NSURLSessionConfiguration *)backgroundSessionConfigurationWithIdentifier:(NSString *)identifier {
    NSURLSessionConfiguration *configuration = [self defaultSessionConfiguration];
    configuration->_identifier = [identifier copy];
    return configuration;
}

+ (NSURLSessionConfiguration *)backgroundSessionConfiguration:(NSString *)identifier {
    return [self backgroundSessionConfigurationWithIdentifier:identifier];
}

- (NSString *)identifier {
    return _identifier;
}

- (id)copyWithZone:(NSZone *)zone {
    NSURLSessionConfiguration *copy = [[NSURLSessionConfiguration alloc] init];
    copy->_identifier = _identifier;
    copy.requestCachePolicy = self.requestCachePolicy;
    copy.timeoutIntervalForRequest = self.timeoutIntervalForRequest;
    copy.timeoutIntervalForResource = self.timeoutIntervalForResource;
    copy.networkServiceType = self.networkServiceType;
    copy.allowsCellularAccess = self.allowsCellularAccess;
    copy.allowsExpensiveNetworkAccess = self.allowsExpensiveNetworkAccess;
    copy.allowsConstrainedNetworkAccess = self.allowsConstrainedNetworkAccess;
    copy.waitsForConnectivity = self.waitsForConnectivity;
    copy.discretionary = self.discretionary;
    copy.sharedContainerIdentifier = self.sharedContainerIdentifier;
    copy.sessionSendsLaunchEvents = self.sessionSendsLaunchEvents;
    copy.connectionProxyDictionary = self.connectionProxyDictionary;
    copy.TLSMinimumSupportedProtocol = self.TLSMinimumSupportedProtocol;
    copy.TLSMaximumSupportedProtocol = self.TLSMaximumSupportedProtocol;
    copy.TLSMinimumSupportedProtocolVersion = self.TLSMinimumSupportedProtocolVersion;
    copy.TLSMaximumSupportedProtocolVersion = self.TLSMaximumSupportedProtocolVersion;
    copy.HTTPShouldUsePipelining = self.HTTPShouldUsePipelining;
    copy.HTTPShouldSetCookies = self.HTTPShouldSetCookies;
    copy.HTTPCookieAcceptPolicy = self.HTTPCookieAcceptPolicy;
    copy.HTTPAdditionalHeaders = self.HTTPAdditionalHeaders;
    copy.HTTPMaximumConnectionsPerHost = self.HTTPMaximumConnectionsPerHost;
    copy.HTTPCookieStorage = self.HTTPCookieStorage;
    copy.URLCredentialStorage = self.URLCredentialStorage;
    copy.URLCache = self.URLCache;
    copy.shouldUseExtendedBackgroundIdleMode = self.shouldUseExtendedBackgroundIdleMode;
    copy.protocolClasses = self.protocolClasses;
    copy.multipathServiceType = self.multipathServiceType;
    return copy;
}

@end

#pragma mark - Session (internal)

@interface NSURLSession ()
- (void)_taskDidFinish:(NSURLSessionTask *)task;
- (NSUInteger)_nextTaskIdentifier;
- (void)_addTask:(NSURLSessionTask *)task;
@end

typedef NS_ENUM(NSInteger, NSURLSessionTaskKind) {
    NSURLSessionTaskKindData,
    NSURLSessionTaskKindUpload,
    NSURLSessionTaskKindDownload,
};

// What happens to the body of the response being received.
typedef NS_ENUM(NSInteger, NSURLSessionBodyMode) {
    NSURLSessionBodyDeliver,        // to the delegate, the handler or the file
    NSURLSessionBodyHold,           // a redirect or a challenge: kept aside
};

#pragma mark - Challenge sender

// The sender of the challenges a session hands its delegate: each answer
// becomes the matching disposition.
@interface _NSURLSessionChallengeSender : NSObject <NSURLAuthenticationChallengeSender>
@property (copy) void (^answer)(NSURLSessionAuthChallengeDisposition, NSURLCredential *);
@end

@implementation _NSURLSessionChallengeSender
- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if (self.answer) self.answer(NSURLSessionAuthChallengeUseCredential, credential);
    self.answer = nil;
}
- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if (self.answer) self.answer(NSURLSessionAuthChallengeUseCredential, nil);
    self.answer = nil;
}
- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if (self.answer) self.answer(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
    self.answer = nil;
}
- (void)performDefaultHandlingForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if (self.answer) self.answer(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    self.answer = nil;
}
- (void)rejectProtectionSpaceAndContinueWithChallenge:(NSURLAuthenticationChallenge *)challenge {
    if (self.answer) self.answer(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    self.answer = nil;
}
@end

#pragma mark - Helpers

static NSString *userAgent(void) {
    static NSString *agent;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *name = [[NSProcessInfo processInfo] processName] ?: @"Unknown";
        NSString *version = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
        struct utsname u;
        uname(&u);
        name = [name stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]] ?: @"Unknown";
        agent = [NSString stringWithFormat:@"%@%@%@ Darwin/%s", name, version ? @"/" : @"",
                                           version ?: @"", u.release];
    });
    return agent;
}

static NSString *headerValue(NSDictionary *headers, NSString *name) {
    for (NSString *key in headers)
        if ([key caseInsensitiveCompare:name] == NSOrderedSame)
            return headers[key];
    return nil;
}

static void removeHeader(NSMutableURLRequest *request, NSString *name) {
    for (NSString *key in [[request allHTTPHeaderFields] allKeys])
        if ([key caseInsensitiveCompare:name] == NSOrderedSame)
            [request setValue:nil forHTTPHeaderField:key];
}

static NSString *MIMETypeForPath(NSString *path) {
    static NSDictionary *types;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        types = @{
            @"txt": @"text/plain", @"text": @"text/plain", @"html": @"text/html", @"htm": @"text/html",
            @"css": @"text/css", @"js": @"text/javascript", @"json": @"application/json",
            @"xml": @"application/xml", @"plist": @"application/x-plist", @"png": @"image/png",
            @"jpg": @"image/jpeg", @"jpeg": @"image/jpeg", @"gif": @"image/gif", @"svg": @"image/svg+xml",
            @"pdf": @"application/pdf", @"zip": @"application/zip", @"gz": @"application/x-gzip",
            @"tar": @"application/x-tar", @"dmg": @"application/x-apple-diskimage",
            @"rtf": @"text/rtf", @"csv": @"text/csv",
        };
    });
    return types[[[path pathExtension] lowercaseString]] ?: @"application/octet-stream";
}

// A data: URL: its bytes and MIME type, or nil if it is malformed.
static NSData *decodeDataURL(NSURL *url, NSString **mimeType, NSString **encoding) {
    NSString *string = [url absoluteString];
    NSRange comma = [string rangeOfString:@","];
    if (![string hasPrefix:@"data:"] || comma.location == NSNotFound)
        return nil;
    NSString *header = [string substringWithRange:NSMakeRange(5, comma.location - 5)];
    NSString *payload = [string substringFromIndex:NSMaxRange(comma)];
    NSArray *parts = [header componentsSeparatedByString:@";"];
    BOOL base64 = [[parts lastObject] caseInsensitiveCompare:@"base64"] == NSOrderedSame;
    NSString *type = [parts count] > 0 && [parts[0] length] > 0 ? [parts[0] lowercaseString] : @"text/plain";
    NSString *charset = [parts count] > 0 && [parts[0] length] > 0 ? nil : @"US-ASCII";
    for (NSString *part in parts)
        if ([[part lowercaseString] hasPrefix:@"charset="])
            charset = [part substringFromIndex:8];
    *mimeType = type;
    *encoding = charset;
    NSString *decoded = [payload stringByRemovingPercentEncoding] ?: payload;
    if (base64)
        return [[NSData alloc] initWithBase64EncodedString:decoded
                                                   options:NSDataBase64DecodingIgnoreUnknownCharacters];
    return [decoded dataUsingEncoding:NSUTF8StringEncoding];
}

static size_t onHeader(char *buffer, size_t size, size_t count, void *task);
static size_t onBody(char *buffer, size_t size, size_t count, void *task);
static size_t onReadBody(char *buffer, size_t size, size_t count, void *task);
static int onSeekBody(void *task, curl_off_t offset, int origin);
static int onProgress(void *task, curl_off_t downloadTotal, curl_off_t downloaded,
                      curl_off_t uploadTotal, curl_off_t uploaded);

#pragma mark - Task

@implementation NSURLSessionTask {
@package
    pthread_mutex_t _lock;
    NSURLSession *_session;             // retained until the task finishes
    NSURLSessionTaskKind _kind;
    NSUInteger _taskIdentifier;
    NSURLRequest *_originalRequest;
    NSURLRequest *_currentRequest;
    NSURLResponse *_response;
    NSError *_error;
    NSURLSessionTaskState _state;
    BOOL _started;
    int64_t _countOfBytesReceived;
    int64_t _countOfBytesSent;
    int64_t _countOfBytesExpectedToSend;
    int64_t _countOfBytesExpectedToReceive;
    NSString *_taskDescription;
    NSDate *_earliestBeginDate;
    float _priority;
    NSProgress *_progress;

    // The completion handler (data: NSData, response, error; download:
    // location, response, error), if the task was made with one.
    id _completionHandler;

    // The body to send: data, a file or a stream.
    NSData *_uploadData;
    NSURL *_uploadFile;
    NSInputStream *_uploadStream;
    BOOL _streamed;

    // Engine thread only.
    CURL *_easy;
    struct curl_slist *_headerList;
    NSMutableData *_collected;
    NSInteger _status;
    NSMutableDictionary *_headers;
    NSString *_lastHeader;
    NSURLSessionBodyMode _bodyMode;
    NSMutableData *_held;
    NSHTTPURLResponse *_heldResponse;
    BOOL _awaitingDisposition;
    NSData *_deferredBody;              // delivered once the delegate allows the response
    BOOL _deferredFinish;
    NSError *_failure;
    BOOL _finished;
    BOOL _cancelRequested;
    NSInteger _redirects;
    NSInteger _authFailures;
    NSURLCredential *_credential;
    NSUInteger _uploadOffset;
    FILE *_uploadFileHandle;
    BOOL _sendBody;
    int64_t _lastSent;
    NSURLSessionTask *_sink;            // a data task that became a download
    FILE *_downloadFile;
    NSString *_downloadPath;
    BOOL _keepDownloadFile;
    int64_t _resumeOffset;
    NSDictionary *_resumeInfo;
}

- (instancetype)_initWithSession:(NSURLSession *)session request:(NSURLRequest *)request
                            kind:(NSURLSessionTaskKind)kind {
    if ((self = [super init]) == nil)
        return nil;
    pthread_mutex_init(&_lock, NULL);
    _session = session;
    _kind = kind;
    _taskIdentifier = [session _nextTaskIdentifier];
    _originalRequest = [request copy];
    _currentRequest = [request copy];
    _state = NSURLSessionTaskStateSuspended;
    _priority = NSURLSessionTaskPriorityDefault;
    _countOfBytesExpectedToReceive = NSURLSessionTransferSizeUnknown;
    _countOfBytesExpectedToSend = 0;
    _progress = [NSProgress progressWithTotalUnitCount:-1];
    return self;
}

- (void)dealloc {
    if (_uploadFileHandle != NULL)
        fclose(_uploadFileHandle);
    if (_downloadFile != NULL)
        fclose(_downloadFile);
    pthread_mutex_destroy(&_lock);
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

#define LOCKED(expression) ({ pthread_mutex_lock(&_lock); __typeof__(expression) _v = (expression); pthread_mutex_unlock(&_lock); _v; })

- (NSUInteger)taskIdentifier { return _taskIdentifier; }
- (NSURLRequest *)originalRequest { return _originalRequest; }
- (NSURLRequest *)currentRequest { return LOCKED(_currentRequest); }
- (NSURLResponse *)response { return LOCKED(_response); }
- (NSError *)error { return LOCKED(_error); }
- (NSURLSessionTaskState)state { return LOCKED(_state); }
- (int64_t)countOfBytesReceived { return LOCKED(_countOfBytesReceived); }
- (int64_t)countOfBytesSent { return LOCKED(_countOfBytesSent); }
- (int64_t)countOfBytesExpectedToSend { return LOCKED(_countOfBytesExpectedToSend); }
- (int64_t)countOfBytesExpectedToReceive { return LOCKED(_countOfBytesExpectedToReceive); }
- (NSProgress *)progress { return _progress; }
- (NSString *)taskDescription { return LOCKED(_taskDescription); }
- (void)setTaskDescription:(NSString *)description {
    NSString *copy = [description copy];
    pthread_mutex_lock(&_lock); _taskDescription = copy; pthread_mutex_unlock(&_lock);
}
- (NSDate *)earliestBeginDate { return LOCKED(_earliestBeginDate); }
- (void)setEarliestBeginDate:(NSDate *)date {
    NSDate *copy = [date copy];
    pthread_mutex_lock(&_lock); _earliestBeginDate = copy; pthread_mutex_unlock(&_lock);
}
- (float)priority { return LOCKED(_priority); }
- (void)setPriority:(float)priority {
    pthread_mutex_lock(&_lock); _priority = priority < 0 ? 0 : priority > 1 ? 1 : priority; pthread_mutex_unlock(&_lock);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p> { taskIdentifier: %lu, state: %ld }",
                                      NSStringFromClass([self class]), (__bridge void *)self,
                                      (unsigned long)_taskIdentifier, (long)[self state]];
}

#pragma mark Control

- (void)resume {
    pthread_mutex_lock(&_lock);
    if (_state != NSURLSessionTaskStateSuspended) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    _state = NSURLSessionTaskStateRunning;
    BOOL first = !_started;
    _started = YES;
    pthread_mutex_unlock(&_lock);

    if (!engineStart()) {
        [self _finishOnEngine:NO error:URLError(NSURLErrorUnknown, [_originalRequest URL])];
        return;
    }
    NSDate *begin = [self earliestBeginDate];
    if (first && begin != nil && [begin timeIntervalSinceNow] > 0) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)([begin timeIntervalSinceNow] * NSEC_PER_SEC)),
                       dispatch_get_global_queue(0, 0), ^{
            enginePerform(^{ [self _start]; });
        });
        return;
    }
    enginePerform(^{
        if (first)
            [self _start];
        else if (self->_easy != NULL && !self->_awaitingDisposition)
            curl.easy_pause(self->_easy, CURLPAUSE_CONT);
    });
}

- (void)suspend {
    pthread_mutex_lock(&_lock);
    BOOL running = _state == NSURLSessionTaskStateRunning;
    if (running)
        _state = NSURLSessionTaskStateSuspended;
    pthread_mutex_unlock(&_lock);
    if (running && _started)
        enginePerform(^{
            if (self->_easy != NULL)
                curl.easy_pause(self->_easy, CURLPAUSE_ALL);
        });
}

- (void)cancel {
    pthread_mutex_lock(&_lock);
    BOOL cancel = _state == NSURLSessionTaskStateRunning || _state == NSURLSessionTaskStateSuspended;
    if (cancel)
        _state = NSURLSessionTaskStateCanceling;
    pthread_mutex_unlock(&_lock);
    if (!cancel)
        return;
    if (!engineStart()) {
        [self _finishOnEngine:NO error:URLError(NSURLErrorCancelled, [_originalRequest URL])];
        return;
    }
    enginePerform(^{ [self _cancelTransfer]; });
}

#pragma mark Delegate

- (id)_delegate {
    return [_session delegate];
}

- (BOOL)_delegateResponds:(SEL)selector {
    id delegate = [_session delegate];
    return delegate != nil && [delegate respondsToSelector:selector];
}

- (void)_onDelegateQueue:(void (^)(void))block {
    [[_session delegateQueue] addOperationWithBlock:block];
}

#pragma mark Engine: starting

- (void)_start {
    if (_finished)
        return;
    if (_cancelRequested) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorCancelled, [_currentRequest URL])];
        return;
    }
    NSURL *url = [_currentRequest URL];
    NSString *scheme = [[url scheme] lowercaseString];

    if ([scheme isEqualToString:@"data"]) {
        [self _loadDataURL:url];
        return;
    }
    if (!([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"] ||
          [scheme isEqualToString:@"file"])) {
        [self _finishOnEngine:YES error:URLError(url ? NSURLErrorUnsupportedURL : NSURLErrorBadURL, url)];
        return;
    }
    // An upload from a stream asks the delegate for the stream.
    if (_streamed && _uploadStream == nil) {
        if (![self _delegateResponds:@selector(URLSession:task:needNewBodyStream:)]) {
            [self _finishOnEngine:YES error:URLError(NSURLErrorRequestBodyStreamExhausted, url)];
            return;
        }
        NSURLSession *session = _session;
        [self _onDelegateQueue:^{
            __block BOOL answered = NO;
            [(id<NSURLSessionTaskDelegate>)[session delegate] URLSession:session task:self
                                                      needNewBodyStream:^(NSInputStream *stream) {
                if (answered) return;
                answered = YES;
                enginePerform(^{
                    if (stream == nil) {
                        [self _finishOnEngine:YES error:URLError(NSURLErrorRequestBodyStreamExhausted, url)];
                        return;
                    }
                    self->_uploadStream = stream;
                    [stream open];
                    [self _startTransfer];
                });
            }];
        }];
        return;
    }
    [self _startTransfer];
}

- (void)_loadDataURL:(NSURL *)url {
    NSString *mimeType = nil, *encoding = nil;
    NSData *data = decodeDataURL(url, &mimeType, &encoding);
    if (data == nil) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorBadURL, url)];
        return;
    }
    NSURLResponse *response = [[NSURLResponse alloc] initWithURL:url MIMEType:mimeType
                                           expectedContentLength:(NSInteger)[data length]
                                                textEncodingName:encoding];
    [self _deliverResponse:response];
    if (_awaitingDisposition) {
        _deferredBody = data;
        _deferredFinish = YES;
        return;
    }
    [self _deliverBody:[data bytes] length:[data length]];
    [self _finishOnEngine:YES error:_failure];
}

// The headers of the request: the request's, then the configuration's,
// then the defaults.
- (struct curl_slist *)_headerListForRequest:(NSURLRequest *)request {
    NSMutableDictionary *headers = [NSMutableDictionary dictionary];
    NSMutableDictionary *names = [NSMutableDictionary dictionary];   // lower case → name
    void (^add)(NSDictionary *, BOOL) = ^(NSDictionary *fields, BOOL replace) {
        for (NSString *name in fields) {
            NSString *lower = [name lowercaseString];
            if (names[lower] != nil && !replace)
                continue;
            if (names[lower] != nil)
                [headers removeObjectForKey:names[lower]];
            names[lower] = name;
            headers[name] = [fields[name] description];
        }
    };
    add([request allHTTPHeaderFields], YES);
    NSHTTPCookieStorage *cookies = [self _cookieStorage];
    if (cookies != nil)
        add([NSHTTPCookie requestHeaderFieldsWithCookies:[cookies cookiesForURL:[request URL]]], NO);
    add([[_session configuration] HTTPAdditionalHeaders], NO);
    add(@{ @"Accept": @"*/*", @"Accept-Language": @"en-us", @"User-Agent": userAgent() }, NO);

    struct curl_slist *list = NULL;
    for (NSString *name in headers) {
        NSString *value = headers[name];
        NSString *line = [value length] > 0 ? [NSString stringWithFormat:@"%@: %@", name, value]
                                            : [NSString stringWithFormat:@"%@;", name];
        list = curl.slist_append(list, [line UTF8String]);
    }
    // No "Expect: 100-continue" before a large body.
    if (names[@"expect"] == nil)
        list = curl.slist_append(list, "Expect:");
    return list;
}

- (void)_startTransfer {
    NSURLRequest *request = _currentRequest;
    NSURL *url = [request URL];
    NSURLSessionConfiguration *configuration = [_session configuration];
    BOOL isFile = [url isFileURL];

    _status = 0;
    _headers = [NSMutableDictionary dictionary];
    _lastHeader = nil;
    _bodyMode = NSURLSessionBodyDeliver;
    _held = nil;
    _heldResponse = nil;
    _lastSent = 0;

    if (isFile) {
        struct stat st;
        NSString *path = [url path];
        if (stat([path fileSystemRepresentation], &st) != 0) {
            [self _finishOnEngine:YES error:URLError(errno == EACCES ? NSURLErrorNoPermissionsToReadFile
                                                                      : NSURLErrorFileDoesNotExist, url)];
            return;
        }
        if (S_ISDIR(st.st_mode)) {
            [self _finishOnEngine:YES error:URLError(NSURLErrorFileIsDirectory, url)];
            return;
        }
        NSURLResponse *response = [[NSURLResponse alloc] initWithURL:url MIMEType:MIMETypeForPath(path)
                                               expectedContentLength:(NSInteger)st.st_size
                                                    textEncodingName:nil];
        [self _deliverResponse:response];
    }

    CURL *easy = curl.easy_init();
    if (easy == NULL) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorUnknown, url)];
        return;
    }
    _easy = easy;
    curl.easy_setopt(easy, CURLOPT_URL, [[url absoluteString] UTF8String]);
    curl.easy_setopt(easy, CURLOPT_PRIVATE, (__bridge void *)self);
    curl.easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl.easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl.easy_setopt(easy, CURLOPT_HEADERFUNCTION, onHeader);
    curl.easy_setopt(easy, CURLOPT_HEADERDATA, (__bridge void *)self);
    curl.easy_setopt(easy, CURLOPT_WRITEFUNCTION, onBody);
    curl.easy_setopt(easy, CURLOPT_WRITEDATA, (__bridge void *)self);
    curl.easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl.easy_setopt(easy, CURLOPT_XFERINFODATA, (__bridge void *)self);
    curl.easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);

    if (!isFile) {
        _headerList = [self _headerListForRequest:request];
        curl.easy_setopt(easy, CURLOPT_HTTPHEADER, _headerList);
        if (headerValue([request allHTTPHeaderFields], @"Accept-Encoding") == nil)
            curl.easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");

        NSString *method = [[request HTTPMethod] uppercaseString] ?: @"GET";
        // The body: the upload task's, else the request's.
        _sendBody = NO;
        _uploadOffset = 0;
        int64_t length = -1;
        if (_kind == NSURLSessionTaskKindUpload && ![method isEqualToString:@"GET"] &&
            ![method isEqualToString:@"HEAD"]) {
            _sendBody = YES;
            if (_uploadData != nil)
                length = (int64_t)[_uploadData length];
            else if (_uploadFile != nil) {
                struct stat st;
                if (_uploadFileHandle != NULL)
                    fclose(_uploadFileHandle);
                _uploadFileHandle = fopen([[_uploadFile path] fileSystemRepresentation], "rb");
                if (_uploadFileHandle == NULL || fstat(fileno(_uploadFileHandle), &st) != 0) {
                    [self _finishOnEngine:YES error:URLError(NSURLErrorFileDoesNotExist, _uploadFile)];
                    return;
                }
                length = (int64_t)st.st_size;
            }
        } else if ([request HTTPBody] != nil && ![method isEqualToString:@"GET"] &&
                   ![method isEqualToString:@"HEAD"]) {
            _sendBody = YES;
            _uploadData = [request HTTPBody];
            length = (int64_t)[_uploadData length];
        } else if ([request HTTPBodyStream] != nil && ![method isEqualToString:@"GET"] &&
                   ![method isEqualToString:@"HEAD"]) {
            _sendBody = YES;
            if (_uploadStream == nil) {
                _uploadStream = [request HTTPBodyStream];
                [_uploadStream open];
            }
        }
        NSString *declared = headerValue([request allHTTPHeaderFields], @"Content-Length");
        if (length < 0 && declared != nil)
            length = [declared longLongValue];

        if ([method isEqualToString:@"HEAD"]) {
            curl.easy_setopt(easy, CURLOPT_NOBODY, 1L);
        } else if (_sendBody || [method isEqualToString:@"POST"]) {
            curl.easy_setopt(easy, CURLOPT_POST, 1L);
            curl.easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)(_sendBody ? length : 0));
            if (_sendBody) {
                curl.easy_setopt(easy, CURLOPT_READFUNCTION, onReadBody);
                curl.easy_setopt(easy, CURLOPT_READDATA, (__bridge void *)self);
                curl.easy_setopt(easy, CURLOPT_SEEKFUNCTION, onSeekBody);
                curl.easy_setopt(easy, CURLOPT_SEEKDATA, (__bridge void *)self);
            }
            if (![method isEqualToString:@"POST"])
                curl.easy_setopt(easy, CURLOPT_CUSTOMREQUEST, [method UTF8String]);
        } else if (![method isEqualToString:@"GET"]) {
            curl.easy_setopt(easy, CURLOPT_CUSTOMREQUEST, [method UTF8String]);
        }
        pthread_mutex_lock(&_lock);
        _countOfBytesExpectedToSend = _sendBody ? (length >= 0 ? length : NSURLSessionTransferSizeUnknown) : 0;
        pthread_mutex_unlock(&_lock);

        // Timeouts: the request's when it has its own, else the configuration's.
        NSTimeInterval idle = [configuration timeoutIntervalForRequest];
        if ([request timeoutInterval] > 0 && [request timeoutInterval] != 60)
            idle = [request timeoutInterval];
        if (idle > 0) {
            curl.easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, (long)(idle * 1000));
            curl.easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl.easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, (long)(idle < 1 ? 1 : idle));
        }
        NSTimeInterval resource = [configuration timeoutIntervalForResource];
        if (resource > 0 && resource < 24 * 24 * 3600)
            curl.easy_setopt(easy, CURLOPT_TIMEOUT_MS, (long)(resource * 1000));

        if ([configuration TLSMinimumSupportedProtocolVersion] >= tls_protocol_version_TLSv13)
            curl.easy_setopt(easy, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1_3);
        else if ([configuration TLSMinimumSupportedProtocolVersion] >= tls_protocol_version_TLSv12)
            curl.easy_setopt(easy, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1_2);

        NSDictionary *proxies = [configuration connectionProxyDictionary];
        if (proxies != nil) {
            BOOL https = [[[url scheme] lowercaseString] isEqualToString:@"https"];
            NSString *host = proxies[https ? @"HTTPSProxy" : @"HTTPProxy"];
            NSNumber *port = proxies[https ? @"HTTPSPort" : @"HTTPPort"];
            NSNumber *enabled = proxies[https ? @"HTTPSEnable" : @"HTTPEnable"];
            if (host != nil && (enabled == nil || [enabled boolValue])) {
                NSString *proxy = port ? [NSString stringWithFormat:@"%@:%@", host, port] : host;
                curl.easy_setopt(easy, CURLOPT_PROXY, [proxy UTF8String]);
            } else {
                curl.easy_setopt(easy, CURLOPT_PROXY, "");
            }
        }

        if (_credential != nil && [_credential user] != nil) {
            curl.easy_setopt(easy, CURLOPT_USERNAME, [[_credential user] UTF8String]);
            curl.easy_setopt(easy, CURLOPT_PASSWORD, [[_credential password] ?: @"" UTF8String]);
            curl.easy_setopt(easy, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
        }
        if (_resumeOffset > 0)
            curl.easy_setopt(easy, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)_resumeOffset);
    }

    if (curl.multi_add_handle(engineMulti, easy) != CURLM_OK) {
        [self _releaseEasy];
        [self _finishOnEngine:YES error:URLError(NSURLErrorUnknown, url)];
        return;
    }
    if ([self state] == NSURLSessionTaskStateSuspended || _awaitingDisposition)
        curl.easy_pause(easy, CURLPAUSE_ALL);
}

- (void)_releaseEasy {
    if (_easy != NULL) {
        curl.multi_remove_handle(engineMulti, _easy);
        curl.easy_cleanup(_easy);
        _easy = NULL;
    }
    if (_headerList != NULL) {
        curl.slist_free_all(_headerList);
        _headerList = NULL;
    }
}

#pragma mark Engine: receiving

- (size_t)_headerLine:(const char *)buffer length:(size_t)length {
    if ([[_currentRequest URL] isFileURL])
        return length;
    NSString *line = [[NSString alloc] initWithBytes:buffer length:length encoding:NSUTF8StringEncoding];
    if (line == nil)
        line = [[NSString alloc] initWithBytes:buffer length:length encoding:NSISOLatin1StringEncoding];
    line = [line stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\r\n"]];

    if ([line hasPrefix:@"HTTP/"]) {
        NSArray *parts = [line componentsSeparatedByString:@" "];
        _status = [parts count] > 1 ? [parts[1] integerValue] : 0;
        _headers = [NSMutableDictionary dictionary];
        _lastHeader = nil;
        // A new response (curl authenticating again, after a 401 it held).
        _bodyMode = NSURLSessionBodyDeliver;
        _held = nil;
        _heldResponse = nil;
        return length;
    }
    if ([line length] == 0) {
        // The end of a 1xx response's headers: the real one follows.
        if (_status >= 100 && _status < 200)
            return length;
        return [self _headersComplete] ? length : 0;
    }
    if (([line hasPrefix:@" "] || [line hasPrefix:@"\t"]) && _lastHeader != nil) {
        NSString *folded = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        _headers[_lastHeader] = [NSString stringWithFormat:@"%@ %@", _headers[_lastHeader], folded];
        return length;
    }
    NSRange colon = [line rangeOfString:@":"];
    if (colon.location == NSNotFound)
        return length;
    NSString *name = [[line substringToIndex:colon.location]
                         stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSString *value = [[line substringFromIndex:NSMaxRange(colon)]
                          stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    for (NSString *existing in [_headers allKeys])
        if ([existing caseInsensitiveCompare:name] == NSOrderedSame) {
            value = [NSString stringWithFormat:@"%@, %@", _headers[existing], value];
            name = existing;
            break;
        }
    _headers[name] = value;
    _lastHeader = name;
    return length;
}

// The cookie store this request reads and fills, or nil.
- (NSHTTPCookieStorage *)_cookieStorage {
    NSURLSessionConfiguration *configuration = [_session configuration];
    if (![configuration HTTPShouldSetCookies] || ![_currentRequest HTTPShouldHandleCookies])
        return nil;
    return [configuration HTTPCookieStorage];
}

// NO stops the transfer (with _failure set).
- (BOOL)_headersComplete {
    NSURL *url = [_currentRequest URL];
    // Every response may set cookies, redirects included.
    NSHTTPCookieStorage *cookies = [self _cookieStorage];
    if (cookies != nil) {
        NSArray *set = [NSHTTPCookie cookiesWithResponseHeaderFields:_headers forURL:url];
        if ([set count] > 0)
            [cookies _setCookies:set forURL:url mainDocumentURL:[_currentRequest mainDocumentURL]
                          policy:[[_session configuration] HTTPCookieAcceptPolicy]];
    }
    NSHTTPURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:_status
                                                             HTTPVersion:@"HTTP/1.1"
                                                            headerFields:[_headers copy]];
    NSString *location = headerValue(_headers, @"Location");
    BOOL redirect = (_status == 301 || _status == 302 || _status == 303 || _status == 307 ||
                     _status == 308) && location != nil;
    if (redirect) {
        if (_redirects >= 16) {
            _failure = URLError(NSURLErrorHTTPTooManyRedirects, url);
            return NO;
        }
        _bodyMode = NSURLSessionBodyHold;
        _held = [NSMutableData data];
        _heldResponse = response;
        return YES;
    }
    if ((_status == 401 || _status == 407) &&
        headerValue(_headers, _status == 401 ? @"WWW-Authenticate" : @"Proxy-Authenticate") != nil) {
        _bodyMode = NSURLSessionBodyHold;
        _held = [NSMutableData data];
        _heldResponse = response;
        return YES;
    }
    // A resumed download: 206 continues the file, 200 starts it again.
    if (_resumeOffset > 0) {
        int64_t offset = _status == 206 ? _resumeOffset : 0;
        if (_status != 206 && _downloadFile != NULL) {
            if (ftruncate(fileno(_downloadFile), 0) != 0) { /* written over below */ }
            fseeko(_downloadFile, 0, SEEK_SET);
        }
        int64_t expected = [response expectedContentLength] >= 0 ? [response expectedContentLength] + offset
                                                                 : NSURLSessionTransferSizeUnknown;
        pthread_mutex_lock(&_lock);
        _countOfBytesReceived = offset;
        pthread_mutex_unlock(&_lock);
        if ([self _delegateResponds:@selector(URLSession:downloadTask:didResumeAtOffset:expectedTotalBytes:)]) {
            NSURLSession *session = _session;
            [self _onDelegateQueue:^{
                [(id<NSURLSessionDownloadDelegate>)[session delegate] URLSession:session
                    downloadTask:(NSURLSessionDownloadTask *)self didResumeAtOffset:offset expectedTotalBytes:expected];
            }];
        }
        _resumeOffset = offset;
    }
    [self _deliverResponse:response];
    return YES;
}

- (NSURLSessionTask *)_target {
    return _sink ?: self;
}

- (BOOL)_isDownload {
    return _sink != nil || _kind == NSURLSessionTaskKindDownload;
}

- (void)_deliverResponse:(NSURLResponse *)response {
    long long expected = [response expectedContentLength];
    pthread_mutex_lock(&_lock);
    _response = response;
    _countOfBytesExpectedToReceive = expected >= 0 ? expected + _resumeOffset : NSURLSessionTransferSizeUnknown;
    pthread_mutex_unlock(&_lock);
    [_progress setTotalUnitCount:expected >= 0 ? expected + _resumeOffset : -1];

    if ([self _isDownload]) {
        [self _openDownloadFile];
        return;
    }
    if (_completionHandler != nil ||
        ![self _delegateResponds:@selector(URLSession:dataTask:didReceiveResponse:completionHandler:)])
        return;

    _awaitingDisposition = YES;
    NSURLSession *session = _session;
    [self _onDelegateQueue:^{
        __block BOOL answered = NO;
        [(id<NSURLSessionDataDelegate>)[session delegate] URLSession:session
            dataTask:(NSURLSessionDataTask *)self didReceiveResponse:response
            completionHandler:^(NSURLSessionResponseDisposition disposition) {
                if (answered) return;
                answered = YES;
                enginePerform(^{ [self _responseDisposition:disposition]; });
            }];
    }];
}

- (void)_responseDisposition:(NSURLSessionResponseDisposition)disposition {
    if (_finished || !_awaitingDisposition)
        return;
    _awaitingDisposition = NO;
    switch (disposition) {
    case NSURLSessionResponseAllow:
        break;
    case NSURLSessionResponseBecomeDownload:
        [self _becomeDownload];
        break;
    default:
        // Cancel; a stream task is not supported: also cancelled.
        [self _cancelTransfer];
        return;
    }
    if (_deferredBody != nil) {
        NSData *body = _deferredBody;
        _deferredBody = nil;
        [self _deliverBody:[body bytes] length:[body length]];
    }
    if (_deferredFinish) {
        [self _finishOnEngine:YES error:_failure];
        return;
    }
    if (_easy != NULL && [self state] == NSURLSessionTaskStateRunning)
        curl.easy_pause(_easy, CURLPAUSE_CONT);
}

// The rest of the body goes to a new download task.
- (void)_becomeDownload {
    NSURLSessionDownloadTask *download =
        [[NSURLSessionDownloadTask alloc] _initWithSession:_session request:_currentRequest
                                                      kind:NSURLSessionTaskKindDownload];
    download->_state = NSURLSessionTaskStateRunning;
    download->_started = YES;
    download->_response = _response;
    download->_countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
    [_session _addTask:download];
    _sink = download;
    [download _openDownloadFile];
    NSURLSession *session = _session;
    if ([self _delegateResponds:@selector(URLSession:dataTask:didBecomeDownloadTask:)])
        [self _onDelegateQueue:^{
            [(id<NSURLSessionDataDelegate>)[session delegate] URLSession:session
                dataTask:(NSURLSessionDataTask *)self didBecomeDownloadTask:download];
        }];
}

- (void)_openDownloadFile {
    NSURLSessionTask *target = [self _target];
    if (target->_downloadFile != NULL)
        return;
    if (target->_downloadPath != nil && target->_resumeOffset > 0) {
        target->_downloadFile = fopen([target->_downloadPath fileSystemRepresentation], "r+b");
        if (target->_downloadFile != NULL) {
            fseeko(target->_downloadFile, target->_resumeOffset, SEEK_SET);
            return;
        }
    }
    NSString *template = [NSTemporaryDirectory() stringByAppendingPathComponent:@"NSURLSessionDownload_XXXXXX.tmp"];
    char path[1024];
    strlcpy(path, [template fileSystemRepresentation], sizeof(path));
    int fd = mkstemps(path, 4);
    if (fd < 0) {
        _failure = URLError(NSURLErrorCannotCreateFile, [_currentRequest URL]);
        return;
    }
    target->_downloadFile = fdopen(fd, "w+b");
    target->_downloadPath = [NSString stringWithUTF8String:path];
}

- (size_t)_body:(const char *)buffer length:(size_t)length {
    if (_failure != nil)
        return 0;
    if (_bodyMode == NSURLSessionBodyHold) {
        [_held appendBytes:buffer length:length];
        return length;
    }
    if (_awaitingDisposition)
        return CURL_WRITEFUNC_PAUSE;
    [self _deliverBody:buffer length:length];
    return _failure != nil ? 0 : length;
}

- (void)_deliverBody:(const void *)buffer length:(size_t)length {
    if (length == 0)
        return;
    NSURLSessionTask *target = [self _target];
    pthread_mutex_lock(&target->_lock);
    target->_countOfBytesReceived += (int64_t)length;
    int64_t received = target->_countOfBytesReceived;
    int64_t expected = target->_countOfBytesExpectedToReceive;
    pthread_mutex_unlock(&target->_lock);
    [target->_progress setCompletedUnitCount:received];

    NSURLSession *session = _session;
    if ([self _isDownload]) {
        if (target->_downloadFile == NULL || fwrite(buffer, 1, length, target->_downloadFile) != length) {
            _failure = URLError(NSURLErrorCannotWriteToFile, [_currentRequest URL]);
            return;
        }
        if (target->_completionHandler == nil &&
            [self _delegateResponds:@selector(URLSession:downloadTask:didWriteData:totalBytesWritten:totalBytesExpectedToWrite:)])
            [self _onDelegateQueue:^{
                [(id<NSURLSessionDownloadDelegate>)[session delegate] URLSession:session
                    downloadTask:(NSURLSessionDownloadTask *)target didWriteData:(int64_t)length
                    totalBytesWritten:received totalBytesExpectedToWrite:expected];
            }];
        return;
    }
    if (_completionHandler != nil) {
        if (_collected == nil)
            _collected = [NSMutableData data];
        [_collected appendBytes:buffer length:length];
        return;
    }
    if ([self _delegateResponds:@selector(URLSession:dataTask:didReceiveData:)]) {
        NSData *data = [NSData dataWithBytes:buffer length:length];
        [self _onDelegateQueue:^{
            [(id<NSURLSessionDataDelegate>)[session delegate] URLSession:session
                dataTask:(NSURLSessionDataTask *)self didReceiveData:data];
        }];
    }
}

- (int)_uploaded:(int64_t)sent total:(int64_t)total {
    if (sent <= _lastSent)
        return 0;
    int64_t delta = sent - _lastSent;
    _lastSent = sent;
    pthread_mutex_lock(&_lock);
    _countOfBytesSent = sent;
    int64_t expected = _countOfBytesExpectedToSend;
    pthread_mutex_unlock(&_lock);
    if ([self _delegateResponds:@selector(URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend:)]) {
        NSURLSession *session = _session;
        [self _onDelegateQueue:^{
            [(id<NSURLSessionTaskDelegate>)[session delegate] URLSession:session task:self
                didSendBodyData:delta totalBytesSent:sent totalBytesExpectedToSend:expected];
        }];
    }
    return 0;
}

- (size_t)_readBody:(char *)buffer length:(size_t)length {
    if (_uploadData != nil) {
        NSUInteger left = [_uploadData length] - _uploadOffset;
        size_t n = left < length ? left : length;
        memcpy(buffer, (const char *)[_uploadData bytes] + _uploadOffset, n);
        _uploadOffset += n;
        return n;
    }
    if (_uploadFileHandle != NULL) {
        size_t n = fread(buffer, 1, length, _uploadFileHandle);
        return (n == 0 && ferror(_uploadFileHandle)) ? CURL_READFUNC_ABORT : n;
    }
    if (_uploadStream != nil) {
        NSInteger n = [_uploadStream read:(uint8_t *)buffer maxLength:length];
        return n < 0 ? CURL_READFUNC_ABORT : (size_t)n;
    }
    return 0;
}

// Sent again from the start (after a redirect or a challenge).
- (int)_seekBody:(curl_off_t)offset origin:(int)origin {
    if (origin != SEEK_SET)
        return CURL_SEEKFUNC_CANTSEEK;
    if (_uploadData != nil && offset >= 0 && (NSUInteger)offset <= [_uploadData length]) {
        _uploadOffset = (NSUInteger)offset;
        return CURL_SEEKFUNC_OK;
    }
    if (_uploadFileHandle != NULL)
        return fseeko(_uploadFileHandle, offset, SEEK_SET) == 0 ? CURL_SEEKFUNC_OK : CURL_SEEKFUNC_FAIL;
    return CURL_SEEKFUNC_CANTSEEK;
}

#pragma mark Engine: completion

- (void)_transferDidComplete:(CURLcode)code {
    [self _releaseEasy];
    if (_finished)
        return;
    if (_cancelRequested) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorCancelled, [_currentRequest URL])];
        return;
    }
    if (_failure != nil) {
        [self _finishOnEngine:YES error:_failure];
        return;
    }
    if (code != CURLE_OK) {
        [self _finishOnEngine:YES error:URLError(URLErrorCodeForCurl(code), [_currentRequest URL])];
        return;
    }
    if (_bodyMode == NSURLSessionBodyHold) {
        if (_status == 401 || _status == 407)
            [self _challenge];
        else
            [self _redirect];
        return;
    }
    if (_response == nil) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorBadServerResponse, [_currentRequest URL])];
        return;
    }
    if (_awaitingDisposition) {
        _deferredFinish = YES;
        return;
    }
    [self _finishOnEngine:YES error:nil];
}

// The held response is the final one after all.
- (void)_deliverHeldResponse {
    NSHTTPURLResponse *response = _heldResponse;
    NSData *body = _held;
    _bodyMode = NSURLSessionBodyDeliver;
    _held = nil;
    _heldResponse = nil;
    [self _deliverResponse:response];
    if (_awaitingDisposition) {
        _deferredBody = body;
        _deferredFinish = YES;
        return;
    }
    [self _deliverBody:[body bytes] length:[body length]];
    [self _finishOnEngine:YES error:_failure];
}

- (void)_redirect {
    NSHTTPURLResponse *response = _heldResponse;
    NSURL *current = [_currentRequest URL];
    NSString *location = [response valueForHTTPHeaderField:@"Location"];
    NSURL *target = location ? [[NSURL URLWithString:location relativeToURL:current] absoluteURL] : nil;
    if (target == nil) {
        [self _deliverHeldResponse];
        return;
    }
    NSMutableURLRequest *request = [_currentRequest mutableCopy];
    [request setURL:target];
    NSString *method = [[request HTTPMethod] uppercaseString] ?: @"GET";
    if ((_status == 303 && ![method isEqualToString:@"HEAD"]) ||
        ((_status == 301 || _status == 302) && [method isEqualToString:@"POST"])) {
        [request setHTTPMethod:@"GET"];
        [request setHTTPBody:nil];
        [request setHTTPBodyStream:nil];
        removeHeader(request, @"Content-Type");
        removeHeader(request, @"Content-Length");
        _kind = _kind == NSURLSessionTaskKindUpload ? NSURLSessionTaskKindData : _kind;
    }
    if (![[[target host] lowercaseString] isEqualToString:[[current host] lowercaseString]])
        removeHeader(request, @"Authorization");
    _redirects++;

    if (![self _delegateResponds:@selector(URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:)]) {
        [self _restartWithRequest:request];
        return;
    }
    NSURLSession *session = _session;
    [self _onDelegateQueue:^{
        __block BOOL answered = NO;
        [(id<NSURLSessionTaskDelegate>)[session delegate] URLSession:session task:self
            willPerformHTTPRedirection:response newRequest:request
            completionHandler:^(NSURLRequest *newRequest) {
                if (answered) return;
                answered = YES;
                enginePerform(^{
                    if (self->_cancelRequested || self->_finished) {
                        [self _finishOnEngine:YES error:URLError(NSURLErrorCancelled, [request URL])];
                    } else if (newRequest != nil) {
                        [self _restartWithRequest:newRequest];
                    } else {
                        [self _deliverHeldResponse];
                    }
                });
            }];
    }];
}

- (void)_restartWithRequest:(NSURLRequest *)request {
    pthread_mutex_lock(&_lock);
    _currentRequest = [request copy];
    pthread_mutex_unlock(&_lock);
    if (_uploadFileHandle != NULL)
        fseeko(_uploadFileHandle, 0, SEEK_SET);
    _uploadOffset = 0;
    // A stream cannot be read twice: the delegate gives a new one.
    if (_uploadStream != nil && [request HTTPMethod] != nil &&
        ![[[request HTTPMethod] uppercaseString] isEqualToString:@"GET"]) {
        _uploadStream = nil;
        _streamed = YES;
    } else {
        _uploadStream = nil;
    }
    [self _start];
}

static NSString *authenticationParameter(NSString *header, NSString *name) {
    NSString *pattern = [name stringByAppendingString:@"=\""];
    NSRange start = [header rangeOfString:pattern options:NSCaseInsensitiveSearch];
    if (start.location == NSNotFound)
        return nil;
    NSUInteger from = NSMaxRange(start);
    NSRange end = [header rangeOfString:@"\"" options:0 range:NSMakeRange(from, [header length] - from)];
    if (end.location == NSNotFound)
        return nil;
    return [header substringWithRange:NSMakeRange(from, end.location - from)];
}

- (void)_challenge {
    NSHTTPURLResponse *response = _heldResponse;
    NSURL *url = [_currentRequest URL];
    BOOL proxy = _status == 407;
    NSString *header = [response valueForHTTPHeaderField:proxy ? @"Proxy-Authenticate" : @"WWW-Authenticate"];
    NSString *scheme = [[[header componentsSeparatedByString:@" "] firstObject] lowercaseString];
    NSString *method = NSURLAuthenticationMethodHTTPBasic;
    if ([scheme isEqualToString:@"digest"]) method = NSURLAuthenticationMethodHTTPDigest;
    else if ([scheme isEqualToString:@"ntlm"]) method = NSURLAuthenticationMethodNTLM;
    else if ([scheme isEqualToString:@"negotiate"]) method = NSURLAuthenticationMethodNegotiate;
    NSString *protocol = [[url scheme] lowercaseString];
    int port = [url port] ? [[url port] intValue] : ([protocol isEqualToString:@"https"] ? 443 : 80);
    NSURLProtectionSpace *space = [[NSURLProtectionSpace alloc] initWithHost:[url host] port:port
                                                                    protocol:protocol
                                                                       realm:authenticationParameter(header, @"realm")
                                                        authenticationMethod:method];
    if (_authFailures >= 8) {
        [self _deliverHeldResponse];
        return;
    }

    void (^answer)(NSURLSessionAuthChallengeDisposition, NSURLCredential *) =
        ^(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
            enginePerform(^{ [self _challengeDisposition:disposition credential:credential]; });
        };
    if (![self _delegateResponds:@selector(URLSession:task:didReceiveChallenge:completionHandler:)]) {
        [self _challengeDisposition:NSURLSessionAuthChallengePerformDefaultHandling credential:nil];
        return;
    }
    _NSURLSessionChallengeSender *sender = [_NSURLSessionChallengeSender new];
    __block BOOL answered = NO;
    void (^once)(NSURLSessionAuthChallengeDisposition, NSURLCredential *) =
        ^(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
            if (answered) return;
            answered = YES;
            answer(disposition, credential);
        };
    sender.answer = once;
    NSURLAuthenticationChallenge *challenge =
        [[NSURLAuthenticationChallenge alloc] initWithProtectionSpace:space proposedCredential:_credential
                                                 previousFailureCount:(int)_authFailures
                                                      failureResponse:response error:nil sender:sender];
    NSURLSession *session = _session;
    [self _onDelegateQueue:^{
        [(id<NSURLSessionTaskDelegate>)[session delegate] URLSession:session task:self
            didReceiveChallenge:challenge completionHandler:once];
    }];
}

- (void)_challengeDisposition:(NSURLSessionAuthChallengeDisposition)disposition
                   credential:(NSURLCredential *)credential {
    if (_finished)
        return;
    if (_cancelRequested || disposition == NSURLSessionAuthChallengeCancelAuthenticationChallenge) {
        [self _finishOnEngine:YES error:URLError(NSURLErrorCancelled, [_currentRequest URL])];
        return;
    }
    if (disposition == NSURLSessionAuthChallengeUseCredential && [credential user] != nil) {
        _credential = credential;
        _authFailures++;
        [self _restartWithRequest:_currentRequest];
        return;
    }
    // Default handling, a rejected space, or no credential: the 401 is the
    // response.
    [self _deliverHeldResponse];
}

- (void)_cancelTransfer {
    if (_finished)
        return;
    _cancelRequested = YES;
    [self _releaseEasy];
    [self _finishOnEngine:YES error:URLError(NSURLErrorCancelled, [_currentRequest URL])];
}

// The end: the delegate (or the completion handler) hears it on the
// delegate queue, then the session forgets the task.
- (void)_finishOnEngine:(BOOL)onEngine error:(NSError *)error {
    if (_finished)
        return;
    _finished = YES;
    if (onEngine)
        [self _releaseEasy];
    if (_uploadFileHandle != NULL) {
        fclose(_uploadFileHandle);
        _uploadFileHandle = NULL;
    }
    [_uploadStream close];

    NSURLSessionTask *target = [self _target];
    if (target->_downloadFile != NULL) {
        fclose(target->_downloadFile);
        target->_downloadFile = NULL;
    }
    if (target != self)
        target->_finished = YES;

    for (NSURLSessionTask *task in target == self ? @[ self ] : @[ self, target ]) {
        pthread_mutex_lock(&task->_lock);
        task->_state = NSURLSessionTaskStateCompleted;
        task->_error = error;
        pthread_mutex_unlock(&task->_lock);
    }

    NSURLSession *session = _session;
    id handler = target->_completionHandler;
    target->_completionHandler = nil;
    NSData *collected = _collected ?: [NSData data];
    NSURLResponse *response = [self response];
    BOOL download = [self _isDownload];
    NSString *path = target->_downloadPath;
    BOOL keepFile = target->_keepDownloadFile;

    [self _onDelegateQueue:^{
        id delegate = [session delegate];
        if (download) {
            NSURL *location = path ? [NSURL fileURLWithPath:path] : nil;
            if (handler != nil) {
                ((void (^)(NSURL *, NSURLResponse *, NSError *))handler)(error ? nil : location, response, error);
            } else if (error == nil && location != nil &&
                       [delegate respondsToSelector:@selector(URLSession:downloadTask:didFinishDownloadingToURL:)]) {
                [delegate URLSession:session downloadTask:(NSURLSessionDownloadTask *)target
                    didFinishDownloadingToURL:location];
            }
            if (path != nil && !keepFile)
                unlink([path fileSystemRepresentation]);
        } else if (handler != nil) {
            ((void (^)(NSData *, NSURLResponse *, NSError *))handler)(error ? nil : collected, response, error);
        }
        if (handler == nil && [delegate respondsToSelector:@selector(URLSession:task:didCompleteWithError:)])
            [delegate URLSession:session task:target didCompleteWithError:error];
        [session _taskDidFinish:target];
        if (target != self)
            [session _taskDidFinish:self];
    }];
}

@end

static size_t onHeader(char *buffer, size_t size, size_t count, void *task) {
    return [(__bridge NSURLSessionTask *)task _headerLine:buffer length:size * count];
}

static size_t onBody(char *buffer, size_t size, size_t count, void *task) {
    return [(__bridge NSURLSessionTask *)task _body:buffer length:size * count];
}

static size_t onReadBody(char *buffer, size_t size, size_t count, void *task) {
    return [(__bridge NSURLSessionTask *)task _readBody:buffer length:size * count];
}

static int onSeekBody(void *task, curl_off_t offset, int origin) {
    return [(__bridge NSURLSessionTask *)task _seekBody:offset origin:origin];
}

static int onProgress(void *task, curl_off_t downloadTotal __attribute__((unused)),
                      curl_off_t downloaded __attribute__((unused)),
                      curl_off_t uploadTotal, curl_off_t uploaded) {
    return [(__bridge NSURLSessionTask *)task _uploaded:uploaded total:uploadTotal];
}

@implementation NSURLSessionDataTask
@end

@implementation NSURLSessionUploadTask
@end

@implementation NSURLSessionDownloadTask

// Resume data: where the bytes are, how many, and what identifies the
// resource (ETag, Last-Modified), as a property list.
- (void)cancelByProducingResumeData:(void (^)(NSData *))completionHandler {
    if (!engineStart()) {
        [self cancel];
        if (completionHandler) completionHandler(nil);
        return;
    }
    void (^handler)(NSData *) = [completionHandler copy];
    enginePerform(^{
        NSData *resumeData = nil;
        NSHTTPURLResponse *response = (NSHTTPURLResponse *)[self response];
        NSString *etag = [response isKindOfClass:[NSHTTPURLResponse class]]
            ? [response valueForHTTPHeaderField:@"ETag"] : nil;
        NSString *modified = [response isKindOfClass:[NSHTTPURLResponse class]]
            ? [response valueForHTTPHeaderField:@"Last-Modified"] : nil;
        if (!self->_finished && self->_downloadPath != nil && (etag != nil || modified != nil)) {
            if (self->_downloadFile != NULL)
                fflush(self->_downloadFile);
            NSMutableDictionary *info = [NSMutableDictionary dictionary];
            info[@"NSURLSessionResumeInfoVersion"] = @1;
            info[@"NSURLSessionDownloadURL"] = [[[self currentRequest] URL] absoluteString];
            info[@"NSURLSessionResumeBytesReceived"] = @([self countOfBytesReceived]);
            info[@"NSURLSessionResumeInfoLocalPath"] = self->_downloadPath;
            if (etag) info[@"NSURLSessionResumeEntityTag"] = etag;
            if (modified) info[@"NSURLSessionResumeServerDownloadDate"] = modified;
            resumeData = [NSPropertyListSerialization dataWithPropertyList:info
                                                                    format:NSPropertyListXMLFormat_v1_0
                                                                   options:0 error:NULL];
            if (resumeData != nil)
                self->_keepDownloadFile = YES;
        }
        pthread_mutex_lock(&self->_lock);
        BOOL active = self->_state == NSURLSessionTaskStateRunning || self->_state == NSURLSessionTaskStateSuspended;
        if (active)
            self->_state = NSURLSessionTaskStateCanceling;
        pthread_mutex_unlock(&self->_lock);
        [self _cancelTransfer];
        if (handler != nil)
            [self _onDelegateQueue:^{ handler(resumeData); }];
    });
}

@end

#pragma mark - Session

@implementation NSURLSession {
    pthread_mutex_t _lock;
    NSURLSessionConfiguration *_configuration;
    id<NSURLSessionDelegate> _delegate;
    NSOperationQueue *_delegateQueue;
    NSString *_sessionDescription;
    NSMutableArray *_tasks;
    atomic_ulong _nextIdentifier;
    BOOL _shared;
    BOOL _invalidating;
    BOOL _invalid;
}

+ (NSURLSession *)sharedSession {
    static NSURLSession *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        shared = [self sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]
                                       delegate:nil delegateQueue:nil];
        shared->_shared = YES;
    });
    return shared;
}

+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration {
    return [self sessionWithConfiguration:configuration delegate:nil delegateQueue:nil];
}

+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration
                                  delegate:(id<NSURLSessionDelegate>)delegate
                             delegateQueue:(NSOperationQueue *)queue {
    NSURLSession *session = [[self alloc] init];
    pthread_mutex_init(&session->_lock, NULL);
    session->_configuration = [configuration copy] ?: [NSURLSessionConfiguration defaultSessionConfiguration];
    session->_delegate = delegate;
    if (queue == nil) {
        queue = [[NSOperationQueue alloc] init];
        [queue setMaxConcurrentOperationCount:1];
        [queue setName:[NSString stringWithFormat:@"NSURLSession %p delegate queue", (__bridge void *)session]];
    }
    session->_delegateQueue = queue;
    session->_tasks = [NSMutableArray array];
    atomic_init(&session->_nextIdentifier, 1);
    return session;
}

- (NSURLSessionConfiguration *)configuration {
    return [_configuration copy];
}

- (id<NSURLSessionDelegate>)delegate {
    pthread_mutex_lock(&_lock);
    id<NSURLSessionDelegate> delegate = _delegate;
    pthread_mutex_unlock(&_lock);
    return delegate;
}

- (NSOperationQueue *)delegateQueue {
    return _delegateQueue;
}

- (NSString *)sessionDescription {
    pthread_mutex_lock(&_lock);
    NSString *description = _sessionDescription;
    pthread_mutex_unlock(&_lock);
    return description;
}

- (void)setSessionDescription:(NSString *)description {
    NSString *copy = [description copy];
    pthread_mutex_lock(&_lock);
    _sessionDescription = copy;
    pthread_mutex_unlock(&_lock);
}

- (NSUInteger)_nextTaskIdentifier {
    return (NSUInteger)atomic_fetch_add(&_nextIdentifier, 1);
}

- (void)_addTask:(NSURLSessionTask *)task {
    pthread_mutex_lock(&_lock);
    BOOL invalid = _invalidating || _invalid;
    if (!invalid)
        [_tasks addObject:task];
    pthread_mutex_unlock(&_lock);
    if (invalid)
        [NSException raise:NSGenericException
                    format:@"Task created in a session that has been invalidated"];
}

- (void)_taskDidFinish:(NSURLSessionTask *)task {
    pthread_mutex_lock(&_lock);
    [_tasks removeObjectIdenticalTo:task];
    BOOL becomeInvalid = _invalidating && !_invalid && [_tasks count] == 0;
    if (becomeInvalid)
        _invalid = YES;
    pthread_mutex_unlock(&_lock);
    if (becomeInvalid)
        [self _becomeInvalid];
}

// The delegate hears it last; the session lets go of it.
- (void)_becomeInvalid {
    id<NSURLSessionDelegate> delegate = [self delegate];
    [_delegateQueue addOperationWithBlock:^{
        if ([delegate respondsToSelector:@selector(URLSession:didBecomeInvalidWithError:)])
            [delegate URLSession:self didBecomeInvalidWithError:nil];
        pthread_mutex_lock(&self->_lock);
        self->_delegate = nil;
        pthread_mutex_unlock(&self->_lock);
    }];
}

- (void)finishTasksAndInvalidate {
    if (_shared)
        return;
    pthread_mutex_lock(&_lock);
    BOOL now = !_invalidating && [_tasks count] == 0;
    _invalidating = YES;
    if (now)
        _invalid = YES;
    pthread_mutex_unlock(&_lock);
    if (now)
        [self _becomeInvalid];
}

- (void)invalidateAndCancel {
    if (_shared)
        return;
    NSArray *tasks;
    pthread_mutex_lock(&_lock);
    tasks = [_tasks copy];
    pthread_mutex_unlock(&_lock);
    [self finishTasksAndInvalidate];
    for (NSURLSessionTask *task in tasks)
        [task cancel];
}

// No cache, cookies or credentials are kept: there is nothing to empty.
- (void)resetWithCompletionHandler:(void (^)(void))completionHandler {
    if (completionHandler)
        [_delegateQueue addOperationWithBlock:completionHandler];
}

- (void)flushWithCompletionHandler:(void (^)(void))completionHandler {
    if (completionHandler)
        [_delegateQueue addOperationWithBlock:completionHandler];
}

- (NSArray *)_runningTasks {
    NSMutableArray *result = [NSMutableArray array];
    pthread_mutex_lock(&_lock);
    for (NSURLSessionTask *task in _tasks)
        if ([task state] != NSURLSessionTaskStateCompleted)
            [result addObject:task];
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)getTasksWithCompletionHandler:(void (^)(NSArray<NSURLSessionDataTask *> *, NSArray<NSURLSessionUploadTask *> *,
                                                NSArray<NSURLSessionDownloadTask *> *))completionHandler {
    NSArray *tasks = [self _runningTasks];
    NSMutableArray *data = [NSMutableArray array], *upload = [NSMutableArray array],
                   *download = [NSMutableArray array];
    for (NSURLSessionTask *task in tasks) {
        if ([task isKindOfClass:[NSURLSessionUploadTask class]]) [upload addObject:task];
        else if ([task isKindOfClass:[NSURLSessionDataTask class]]) [data addObject:task];
        else if ([task isKindOfClass:[NSURLSessionDownloadTask class]]) [download addObject:task];
    }
    [_delegateQueue addOperationWithBlock:^{ completionHandler(data, upload, download); }];
}

- (void)getAllTasksWithCompletionHandler:(void (^)(NSArray<__kindof NSURLSessionTask *> *))completionHandler {
    NSArray *tasks = [self _runningTasks];
    [_delegateQueue addOperationWithBlock:^{ completionHandler(tasks); }];
}

#pragma mark Making tasks

- (id)_taskOfClass:(Class)class request:(NSURLRequest *)request kind:(NSURLSessionTaskKind)kind
           handler:(id)handler {
    if (request == nil || [request URL] == nil)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSURLSession taskWithRequest:]: nil URL"];
    NSURLRequest *effective = request;
    // The configuration's cache policy and timeout, unless the request has its own.
    NSURLSessionTask *task = [[class alloc] _initWithSession:self request:effective kind:kind];
    task->_completionHandler = [handler copy];
    [self _addTask:task];
    return task;
}

static NSURLRequest *requestForURL(NSURL *url) {
    if (url == nil)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSURLSession taskWithURL:]: nil URL"];
    return [NSURLRequest requestWithURL:url];
}

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request {
    return [self _taskOfClass:[NSURLSessionDataTask class] request:request kind:NSURLSessionTaskKindData handler:nil];
}

- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url {
    return [self dataTaskWithRequest:requestForURL(url)];
}

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request
                            completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [self _taskOfClass:[NSURLSessionDataTask class] request:request kind:NSURLSessionTaskKindData
                      handler:completionHandler];
}

- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url
                        completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [self dataTaskWithRequest:requestForURL(url) completionHandler:completionHandler];
}

- (NSURLSessionUploadTask *)_uploadWithRequest:(NSURLRequest *)request data:(NSData *)data file:(NSURL *)file
                                      streamed:(BOOL)streamed handler:(id)handler {
    NSURLSessionUploadTask *task = [self _taskOfClass:[NSURLSessionUploadTask class] request:request
                                                 kind:NSURLSessionTaskKindUpload handler:handler];
    task->_uploadData = data ? [data copy] : (file == nil && !streamed ? [NSData data] : nil);
    task->_uploadFile = file;
    task->_streamed = streamed;
    return task;
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL {
    return [self _uploadWithRequest:request data:nil file:fileURL streamed:NO handler:nil];
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData {
    return [self _uploadWithRequest:request data:bodyData ?: [NSData data] file:nil streamed:NO handler:nil];
}

- (NSURLSessionUploadTask *)uploadTaskWithStreamedRequest:(NSURLRequest *)request {
    return [self _uploadWithRequest:request data:nil file:nil streamed:YES handler:nil];
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL
                                completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [self _uploadWithRequest:request data:nil file:fileURL streamed:NO handler:completionHandler];
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData
                                completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [self _uploadWithRequest:request data:bodyData ?: [NSData data] file:nil streamed:NO
                            handler:completionHandler];
}

- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request {
    return [self _taskOfClass:[NSURLSessionDownloadTask class] request:request kind:NSURLSessionTaskKindDownload
                      handler:nil];
}

- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url {
    return [self downloadTaskWithRequest:requestForURL(url)];
}

- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request
                                    completionHandler:(void (^)(NSURL *, NSURLResponse *, NSError *))completionHandler {
    return [self _taskOfClass:[NSURLSessionDownloadTask class] request:request kind:NSURLSessionTaskKindDownload
                      handler:completionHandler];
}

- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url
                                completionHandler:(void (^)(NSURL *, NSURLResponse *, NSError *))completionHandler {
    return [self downloadTaskWithRequest:requestForURL(url) completionHandler:completionHandler];
}

- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData {
    return [self downloadTaskWithResumeData:resumeData completionHandler:nil];
}

// Resume data from cancelByProducingResumeData: continues where the bytes
// stopped, if the file is still there and the resource has not changed
// (If-Range); otherwise the download starts again.
- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData
                                       completionHandler:(void (^)(NSURL *, NSURLResponse *, NSError *))completionHandler {
    NSDictionary *info = resumeData ? [NSPropertyListSerialization propertyListWithData:resumeData options:0
                                                                                  format:NULL error:NULL] : nil;
    NSString *urlString = [info isKindOfClass:[NSDictionary class]] ? info[@"NSURLSessionDownloadURL"] : nil;
    NSURL *url = urlString ? [NSURL URLWithString:urlString] : nil;
    if (url == nil) {
        // Not resume data: the task fails at once.
        NSURLSessionDownloadTask *task =
            [self _taskOfClass:[NSURLSessionDownloadTask class]
                       request:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]
                          kind:NSURLSessionTaskKindDownload handler:completionHandler];
        return task;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    NSString *path = info[@"NSURLSessionResumeInfoLocalPath"];
    int64_t bytes = [info[@"NSURLSessionResumeBytesReceived"] longLongValue];
    NSString *validator = info[@"NSURLSessionResumeEntityTag"] ?: info[@"NSURLSessionResumeServerDownloadDate"];
    struct stat st;
    BOOL usable = path != nil && bytes > 0 && validator != nil &&
                  stat([path fileSystemRepresentation], &st) == 0 && st.st_size >= bytes &&
                  truncate([path fileSystemRepresentation], bytes) == 0;
    if (usable)
        [request setValue:validator forHTTPHeaderField:@"If-Range"];
    NSURLSessionDownloadTask *task = [self _taskOfClass:[NSURLSessionDownloadTask class] request:request
                                                   kind:NSURLSessionTaskKindDownload handler:completionHandler];
    if (usable) {
        task->_downloadPath = path;
        task->_resumeOffset = bytes;
    }
    return task;
}

@end
