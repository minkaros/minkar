/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AVFoundation's errors.
 */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSErrorDomain const AVFoundationErrorDomain;

FOUNDATION_EXPORT NSString *const AVErrorDeviceKey;
FOUNDATION_EXPORT NSString *const AVErrorTimeKey;
FOUNDATION_EXPORT NSString *const AVErrorFileSizeKey;
FOUNDATION_EXPORT NSString *const AVErrorPIDKey;
FOUNDATION_EXPORT NSString *const AVErrorRecordingSuccessfullyFinishedKey;
FOUNDATION_EXPORT NSString *const AVErrorMediaTypeKey;
FOUNDATION_EXPORT NSString *const AVErrorMediaSubTypeKey;
FOUNDATION_EXPORT NSString *const AVErrorPresentationTimeStampKey;
FOUNDATION_EXPORT NSString *const AVErrorPersistentTrackIDKey;
FOUNDATION_EXPORT NSString *const AVErrorFileTypeKey;

typedef NS_ERROR_ENUM(AVFoundationErrorDomain, AVError) {
    AVErrorUnknown = -11800,
    AVErrorOutOfMemory = -11801,
    AVErrorSessionNotRunning = -11803,
    AVErrorDeviceAlreadyUsedByAnotherSession = -11804,
    AVErrorNoDataCaptured = -11805,
    AVErrorSessionConfigurationChanged = -11806,
    AVErrorDiskFull = -11807,
    AVErrorDeviceWasDisconnected = -11808,
    AVErrorMediaChanged = -11809,
    AVErrorMaximumDurationReached = -11810,
    AVErrorMaximumFileSizeReached = -11811,
    AVErrorMediaDiscontinuity = -11812,
    AVErrorMaximumNumberOfSamplesForFileFormatReached = -11813,
    AVErrorDeviceNotConnected = -11814,
    AVErrorDeviceInUseByAnotherApplication = -11815,
    AVErrorDeviceLockedForConfigurationByAnotherProcess = -11817,
    AVErrorExportFailed = -11820,
    AVErrorDecodeFailed = -11821,
    AVErrorInvalidSourceMedia = -11822,
    AVErrorFileAlreadyExists = -11823,
    AVErrorCompositionTrackSegmentsNotContiguous = -11824,
    AVErrorInvalidCompositionTrackSegmentDuration = -11825,
    AVErrorInvalidCompositionTrackSegmentSourceStartTime = -11826,
    AVErrorInvalidCompositionTrackSegmentSourceDuration = -11827,
    AVErrorFileFormatNotRecognized = -11828,
    AVErrorFileFailedToParse = -11829,
    AVErrorMaximumStillImageCaptureRequestsExceeded = -11830,
    AVErrorContentIsProtected = -11831,
    AVErrorNoImageAtTime = -11832,
    AVErrorDecoderNotFound = -11833,
    AVErrorEncoderNotFound = -11834,
    AVErrorContentIsNotAuthorized = -11835,
    AVErrorApplicationIsNotAuthorized = -11836,
    AVErrorOperationNotSupportedForAsset = -11838,
    AVErrorDecoderTemporarilyUnavailable = -11839,
    AVErrorEncoderTemporarilyUnavailable = -11840,
    AVErrorInvalidVideoComposition = -11841,
    AVErrorReferenceForbiddenByReferencePolicy = -11842,
    AVErrorInvalidOutputURLPathExtension = -11843,
    AVErrorScreenCaptureFailed = -11844,
    AVErrorDisplayWasDisabled = -11845,
    AVErrorTorchLevelUnavailable = -11846,
    AVErrorIncompatibleAsset = -11848,
    AVErrorFailedToLoadMediaData = -11849,
    AVErrorServerIncorrectlyConfigured = -11850,
    AVErrorApplicationIsNotAuthorizedToUseDevice = -11852,
    AVErrorFailedToParse = -11853,
    AVErrorFileTypeDoesNotSupportSampleReferences = -11854,
    AVErrorUndecodableMediaData = -11855,
    AVErrorAirPlayControllerRequiresInternet = -11856,
    AVErrorAirPlayReceiverRequiresInternet = -11857,
    AVErrorVideoCompositorFailed = -11858,
    AVErrorCreateContentKeyRequestFailed = -11860,
    AVErrorUnsupportedOutputSettings = -11861,
    AVErrorOperationNotAllowed = -11862,
    AVErrorContentIsUnavailable = -11863,
    AVErrorFormatUnsupported = -11864,
    AVErrorMalformedDepth = -11865,
    AVErrorContentNotUpdated = -11866,
    AVErrorNoLongerPlayable = -11867,
    AVErrorNoCompatibleAlternatesForExternalDisplay = -11868,
    AVErrorNoSourceTrack = -11869,
};
