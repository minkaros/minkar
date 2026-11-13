/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Vision/Vision.h>

NSString *const VNErrorDomain = @"com.apple.Vision";

@implementation VNRequest {
    VNRequestCompletionHandler _handler;
}

- (instancetype)init {
    return [self initWithCompletionHandler:nil];
}

- (instancetype)initWithCompletionHandler:(VNRequestCompletionHandler)completionHandler {
    if ((self = [super init]) != nil)
        _handler = [completionHandler copy];
    return self;
}

- (VNRequestCompletionHandler)completionHandler {
    return _handler;
}

- (NSArray *)results {
    return nil;
}

- (id)copyWithZone:(NSZone *)zone {
    VNRequest *copy = [[[self class] alloc] initWithCompletionHandler:_handler];
    copy.revision = self.revision;
    copy.usesCPUOnly = self.usesCPUOnly;
    return copy;
}

- (void)cancel {
}

@end

@implementation VNImageBasedRequest

- (instancetype)initWithCompletionHandler:(VNRequestCompletionHandler)completionHandler {
    if ((self = [super initWithCompletionHandler:completionHandler]) != nil)
        _regionOfInterest = CGRectMake(0, 0, 1, 1);
    return self;
}

@end

@implementation VNRecognizeTextRequest

+ (NSArray<NSString *> *)supportedRecognitionLanguagesForTextRecognitionLevel:(VNRequestTextRecognitionLevel)level
                                                                     revision:(NSUInteger)revision
                                                                        error:(NSError **)error {
    return @[];
}

- (NSArray<NSString *> *)supportedRecognitionLanguagesAndReturnError:(NSError **)error {
    return @[];
}

- (instancetype)initWithCompletionHandler:(VNRequestCompletionHandler)completionHandler {
    if ((self = [super initWithCompletionHandler:completionHandler]) != nil) {
        _recognitionLanguages = @[];
        _customWords = @[];
        _usesLanguageCorrection = YES;
    }
    return self;
}

@end

@implementation VNImageRequestHandler

- (instancetype)initWithCGImage:(CGImageRef)image options:(NSDictionary *)options {
    return [super init];
}

- (instancetype)initWithURL:(NSURL *)imageURL options:(NSDictionary *)options {
    return [super init];
}

- (instancetype)initWithData:(NSData *)imageData options:(NSDictionary *)options {
    return [super init];
}

- (BOOL)performRequests:(NSArray<VNRequest *> *)requests error:(NSError **)error {
    NSError *unsupported = [NSError errorWithDomain:VNErrorDomain code:VNErrorUnsupportedRequest
        userInfo:@{ NSLocalizedDescriptionKey: @"Image analysis is not available on this system." }];
    for (VNRequest *request in requests)
        if (request.completionHandler != nil)
            request.completionHandler(request, unsupported);
    if (error != NULL)
        *error = unsupported;
    return NO;
}

@end
