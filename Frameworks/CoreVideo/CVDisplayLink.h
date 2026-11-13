#include <CoreFoundation/CoreFoundation.h>
#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#import <CoreVideo/CVBase.h>
#import <CoreVideo/CVReturn.h>
#import <OpenGL/OpenGL.h>
#include <CoreGraphics/CGDirectDisplay.h>

/* Usable from C, as the reference platform's: an object pointer in
   Objective-C, an opaque one in C. */
#ifdef __OBJC__
@class CVDisplayLink;
typedef CVDisplayLink *CVDisplayLinkRef;
#else
typedef struct CVDisplayLink *CVDisplayLinkRef;
#endif

typedef CVReturn (*CVDisplayLinkOutputCallback)(CVDisplayLinkRef, const CVTimeStamp *, const CVTimeStamp *, CVOptionFlags, CVOptionFlags *, void *);

COREVIDEO_EXPORT CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef *result);
COREVIDEO_EXPORT CVReturn CVDisplayLinkCreateWithCGDisplay(CGDirectDisplayID displayID, CVDisplayLinkRef *result);
COREVIDEO_EXPORT CVReturn CVDisplayLinkCreateWithCGDisplays(CGDirectDisplayID *displayArray, CFIndex count, CVDisplayLinkRef *result);
COREVIDEO_EXPORT CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef self, CVDisplayLinkOutputCallback callback, void *userInfo);
COREVIDEO_EXPORT CVReturn CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(CVDisplayLinkRef self, CGLContextObj cglContext, CGLPixelFormatObj cglPixelFormat);

COREVIDEO_EXPORT CVReturn CVDisplayLinkStart(CVDisplayLinkRef self);
COREVIDEO_EXPORT CGDirectDisplayID CVDisplayLinkGetCurrentCGDisplay(CVDisplayLinkRef self);
COREVIDEO_EXPORT CVReturn CVDisplayLinkSetCurrentCGDisplay(CVDisplayLinkRef self, CGDirectDisplayID displayID);
COREVIDEO_EXPORT CVTime CVDisplayLinkGetNominalOutputVideoRefreshPeriod(CVDisplayLinkRef self);
COREVIDEO_EXPORT double CVDisplayLinkGetActualOutputVideoRefreshPeriod(CVDisplayLinkRef self);
COREVIDEO_EXPORT CVTime CVDisplayLinkGetOutputVideoLatency(CVDisplayLinkRef self);
COREVIDEO_EXPORT CVReturn CVDisplayLinkGetCurrentTime(CVDisplayLinkRef self, CVTimeStamp *outTime);
COREVIDEO_EXPORT CVReturn CVDisplayLinkStop(CVDisplayLinkRef self);
COREVIDEO_EXPORT Boolean CVDisplayLinkIsRunning(CVDisplayLinkRef self);

COREVIDEO_EXPORT CVDisplayLinkRef CVDisplayLinkRetain(CVDisplayLinkRef self);
COREVIDEO_EXPORT void CVDisplayLinkRelease(CVDisplayLinkRef self);
