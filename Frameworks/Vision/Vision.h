/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const VNErrorDomain;

typedef NS_ENUM(NSInteger, VNErrorCode) {
    VNErrorOK = 0,
    VNErrorRequestCancelled,
    VNErrorInvalidFormat,
    VNErrorOperationFailed,
    VNErrorOutOfBoundsError,
    VNErrorInvalidOption,
    VNErrorIOError,
    VNErrorMissingOption,
    VNErrorNotImplemented,
    VNErrorInternalError,
    VNErrorOutOfMemory,
    VNErrorUnknownError,
    VNErrorInvalidOperation,
    VNErrorInvalidImage,
    VNErrorInvalidArgument,
    VNErrorInvalidModel,
    VNErrorUnsupportedRevision,
    VNErrorDataUnavailable,
    VNErrorTimeStampNotFound,
    VNErrorUnsupportedRequest,
};

typedef NS_ENUM(NSInteger, VNRequestTextRecognitionLevel) {
    VNRequestTextRecognitionLevelAccurate = 0,
    VNRequestTextRecognitionLevelFast = 1,
};

@class VNRequest;
typedef void (^VNRequestCompletionHandler)(VNRequest *request, NSError * _Nullable error);

@interface VNRequest : NSObject <NSCopying>
- (instancetype)init;
- (instancetype)initWithCompletionHandler:(nullable VNRequestCompletionHandler)completionHandler;
@property (readonly, nullable, copy) VNRequestCompletionHandler completionHandler;
@property (readonly, nullable, copy) NSArray *results;
@property (assign) NSUInteger revision;
@property (assign) BOOL usesCPUOnly;
- (void)cancel;
@end

@interface VNImageBasedRequest : VNRequest
@property (assign) CGRect regionOfInterest;
@end

@interface VNRecognizeTextRequest : VNImageBasedRequest
+ (nullable NSArray<NSString *> *)supportedRecognitionLanguagesForTextRecognitionLevel:(VNRequestTextRecognitionLevel)level
                                                                             revision:(NSUInteger)revision
                                                                                error:(NSError **)error;
- (nullable NSArray<NSString *> *)supportedRecognitionLanguagesAndReturnError:(NSError **)error;
@property (copy) NSArray<NSString *> *recognitionLanguages;
@property (copy) NSArray<NSString *> *customWords;
@property VNRequestTextRecognitionLevel recognitionLevel;
@property BOOL usesLanguageCorrection;
@property BOOL automaticallyDetectsLanguage;
@property float minimumTextHeight;
@end

@interface VNImageRequestHandler : NSObject
- (instancetype)initWithCGImage:(CGImageRef)image options:(NSDictionary *)options;
- (instancetype)initWithURL:(NSURL *)imageURL options:(NSDictionary *)options;
- (instancetype)initWithData:(NSData *)imageData options:(NSDictionary *)options;
- (BOOL)performRequests:(NSArray<VNRequest *> *)requests error:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
