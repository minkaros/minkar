/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSURL, NSInputStream, NSData, NSString, NSDictionary<KeyType, ObjectType>, NSMutableDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, NSURLRequestCachePolicy) {
    NSURLRequestUseProtocolCachePolicy = 0,
    NSURLRequestReloadIgnoringLocalCacheData = 1,
    NSURLRequestReloadIgnoringLocalAndRemoteCacheData = 4,
    NSURLRequestReloadIgnoringCacheData = NSURLRequestReloadIgnoringLocalCacheData,
    NSURLRequestReturnCacheDataElseLoad = 2,
    NSURLRequestReturnCacheDataDontLoad = 3,
    NSURLRequestReloadRevalidatingCacheData = 5,
};

typedef NS_ENUM(NSUInteger, NSURLRequestNetworkServiceType) {
    NSURLNetworkServiceTypeDefault = 0,
    NSURLNetworkServiceTypeVoIP = 1,
    NSURLNetworkServiceTypeVideo = 2,
    NSURLNetworkServiceTypeBackground = 3,
    NSURLNetworkServiceTypeVoice = 4,
    NSURLNetworkServiceTypeResponsiveData = 6,
    NSURLNetworkServiceTypeAVStreaming = 8,
    NSURLNetworkServiceTypeResponsiveAV = 9,
    NSURLNetworkServiceTypeCallSignaling = 11,
};

@interface NSURLRequest : NSObject <NSSecureCoding, NSCopying, NSMutableCopying> {
    NSURL *_url;
    NSURLRequestCachePolicy _cachePolicy;
    NSTimeInterval _timeoutInterval;
    NSString *_method;
    id _bodyDataOrStream;
    NSMutableDictionary *_headerFields;
    NSURL *_mainDocumentURL;
    BOOL _handleCookies;
    BOOL _usePipelining;
    BOOL _allowsCellular;
    BOOL _allowsExpensive;
    BOOL _allowsConstrained;
    NSURLRequestNetworkServiceType _serviceType;
}

+ (instancetype)requestWithURL:(NSURL *)URL;
@property (class, readonly) BOOL supportsSecureCoding;
+ (instancetype)requestWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval;
- (instancetype)initWithURL:(NSURL *)URL;
- (instancetype)initWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval NS_DESIGNATED_INITIALIZER;

@property (nullable, readonly, copy) NSURL *URL;
@property (readonly) NSURLRequestCachePolicy cachePolicy;
@property (readonly) NSTimeInterval timeoutInterval;
@property (nullable, readonly, copy) NSURL *mainDocumentURL;
@property (readonly) NSURLRequestNetworkServiceType networkServiceType;
@property (readonly) BOOL allowsCellularAccess;
@property (readonly) BOOL allowsExpensiveNetworkAccess;
@property (readonly) BOOL allowsConstrainedNetworkAccess;

@end

@interface NSURLRequest (NSHTTPURLRequest)
@property (nullable, readonly, copy) NSString *HTTPMethod;
@property (nullable, readonly, copy) NSDictionary<NSString *, NSString *> *allHTTPHeaderFields;
- (nullable NSString *)valueForHTTPHeaderField:(NSString *)field;
@property (nullable, readonly, copy) NSData *HTTPBody;
@property (nullable, readonly, retain) NSInputStream *HTTPBodyStream;
@property (readonly) BOOL HTTPShouldHandleCookies;
@property (readonly) BOOL HTTPShouldUsePipelining;
@end

NS_ASSUME_NONNULL_END
