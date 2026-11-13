#import <CoreFoundation/CoreFoundation.h>

#define COREVIDEO_EXPORT extern

typedef uint64_t CVOptionFlags;

struct CVSMPTETime {
    SInt16 subframes;
    SInt16 subframeDivisor;
    UInt32 counter;
    UInt32 type;
    UInt32 flags;
    SInt16 hours;
    SInt16 minutes;
    SInt16 seconds;
    SInt16 frames;
};

typedef struct CVSMPTETime CVSMPTETime;

typedef struct {
    uint32_t version;
    int32_t videoTimeScale;
    int64_t videoTime;
    uint64_t hostTime;
    double rateScalar;
    int64_t videoRefreshPeriod;
    CVSMPTETime smpteTime;
    uint64_t flags;
    uint64_t reserved;
} CVTimeStamp;

/* CVTimeStamp's flags. */
enum {
    kCVTimeStampVideoTimeValid = (1 << 0),
    kCVTimeStampHostTimeValid = (1 << 1),
    kCVTimeStampSMPTETimeValid = (1 << 2),
    kCVTimeStampVideoRefreshPeriodValid = (1 << 3),
    kCVTimeStampRateScalarValid = (1 << 4),
    kCVTimeStampTopField = (1 << 16),
    kCVTimeStampBottomField = (1 << 17),
    kCVTimeStampVideoHostTimeValid = (kCVTimeStampVideoTimeValid | kCVTimeStampHostTimeValid),
    kCVTimeStampIsInterlaced = (kCVTimeStampTopField | kCVTimeStampBottomField)
};

/* The host clock: mach absolute time. */
COREVIDEO_EXPORT uint64_t CVGetCurrentHostTime(void);
COREVIDEO_EXPORT double CVGetHostClockFrequency(void);
COREVIDEO_EXPORT uint32_t CVGetHostClockMinimumTimeDelta(void);

/* A time as a rational number of seconds (CVDisplayLink's periods). */
typedef struct {
    int64_t timeValue;
    int32_t timeScale;
    int32_t flags;
} CVTime;

enum {
    kCVTimeIsIndefinite = 1 << 0
};
