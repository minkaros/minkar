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

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSHTTPCookieStorage.h>
#import <Foundation/NSProgress.h>

@class NSCachedURLResponse, NSData, NSError, NSHTTPURLResponse, NSInputStream, NSOperationQueue, NSURL, NSURLAuthenticationChallenge, NSURLCache, NSURLCredential, NSURLCredentialStorage, NSURLProtectionSpace, NSURLResponse;
@class NSURLSession, NSURLSessionConfiguration, NSURLSessionDataTask, NSURLSessionDownloadTask, NSURLSessionTask, NSURLSessionUploadTask;
@protocol NSURLSessionDelegate;

FOUNDATION_EXPORT const int64_t NSURLSessionTransferSizeUnknown;

@interface NSURLSession : NSObject

@property (class, readonly, retain) NSURLSession *sharedSession;

+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration;
+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration
                                  delegate:(id<NSURLSessionDelegate>)delegate
                             delegateQueue:(NSOperationQueue *)queue;

@property (readonly, retain) NSOperationQueue *delegateQueue;
@property (readonly, retain) id<NSURLSessionDelegate> delegate;
@property (readonly, copy) NSURLSessionConfiguration *configuration;
@property (copy) NSString *sessionDescription;

- (void)finishTasksAndInvalidate;
- (void)invalidateAndCancel;
- (void)resetWithCompletionHandler:(void (^)(void))completionHandler;
- (void)flushWithCompletionHandler:(void (^)(void))completionHandler;
- (void)getTasksWithCompletionHandler:(void (^)(NSArray<NSURLSessionDataTask *> *dataTasks,
                                                NSArray<NSURLSessionUploadTask *> *uploadTasks,
                                                NSArray<NSURLSessionDownloadTask *> *downloadTasks))completionHandler;
- (void)getAllTasksWithCompletionHandler:(void (^)(NSArray<__kindof NSURLSessionTask *> *tasks))completionHandler;

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData;
- (NSURLSessionUploadTask *)uploadTaskWithStreamedRequest:(NSURLRequest *)request;
- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request;
- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url;
- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData;

@end

@interface NSURLSession (NSURLSessionAsynchronousConvenience)

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request
                            completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url
                        completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromFile:(NSURL *)fileURL
                                completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData
                                completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request
                                    completionHandler:(void (^)(NSURL *location, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url
                                completionHandler:(void (^)(NSURL *location, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionDownloadTask *)downloadTaskWithResumeData:(NSData *)resumeData
                                       completionHandler:(void (^)(NSURL *location, NSURLResponse *response, NSError *error))completionHandler;

@end

typedef NS_ENUM(NSInteger, NSURLSessionTaskState) {
    NSURLSessionTaskStateRunning = 0,
    NSURLSessionTaskStateSuspended = 1,
    NSURLSessionTaskStateCanceling = 2,
    NSURLSessionTaskStateCompleted = 3,
};

FOUNDATION_EXPORT const float NSURLSessionTaskPriorityDefault;
FOUNDATION_EXPORT const float NSURLSessionTaskPriorityLow;
FOUNDATION_EXPORT const float NSURLSessionTaskPriorityHigh;

@interface NSURLSessionTask : NSObject <NSCopying, NSProgressReporting>

@property (readonly) NSUInteger taskIdentifier;
@property (readonly, copy) NSURLRequest *originalRequest;
@property (readonly, copy) NSURLRequest *currentRequest;
@property (readonly, copy) NSURLResponse *response;
@property (readonly, retain) NSProgress *progress;
@property (copy) NSDate *earliestBeginDate;
@property int64_t countOfBytesClientExpectsToSend;
@property int64_t countOfBytesClientExpectsToReceive;
@property (readonly) int64_t countOfBytesReceived;
@property (readonly) int64_t countOfBytesSent;
@property (readonly) int64_t countOfBytesExpectedToSend;
@property (readonly) int64_t countOfBytesExpectedToReceive;
@property (copy) NSString *taskDescription;
@property (readonly) NSURLSessionTaskState state;
@property (readonly, copy) NSError *error;
@property float priority;

- (void)cancel;
- (void)suspend;
- (void)resume;

@end

@interface NSURLSessionDataTask : NSURLSessionTask
@end

@interface NSURLSessionUploadTask : NSURLSessionDataTask
@end

@interface NSURLSessionDownloadTask : NSURLSessionTask
- (void)cancelByProducingResumeData:(void (^)(NSData *resumeData))completionHandler;
@end

typedef NS_ENUM(NSInteger, NSURLSessionMultipathServiceType) {
    NSURLSessionMultipathServiceTypeNone = 0,
    NSURLSessionMultipathServiceTypeHandover = 1,
    NSURLSessionMultipathServiceTypeInteractive = 2,
    NSURLSessionMultipathServiceTypeAggregate = 3
};

typedef NS_ENUM(uint16_t, tls_protocol_version_t) {
    tls_protocol_version_TLSv10 = 0x0301,
    tls_protocol_version_TLSv11 = 0x0302,
    tls_protocol_version_TLSv12 = 0x0303,
    tls_protocol_version_TLSv13 = 0x0304,
    tls_protocol_version_DTLSv10 = 0xfeff,
    tls_protocol_version_DTLSv12 = 0xfefd,
};

@interface NSURLSessionConfiguration : NSObject <NSCopying>

@property (class, readonly, strong) NSURLSessionConfiguration *defaultSessionConfiguration;
@property (class, readonly, strong) NSURLSessionConfiguration *ephemeralSessionConfiguration;
+ (NSURLSessionConfiguration *)backgroundSessionConfigurationWithIdentifier:(NSString *)identifier;
+ (NSURLSessionConfiguration *)backgroundSessionConfiguration:(NSString *)identifier;

@property (readonly, copy) NSString *identifier;
@property NSURLRequestCachePolicy requestCachePolicy;
@property NSTimeInterval timeoutIntervalForRequest;
@property NSTimeInterval timeoutIntervalForResource;
@property NSURLRequestNetworkServiceType networkServiceType;
@property BOOL allowsCellularAccess;
@property BOOL allowsExpensiveNetworkAccess;
@property BOOL allowsConstrainedNetworkAccess;
@property BOOL waitsForConnectivity;
@property (getter=isDiscretionary) BOOL discretionary;
@property (copy) NSString *sharedContainerIdentifier;
@property BOOL sessionSendsLaunchEvents;
@property (copy) NSDictionary *connectionProxyDictionary;
@property uint16_t TLSMinimumSupportedProtocol;
@property uint16_t TLSMaximumSupportedProtocol;
@property tls_protocol_version_t TLSMinimumSupportedProtocolVersion;
@property tls_protocol_version_t TLSMaximumSupportedProtocolVersion;
@property BOOL HTTPShouldUsePipelining;
@property BOOL HTTPShouldSetCookies;
@property NSHTTPCookieAcceptPolicy HTTPCookieAcceptPolicy;
@property (copy) NSDictionary *HTTPAdditionalHeaders;
@property NSInteger HTTPMaximumConnectionsPerHost;
@property (retain) NSHTTPCookieStorage *HTTPCookieStorage;
@property (retain) NSURLCredentialStorage *URLCredentialStorage;
@property (retain) NSURLCache *URLCache;
@property BOOL shouldUseExtendedBackgroundIdleMode;
@property (copy) NSArray<Class> *protocolClasses;
@property NSURLSessionMultipathServiceType multipathServiceType;

@end

typedef NS_ENUM(NSInteger, NSURLSessionDelayedRequestDisposition) {
    NSURLSessionDelayedRequestContinueLoading = 0,
    NSURLSessionDelayedRequestUseNewRequest = 1,
    NSURLSessionDelayedRequestCancel = 2,
};

typedef NS_ENUM(NSInteger, NSURLSessionAuthChallengeDisposition) {
    NSURLSessionAuthChallengeUseCredential = 0,
    NSURLSessionAuthChallengePerformDefaultHandling = 1,
    NSURLSessionAuthChallengeCancelAuthenticationChallenge = 2,
    NSURLSessionAuthChallengeRejectProtectionSpace = 3,
};

typedef NS_ENUM(NSInteger, NSURLSessionResponseDisposition) {
    NSURLSessionResponseCancel = 0,
    NSURLSessionResponseAllow = 1,
    NSURLSessionResponseBecomeDownload = 2,
    NSURLSessionResponseBecomeStream = 3,
};

@protocol NSURLSessionDelegate <NSObject>
@optional
- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error;
- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler;
@end

@protocol NSURLSessionTaskDelegate <NSURLSessionDelegate>
@optional
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
willBeginDelayedRequest:(NSURLRequest *)request
 completionHandler:(void (^)(NSURLSessionDelayedRequestDisposition disposition, NSURLRequest *newRequest))completionHandler;
- (void)URLSession:(NSURLSession *)session taskIsWaitingForConnectivity:(NSURLSessionTask *)task;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request
 completionHandler:(void (^)(NSURLRequest *))completionHandler;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
 needNewBodyStream:(void (^)(NSInputStream *bodyStream))completionHandler;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
   didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent
totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error;
@end

@protocol NSURLSessionDataDelegate <NSURLSessionTaskDelegate>
@optional
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask
didReceiveResponse:(NSURLResponse *)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler;
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask
didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask;
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data;
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask
 willCacheResponse:(NSCachedURLResponse *)proposedResponse
 completionHandler:(void (^)(NSCachedURLResponse *cachedResponse))completionHandler;
@end

@protocol NSURLSessionDownloadDelegate <NSURLSessionTaskDelegate>
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask
didFinishDownloadingToURL:(NSURL *)location;
@optional
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask
      didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten
totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite;
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask
 didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes;
@end

FOUNDATION_EXPORT NSString *const NSURLSessionDownloadTaskResumeData;

NS_ASSUME_NONNULL_BEGIN

/* -- Metrics (API 19): what a task's transactions took. -- */

typedef NS_ENUM(uint16_t, tls_ciphersuite_t) {
    tls_ciphersuite_RSA_WITH_AES_128_GCM_SHA256 = 0x009C,
    tls_ciphersuite_RSA_WITH_AES_256_GCM_SHA384 = 0x009D,
    tls_ciphersuite_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 = 0xC02B,
    tls_ciphersuite_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 = 0xC02C,
    tls_ciphersuite_ECDHE_RSA_WITH_AES_128_GCM_SHA256 = 0xC02F,
    tls_ciphersuite_ECDHE_RSA_WITH_AES_256_GCM_SHA384 = 0xC030,
    tls_ciphersuite_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 = 0xCCA9,
    tls_ciphersuite_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 = 0xCCA8,
    tls_ciphersuite_AES_128_GCM_SHA256 = 0x1301,
    tls_ciphersuite_AES_256_GCM_SHA384 = 0x1302,
    tls_ciphersuite_CHACHA20_POLY1305_SHA256 = 0x1303,
};

typedef NS_ENUM(NSInteger, NSURLSessionTaskMetricsResourceFetchType) {
    NSURLSessionTaskMetricsResourceFetchTypeUnknown,
    NSURLSessionTaskMetricsResourceFetchTypeNetworkLoad,
    NSURLSessionTaskMetricsResourceFetchTypeServerPush,
    NSURLSessionTaskMetricsResourceFetchTypeLocalCache,
};

@class NSDateInterval;

@interface NSURLSessionTaskTransactionMetrics : NSObject
@property (copy, readonly) NSURLRequest *request;
@property (nullable, copy, readonly) NSURLResponse *response;
@property (nullable, copy, readonly) NSDate *fetchStartDate;
@property (nullable, copy, readonly) NSDate *domainLookupStartDate;
@property (nullable, copy, readonly) NSDate *domainLookupEndDate;
@property (nullable, copy, readonly) NSDate *connectStartDate;
@property (nullable, copy, readonly) NSDate *secureConnectionStartDate;
@property (nullable, copy, readonly) NSDate *secureConnectionEndDate;
@property (nullable, copy, readonly) NSDate *connectEndDate;
@property (nullable, copy, readonly) NSDate *requestStartDate;
@property (nullable, copy, readonly) NSDate *requestEndDate;
@property (nullable, copy, readonly) NSDate *responseStartDate;
@property (nullable, copy, readonly) NSDate *responseEndDate;
@property (nullable, copy, readonly) NSString *networkProtocolName;
@property (assign, readonly, getter=isProxyConnection) BOOL proxyConnection;
@property (assign, readonly, getter=isReusedConnection) BOOL reusedConnection;
@property (assign, readonly) NSURLSessionTaskMetricsResourceFetchType resourceFetchType;
@property (readonly) int64_t countOfRequestHeaderBytesSent;
@property (readonly) int64_t countOfRequestBodyBytesSent;
@property (readonly) int64_t countOfRequestBodyBytesBeforeEncoding;
@property (readonly) int64_t countOfResponseHeaderBytesReceived;
@property (readonly) int64_t countOfResponseBodyBytesReceived;
@property (readonly) int64_t countOfResponseBodyBytesAfterDecoding;
@property (nullable, copy, readonly) NSString *localAddress;
@property (nullable, copy, readonly) NSNumber *localPort NS_REFINED_FOR_SWIFT;
@property (nullable, copy, readonly) NSString *remoteAddress;
@property (nullable, copy, readonly) NSNumber *remotePort NS_REFINED_FOR_SWIFT;
@property (nullable, copy, readonly) NSNumber *negotiatedTLSProtocolVersion NS_REFINED_FOR_SWIFT;
@property (nullable, copy, readonly) NSNumber *negotiatedTLSCipherSuite NS_REFINED_FOR_SWIFT;
@property (readonly, getter=isCellular) BOOL cellular;
@property (readonly, getter=isExpensive) BOOL expensive;
@property (readonly, getter=isConstrained) BOOL constrained;
@property (readonly, getter=isMultipath) BOOL multipath;
@end

@interface NSURLSessionTaskMetrics : NSObject
@property (copy, readonly) NSArray<NSURLSessionTaskTransactionMetrics *> *transactionMetrics;
@property (copy, readonly) NSDateInterval *taskInterval;
@property (assign, readonly) NSUInteger redirectCount;
@end

/* -- WebSockets (API 19). Minkar's sessions have no WebSocket transport
 * yet: such a task fails, as an unsupported URL would. -- */

typedef NS_ENUM(NSInteger, NSURLSessionWebSocketMessageType) {
    NSURLSessionWebSocketMessageTypeData = 0,
    NSURLSessionWebSocketMessageTypeString = 1,
};

NS_REFINED_FOR_SWIFT
@interface NSURLSessionWebSocketMessage : NSObject {
    NSURLSessionWebSocketMessageType _type;
    NSData *_data;
    NSString *_string;
}
- (instancetype)initWithData:(NSData *)data;
- (instancetype)initWithString:(NSString *)string;
@property (readonly) NSURLSessionWebSocketMessageType type;
@property (nullable, readonly, copy) NSData *data;
@property (nullable, readonly, copy) NSString *string;
@end

typedef NS_ENUM(NSInteger, NSURLSessionWebSocketCloseCode) {
    NSURLSessionWebSocketCloseCodeInvalid = 0,
    NSURLSessionWebSocketCloseCodeNormalClosure = 1000,
    NSURLSessionWebSocketCloseCodeGoingAway = 1001,
    NSURLSessionWebSocketCloseCodeProtocolError = 1002,
    NSURLSessionWebSocketCloseCodeUnsupportedData = 1003,
    NSURLSessionWebSocketCloseCodeNoStatusReceived = 1005,
    NSURLSessionWebSocketCloseCodeAbnormalClosure = 1006,
    NSURLSessionWebSocketCloseCodeInvalidFramePayloadData = 1007,
    NSURLSessionWebSocketCloseCodePolicyViolation = 1008,
    NSURLSessionWebSocketCloseCodeMessageTooBig = 1009,
    NSURLSessionWebSocketCloseCodeMandatoryExtensionMissing = 1010,
    NSURLSessionWebSocketCloseCodeInternalServerError = 1011,
    NSURLSessionWebSocketCloseCodeTLSHandshakeFailure = 1015,
};

NS_SWIFT_NAME(URLSessionWebSocketTask)
@interface NSURLSessionWebSocketTask : NSURLSessionTask
- (void)sendMessage:(NSURLSessionWebSocketMessage *)message completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_REFINED_FOR_SWIFT;
- (void)receiveMessageWithCompletionHandler:(void (^)(NSURLSessionWebSocketMessage * _Nullable message, NSError * _Nullable error))completionHandler NS_REFINED_FOR_SWIFT;
- (void)sendPingWithPongReceiveHandler:(void (^)(NSError * _Nullable error))pongReceiveHandler;
- (void)cancelWithCloseCode:(NSURLSessionWebSocketCloseCode)closeCode reason:(nullable NSData *)reason;
@property NSInteger maximumMessageSize;
@property (readonly) NSURLSessionWebSocketCloseCode closeCode;
@property (nullable, readonly, copy) NSData *closeReason;
@end

@interface NSURLSession (NSURLSessionWebSocket)
- (NSURLSessionWebSocketTask *)webSocketTaskWithURL:(NSURL *)url;
- (NSURLSessionWebSocketTask *)webSocketTaskWithURL:(NSURL *)url protocols:(NSArray<NSString *> *)protocols;
- (NSURLSessionWebSocketTask *)webSocketTaskWithRequest:(NSURLRequest *)request;
@end

NS_ASSUME_NONNULL_END
