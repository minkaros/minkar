#import <Foundation/NSObject.h>

enum {
    NSURLErrorUnknown = -1,
    NSURLErrorBackgroundSessionRequiresSharedContainer = -995,
    NSURLErrorBackgroundSessionInUseByAnotherProcess = -996,
    NSURLErrorBackgroundSessionWasDisconnected = -997,
    NSURLErrorCancelled = -999,
    NSURLErrorBadURL = -1000,
    NSURLErrorTimedOut = -1001,
    NSURLErrorUnsupportedURL = -1002,
    NSURLErrorCannotFindHost = -1003,
    NSURLErrorCannotConnectToHost = -1004,
    NSURLErrorNetworkConnectionLost = -1005,
    NSURLErrorDNSLookupFailed = -1006,
    NSURLErrorHTTPTooManyRedirects = -1007,
    NSURLErrorResourceUnavailable = -1008,
    NSURLErrorNotConnectedToInternet = -1009,
    NSURLErrorRedirectToNonExistentLocation = -1010,
    NSURLErrorBadServerResponse = -1011,
    NSURLErrorUserCancelledAuthentication = -1012,
    NSURLErrorUserAuthenticationRequired = -1013,
    NSURLErrorZeroByteResource = -1014,
    NSURLErrorCannotDecodeRawData = -1015,
    NSURLErrorCannotDecodeContentData = -1016,
    NSURLErrorCannotParseResponse = -1017,
    NSURLErrorInternationalRoamingOff = -1018,
    NSURLErrorCallIsActive = -1019,
    NSURLErrorDataNotAllowed = -1020,
    NSURLErrorRequestBodyStreamExhausted = -1021,
    NSURLErrorAppTransportSecurityRequiresSecureConnection = -1022,
    NSURLErrorFileDoesNotExist = -1100,
    NSURLErrorFileIsDirectory = -1101,
    NSURLErrorNoPermissionsToReadFile = -1102,
    NSURLErrorDataLengthExceedsMaximum = -1103,
    NSURLErrorFileOutsideSafeArea = -1104,
    NSURLErrorSecureConnectionFailed = -1200,
    NSURLErrorServerCertificateHasBadDate = -1201,
    NSURLErrorServerCertificateUntrusted = -1202,
    NSURLErrorServerCertificateHasUnknownRoot = -1203,
    NSURLErrorServerCertificateNotYetValid = -1204,
    NSURLErrorClientCertificateRejected = -1205,
    NSURLErrorClientCertificateRequired = -1206,
    NSURLErrorCannotLoadFromNetwork = -2000,
    NSURLErrorCannotCreateFile = -3000,
    NSURLErrorCannotOpenFile = -3001,
    NSURLErrorCannotCloseFile = -3002,
    NSURLErrorCannotWriteToFile = -3003,
    NSURLErrorCannotRemoveFile = -3004,
    NSURLErrorCannotMoveFile = -3005,
    NSURLErrorDownloadDecodingFailedMidStream = -3006,
    NSURLErrorDownloadDecodingFailedToComplete = -3007,
};

FOUNDATION_EXPORT NSString *const NSURLErrorDomain;
FOUNDATION_EXPORT NSString *const NSURLErrorFailingURLErrorKey;
FOUNDATION_EXPORT NSString *const NSURLErrorFailingURLStringErrorKey;
FOUNDATION_EXPORT NSString *const NSURLErrorFailingURLPeerTrustErrorKey;
FOUNDATION_EXPORT NSString *const NSURLErrorBackgroundTaskCancelledReasonKey;
FOUNDATION_EXPORT NSString *const NSURLErrorNetworkUnavailableReasonKey;

enum {
    NSURLErrorCancelledReasonUserForceQuitApplication = 0,
    NSURLErrorCancelledReasonBackgroundUpdatesDisabled = 1,
    NSURLErrorCancelledReasonInsufficientSystemResources = 2
};

typedef NS_ENUM(NSInteger, NSURLErrorNetworkUnavailableReason) {
    NSURLErrorNetworkUnavailableReasonCellular = 0,
    NSURLErrorNetworkUnavailableReasonExpensive = 1,
    NSURLErrorNetworkUnavailableReasonConstrained = 2
};
